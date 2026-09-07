package com.winland.server.engine

import android.content.Context
import android.os.Environment
import android.util.Log
import com.winland.server.utils.getUnifiedFilesDir
import com.winland.server.utils.getUnifiedRootfsDir
import com.winland.server.utils.getUnifiedTmpDir
import com.winland.server.utils.RootUtils
import java.io.File
import java.util.concurrent.TimeUnit
import java.util.concurrent.TimeoutException
import java.util.concurrent.atomic.AtomicBoolean
import com.winland.server.ExecutionModeManager
import com.winland.server.NativeBridge
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock

object ChrootInstaller {

    private const val TAG = "ChrootInstaller"
    private val isBootingOrRunning = AtomicBoolean(false)
    private val isInstalling = AtomicBoolean(false)
    private val rootCommandMutex = Mutex()

    private val _logFlow = MutableSharedFlow<String>(replay = 100)
    val logFlow = _logFlow.asSharedFlow()

    private fun getMarkerExtracted(distroId: String) = "rootfs_extracted_$distroId"
    private fun getMarkerInstalled(distroId: String) = "rootfs_installed_$distroId"

    data class ChrootStatus(
        val installed: Boolean,
        val ready: Boolean,
        val reason: String,
        val extracted: Boolean
    )

    private fun hasExtractedRootfsLayout(rootfsDir: String): Boolean {
        val rootDir = File(rootfsDir)
        val etcDir = File(rootDir, "etc")
        val usrDir = File(rootDir, "usr")
        return rootDir.isDirectory && etcDir.isDirectory && usrDir.isDirectory
    }

    private fun ensureDirectory(path: String, label: String): File {
        val dir = File(path)
        if (dir.exists() && !dir.isDirectory) {
            _logFlow.tryEmit("WARN: replacing stale file at $label: $path")
            if (!dir.delete()) {
                throw IllegalStateException("failed to remove stale file at $label: $path")
            }
        }

        if (!dir.exists() && !dir.mkdirs()) {
            throw IllegalStateException("failed to create $label directory: $path")
        }

        return dir
    }


    private fun ensureRootfsDirectory(path: String): File {
        val dir = File(path)
        if (dir.exists() && !dir.isDirectory) {
            throw IllegalStateException("rootfs path is not a directory: $path")
        }
        if (!dir.exists() && !dir.mkdirs()) {
            throw IllegalStateException("failed to create rootfs directory: $path")
        }
        return dir
    }

    fun isInstallInProgress(): Boolean = isInstalling.get()

    fun isStartOrRunInProgress(): Boolean = isBootingOrRunning.get()

    fun getChrootStatus(context: Context, distroId: String): ChrootStatus {
        val filesDir = context.getUnifiedFilesDir()
        val rootfsDir = context.getUnifiedRootfsDir(distroId)
        try {
            ensureDirectory("$filesDir/profileInstalled", "profileInstalled")
        } catch (_: Exception) {
            return ChrootStatus(
                installed = false,
                ready = false,
                reason = "Runtime marker directory is broken",
                extracted = false
            )
        }
        val extractedMarker = File("$filesDir/profileInstalled/${getMarkerExtracted(distroId)}")
        val installedMarker = File("$filesDir/profileInstalled/${getMarkerInstalled(distroId)}")
        val extractedLayoutPresent = hasExtractedRootfsLayout(rootfsDir)

        if (!extractedMarker.exists() && !extractedLayoutPresent) {
            return ChrootStatus(
                installed = false,
                ready = false,
                reason = "Not installed or incomplete rootfs",
                extracted = false
            )
        }

        if (!extractedMarker.exists() && extractedLayoutPresent) {
            try {
                extractedMarker.parentFile?.mkdirs()
                extractedMarker.writeText("ok")
            } catch (_: Exception) {
                // If marker write fails, still report based on actual rootfs contents.
            }
        }

        if (!hasExtractedRootfsLayout(rootfsDir)) {
            return ChrootStatus(
                installed = false,
                ready = false,
                reason = "Rootfs layout is missing or broken",
                extracted = false
            )
        }

        if (!installedMarker.exists()) {
            return ChrootStatus(
                installed = false,
                ready = false,
                reason = "Rootfs extracted. Setup required",
                extracted = true
            )
        }

        // Self-heal runtime tmp directory if user manually deletes it.
        try {
            ensureDirectory(context.getUnifiedTmpDir(), "tmp")
        } catch (_: Exception) {
            return ChrootStatus(
                installed = true,
                ready = false,
                reason = "Environment missing (tmp dir) and recreation failed",
                extracted = true
            )
        }

        return ChrootStatus(
            installed = true,
            ready = true,
            reason = "Ready",
            extracted = true
        )
    }

