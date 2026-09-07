package com.winland.server.engine

import android.content.Context
import android.util.Log
import com.winland.server.utils.getUnifiedFilesDir
import java.io.File

/**
 * Manager for the rootless `proot` backend.
 *
 * proot is a user-space implementation of `chroot`/`mount --bind` based on
 * `ptrace()` syscall interception, so it runs without root privileges.
 *
 * The prebuilt `proot` binary (aarch64-linux-android, static) is shipped as
 * `jniLibs/arm64-v8a/libproot.so` (+ `libproot-loader.so`), so the OS
 * installs it into `nativeLibraryDir` (`apk_data_file` SELinux label) where
 * `execve()` is allowed. On targetSdk 29+ the app-private `filesDir`
 * (`app_data_file`) is subject to W^X and can NOT be executed from, so the
 * native-library path is always preferred; the legacy `assets/bin/` ->
 * `filesDir/bin/` copy is kept only as a fallback for old devices.
 */
object ProotManager {

    private const val TAG = "ProotManager"

    const val PROOT_ASSET = "bin/proot"
    const val PROOT_LOADER_ASSET = "bin/proot-loader"
    const val PROOT_LOADER32_ASSET = "bin/proot-loader32"

    const val PROOT_BIN_NAME = "proot"
    const val PROOT_LOADER_NAME = "proot-loader"
    const val PROOT_LOADER32_NAME = "proot-loader32"

    /** File names of the proot binaries inside `nativeLibraryDir`. */
    const val PROOT_NATIVE_LIB = "libproot.so"
    const val PROOT_LOADER_NATIVE_LIB = "libproot-loader.so"

    /** Directory the OS extracts jniLibs .so files into (exec-allowed). */
    fun nativeLibDir(context: Context): String =
        context.applicationInfo.nativeLibraryDir

    fun nativeProotPath(context: Context): String =
        File(nativeLibDir(context), PROOT_NATIVE_LIB).absolutePath

    fun nativeLoaderPath(context: Context): String =
        File(nativeLibDir(context), PROOT_LOADER_NATIVE_LIB).absolutePath

    /**
     * Preferred proot path: `nativeLibraryDir/libproot.so` when present,
     * otherwise the legacy `filesDir/bin/proot` fallback.
     */
    fun prootPath(context: Context): String {
        val native = File(nativeProotPath(context))
        if (native.isFile) return native.absolutePath
        return File(File(context.getUnifiedFilesDir(), "bin"), PROOT_BIN_NAME).absolutePath
    }

    fun loaderPath(context: Context): String {
        val native = File(nativeLoaderPath(context))
        if (native.isFile) return native.absolutePath
        return File(File(context.getUnifiedFilesDir(), "bin"), PROOT_LOADER_NAME).absolutePath
    }

    fun loader32Path(context: Context): String =
        File(File(context.getUnifiedFilesDir(), "bin"), PROOT_LOADER32_NAME).absolutePath

    /**
     * True when the proot binary exists and is executable.
     * Prefers `nativeLibraryDir/libproot.so` (exec-allowed on all API
     * levels); falls back to deploying `assets/bin/proot` into
     * `filesDir/bin/` on old devices without W^X enforcement.
     */
    fun isAvailable(context: Context): Boolean {
        val native = File(nativeProotPath(context))
        if (native.isFile && native.canExecute()) return true
        val bin = File(prootPath(context))
        if (bin.isFile && bin.canExecute()) return true
        return try {
            deployProot(context)
            val redeployed = File(prootPath(context))
            redeployed.isFile && redeployed.canExecute()
        } catch (e: Exception) {
            Log.e(TAG, "proot availability check failed", e)
            false
        }
    }

