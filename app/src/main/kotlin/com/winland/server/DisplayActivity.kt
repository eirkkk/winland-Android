
package com.winland.server

import android.app.Activity
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.res.Configuration
import android.os.Build
import android.os.Bundle
import android.os.SystemClock
import android.system.ErrnoException
import android.system.Os
import android.system.OsConstants
import android.text.InputType
import android.util.Log
import android.util.DisplayMetrics
import android.view.HapticFeedbackConstants
import android.view.KeyEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.view.WindowInsets
import android.view.inputmethod.BaseInputConnection
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputConnection
import android.view.inputmethod.InputMethodManager
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Keyboard
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.SmallFloatingActionButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.SideEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.view.WindowCompat
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.*
import com.winland.server.utils.*
import com.winland.server.engine.ChrootInstaller
import com.winland.server.ui.theme.WinlandServerTheme
import kotlinx.coroutines.launch
import java.io.File
import java.lang.ref.WeakReference
import java.util.concurrent.atomic.AtomicBoolean

class DisplayActivity : ComponentActivity() {

    private inline fun runIfNativeLoaded(actionName: String, block: () -> Unit) {
        if (!NativeBridge.isLoaded()) {
            Log.w("DisplayActivity", "Skipping $actionName: native library is not loaded yet")
            return
        }
        runCatching { block() }
            .onFailure { Log.w("DisplayActivity", "Native call failed for $actionName", it) }
    }

    private inline fun traceLifecycle(name: String, block: () -> Unit) {
        val start = SystemClock.elapsedRealtime()
        Log.i("DisplayActivity", "$name start")
        try {
            block()
        } finally {
            val elapsed = SystemClock.elapsedRealtime() - start
            Log.i("DisplayActivity", "$name end (${elapsed}ms)")
        }
    }

    companion object {
        private const val MOVE_DISPATCH_MIN_INTERVAL_MS = 8L
        // Reference to the active SurfaceView for drawing frames
        @Volatile
        private var activeSurfaceView: WaylandInputSurfaceView? = null
        @Volatile
        private var currentActivityRef: WeakReference<Activity>? = null
        @Volatile
        private var surfaceTouchTotal: Long = 0
        @Volatile
        private var surfaceTouchDown: Long = 0
        @Volatile
        private var surfaceTouchMove: Long = 0
        @Volatile
        private var surfaceTouchUp: Long = 0
        @Volatile
        private var surfaceTouchCancel: Long = 0
        @Volatile
        private var surfaceLastTouch: String = "none"

        /**
         * Called from JNI (NativeBridge.onKeyboardInitFailed) to إظهار رسالة فشل تهيئة الكيبورد.
         */
        @JvmStatic
        fun onKeyboardInitFailed(reason: String) {
            Log.e("DisplayActivity", "Keyboard initialization failed: $reason")
            val msg = if (reason.contains("keymap", true) || reason.contains("xkbcommon", true)) {
                "Keyboard initialization failed: keymap or xkbcommon files are missing in chroot. Please ensure xkb files are copied to /usr/share/X11/xkb inside chroot."
            } else {
                "Keyboard initialization failed: $reason"
            }
            // Show Toast only if we have a valid activity
            val ctx = currentActivity
            if (ctx is Activity && !ctx.isFinishing && !ctx.isDestroyed) {
                ctx.runOnUiThread {
                    try {
                        Toast.makeText(ctx, msg, Toast.LENGTH_LONG).show()
                    } catch (e: Exception) {
                        Log.w("DisplayActivity", "Safe toast failed: ${e.message}")
                    }
                }
            }
        }

        // Helper to get current activity for static calls
        val currentActivity: Activity?
            get() = try {
                currentActivityRef?.get()
            } catch (_: Exception) { null }

        @JvmStatic
        fun recordSurfaceTouch(
            action: Int,
            pointerId: Int,
            pointerCount: Int,
            x: Float,
            y: Float,
            hasFocus: Boolean
        ) {
            surfaceTouchTotal += 1
            when (action) {
                android.view.MotionEvent.ACTION_DOWN,
                android.view.MotionEvent.ACTION_POINTER_DOWN -> surfaceTouchDown += 1
                android.view.MotionEvent.ACTION_MOVE -> surfaceTouchMove += 1
                android.view.MotionEvent.ACTION_UP,
                android.view.MotionEvent.ACTION_POINTER_UP -> surfaceTouchUp += 1
                android.view.MotionEvent.ACTION_CANCEL -> surfaceTouchCancel += 1
            }
            surfaceLastTouch =
                "action=${motionActionName(action)} id=$pointerId pointers=$pointerCount x=${x.toInt()} y=${y.toInt()} focus=$hasFocus"
        }

        @JvmStatic
        fun resetSurfaceTouchStats() {
            surfaceTouchTotal = 0
            surfaceTouchDown = 0
            surfaceTouchMove = 0
            surfaceTouchUp = 0
            surfaceTouchCancel = 0
            surfaceLastTouch = "none"
        }

        private fun motionActionName(action: Int): String = when (action) {
            android.view.MotionEvent.ACTION_DOWN -> "DOWN"
            android.view.MotionEvent.ACTION_UP -> "UP"
            android.view.MotionEvent.ACTION_MOVE -> "MOVE"
            android.view.MotionEvent.ACTION_CANCEL -> "CANCEL"
            android.view.MotionEvent.ACTION_POINTER_DOWN -> "POINTER_DOWN"
            android.view.MotionEvent.ACTION_POINTER_UP -> "POINTER_UP"
            else -> action.toString()
        }

        /** Called from JNI when a Wayland app enables text input — show Android soft keyboard. */
        @JvmStatic
        fun showSoftKeyboard() {
            val ctx = currentActivity ?: return
            ctx.runOnUiThread {
                val imm = ctx.getSystemService(Context.INPUT_METHOD_SERVICE) as? android.view.inputmethod.InputMethodManager ?: return@runOnUiThread
                val target = activeSurfaceView ?: ctx.window.decorView
                target.requestFocus()
                imm.showSoftInput(target, android.view.inputmethod.InputMethodManager.SHOW_IMPLICIT)
            }
        }

        /** Called from JNI when a Wayland app disables text input — hide Android soft keyboard. */
        @JvmStatic
        fun hideSoftKeyboard() {
            val ctx = currentActivity ?: return
            ctx.runOnUiThread {
                val imm = ctx.getSystemService(Context.INPUT_METHOD_SERVICE) as? android.view.inputmethod.InputMethodManager ?: return@runOnUiThread
                val token = ctx.window.decorView.windowToken
                imm.hideSoftInputFromWindow(token, 0)
            }
        }
    }