    suspend fun extractRootfs(context: Context, filename: String, distroId: String): Result<Unit> {
        val filesDir = context.getUnifiedFilesDir()
        val downloadsDir = "$filesDir/downloads"
        val rootfsDir = context.getUnifiedRootfsDir(distroId)
        val stagedRootfsDir = "$filesDir/rootfs_${distroId}.new"
        val backupRootfsDir = "$filesDir/rootfs_${distroId}_prev"
        val tmpDir = context.getUnifiedTmpDir()
        val profileInstalledDir = "$filesDir/profileInstalled"

        try {
            ensureDirectory(downloadsDir, "downloads")
            ensureRootfsDirectory(rootfsDir)
            ensureDirectory(tmpDir, "tmp")
            ensureDirectory(profileInstalledDir, "profileInstalled")
        } catch (error: Exception) {
            _logFlow.tryEmit("ERROR: ${error.message}")
            return Result.failure(error)
        }

        val rootfsArchive = File(downloadsDir, filename)
        if (!rootfsArchive.exists()) {
            _logFlow.tryEmit("ERROR: Rootfs file not found: ${rootfsArchive.absolutePath}")
            return Result.failure(IllegalStateException("rootfs archive not found"))
        }

        if (rootfsArchive.length() < 10L * 1024L * 1024L) {
            _logFlow.tryEmit("ERROR: Rootfs archive is too small/corrupted: ${rootfsArchive.length()} bytes")
            return Result.failure(IllegalStateException("rootfs archive is invalid or incomplete; please re-download"))
        }
        
        // --- [Winland OS: Proot (rootless) extraction path] ---
        // Pure-Java extraction: the shell script depends on
        // $filesDir/bin/busybox, which is not executable on targetSdk 29+
        // devices (W^X). Everything here is app-private storage owned by the
        // app UID, so no privilege — and no busybox — is needed.
        if (ExecutionModeManager.isProot(context)) {
            return extractRootfsProot(
                context = context,
                rootfsArchive = rootfsArchive,
                stagedRootfsDir = stagedRootfsDir,
                rootfsDir = rootfsDir,
                backupRootfsDir = backupRootfsDir,
                profileInstalledDir = profileInstalledDir,
                distroId = distroId
            )
        }

        // --- [Winland OS: Deploy Internal Busybox BEFORE extraction] ---
        deployInternalBusybox(context, filesDir)

        val scriptFile = File(context.cacheDir, "extract_rootfs.sh")
        val script = ChrootScriptBuilder.buildExtractScript(
            filesDir = filesDir,
            downloadsDir = downloadsDir,
            rootfsDir = rootfsDir,
            stagedRootfsDir = stagedRootfsDir,
            backupRootfsDir = backupRootfsDir,
            tmpDir = tmpDir,
            profileInstalledDir = profileInstalledDir,
            filename = filename,
            extractedMarker = getMarkerExtracted(distroId),
            installedMarker = getMarkerInstalled(distroId)
        )

        scriptFile.writeText(script)
        return dispatchCommand(
            context = context,
            command = "sh ${scriptFile.absolutePath}",
            timeoutMinutes = 120,
            operation = "extractRootfs"
        )
    }

