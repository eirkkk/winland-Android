package com.winland.server

import android.content.Context

/**
 * Execution backend for Linux guest operations.
 *
 * - [ROOT]: classic path via `su` + real `chroot`/`mount` syscalls.
 * - [PROOT]: rootless path via the bundled `proot` binary (ptrace-based
 *   syscall interception). No root permission required.
 */
enum class ExecutionMode {
    ROOT,
    PROOT;

    companion object {
        fun fromName(name: String?): ExecutionMode {
            return try {
                valueOf(name ?: "ROOT")
            } catch (_: Exception) {
                ROOT
            }
        }
    }
}

/**
 * Central store for the current [ExecutionMode], persisted in
 * `winland_prefs` so engine singletons (which only receive a [Context])
 * can branch between root and proot code paths without signature changes.
 */
object ExecutionModeManager {
    private const val PREFS = "winland_prefs"
    private const val KEY_MODE = "execution_mode"

    @Volatile
    private var cached: ExecutionMode? = null

    fun get(context: Context): ExecutionMode {
        cached?.let { return it }
        val name = context.applicationContext
            .getSharedPreferences(PREFS, Context.MODE_PRIVATE)
            .getString(KEY_MODE, null)
        val mode = ExecutionMode.fromName(name)
        cached = mode
        return mode
    }

    fun set(context: Context, mode: ExecutionMode) {
        cached = mode
        context.applicationContext
            .getSharedPreferences(PREFS, Context.MODE_PRIVATE)
            .edit()
            .putString(KEY_MODE, mode.name)
            .apply()
    }

    fun isProot(context: Context): Boolean = get(context) == ExecutionMode.PROOT

    fun invalidate() {
        cached = null
    }
}