    private lateinit var clipboardManager: ClipboardManager
    private val waylandClipboardListener: (String) -> Unit = { text -> updateAndroidClipboard(text) }
    @Volatile
    private var suppressNextClipboardSync = false
    @Volatile
    private var lastSyncedClipboardText: String? = null
    @Volatile
    private var clipboardListenerRegistered = false
    @Volatile
    private var isActivityForeground = false
    private val didRequestGuestStart = AtomicBoolean(false)
    private var isNativeBridgeInitialized = false
    private val primaryClipChangedListener = ClipboardManager.OnPrimaryClipChangedListener {
        if (suppressNextClipboardSync) {
            suppressNextClipboardSync = false
            return@OnPrimaryClipChangedListener
        }

        // Android restricts clipboard access for background apps.
        if (!isActivityForeground || !hasWindowFocus()) {
            // Ignore silent sync to reduce error messages
            return@OnPrimaryClipChangedListener
        }
        try {
            clipboardManager.primaryClip?.getItemAt(0)?.text?.let { text ->
                val value = text.toString()
                if (value == lastSyncedClipboardText) {
                    return@let
                }
                lastSyncedClipboardText = value
                NativeBridge.sendClipboardTextToWayland(value)
            }
        } catch (e: Exception) {
            Log.w("DisplayActivity", "Clipboard read denied", e)
        }
    }

    private var distroId: String = "ubuntu"