    /**
     * Rootless extraction: decode the archive with the built-in Java
     * decoder, then unwrap/validate/swap exactly like the shell script.
     * No busybox, no mounts, no privileges required.
     */
    private suspend fun extractRootfsProot(
        context: Context,
        rootfsArchive: File,
        stagedRootfsDir: String,
        rootfsDir: String,
        backupRootfsDir: String,
        profileInstalledDir: String,
        distroId: String
    ): Result<Unit> {
        val log: (String) -> Unit = { _logFlow.tryEmit(it) }
        log("INFO: preparing directory layout...")
        val staged = File(stagedRootfsDir)
        if (staged.exists()) staged.deleteRecursively()
        if (!staged.mkdirs()) {
            val error = "failed to create staging directory: $stagedRootfsDir"
            log("ERROR: $error")
            return Result.failure(IllegalStateException(error))
        }

        log("INFO: extracting rootfs with built-in decoder...")
        val extractResult = ProotRootfsExtractor.extract(rootfsArchive, staged, log)
        if (extractResult.isFailure) {
            val error = extractResult.exceptionOrNull()
            log("ERROR: rootfs extraction failed: ${error?.message}")
            log("ERROR: removed invalid archive; please download again")
            staged.deleteRecursively()
            rootfsArchive.delete()
            return Result.failure(
                IllegalStateException("rootfs extraction failed; archive may be incomplete or invalid", error)
            )
        }

        val finalizeResult = ProotRootfsExtractor.finalizeStagedRootfs(
            stagedRootfsDir = staged,
            rootfsDir = File(rootfsDir),
            backupRootfsDir = File(backupRootfsDir),
            profileInstalledDir = File(profileInstalledDir),
            extractedMarker = getMarkerExtracted(distroId),
            onLog = log
        )
        if (finalizeResult.isFailure) {
            val error = finalizeResult.exceptionOrNull()
            log("ERROR: ${error?.message}")
            staged.deleteRecursively()
            return Result.failure(error ?: IllegalStateException("rootfs finalization failed"))
        }
        return Result.success(Unit)
    }

