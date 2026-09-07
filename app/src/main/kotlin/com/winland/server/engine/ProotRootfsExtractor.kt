package com.winland.server.engine

import android.util.Log
import java.io.BufferedInputStream
import java.io.File
import java.io.FileInputStream
import java.nio.file.Files
import java.util.zip.GZIPInputStream
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.withContext
import org.apache.commons.compress.archivers.tar.TarArchiveEntry
import org.apache.commons.compress.archivers.tar.TarArchiveInputStream
import org.tukaani.xz.XZInputStream

/**
 * Pure-Java rootfs extractor for rootless (proot) mode.
 *
 * The shell extraction script relies on `$filesDir/bin/busybox`, which can
 * not be executed on targetSdk 29+ devices (W^X: `app_data_file` is not
 * executable for `untrusted_app`). In proot mode every extraction path is
 * app-private storage owned by the app UID, so no privilege is needed and
 * the whole flow — tar decode, wrapping-dir unwrap, validation, staged
 * swap — can run in-process with zero native dependencies.
 *
 * Supported formats (auto-detected by magic bytes, like the shell script):
 * `.tar.gz` / `.tgz`, `.tar.xz`, plain `.tar`.
 *
 * Symlinks and executable bits are preserved (critical for a bootable
 * guest: `/bin/sh`, `/bin/bash`, ...). Owner/group and suid bits can not
 * be applied without root; proot's `-0` fake-root covers the guest side.
 */
object ProotRootfsExtractor {

    private const val TAG = "ProotRootfsExtractor"

    private val GZIP_MAGIC = byteArrayOf(0x1f.toByte(), 0x8b.toByte())
    private val XZ_MAGIC = byteArrayOf(0xfd.toByte(), 0x37, 0x7a, 0x58, 0x5a, 0x00)

    data class ExtractStats(
        val entries: Long = 0,
        val files: Long = 0,
        val dirs: Long = 0,
        val symlinks: Long = 0,
        val skippedSpecial: Long = 0
    )

    /**
     * Extracts [archive] into [destDir] (created/cleaned by the caller).
     * [onLog] receives human-readable progress lines for the app log panel.
     */
    suspend fun extract(
        archive: File,
        destDir: File,
        onLog: (String) -> Unit = {}
    ): Result<ExtractStats> = withContext(Dispatchers.IO) {
        try {
            onLog("INFO: extracting rootfs with built-in decoder (proot mode)...")
            val stats = extractTar(archive, destDir, onLog)
            onLog(
                "INFO: extraction complete: ${stats.files} files, " +
                    "${stats.dirs} dirs, ${stats.symlinks} symlinks " +
                    "(${stats.skippedSpecial} special entries skipped)"
            )
            Result.success(stats)
        } catch (e: Exception) {
            Log.e(TAG, "extraction failed", e)
            Result.failure(e)
        }
    }

    private suspend fun extractTar(
        archive: File,
        destDir: File,
        onLog: (String) -> Unit
    ): ExtractStats {
        var entries = 0L
        var files = 0L
        var dirs = 0L
        var symlinks = 0L
        var skippedSpecial = 0L

        val destCanonical = destDir.canonicalPath
        BufferedInputStream(FileInputStream(archive), 256 * 1024).use { buffered ->
            buffered.mark(8)
            val magic = ByteArray(6)
            val read = buffered.read(magic)
            buffered.reset()
            val rawIn = when {
                read >= 2 && magic[0] == GZIP_MAGIC[0] && magic[1] == GZIP_MAGIC[1] -> {
                    onLog("INFO: detected gzip-compressed tar")
                    GZIPInputStream(buffered)
                }
                read >= 6 && XZ_MAGIC.indices.all { magic[it] == XZ_MAGIC[it] } -> {
                    onLog("INFO: detected xz-compressed tar")
                    XZInputStream(buffered)
                }
                else -> {
                    onLog("INFO: unknown extension, treating as plain tar")
                    buffered
                }
            }
            TarArchiveInputStream(rawIn).use { tar ->
                var entry: TarArchiveEntry? = tar.nextEntry
                while (entry != null) {
                    currentCoroutineContext().ensureActive()
                    val target = safeTarget(destDir, destCanonical, entry.name)
                    if (target == null) {
                        Log.w(TAG, "skipping unsafe entry: ${entry.name}")
                        skippedSpecial++
                    } else when {
                        entry.isDirectory -> {
                            if (!target.exists() && !target.mkdirs()) {
                                Log.w(TAG, "mkdirs failed: ${target.absolutePath}")
                            }
                            applyExecBit(target, entry.mode)
                            dirs++
                        }
                        entry.isSymbolicLink -> {
                            target.parentFile?.mkdirs()
                            if (target.exists() || Files.isSymbolicLink(target.toPath())) {
                                target.delete()
                            }
                            try {
                                Files.createSymbolicLink(
                                    target.toPath(),
                                    File(entry.linkName).toPath()
                                )
                                symlinks++
                            } catch (e: Exception) {
                                Log.w(TAG, "symlink failed: ${entry.name} -> ${entry.linkName}", e)
                                skippedSpecial++
                            }
                        }
                        entry.isLink -> {
                            // Hard link: resolve inside dest; fall back to a copy.
                            target.parentFile?.mkdirs()
                            val linkTarget = safeTarget(destDir, destCanonical, entry.linkName)
                            try {
                                if (linkTarget != null && linkTarget.exists()) {
                                    Files.createLink(target.toPath(), linkTarget.toPath())
                                } else {
                                    copyEntryData(tar, target)
                                }
                                applyExecBit(target, entry.mode)
                                files++
                            } catch (e: Exception) {
                                Log.w(TAG, "hardlink failed, copying: ${entry.name}", e)
                                copyEntryData(tar, target)
                                applyExecBit(target, entry.mode)
                                files++
                            }
                        }
                        entry.isFile -> {
                            target.parentFile?.mkdirs()
                            copyEntryData(tar, target)
                            applyExecBit(target, entry.mode)
                            files++
                        }
                        else -> {
                            // char/block device, fifo, socket: not creatable as app UID
                            // (and unneeded — proot binds host /dev).
                            skippedSpecial++
                        }
                    }
                    entries++
                    if (entries % 2000L == 0L) {
                        onLog("INFO: extracted $entries entries...")
                    }
                    entry = tar.nextEntry
                }
            }
        }
        return ExtractStats(entries, files, dirs, symlinks, skippedSpecial)
    }