    override fun onCreate(savedInstanceState: Bundle?) {
        distroId = intent.getStringExtra("distro_id") ?: "ubuntu"
        Log.i("WinlandDiag", "onCreate: Entry. Distro: $distroId. Native libraries loaded: ${NativeBridge.isLoaded()}")
        super.onCreate(savedInstanceState)
        markAsCurrentActivity()

        clipboardManager = getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        NativeBridge.setClipboardListener(waylandClipboardListener)

        setContent {
            var showTouchMonitor by remember { mutableStateOf(false) }
            var touchMonitorText by remember { mutableStateOf("") }
            var keyboardVisible by remember { mutableStateOf(false) }
            var ctrlActive by remember { mutableStateOf(false) }
            var altActive by remember { mutableStateOf(false) }

            LaunchedEffect(showTouchMonitor) {
                if (showTouchMonitor) {
                    while (showTouchMonitor) {
                        touchMonitorText = buildTouchMonitorReport()
                        delay(500)
                    }
                }
            }

            val imeCheckView = LocalView.current
            LaunchedEffect(Unit) {
                while (true) {
                    val imeVisible = ViewCompat.getRootWindowInsets(imeCheckView)
                        ?.isVisible(WindowInsetsCompat.Type.ime()) ?: false
                    if (imeVisible != keyboardVisible) {
                        keyboardVisible = imeVisible
                    }
                    delay(100)
                }
            }

            // Immersive Full Screen - AndroidX Compat
            // FIXED: Safety check for decorView attachment
            SideEffect {
                WindowCompat.setDecorFitsSystemWindows(window, false)
                val controller = WindowInsetsControllerCompat(window, window.decorView)
                controller.let {
                    it.hide(WindowInsetsCompat.Type.systemBars())
                    it.systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
                }
            }

            WinlandServerTheme {
                Log.i("WinlandDiag", "WinlandServerTheme: Entry")
                Box(modifier = Modifier
                    .fillMaxSize()
                    .background(Color.Black)
                ) {
                    LinuxDisplay()

                    if (keyboardVisible) {
                        Column(
                            modifier = Modifier
                                .align(Alignment.BottomStart)
                                .imePadding()
                        ) {
                            ExtraKeysBar(
                                ctrlActive = ctrlActive,
                                altActive = altActive,
                                onModifierToggle = { key, active ->
                                    if (key == "CTRL") ctrlActive = active
                                    if (key == "ALT") altActive = active
                                }
                            )
                        }
                    }

                    if (showTouchMonitor) {
                        Box(
                            modifier = Modifier
                                .align(Alignment.TopStart)
                                .padding(12.dp)
                                .background(Color.Black.copy(alpha = 0.82f))
                                .padding(horizontal = 10.dp, vertical = 8.dp)
                        ) {
                            Text(
                                text = touchMonitorText,
                                color = Color.White,
                                style = MaterialTheme.typography.labelSmall,
                                fontFamily = FontFamily.Monospace
                            )
                        }
                    }

                    SmallFloatingActionButton(
                        onClick = {
                            showTouchMonitor = !showTouchMonitor
                            if (showTouchMonitor) {
                                resetSurfaceTouchStats()
                                touchMonitorText = buildTouchMonitorReport()
                            }
                        },
                        modifier = Modifier
                            .align(Alignment.TopEnd)
                            .padding(16.dp),
                        containerColor = if (showTouchMonitor) {
                            MaterialTheme.colorScheme.tertiary.copy(alpha = 0.75f)
                        } else {
                            MaterialTheme.colorScheme.secondary.copy(alpha = 0.6f)
                        }
                    ) {
                        Text("MON", color = Color.White, style = MaterialTheme.typography.labelSmall)
                    }

                    // Translucent floating keyboard button — always visible
                    SmallFloatingActionButton(
                        onClick = { toggleKeyboard() },
                        modifier = Modifier
                            .align(Alignment.BottomEnd)
                            .padding(16.dp),
                        containerColor = MaterialTheme.colorScheme.primary.copy(alpha = 0.5f)
                    ) {
                        Icon(Icons.Default.Keyboard, contentDescription = "Toggle Keyboard")
                    }
                }
            }
        }
    }

    @Suppress("DEPRECATION")
    private fun View.hapticTap() {
        val constant = if (Build.VERSION.SDK_INT >= 30) {
            HapticFeedbackConstants.KEYBOARD_TAP
        } else {
            HapticFeedbackConstants.CONFIRM
        }
        performHapticFeedback(constant, HapticFeedbackConstants.FLAG_IGNORE_GLOBAL_SETTING)
    }

    @Composable
    fun ExtraKeysBar(
        ctrlActive: Boolean,
        altActive: Boolean,
        onModifierToggle: (key: String, active: Boolean) -> Unit,
    ) {
        val view = LocalView.current
        val regularKeys = listOf("ESC", "TAB", "↑", "↓", "←", "→")

        LazyRow(
            modifier = Modifier
                .fillMaxWidth()
                .background(Color.Black.copy(alpha = 0.5f))
                .padding(4.dp),
            horizontalArrangement = Arrangement.spacedBy(4.dp)
        ) {
            item {
                Button(
                    onClick = {
                        view.hapticTap()
                        forceShowKeyboard()
                    },
                    contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = 8.dp, vertical = 4.dp),
                    colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.primary.copy(alpha = 0.7f))
                ) {
                    Icon(Icons.Default.Keyboard, contentDescription = "Show Keyboard", tint = Color.White)
                }
            }