    suspend fun setupRootfs(context: Context, distroId: String): Result<Unit> {
        if (!isInstalling.compareAndSet(false, true)) {
            return Result.failure(IllegalStateException("setup already in progress"))
        }

        try {
            val filesDir = context.getUnifiedFilesDir()
            val rootfsDir = context.getUnifiedRootfsDir(distroId)
            val tmpDir = context.getUnifiedTmpDir()
            val profileInstalledDir = "$filesDir/profileInstalled"
            val extractedMarker = File("$profileInstalledDir/${getMarkerExtracted(distroId)}")
            val installedMarker = File("$profileInstalledDir/${getMarkerInstalled(distroId)}")
            val appUid = android.os.Process.myUid()

            try {
                ensureRootfsDirectory(rootfsDir)
                ensureDirectory(tmpDir, "tmp")
                ensureDirectory(profileInstalledDir, "profileInstalled")
            } catch (error: Exception) {
                _logFlow.tryEmit("ERROR: ${error.message}")
                return Result.failure(error)
            }

            if (!extractedMarker.exists() || !hasExtractedRootfsLayout(rootfsDir)) {
                return Result.failure(IllegalStateException("rootfs is not extracted; press Install first"))
            }

            val setupName = "setup_${distroId}.sh"
            val setupTarget = File(tmpDir, setupName)
            setupTarget.parentFile?.mkdirs()
            context.assets.open(setupName).use { input ->
                setupTarget.outputStream().use { output ->
                    input.copyTo(output)
                }
            }

            // --- [Winland OS: Proot (rootless) setup path] ---
            if (ExecutionModeManager.isProot(context)) {
                _logFlow.tryEmit("INFO: Deploying proot backend (rootless mode)...")
                try {
                    ProotManager.deployProot(context)
                    ProotManager.prepareGuestDirs(rootfsDir, filesDir, tmpDir)
                } catch (error: Exception) {
                    _logFlow.tryEmit("ERROR: proot deploy failed: ${error.message}")
                    return Result.failure(error)
                }

                // Stage the distro setup script INSIDE the guest (the root
                // path does this via the winland-setup native binary, which
                // performs mounts and is therefore root-only). Pure file copy
                // as the app UID — no privilege needed.
                try {
                    val guestSetupDir = File("$rootfsDir/tmp")
                    if (!guestSetupDir.exists()) guestSetupDir.mkdirs()
                    val guestSetup = File(guestSetupDir, setupName)
                    setupTarget.copyTo(guestSetup, overwrite = true)
                    guestSetup.setExecutable(true, false)
                    _logFlow.tryEmit("INFO: staged $setupName into guest /tmp")
                } catch (error: Exception) {
                    _logFlow.tryEmit("ERROR: failed to stage setup script into guest: ${error.message}")
                    return Result.failure(error)
                }

                _logFlow.tryEmit("INFO: Executing rootless (proot) environment setup...")
                val externalStoragePath = Environment.getExternalStorageDirectory().path
                val prootScriptFile = File(context.cacheDir, "post_setup_rootfs_proot.sh")
                val prootScript = ChrootScriptBuilder.buildProotPostSetupScript(
                    filesDir = filesDir,
                    rootfsDir = rootfsDir,
                    tmpDir = tmpDir,
                    nativeLibDir = context.applicationInfo.nativeLibraryDir,
                    profileInstalledDir = profileInstalledDir,
                    externalStoragePath = externalStoragePath,
                    distroId = distroId,
                    installedMarker = getMarkerInstalled(distroId)
                )
                prootScriptFile.writeText(prootScript)
                val prootResult = dispatchCommand(
                    context = context,
                    command = "sh ${prootScriptFile.absolutePath}",
                    timeoutMinutes = 240,
                    operation = "postSetupRootfsProot"
                )

                return if (prootResult.isSuccess && installedMarker.exists()) {
                    Result.success(Unit)
                } else {
                    Result.failure(prootResult.exceptionOrNull() ?: IllegalStateException("proot setup did not create marker file"))
                }
            }

            // --- [Winland OS: Native Setup Migration] ---
            _logFlow.tryEmit("INFO: Deploying native setup binary...")
            deployNativeSetupBinary(context, filesDir)
            
            _logFlow.tryEmit("INFO: Executing native environment setup via root binary...")
            val setupBin = File(filesDir, "bin/winland-setup").absolutePath
            val nativeResult = executeRootCommand(
                command = "$setupBin \"$rootfsDir\" \"$filesDir\" ${appUid.toString().trim()}",
                timeoutMinutes = 5,
                operation = "nativeSetupBinary"
            )
            
            if (nativeResult.isFailure) {
                _logFlow.tryEmit("CRITICAL ERROR: Native setup binary failed.")
                return Result.failure(nativeResult.exceptionOrNull() ?: IllegalStateException("Native setup failed"))
            }
            _logFlow.tryEmit("INFO: Native mounts and SELinux preparation successful.")

            // Now run the logical setup (DNS, APT, setup.sh)
            val scriptFile = File(context.cacheDir, "post_setup_rootfs.sh")
            val script = """
                #!/system/bin/sh
                set -e
                
                # Internal Busybox for reliable chroot
                BB="$filesDir/bin/busybox"
                [ -x "${'$'}BB" ] || BB="busybox"
                
                # 1. DNS and APT fixes (Done from Android side to be sure)
                mkdir -p $rootfsDir/etc/apt/apt.conf.d
                echo 'APT::Sandbox::User "root";' > $rootfsDir/etc/apt/apt.conf.d/01-android-nosandbox
                echo 'nameserver 1.1.1.1' > $rootfsDir/etc/resolv.conf
                echo 'nameserver 8.8.8.8' >> $rootfsDir/etc/resolv.conf
                
                # 2. Linux-level user management (Requires CHROOT)
                "${'$'}BB" chroot $rootfsDir /bin/bash -c '
                    export HOME=/root
                    export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
                    /usr/bin/grep -q "^aid_inet:" /etc/group || /usr/sbin/groupadd -g 3003 aid_inet || true
                    /usr/bin/id _apt >/dev/null 2>&1 && /usr/sbin/usermod -G nogroup -g aid_inet _apt || true
                '
                
                # 3. Execution of distro-specific setup script
                chmod +x $rootfsDir/tmp/setup_$distroId.sh
                "${'$'}BB" chroot $rootfsDir /bin/bash /tmp/setup_$distroId.sh
                
                touch $profileInstalledDir/${getMarkerInstalled(distroId)}
            """.trimIndent()

            scriptFile.writeText(script)
            val result = executeRootCommand(
                command = "sh ${scriptFile.absolutePath}",
                timeoutMinutes = 240,
                operation = "postSetupRootfs"
            )

            return if (result.isSuccess && installedMarker.exists()) {
                Result.success(Unit)
            } else {
                Result.failure(result.exceptionOrNull() ?: IllegalStateException("setup did not create marker file"))
            }
        } finally {
            isInstalling.set(false)
        }
    }