    /** Rejects absolute paths and `..` escapes so entries stay inside [destDir]. */
    private fun safeTarget(destDir: File, destCanonical: String, entryName: String): File? {
        var name = entryName.trimStart('/')
        if (name.isEmpty() || name == "." || name == "./") return null
        val target = File(destDir, name)
        val canonical = try {
            target.canonicalPath
        } catch (_: Exception) {
            return null
        }
        if (canonical != destCanonical && !canonical.startsWith("$destCanonical/")) return null
        return target
    }

    private fun copyEntryData(tar: TarArchiveInputStream, target: File) {
        if (target.exists()) target.delete()
        target.outputStream().use { out ->
            tar.copyTo(out)
        }
    }

    /** Restores owner-executable bits from the tar mode (needed for guest shells). */
    private fun applyExecBit(target: File, mode: Int) {
        if (mode and 0b111 != 0) {
            target.setExecutable(true, false)
        }
    }

    /**
     * Mirrors the shell script's post-extraction stages: wrapping-directory
     * unwrap, rootfs validation, staged -> live swap with backup, and the
     * extracted marker. Returns failure with a clear message on any step.
     */
    suspend fun finalizeStagedRootfs(
        stagedRootfsDir: File,
        rootfsDir: File,
        backupRootfsDir: File,
        profileInstalledDir: File,
        extractedMarker: String,
        onLog: (String) -> Unit = {}
    ): Result<Unit> = withContext(Dispatchers.IO) {
        try {
            onLog("INFO: checking for wrapping directory...")
            val children = stagedRootfsDir.listFiles()?.toList().orEmpty()
            if (children.size == 1 && children[0].isDirectory) {
                onLog("INFO: detected wrapping directory '${children[0].name}', relocating...")
                val inner = File(stagedRootfsDir.parentFile, "${stagedRootfsDir.name}_inner")
                if (inner.exists()) inner.deleteRecursively()
                if (!children[0].renameTo(inner)) {
                    return@withContext Result.failure(
                        IllegalStateException("failed to relocate wrapping directory")
                    )
                }
                stagedRootfsDir.deleteRecursively()
                if (!inner.renameTo(stagedRootfsDir)) {
                    return@withContext Result.failure(
                        IllegalStateException("failed to finalize wrapping directory relocation")
                    )
                }
                onLog("INFO: relocation complete")
            }

            onLog("INFO: validating extracted rootfs...")
            val bash = File(stagedRootfsDir, "bin/bash")
            if (!bash.isFile) {
                val dash = File(stagedRootfsDir, "bin/dash")
                val sh = File(stagedRootfsDir, "bin/sh")
                val linkTarget = when {
                    dash.isFile -> "dash"
                    sh.isFile -> "sh"
                    else -> return@withContext Result.failure(
                        IllegalStateException("rootfs invalid (missing /bin/bash, /bin/dash, or /bin/sh)")
                    )
                }
                onLog("INFO: /bin/bash not found, symlinking /bin/$linkTarget -> /bin/bash")
                try {
                    Files.createSymbolicLink(bash.toPath(), File(linkTarget).toPath())
                } catch (e: Exception) {
                    return@withContext Result.failure(
                        IllegalStateException("failed to link /bin/bash: ${e.message}", e)
                    )
                }
            }
            if (!File(stagedRootfsDir, "usr/bin/grep").isFile) {
                return@withContext Result.failure(
                    IllegalStateException("rootfs invalid (missing /usr/bin/grep)")
                )
            }
            if (!File(stagedRootfsDir, "usr/bin/apt-get").isFile) {
                return@withContext Result.failure(
                    IllegalStateException("rootfs invalid (missing /usr/bin/apt-get)")
                )
            }

            onLog("INFO: finalizing rootfs swap...")
            if (backupRootfsDir.exists()) backupRootfsDir.deleteRecursively()
            if (rootfsDir.exists()) {
                if (!rootfsDir.renameTo(backupRootfsDir)) {
                    Log.w(TAG, "could not back up previous rootfs; removing instead")
                    rootfsDir.deleteRecursively()
                }
            }
            if (!stagedRootfsDir.renameTo(rootfsDir)) {
                return@withContext Result.failure(
                    IllegalStateException("failed to move staged rootfs into place")
                )
            }

            if (!profileInstalledDir.exists()) profileInstalledDir.mkdirs()
            File(profileInstalledDir, extractedMarker).apply {
                if (!exists()) createNewFile()
            }
            onLog("INFO: Extract Done.")
            Result.success(Unit)
        } catch (e: Exception) {
            Log.e(TAG, "finalize failed", e)
            Result.failure(e)
        }
    }
}
