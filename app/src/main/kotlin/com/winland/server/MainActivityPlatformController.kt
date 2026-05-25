package com.winland.server

import android.Manifest
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbManager
import android.os.Build
import androidx.activity.result.ActivityResultLauncher
import androidx.core.content.ContextCompat

class MainActivityPlatformController(
    private val activity: MainActivity,
    private val usbManager: UsbManager,
    private val showToast: (String, Boolean) -> Unit
) {
    companion object {
        private const val ACTION_USB_PERMISSION = "com.winland.server.USB_PERMISSION"
    }

    private val usbReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            when (intent?.action) {
                UsbManager.ACTION_USB_DEVICE_DETACHED -> {
                    NativeBridge.closeUsbRedirection()
                    showToast("USB device detached", false)
                }

                ACTION_USB_PERMISSION -> {
                    val device: UsbDevice? = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                        intent.getParcelableExtra(UsbManager.EXTRA_DEVICE, UsbDevice::class.java)
                    } else {
                        @Suppress("DEPRECATION")
                        intent.getParcelableExtra(UsbManager.EXTRA_DEVICE)
                    }

                    if (intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)) {
                        device?.let {
                            val connection = usbManager.openDevice(it)
                            if (connection == null) {
                                showToast("Failed to open USB device", true)
                                return
                            }
                            try {
                                val fd = connection.fileDescriptor
                                uniffi.winland_core.initUsb(fd)
                                showToast("USB Attached to Linux", false)
                            } finally {
                                connection.close()
                            }
                        }
                    }
                }
            }
        }
    }

    fun registerUsbReceiver() {
        val filter = IntentFilter().apply {
            addAction(ACTION_USB_PERMISSION)
            addAction(UsbManager.ACTION_USB_DEVICE_DETACHED)
        }
        ContextCompat.registerReceiver(activity, usbReceiver, filter, ContextCompat.RECEIVER_NOT_EXPORTED)
    }

    fun unregisterUsbReceiver() {
        try {
            activity.unregisterReceiver(usbReceiver)
        } catch (_: Exception) {
        }
    }

    fun requestRequiredPermissions(permissionLauncher: ActivityResultLauncher<Array<String>>) {
        val permissions = mutableListOf(
            Manifest.permission.RECORD_AUDIO,
            Manifest.permission.CAMERA
        )
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            permissions.add(Manifest.permission.POST_NOTIFICATIONS)
        }
        if (ContextCompat.checkSelfPermission(activity, Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            android.util.Log.d("Winland", "Requesting camera permission")
        }
        permissionLauncher.launch(permissions.toTypedArray())
    }

    fun requestUsbPermission() {
        val device = usbManager.deviceList.values.firstOrNull()
        if (device != null) {
            val permissionIntent = PendingIntent.getBroadcast(
                activity,
                0,
                Intent(ACTION_USB_PERMISSION),
                PendingIntent.FLAG_IMMUTABLE
            )
            usbManager.requestPermission(device, permissionIntent)
        } else {
            showToast("No USB device found", false)
        }
    }
}