    suspend fun installRootfs(context: Context, filename: String, distroId: String): Result<Unit> {
        val extract = extractRootfs(context, filename, distroId)
        if (extract.isFailure) {
            return Result.failure(extract.exceptionOrNull() ?: IllegalStateException("extract failed"))
        }
        return setupRootfs(context, distroId)
    }

    suspend fun startChroot(context: Context, distroId: String, density: Float): Result<Unit> {
        if (!isBootingOrRunning.compareAndSet(false, true)) {
            _logFlow.tryEmit("INFO: chroot already starting/running; skipping duplicate request")
            return Result.failure(IllegalStateException("chroot already starting/running"))
        }

        val filesDir = context.getUnifiedFilesDir()
        val rootfsDir = context.getUnifiedRootfsDir(distroId)
        val tmpDir = context.getUnifiedTmpDir()
        val externalStoragePath = Environment.getExternalStorageDirectory().path
        val installMarker = File("$filesDir/profileInstalled/${getMarkerInstalled(distroId)}")


        if (!installMarker.exists()) {
            _logFlow.tryEmit("ERROR: rootfs is not installed yet (missing marker)")
            isBootingOrRunning.set(false)
            return Result.failure(IllegalStateException("missing install marker"))
        }

        if (!hasExtractedRootfsLayout(rootfsDir)) {
            _logFlow.tryEmit("ERROR: invalid rootfs layout (missing rootfs/etc/usr)")
            isBootingOrRunning.set(false)
            return Result.failure(IllegalStateException("invalid rootfs layout"))
        }

        try {
            ensureDirectory(tmpDir, "tmp")
            ensureDirectory("$filesDir/profileInstalled", "profileInstalled")
        } catch (error: Exception) {
            _logFlow.tryEmit("ERROR: ${error.message}")
            isBootingOrRunning.set(false)
            return Result.failure(error)
        }

        // --- Cross-mode stale session cleanup ---
        // Switching execution modes (or restarting the app) never stops the
        // previous session: a lingering desktop fights the new boot over /tmp
        // sockets (pulseaudio `native`, fifos, XDG links) and fails the boot.
        // Kill leftovers from BOTH modes, best-effort:
        // - direct runner (app UID) kills proot-mode leftovers,
        // - root runner (su) kills root-mode leftovers (app UID cannot
        //   signal uid-0 processes, so both runners are needed; skipped
        //   entirely when no root is available).
        _logFlow.tryEmit("INFO: cleaning stale session processes (both modes)...")
        val killScript = ChrootScriptBuilder.buildProotStopScript(filesDir, rootfsDir)
        runCatching {
            executeDirectCommand(
                command = killScript,
                timeoutMinutes = 2,
                operation = "preBootKillDirect"
            )
        }
        if (RootUtils.isRootAvailable()) {
            runCatching {
                executeRootCommand(
                    command = killScript,
                    timeoutMinutes = 2,
                    operation = "preBootKillRoot"
                )
            }
        }

        val bootScript = if (ExecutionModeManager.isProot(context)) {
            _logFlow.tryEmit("INFO: booting desktop via proot (rootless mode)")
            try {
                ProotManager.deployProot(context)
                ProotManager.prepareGuestDirs(rootfsDir, filesDir, tmpDir)
            } catch (error: Exception) {
                _logFlow.tryEmit("ERROR: proot deploy failed: ${error.message}")
                isBootingOrRunning.set(false)
                return Result.failure(error)
            }
            ChrootScriptBuilder.buildProotRunScript(
                filesDir = filesDir,
                rootfsDir = rootfsDir,
                tmpDir = tmpDir,
                nativeLibDir = context.applicationInfo.nativeLibraryDir,
                externalStoragePath = externalStoragePath,
                density = density,
                distroId = distroId
            )
        } else {
            ChrootScriptBuilder.buildRunScript(
                filesDir = filesDir,
                rootfsDir = rootfsDir,
                tmpDir = tmpDir,
                externalStoragePath = externalStoragePath,
                density = density,
                distroId = distroId
            )
        }

        return try {
            NativeBridge.startRecording()
            val result = dispatchCommand(
                context = context,
                command = bootScript,
                timeoutMinutes = 30,
                operation = "startChroot"
            )
            if (result.isSuccess) {
                NativeBridge.markSessionActive(context)
            }
            result
        } finally {
            isBootingOrRunning.set(false)
        }
    }

