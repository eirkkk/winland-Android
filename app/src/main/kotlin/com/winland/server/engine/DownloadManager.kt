package com.winland.server.engine

import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.io.InputStream
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean

object DownloadManager {

    private const val TAG = "WinlandDownloader"
    private val isDownloading = AtomicBoolean(false)
    // FIXED: Handle timeouts for network operations
    private val client = OkHttpClient.Builder()
        .connectTimeout(30, TimeUnit.SECONDS)
        .readTimeout(60, TimeUnit.SECONDS)
        .build()

    fun isDownloadInProgress(): Boolean = isDownloading.get()

    private fun looksLikeSupportedArchive(file: File): Boolean {
        if (!file.exists() || file.length() < 1024L) return false
        return try {
            val header = ByteArray(6)
            file.inputStream().use { input ->
                val read = input.read(header)
                if (read < 3) return false
            }

            val isGzip = header[0] == 0x1f.toByte() && header[1] == 0x8b.toByte()
            val isXz = header[0] == 0xFD.toByte() && header[1] == 0x37.toByte() && header[2] == 0x7A.toByte()
            isGzip || isXz
        } catch (e: Exception) {
            Log.e(TAG, "Failed to inspect archive signature", e)
            false
        }
    }

    suspend fun downloadRootfs(
        urlString: String,
        targetFile: File,
        onProgress: (Float) -> Unit
    ): Boolean = withContext(Dispatchers.IO) {
        if (!isDownloading.compareAndSet(false, true)) {
            Log.w(TAG, "Download already in progress; skipping duplicate request")
            return@withContext false
        }

        Log.d(TAG, "Download started for URL: $urlString")
        try {
            if (targetFile.parentFile?.exists() == false) {
                targetFile.parentFile?.mkdirs()
            }

            val tempFile = File(targetFile.parentFile, "${targetFile.name}.part")
            if (tempFile.exists()) {
                tempFile.delete()
            }

            val request = Request.Builder().url(urlString).build()
            client.newCall(request).execute().use { response ->
                if (!response.isSuccessful) {
                    Log.e(TAG, "Download failed with response code: ${response.code}")
                    return@withContext false
                }

                val contentType = response.header("Content-Type")?.lowercase() ?: ""
                if (contentType.contains("text/html") || contentType.contains("application/json")) {
                    Log.e(TAG, "Unexpected content type for rootfs archive: $contentType")
                    return@withContext false
                }

                val body = response.body
                if (body == null) {
                    Log.e(TAG, "Response body is null")
                    return@withContext false
                }

                val contentLength = body.contentLength()
                val inputStream: InputStream = body.byteStream()
                val outputStream = FileOutputStream(tempFile)

            val buffer = ByteArray(8192)
            var bytesRead: Int
            var totalBytesRead: Long = 0

            Log.d(TAG, "Writing to file: ${targetFile.absolutePath}")

                while (true) {
                    bytesRead = inputStream.read(buffer)
                    if (bytesRead == -1) break

                    outputStream.write(buffer, 0, bytesRead)
                    totalBytesRead += bytesRead
                    if (contentLength > 0) {
                        onProgress(totalBytesRead.toFloat() / contentLength.toFloat())
                    }
                }

                outputStream.flush()
                outputStream.close()
                inputStream.close()

                if (contentLength > 0 && totalBytesRead != contentLength) {
                    Log.e(TAG, "Download incomplete: read=$totalBytesRead expected=$contentLength")
                    tempFile.delete()
                    return@withContext false
                }

                if (!looksLikeSupportedArchive(tempFile)) {
                    Log.e(TAG, "Downloaded file is not a valid gzip/xz archive: ${tempFile.absolutePath}")
                    tempFile.delete()
                    return@withContext false
                }

                if (targetFile.exists() && !targetFile.delete()) {
                    Log.e(TAG, "Failed to replace old target file: ${targetFile.absolutePath}")
                    tempFile.delete()
                    return@withContext false
                }

                if (!tempFile.renameTo(targetFile)) {
                    Log.e(TAG, "Failed to move temp file into place: ${targetFile.absolutePath}")
                    tempFile.delete()
                    return@withContext false
                }
            }

            Log.d(TAG, "Download completed successfully")
            true
        } catch (e: IOException) {
            // FIXED: Explicitly handle network IO exceptions
            Log.e(TAG, "Network error during download", e)
            false
        } catch (e: Exception) {
            Log.e(TAG, "Unexpected error during download", e)
            false
        } finally {
            isDownloading.set(false)
        }
    }
}
