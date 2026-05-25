package com.winland.server.engine

import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import android.util.Log
import kotlinx.coroutines.*
import java.net.InetAddress
import java.net.ServerSocket
import java.net.Socket

class WinlandAudioServer {
    private val TAG = "WinlandAudioServer"
    private var serverSocket: ServerSocket? = null
    private var audioTrack: AudioTrack? = null
    private var isRunning = false
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private var serverJob: Job? = null

    fun start() {
        if (isRunning) {
            Log.d(TAG, "Audio server already running")
            return
        }

        isRunning = true
        initAudioTrack()
        serverJob = scope.launch {
            while (isActive && isRunning) {
                try {
                    serverSocket = ServerSocket(47130, 50, InetAddress.getByName("127.0.0.1")).apply {
                        reuseAddress = true
                    }
                    Log.i(TAG, "Audio Server listening on 127.0.0.1:47130")
                    while (isRunning) {
                        val client = serverSocket?.accept()
                        if (client != null) {
                            handleClient(client)
                        }
                    }
                } catch (e: java.net.BindException) {
                    if (isRunning) {
                        Log.w(TAG, "Port 47130 already in use, retrying in 2s...")
                        delay(2000)
                    }
                } catch (e: Exception) {
                    if (isRunning) {
                        Log.e(TAG, "Audio Server error; retrying in 1s", e)
                        delay(1000)
                    }
                } finally {
                    try {
                        serverSocket?.close()
                    } catch (_: Exception) {
                    }
                    serverSocket = null
                }
            }
        }
    }

    private fun initAudioTrack() {
        val minBufferSize = AudioTrack.getMinBufferSize(
            44100,
            AudioFormat.CHANNEL_OUT_STEREO,
            AudioFormat.ENCODING_PCM_16BIT
        )
        audioTrack = AudioTrack.Builder()
            .setAudioAttributes(
                AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_MEDIA)
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                    .build()
            )
            .setAudioFormat(
                AudioFormat.Builder()
                    .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                    .setSampleRate(44100)
                    .setChannelMask(AudioFormat.CHANNEL_OUT_STEREO)
                    .build()
            )
            .setBufferSizeInBytes(minBufferSize)
            .setTransferMode(AudioTrack.MODE_STREAM)
            .build()
        audioTrack?.play()
    }

    private suspend fun handleClient(socket: Socket) = withContext(Dispatchers.IO) {
        Log.i(TAG, "Client connected for audio stream")
        val inputStream = socket.getInputStream()
        val buffer = ByteArray(8192) // Doubled buffer for smoother throughput
        try {
            while (isRunning && !socket.isClosed) {
                val bytesRead = inputStream.read(buffer)
                if (bytesRead == -1) break
                if (bytesRead > 0) {
                    audioTrack?.write(buffer, 0, bytesRead)
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Audio stream handling error", e)
        } finally {
            socket.close()
            Log.i(TAG, "Audio client disconnected")
        }
    }

    fun stop() {
        isRunning = false
        serverJob?.cancel()
        serverJob = null
        try { serverSocket?.close() } catch (_: Exception) {}
        try { audioTrack?.stop() } catch (_: Exception) {}
        try { audioTrack?.release() } catch (_: Exception) {}
        serverSocket = null
        audioTrack = null
    }
}