    suspend fun stopChroot(context: Context, distroId: String = "ubuntu"): Result<Unit> {
        val filesDir = context.getUnifiedFilesDir()
        val rootfsDir = context.getUnifiedRootfsDir(distroId)
        val proot = ExecutionModeManager.isProot(context)

        val stopScript = if (proot) {
            ChrootScriptBuilder.buildProotStopScript(filesDir, rootfsDir)
        } else {
            ChrootScriptBuilder.buildStopScript(filesDir, rootfsDir)
        }

        _logFlow.tryEmit(if (proot) "INFO: stopping proot session..." else "INFO: stopping rootful chroot session...")
        NativeBridge.stopRecording()
        val result = dispatchCommand(
            context = context,
            command = stopScript,
            timeoutMinutes = 10,
            operation = "stopChroot"
        )
        isBootingOrRunning.set(false)
        if (result.isSuccess) {
            NativeBridge.markSessionInactive(context)
        }
        return result
    }

    suspend fun restartChroot(context: Context, distroId: String): Result<Unit> {
        val proot = ExecutionModeManager.isProot(context)
        _logFlow.tryEmit(if (proot) "INFO: restarting rootless (proot) session..." else "INFO: restarting rootful chroot session...")
        val stop = stopChroot(context, distroId)
        if (stop.isFailure && !proot) {
            return Result.failure(stop.exceptionOrNull() ?: IllegalStateException("failed to stop before restart"))
        }
        if (stop.isFailure) {
            // Proot has no mounts/locks to release; a failed stop (e.g.
            // nothing running yet) must not block the restart.
            _logFlow.tryEmit("WARN: proot stop reported failure (${stop.exceptionOrNull()?.message}); continuing restart anyway")
        }
        return startChroot(context, distroId, context.resources.displayMetrics.density)
    }