    /**
     * Ensures a usable proot binary. No-op when the native-library copy is
     * already executable; otherwise copies from `assets/bin/` to
     * `filesDir/bin/` (legacy fallback path) and marks it executable.
     */
    fun deployProot(context: Context) {
        val native = File(nativeProotPath(context))
        if (native.isFile && native.canExecute()) {
            Log.i(TAG, "proot resolved from nativeLibraryDir: ${native.absolutePath}")
            return
        }
        val binDir = File(context.getUnifiedFilesDir(), "bin")
        if (!binDir.exists()) binDir.mkdirs()

        copyAssetIfPresent(context, PROOT_ASSET, File(binDir, PROOT_BIN_NAME), required = !native.isFile)
        copyAssetIfPresent(context, PROOT_LOADER_ASSET, File(binDir, PROOT_LOADER_NAME), required = false)
        copyAssetIfPresent(context, PROOT_LOADER32_ASSET, File(binDir, PROOT_LOADER32_NAME), required = false)

        Log.i(TAG, "proot deployed to ${binDir.absolutePath}")
    }

    private fun copyAssetIfPresent(context: Context, asset: String, target: File, required: Boolean) {
        try {
            context.assets.open(asset).use { input ->
                target.outputStream().use { output ->
                    input.copyTo(output)
                }
            }
            target.setExecutable(true, false)
            Log.i(TAG, "Deployed $asset -> ${target.absolutePath} (${target.length()} bytes)")
        } catch (e: Exception) {
            if (required) {
                Log.e(TAG, "Required proot asset missing: $asset", e)
                throw IllegalStateException("proot binary missing from assets ($asset)", e)
            } else {
                Log.w(TAG, "Optional proot asset missing (skipped): $asset")
            }
        }
    }

    /**
     * Builds the common proot argument prefix for entering [rootfsDir].
     *
     * Uses `-0` (fake root uid/gid inside the guest) so package managers
     * and desktop services behave as if running as root.
     */
    fun baseArgs(
        context: Context,
        rootfsDir: String,
        filesDir: String,
        tmpDir: String,
        externalStoragePath: String
    ): List<String> {
        return listOf(
            prootPath(context),
            "-0",
            "-r", rootfsDir,
            "-w", "/root",
            "--link2symlink",
            "-b", "/proc",
            "-b", "/sys",
            "-b", "/dev",
            "-b", "/dev/pts",
            "-b", "$tmpDir/dev/shm:/dev/shm",
            "-b", "/dev/null:/dev/null",
            "-b", "/dev/zero:/dev/zero",
            "-b", "/dev/random:/dev/random",
            "-b", "/dev/urandom:/dev/urandom",
            "-b", "$tmpDir:/tmp",
            "-b", "$externalStoragePath:$rootfsDir/external_storage"
        )
    }

    /**
     * Host-side directory preparation for a proot session. Everything runs
     * as the app UID inside app-private storage, so no privileges needed.
     */
    fun prepareGuestDirs(rootfsDir: String, filesDir: String, tmpDir: String) {
        val dirs = listOf(
            "$rootfsDir/proc",
            "$rootfsDir/sys",
            "$rootfsDir/dev",
            "$rootfsDir/dev/pts",
            "$rootfsDir/dev/shm",
            "$rootfsDir/tmp",
            "$rootfsDir/external_storage",
            "$rootfsDir/tmp/pulse-runtime",
            "$rootfsDir/tmp/audio_bridge",
            tmpDir,
            "$tmpDir/proot-tmp",
            "$tmpDir/dev",
            "$tmpDir/dev/shm",
            "$tmpDir/pulse-runtime",
            "$tmpDir/audio_bridge"
        )
        for (dir in dirs) {
            try {
                val f = File(dir)
                if (!f.exists()) f.mkdirs()
            } catch (e: Exception) {
                Log.w(TAG, "Failed to create proot guest dir: $dir", e)
            }
        }
    }

    /**
     * Shell snippet exporting the proot runtime environment. Prepend to any
     * generated proot script.
     */
    fun envExports(context: Context, tmpDir: String): String {
        return """
            PROOT_TMP_DIR="$tmpDir/proot-tmp"
            export PROOT_TMP_DIR
            PROOT_LOADER="${loaderPath(context)}"
            if [ -f "${'$'}PROOT_LOADER" ]; then
                export PROOT_LOADER
            fi
            PROOT_LOADER_32="${loader32Path(context)}"
            if [ -f "${'$'}PROOT_LOADER_32" ]; then
                export PROOT_LOADER_32
            fi
            PROOT_NO_SECCOMP=1
            export PROOT_NO_SECCOMP
        """.trimIndent()
    }
}
