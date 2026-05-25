package com.winland.server

import android.app.Application
import android.util.Log

class WinlandApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        Log.i("WinlandApplication", "Application initialized")
        // Preload native libraries before first activity uses JNI.
        NativeBridge.preloadLibraries()
    }
}
