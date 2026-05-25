package com.winland.server.utils

import com.winland.server.engine.RootCommandRunner
import java.util.concurrent.TimeUnit

object RootUtils {
    fun isRootAvailable(): Boolean {
        return RootCommandRunner.executeBlocking("true", 5, TimeUnit.SECONDS)
    }

    fun executeRootCommand(command: String): Boolean {
        return RootCommandRunner.executeBlocking(command, 30, TimeUnit.SECONDS)
    }
}