    suspend fun executeRootCommand(
        command: String,
        timeoutMinutes: Long,
        operation: String
    ): Result<Unit> {
        return rootCommandMutex.withLock {
            Log.i(TAG, "Root command started: op=$operation timeout=${timeoutMinutes}m")
            val result = RootCommandRunner.execute(
                command = command,
                timeout = timeoutMinutes,
                timeUnit = TimeUnit.MINUTES,
                onStdout = { line ->
                    Log.i(TAG, "Root stdout: $line")
                    _logFlow.tryEmit(line)
                },
                onStderr = { line ->
                    Log.e(TAG, "Root stderr: $line")
                    _logFlow.tryEmit("ERROR: $line")
                }
            )

            if (result.isFailure) {
                val error = result.exceptionOrNull()
                val message = error?.message ?: "$operation failed"
                if (error is TimeoutException || message.contains("exit code -1")) {
                    _logFlow.tryEmit("ERROR: $operation timed out after ${timeoutMinutes}m")
                } else {
                    _logFlow.tryEmit("CRITICAL ERROR: $message")
                }
            }

            Log.i(TAG, "Command execution finished: op=$operation success=${result.isSuccess}")
            result.mapCatching { Unit }.recoverCatching { error ->
                throw IllegalStateException(error.message ?: "$operation failed", error)
            }
        }
    }
    /**
     * Dispatches [command] to the root (`su`) or rootless (`sh` direct)
     * runner depending on the active [ExecutionModeManager] mode.
     */
    suspend fun dispatchCommand(
        context: Context,
        command: String,
        timeoutMinutes: Long,
        operation: String
    ): Result<Unit> {
        return if (ExecutionModeManager.isProot(context)) {
            executeDirectCommand(
                command = command,
                timeoutMinutes = timeoutMinutes,
                operation = operation
            )
        } else {
            executeRootCommand(
                command = command,
                timeoutMinutes = timeoutMinutes,
                operation = operation
            )
        }
    }

    suspend fun executeDirectCommand(
        command: String,
        timeoutMinutes: Long,
        operation: String
    ): Result<Unit> {
        return rootCommandMutex.withLock {
            Log.i(TAG, "Direct command started: op=$operation timeout=${timeoutMinutes}m")
            val result = RootCommandRunner.executeDirect(
                command = command,
                timeout = timeoutMinutes,
                timeUnit = TimeUnit.MINUTES,
                onStdout = { line ->
                    Log.i(TAG, "Direct stdout: $line")
                    _logFlow.tryEmit(line)
                },
                onStderr = { line ->
                    Log.e(TAG, "Direct stderr: $line")
                    _logFlow.tryEmit("ERROR: $line")
                }
            )

            if (result.isFailure) {
                val error = result.exceptionOrNull()
                val message = error?.message ?: "$operation failed"
                if (error is TimeoutException || message.contains("exit code -1")) {
                    _logFlow.tryEmit("ERROR: $operation timed out after ${timeoutMinutes}m")
                } else {
                    _logFlow.tryEmit("CRITICAL ERROR: $message")
                }
            }

            Log.i(TAG, "Direct command execution finished: op=$operation success=${result.isSuccess}")
            result.mapCatching { Unit }.recoverCatching { error ->
                throw IllegalStateException(error.message ?: "$operation failed", error)
            }
        }
    }

    private fun deployInternalBusybox(context: Context, filesDir: String) {
        val binDir = File(filesDir, "bin")
        if (!binDir.exists()) binDir.mkdirs()
        val busyboxTarget = File(binDir, "busybox")
        try {
            context.assets.open("bin/busybox").use { input ->
                busyboxTarget.outputStream().use { output ->
                    input.copyTo(output)
                }
            }
            busyboxTarget.setExecutable(true, false)
            Log.i(TAG, "Busybox deployed and executable bit set: ${busyboxTarget.absolutePath}")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to deploy Busybox: ${e.message}")
        }
    }

    private fun deployNativeSetupBinary(context: Context, filesDir: String) {
        val binDir = File(filesDir, "bin")
        if (!binDir.exists()) binDir.mkdirs()
        val target = File(binDir, "winland-setup")
        try {
            context.assets.open("bin/winland-setup").use { input ->
                target.outputStream().use { output ->
                    input.copyTo(output)
                }
            }
            target.setExecutable(true, false)
            Log.i(TAG, "Native setup binary deployed: ${target.absolutePath}")
        } catch (e: Exception) {
            Log.e(TAG, "Native setup binary not found in assets; falling back to legacy paths.")
        }
    }
}
