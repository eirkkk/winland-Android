package com.winland.server.utils

import android.content.Context
import com.winland.server.engine.ProotManager
import com.winland.server.engine.RootCommandRunner
import java.util.concurrent.TimeUnit

object RootUtils {
    fun isRootAvailable(): Boolean {
        return RootCommandRunner.executeBlocking("true", 5, TimeUnit.SECONDS)
    }

    fun executeRootCommand(command: String): Boolean {
        return RootCommandRunner.executeBlocking(command, 30, TimeUnit.SECONDS)
    }

    /**
     * True when the bundled proot binary can be deployed and executed.
     * Used to decide whether PROOT mode can be offered on non-rooted devices.
     */
    fun isProotAvailable(context: Context): Boolean {
        return try {
            ProotManager.isAvailable(context)
        } catch (_: Exception) {
            false
        }
    }

    fun executeDirectCommand(command: String): Boolean {
        return RootCommandRunner.executeDirectBlocking(command, 30, TimeUnit.SECONDS)
    }
}