            // CTRL — toggle modifier with visual state
            item {
                val bgColor = if (ctrlActive) Color(0xFF00BCD4) else Color.DarkGray
                Button(
                    onClick = {
                        view.hapticTap()
                        val newState = !ctrlActive
                        onModifierToggle("CTRL", newState)
                        runIfNativeLoaded("CTRL") {
                            NativeBridge.sendKeyEvent(KeyEvent.KEYCODE_CTRL_LEFT, newState)
                        }
                    },
                    contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = 8.dp, vertical = 4.dp),
                    colors = ButtonDefaults.buttonColors(containerColor = bgColor)
                ) {
                    Text("CTRL", color = Color.White, style = MaterialTheme.typography.labelSmall,
                        fontWeight = if (ctrlActive) androidx.compose.ui.text.font.FontWeight.Bold else androidx.compose.ui.text.font.FontWeight.Normal)
                }
            }

            // ALT — toggle modifier with visual state
            item {
                val bgColor = if (altActive) Color(0xFFFF9800) else Color.DarkGray
                Button(
                    onClick = {
                        view.hapticTap()
                        val newState = !altActive
                        onModifierToggle("ALT", newState)
                        runIfNativeLoaded("ALT") {
                            NativeBridge.sendKeyEvent(KeyEvent.KEYCODE_ALT_LEFT, newState)
                        }
                    },
                    contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = 8.dp, vertical = 4.dp),
                    colors = ButtonDefaults.buttonColors(containerColor = bgColor)
                ) {
                    Text("ALT", color = Color.White, style = MaterialTheme.typography.labelSmall,
                        fontWeight = if (altActive) androidx.compose.ui.text.font.FontWeight.Bold else androidx.compose.ui.text.font.FontWeight.Normal)
                }
            }

            items(regularKeys) { key ->
                Button(
                    onClick = {
                        view.hapticTap()
                        simulateKey(key)
                    },
                    contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = 8.dp, vertical = 4.dp),
                    colors = ButtonDefaults.buttonColors(containerColor = Color.DarkGray)
                ) {
                    Text(key, color = Color.White, style = MaterialTheme.typography.labelSmall)
                }
            }
        }
    }

    private fun simulateKey(key: String) {
        val keyCode = when(key) {
            "ESC" -> KeyEvent.KEYCODE_ESCAPE
            "TAB" -> KeyEvent.KEYCODE_TAB
            "↑" -> KeyEvent.KEYCODE_DPAD_UP
            "↓" -> KeyEvent.KEYCODE_DPAD_DOWN
            "←" -> KeyEvent.KEYCODE_DPAD_LEFT
            "→" -> KeyEvent.KEYCODE_DPAD_RIGHT
            else -> 0
        }
        if (keyCode != 0) {
            runIfNativeLoaded("simulateKey:$key") {
                NativeBridge.sendKeyEvent(keyCode, true)
                NativeBridge.sendKeyEvent(keyCode, false)
            }
        }
    }

    private fun buildTouchMonitorReport(): String {
        val nativeStats = runCatching { NativeBridge.getWaylandRuntimeStats() }
            .getOrElse { "native_error=${it.message ?: "unknown"}" }
        val injectorTouches = extractLong(nativeStats, "td=(\\d+)") +
            extractLong(nativeStats, "tm=(\\d+)") +
            extractLong(nativeStats, "tu=(\\d+)") +
            extractLong(nativeStats, "tc=(\\d+)")
        val smithayInjected = extractLong(nativeStats, "injected_events=(\\d+)")
        val diagnosis = when {
            surfaceTouchTotal == 0L -> "diagnosis=system_or_surface_view"
            surfaceTouchTotal > 0L && injectorTouches == 0L -> "diagnosis=android_to_jni_gap"
            injectorTouches > 0L && smithayInjected == 0L -> "diagnosis=smithay_injection_gap"
            else -> "diagnosis=smithay_receiving_events"
        }

        return buildString {
            appendLine(diagnosis)
            appendLine(
                "surface(total=$surfaceTouchTotal down=$surfaceTouchDown move=$surfaceTouchMove up=$surfaceTouchUp cancel=$surfaceTouchCancel)"
            )
            appendLine("surface_last=$surfaceLastTouch")
            val lastSeat = Regex("(?:last_seat|last_seat_dispatch)=([^|]+)")
                .find(nativeStats)
                ?.groupValues
                ?.getOrNull(1)
                ?.trim()
                ?: "unknown"
            appendLine("seat_dispatch=$lastSeat")
            append("native=$nativeStats")
        }
    }

    private fun extractLong(text: String, pattern: String): Long {
        return Regex(pattern).find(text)?.groupValues?.getOrNull(1)?.toLongOrNull() ?: 0L
    }

    @Composable
    fun LinuxDisplay() {
        AndroidView(
            factory = { context ->
                Log.i("WinlandDiag", "LinuxDisplay: AndroidView Factory invoked")
                WaylandInputSurfaceView(context).apply {
                    requestFocus()
                    holder.addCallback(object : SurfaceHolder.Callback {
                        override fun surfaceCreated(holder: SurfaceHolder) {
                            Log.i("WinlandDiag", "surfaceCreated: Surface is ready")
                            android.util.Log.i("DisplayActivity", "com.winland.server: surfaceCreated")
                            
                            // [Crystal Clear Fix]: Force RGBA_8888 for exact pixel mapping
                            holder.setFormat(android.graphics.PixelFormat.RGBA_8888)

                            // Surface ready for rendering from native backend
                            // Winland Fix: Use a delay to ensure Android Surface plumbing is fully settled 
                            // before the heavy EGL thread binds to it. Prevents EGL_BAD_ACCESS on Adreno.
                            lifecycleScope.launch(Dispatchers.Main) {
                                delay(300)
                                
                                // --- [Winland OS: Secure Environment] ---
                                // Note: Socket ownership and XKB permissions are now handled by 'winland-setup' or
                                // the ChrootInstaller backend to prevent UI freezes and seccomp violations.
                                
                                if (!NativeBridge.isLoaded()) {
                                    handleNativeInitFailure("Native libraries are not loaded")
                                    return@launch
                                }

                                // --- [Winland OS: Pre-flight Environment Validation] ---
                                val status = withContext(Dispatchers.IO) {
                                    ChrootInstaller.getChrootStatus(context, distroId)
                                }
                                if (!status.ready) {
                                    val err = "Environment not ready: ${status.reason}"
                                    Log.e("DisplayActivity", err)
                                    withContext(Dispatchers.Main) {
                                        Toast.makeText(context, err, Toast.LENGTH_LONG).show()
                                    }
                                    handleNativeInitFailure(err)
                                    return@launch
                                }


                                if (isNativeBridgeInitialized) {
                                    Log.i("WinlandDiag", "NativeBridge already initialized, rebinding surface...")
                                    NativeBridge.rebindSurface(holder.surface)
                                    return@launch
                                }

                                val nativeInitOk = withContext(Dispatchers.IO) {
                                    NativeBridge.initWaylandConnection(holder.surface, this@DisplayActivity)
                                }
                                isNativeBridgeInitialized = nativeInitOk
                                Log.i("WinlandDiag", "NativeBridge.initWaylandConnection: result=$nativeInitOk")
                                
                                if (nativeInitOk) {
                                    // Set refresh rate for smooth 120Hz sync
                                    val refreshRate = if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.R) {
                                        @Suppress("DEPRECATION")
                                        display?.mode?.refreshRate ?: 60f
                                    } else {
                                        @Suppress("DEPRECATION")
                                        windowManager.defaultDisplay.refreshRate
                                    }
                                    NativeBridge.setRefreshRate(refreshRate)
                                    android.util.Log.i("DisplayActivity", "Configured native refresh rate: $refreshRate Hz")

                                    val prefs = context.getSharedPreferences("winland_settings", Context.MODE_PRIVATE)
                                    NativeBridge.setScrollSensitivity(prefs.getFloat("scroll_sensitivity", 1.0f))
                                    NativeBridge.initAudioBridge()
                                    

                                    if (didRequestGuestStart.compareAndSet(false, true)) {
                                        lifecycleScope.launch(Dispatchers.IO) {
                                            Log.i("WinlandDiag", "Guest Start: Waiting for Wayland socket probe...")

                                            // Silent guard: ensure compositor thread has booted before probing
                                            var waited = 0
                                            while (!WinlandService.socketRuntimeReady && waited < 50) {
                                                delay(100)
                                                waited++
                                            }
                                            if (!WinlandService.socketRuntimeReady) {
                                                Log.w("WinlandDiag", "Guest Start: Runtime not ready after 5s, probing anyway")
                                            } else if (waited > 0) {
                                                Log.i("WinlandDiag", "Guest Start: Compositor runtime ready after ${waited * 100}ms")
                                            }

                                            val socketReady = waitForWaylandSocket(context.getUnifiedFilesDir().let { java.io.File(it) })
                                            
                                            if (socketReady) {
                                                Log.i("WinlandDiag", "Guest Start: Socket detected! Booting Linux desktop ($distroId)...")
                                                val res = ChrootInstaller.startChroot(context, distroId, context.resources.displayMetrics.density)
                                                if (res.isFailure) {
                                                    val err = res.exceptionOrNull()
                                                    Log.e("WinlandDiag", "Guest Start: FAILED - ${err?.message}")
                                                    withContext(Dispatchers.Main) {
                                                        Toast.makeText(context, "Linux Start Failed: ${err?.message}", Toast.LENGTH_LONG).show()
                                                    }
                                                }
                                            } else {
                                                Log.e("WinlandDiag", "Guest Start: ABORTED - Wayland socket timeout")
                                                didRequestGuestStart.set(false)
                                                withContext(Dispatchers.Main) {
                                                    Toast.makeText(context, "Graphics Bridge Timeout", Toast.LENGTH_LONG).show()
                                                }
                                            }
                                        }
                                    }

                                    withContext(Dispatchers.Main) {
                                        android.widget.Toast.makeText(context, "Wayland Engine Started", android.widget.Toast.LENGTH_SHORT).show()
                                    }
                                } else {
                                    handleNativeInitFailure("Failed to initialize Wayland native bridge")
                                }
                            }
                        }
                        override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
                            android.util.Log.i("DisplayActivity", "com.winland.server: surfaceChanged format=$format width=$width height=$height")
                            // Subtract camera cutout / status bar from height to get safe area
                            // below the camera notch. This ensures the Linux desktop never overlaps
                            // the punch-hole area.
                            val cutoutHeight = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                                val insets = window.decorView.rootWindowInsets
                                if (insets != null) {
                                    insets.getInsetsIgnoringVisibility(WindowInsets.Type.statusBars()).top
                                } else 0
                            } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                                window.decorView.rootWindowInsets?.displayCutout?.safeInsetTop ?: 0
                            } else 0
                            val safeWidth = width
                            val safeHeight = if (cutoutHeight > 0) height - cutoutHeight else height
                            if (cutoutHeight > 0) {
                                android.util.Log.i("DisplayActivity", "Cutout height=$cutoutHeight, safe area=${safeWidth}x${safeHeight}")
                            }
                            val metrics = DisplayMetrics()
                            @Suppress("DEPRECATION")
                            windowManager.defaultDisplay.getRealMetrics(metrics)
                            val physW = (metrics.widthPixels * 25.4f / metrics.xdpi).toInt()
                            val physH = (metrics.heightPixels * 25.4f / metrics.ydpi).toInt()
                            runIfNativeLoaded("onSurfaceChanged") {
                                NativeBridge.onSurfaceChanged(safeWidth, safeHeight, physW, physH)
                                if (cutoutHeight > 0) {
                                    NativeBridge.setYOffset(cutoutHeight)
                                }
                            }
                            // Do NOT call initWaylandConnection here — surfaceCreated's coroutine
                            // handles initialization after a 300ms settling delay. Calling it here
                            // races with that coroutine (no delay), causing double EGL init.
                        }
                        override fun surfaceDestroyed(holder: SurfaceHolder) {
                            android.util.Log.w("DisplayActivity", "com.winland.server: surfaceDestroyed")
                            // Direct call stops rendering immediately. No async fallback needed —
                            // onStop() provides the lifecycle safety net.
                            runCatching {
                                NativeBridge.suspendRendering()
                            }.onFailure {
                                Log.w("DisplayActivity", "Immediate suspendRendering failed in surfaceDestroyed", it)
                            }
                            activeSurfaceView = null
                        }
                    })

                    }
                },
            modifier = Modifier.fillMaxSize()
        )
    }

    private fun handleNativeInitFailure(message: String) {
        didRequestGuestStart.set(false)
        val diag = "com.winland.server: native init failure: $message"
        android.util.Log.e("DisplayActivity", diag)
        appendWaylandDiag(diag)
        // Do not try to show Toast if Activity is finishing
        if (!isFinishing && !isDestroyed) {
            runOnUiThread {
                Toast.makeText(this, message, Toast.LENGTH_LONG).show()
                finish()
            }
        }
    }

    private fun appendWaylandDiag(message: String) {
        runCatching {
            val tmpDir = File(filesDir, "tmp")
            if (!tmpDir.exists()) {
                tmpDir.mkdirs()
            }
            val logFile = File(tmpDir, "wayland-debug.txt")
            logFile.appendText("${System.currentTimeMillis()} $message\n")
        }
    }

    private fun setupClipboardListener() {
        if (!clipboardListenerRegistered) {
            clipboardManager.addPrimaryClipChangedListener(primaryClipChangedListener)
            clipboardListenerRegistered = true
        }
    }

    private fun teardownClipboardListener() {
        if (clipboardListenerRegistered) {
            clipboardManager.removePrimaryClipChangedListener(primaryClipChangedListener)
            clipboardListenerRegistered = false
        }
    }

    // This could be called from NativeBridge.onWaylandClipboardChanged
    fun updateAndroidClipboard(text: String) {
        runOnUiThread {
            if (text == lastSyncedClipboardText) {
                return@runOnUiThread
            }

            // Android security: Cannot set clipboard unless in foreground and focused.
            if (!isActivityForeground || !hasWindowFocus()) {
                Log.d("DisplayActivity", "Ignoring background clipboard update from Wayland")
                return@runOnUiThread
            }

            try {
                suppressNextClipboardSync = true
                lastSyncedClipboardText = text
                val clip = ClipData.newPlainText("Wayland", text)
                clipboardManager.setPrimaryClip(clip)
                Log.i("DisplayActivity", "Synced Wayland clipboard to Android")
            } catch (e: Exception) {
                Log.w("DisplayActivity", "Clipboard access denied (likely focus lost)", e)
                suppressNextClipboardSync = false
            }
        }
    }

    override fun onDestroy() {
        traceLifecycle("onDestroy") {
            clearCurrentActivityIfSelf()
            NativeBridge.setClipboardListener(null)
            teardownClipboardListener()
            super.onDestroy()
        }
    }

    override fun onStart() {
        traceLifecycle("onStart") {
            super.onStart()
            markAsCurrentActivity()
        }
    }

    override fun onResume() {
        traceLifecycle("onResume") {
            super.onResume()
            isActivityForeground = true
            markAsCurrentActivity()
            setupClipboardListener()
            setNativeRenderingActiveAsync(active = true)
        }
    }

    override fun onStop() {
        traceLifecycle("onStop") {
            isActivityForeground = false
            teardownClipboardListener()
            setNativeRenderingActiveAsync(active = false)
            clearCurrentActivityIfSelf()
            super.onStop()
        }
    }

    private fun setNativeRenderingActiveAsync(active: Boolean) {
        lifecycleScope.launch(Dispatchers.Default) {
            runCatching {
                if (active) {
                    NativeBridge.resumeRendering()
                } else {
                    NativeBridge.suspendRendering()
                }
            }.onFailure {
                Log.w("DisplayActivity", "Failed to toggle rendering state active=$active", it)
            }
        }
    }

    private fun markAsCurrentActivity() {
        currentActivityRef = WeakReference(this)
        Log.d("DisplayActivity", "Current activity attached")
    }

    private fun clearCurrentActivityIfSelf() {
        val current = currentActivityRef?.get()
        if (current === this) {
            currentActivityRef = null
            Log.d("DisplayActivity", "Current activity detached")
        }
    }

    private fun toggleKeyboard() {
        val imm = getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
        val target = activeSurfaceView ?: window.decorView
        val decor = window.decorView
        val imeVisible = ViewCompat.getRootWindowInsets(decor)?.isVisible(WindowInsetsCompat.Type.ime()) == true
        if (imeVisible) {
            imm.hideSoftInputFromWindow(decor.windowToken, 0)
        } else {
            target.requestFocus()
            imm.showSoftInput(target, InputMethodManager.SHOW_IMPLICIT)
        }
    }

    // SHOW_FORCED is deprecated in API 33 but intentionally used here: the force-keyboard button
    // is specifically for use in games/apps that don't declare text fields, so forcing is correct.
    @Suppress("DEPRECATION")
    private fun forceShowKeyboard() {
        val imm = getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
        val target = activeSurfaceView ?: window.decorView
        target.requestFocus()
        imm.showSoftInput(target, InputMethodManager.SHOW_FORCED)
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        // Wayland resize events should be handled here if supported by the bridge
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent?): Boolean {
        runIfNativeLoaded("onKeyDown:$keyCode") {
            NativeBridge.sendKeyEvent(keyCode, true)
        }
        return super.onKeyDown(keyCode, event)
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent?): Boolean {
        runIfNativeLoaded("onKeyUp:$keyCode") {
            NativeBridge.sendKeyEvent(keyCode, false)
        }
        return super.onKeyUp(keyCode, event)
    }

    @android.annotation.SuppressLint("ClickableViewAccessibility")
    private class WaylandInputSurfaceView(context: Context) : SurfaceView(context) {
        private var lastMoveDispatchUptimeMs: Long = 0L

        init {
            // setZOrderOnTop removed: default Z-order lets Compose overlays render above SurfaceView
            holder.setFormat(android.graphics.PixelFormat.RGBA_8888)
            isFocusable = true
            isFocusableInTouchMode = true
            activeSurfaceView = this
        }

        @android.annotation.SuppressLint("ClickableViewAccessibility")
        override fun onTouchEvent(event: android.view.MotionEvent): Boolean {
            if (!holder.surface.isValid) {
                // Drop input while the Surface is invalid to avoid JNI pressure during teardown/rebind.
                return true
            }

            val actionMasked = event.actionMasked
            when (actionMasked) {
                android.view.MotionEvent.ACTION_DOWN,
                android.view.MotionEvent.ACTION_POINTER_DOWN -> {
                    // Prevent any parent ViewGroup or Compose container from stealing events
                    parent?.requestDisallowInterceptTouchEvent(true)
                    requestFocus()
                    val i = event.actionIndex
                    recordSurfaceTouch(actionMasked, event.getPointerId(i), event.pointerCount, event.getX(i), event.getY(i), hasWindowFocus())
                    if (NativeBridge.isLoaded()) {
                        NativeBridge.sendTouchEvent(actionMasked, event.getPointerId(i), event.getX(i), event.getY(i))
                    }
                }
                android.view.MotionEvent.ACTION_MOVE -> {
                    val now = SystemClock.uptimeMillis()
                    if (now - lastMoveDispatchUptimeMs < MOVE_DISPATCH_MIN_INTERVAL_MS) {
                        return true
                    }
                    lastMoveDispatchUptimeMs = now

                    for (i in 0 until event.pointerCount) {
                        recordSurfaceTouch(
                            android.view.MotionEvent.ACTION_MOVE,
                            event.getPointerId(i),
                            event.pointerCount,
                            event.getX(i),
                            event.getY(i),
                            hasWindowFocus()
                        )
                        if (NativeBridge.isLoaded()) {
                            NativeBridge.sendTouchEvent(
                                android.view.MotionEvent.ACTION_MOVE,
                                event.getPointerId(i),
                                event.getX(i),
                                event.getY(i)
                            )
                        }
                    }
                }
                android.view.MotionEvent.ACTION_UP,
                android.view.MotionEvent.ACTION_POINTER_UP -> {
                    val i = event.actionIndex
                    recordSurfaceTouch(actionMasked, event.getPointerId(i), event.pointerCount, event.getX(i), event.getY(i), hasWindowFocus())
                    if (NativeBridge.isLoaded()) {
                        NativeBridge.sendTouchEvent(actionMasked, event.getPointerId(i), event.getX(i), event.getY(i))
                    }
                    if (actionMasked == android.view.MotionEvent.ACTION_UP) {
                        parent?.requestDisallowInterceptTouchEvent(false)
                        performClick()
                    }
                }
                android.view.MotionEvent.ACTION_CANCEL -> {
                    parent?.requestDisallowInterceptTouchEvent(false)
                    for (i in 0 until event.pointerCount) {
                        val pointerId = event.getPointerId(i)
                        val x = event.getX(i)
                        val y = event.getY(i)
                        recordSurfaceTouch(android.view.MotionEvent.ACTION_CANCEL, pointerId, event.pointerCount, x, y, hasWindowFocus())
                        if (NativeBridge.isLoaded()) {
                            NativeBridge.sendTouchEvent(android.view.MotionEvent.ACTION_CANCEL,
                                pointerId, x, y)
                        }
                    }
                }
            }
            return true
        }

        override fun onDetachedFromWindow() {
            if (activeSurfaceView === this) activeSurfaceView = null
            super.onDetachedFromWindow()
        }

        override fun onCheckIsTextEditor(): Boolean = true

        override fun performClick(): Boolean {
            super.performClick()
            return true
        }

        override fun onCreateInputConnection(outAttrs: EditorInfo): InputConnection {
            outAttrs.inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_MULTI_LINE
            outAttrs.imeOptions = EditorInfo.IME_ACTION_NONE

            return object : BaseInputConnection(this, false) {
                override fun commitText(text: CharSequence?, newCursorPosition: Int): Boolean {
                    if (!text.isNullOrEmpty()) {
                        if (NativeBridge.isLoaded()) {
                            NativeBridge.sendTextInput(text.toString())
                        }
                    }
                    return true
                }

                override fun setComposingText(text: CharSequence?, newCursorPosition: Int): Boolean {
                    if (!text.isNullOrEmpty()) {
                        if (NativeBridge.isLoaded()) {
                            NativeBridge.sendTextInput(text.toString())
                        }
                    }
                    return true
                }

                override fun deleteSurroundingText(beforeLength: Int, afterLength: Int): Boolean {
                    if (beforeLength > 0) {
                        if (NativeBridge.isLoaded()) {
                            NativeBridge.sendTextInput("\b")
                        }
                    }
                    return true
                }

                override fun sendKeyEvent(event: KeyEvent): Boolean {
                    if (NativeBridge.isLoaded()) {
                        NativeBridge.sendKeyEvent(event.keyCode, event.action == KeyEvent.ACTION_DOWN)
                    }
                    return true
                }
            }
        }
    }
    private suspend fun waitForWaylandSocket(filesDir: File): Boolean {
        val socketPath = File(filesDir, "tmp/wayland-0")
        Log.i("WinlandDiag", "waitForWaylandSocket: Probing path=${socketPath.absolutePath}")
        
        repeat(150) { i ->
            val exists = socketPath.exists()
            val isSock = isUnixSocket(socketPath)
            
            if (exists || isSock) {
                Log.i("WinlandDiag", "waitForWaylandSocket: SUCCESS at iteration $i! exists=$exists, isSock=$isSock")
                return true
            }
            if (i % 20 == 0) {
                Log.d("WinlandDiag", "waitForWaylandSocket: Still waiting... iter=$i")
            }
            delay(100)
        }
        
        val finalExists = socketPath.exists()
        val finalIsSock = isUnixSocket(socketPath)
        Log.e("WinlandDiag", "waitForWaylandSocket: TIMEOUT! exists=$finalExists, isSock=$finalIsSock")
        return finalExists || finalIsSock
    }

    private fun isUnixSocket(file: File): Boolean {
        return try {
            val stat = Os.lstat(file.absolutePath)
            OsConstants.S_ISSOCK(stat.st_mode)
        } catch (_: ErrnoException) {
            false
        } catch (_: Exception) {
            false
        }
    }
}
