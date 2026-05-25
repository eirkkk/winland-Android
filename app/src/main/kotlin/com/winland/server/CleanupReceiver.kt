package com.winland.server

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.util.Log
import com.winland.server.engine.ChrootInstaller
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch

/**
 * Static BroadcastReceiver that triggers a safe unmount of all chroot bind-mounts
 * on the following events:
 *   - ACTION_SHUTDOWN  : device is powering off
 *   - ACTION_REBOOT    : device is rebooting
 *
 * This prevents orphaned kernel mounts (especially /sdcard) from persisting after
 * the app process is killed, which could interfere with file-system integrity or
 * confuse the package manager when app data is being deleted during uninstall.
 *
 * Note: Android does NOT allow an app to intercept its own uninstall broadcast
 * (PACKAGE_FULLY_REMOVED is delivered AFTER the process is dead). The safest
 * mitigation for the uninstall case is:
 *   1. This receiver cleans up on shutdown/reboot.
 *   2. WinlandService.onDestroy() calls stopChroot() which also unmounts.
 *   3. The user is shown a persistent warning in the notification to stop the
 *      service before uninstalling.
 */
class CleanupReceiver : BroadcastReceiver() {

    override fun onReceive(context: Context, intent: Intent) {
        when (intent.action) {
            Intent.ACTION_SHUTDOWN,
            "android.intent.action.REBOOT" -> {
                Log.i(TAG, "System shutdown/reboot detected — running safe unmount")
                val pendingResult = goAsync()
                safeUnmountAsync(context) {
                    pendingResult.finish()
                }
            }
        }
    }

    companion object {
        private const val TAG = "CleanupReceiver"
        private val cleanupScope = CoroutineScope(SupervisorJob() + Dispatchers.IO)

        /**
         * Run unmount for all chroot bind-points using root on a background thread.
         * This method is safe to call from lifecycle callbacks because it returns immediately.
         */
        fun safeUnmountAsync(context: Context, onFinished: (() -> Unit)? = null) {
            val appContext = context.applicationContext
            cleanupScope.launch {
                try {
                    safeUnmountSuspend(appContext)
                } finally {
                    runCatching { onFinished?.invoke() }
                        .onFailure { Log.w(TAG, "onFinished callback failed", it) }
                }
            }
        }

        /**
         * Suspend variant for structured callers that need to await completion.
         */
        suspend fun safeUnmountSuspend(context: Context): Result<Unit> {
            try {
                val result = ChrootInstaller.stopChroot(context)
                if (result.isSuccess) {
                    Log.i(TAG, "Safe unmount completed")
                } else {
                    Log.e(TAG, "Safe unmount failed: ${result.exceptionOrNull()?.message}")
                }
                return result
            } catch (e: Exception) {
                Log.e(TAG, "Error during safe unmount", e)
                return Result.failure(e)
            }
        }
    }
}
