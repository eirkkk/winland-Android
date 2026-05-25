package com.winland.server.engine

import android.content.Context
import android.util.Log
import androidx.camera.core.*
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.core.content.ContextCompat
import androidx.lifecycle.LifecycleOwner
import kotlinx.coroutines.*
import java.io.OutputStream
import java.net.InetAddress
import java.net.ServerSocket
import java.net.Socket
import java.util.concurrent.Executors

class WinlandCameraBridge(private val context: Context) {
    private val TAG = "WinlandCamera"
    private var serverSocket: ServerSocket? = null
    private var clientSocket: Socket? = null
    private var outputStream: OutputStream? = null
    private var isRunning = false
    private val scope = CoroutineScope(Dispatchers.IO)
    private val cameraExecutor = Executors.newSingleThreadExecutor()
    private val frameSignalBuffer = ByteArray(8)
    @Volatile
    private var metadataOnlyNoticeLogged = false

    fun start(lifecycleOwner: LifecycleOwner) {
        isRunning = true
        startServer()
        setupCamera(lifecycleOwner)
    }

    private fun startServer() {
        scope.launch {
            try {
                serverSocket = ServerSocket(8000, 50, InetAddress.getLoopbackAddress())
                Log.i(TAG, "Camera Server listening on port 8000")
                while (isRunning) {
                    val client = serverSocket?.accept()
                    if (client != null) {
                        Log.i(TAG, "Linux client connected for camera stream")
                        synchronized(this@WinlandCameraBridge) {
                            clientSocket?.close()
                            clientSocket = client
                            outputStream = client.getOutputStream()
                        }
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Camera Server error", e)
            }
        }
    }

    private fun setupCamera(lifecycleOwner: LifecycleOwner) {
        val cameraProviderFuture = ProcessCameraProvider.getInstance(context)
        cameraProviderFuture.addListener({
            val cameraProvider = cameraProviderFuture.get()
            val imageAnalysis = ImageAnalysis.Builder()
                .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
                .build()

            imageAnalysis.setAnalyzer(cameraExecutor) { imageProxy ->
                // In a production app, convert YUV to MJPEG or raw byte stream
                // For demonstration, we just send dummy metadata to keep the loop active
                if (!metadataOnlyNoticeLogged) {
                    Log.w(TAG, "Camera bridge is running in metadata-only mode (width/height), not full frame payload")
                    metadataOnlyNoticeLogged = true
                }
                sendFrameData(imageProxy)
                imageProxy.close()
            }

            try {
                cameraProvider.unbindAll()
                cameraProvider.bindToLifecycle(
                    lifecycleOwner,
                    CameraSelector.DEFAULT_BACK_CAMERA,
                    imageAnalysis
                )
            } catch (e: Exception) {
                Log.e(TAG, "Camera binding failed", e)
            }
        }, ContextCompat.getMainExecutor(context))
    }

    private fun sendFrameData(image: ImageProxy) {
        synchronized(this) {
            outputStream?.let { stream ->
                try {
                    writeInt(frameSignalBuffer, 0, image.width)
                    writeInt(frameSignalBuffer, 4, image.height)
                    stream.write(frameSignalBuffer)
                    stream.flush()
                } catch (e: Exception) {
                    Log.e(TAG, "Failed to stream camera frame", e)
                    clientSocket = null
                    outputStream = null
                }
            }
        }
    }

    private fun writeInt(buffer: ByteArray, offset: Int, value: Int) {
        buffer[offset] = (value ushr 24).toByte()
        buffer[offset + 1] = (value ushr 16).toByte()
        buffer[offset + 2] = (value ushr 8).toByte()
        buffer[offset + 3] = value.toByte()
    }

    fun stop() {
        isRunning = false
        serverSocket?.close()
        clientSocket?.close()
        cameraExecutor.shutdown()
        scope.cancel()
    }
}
