package com.winland.server.engine.usecase

import android.content.Context
import com.winland.server.LinuxDistro
import com.winland.server.engine.DownloadManager
import com.winland.server.utils.getUnifiedFilesDir
import java.io.File

class DownloadAndExtractRootfsUseCase(
    private val extractRootfsUseCase: ExtractRootfsUseCase = ExtractRootfsUseCase()
) {

    enum class FailurePhase {
        DOWNLOAD,
        EXTRACT
    }

    data class Result(
        val success: Boolean,
        val failurePhase: FailurePhase? = null
    )

    suspend operator fun invoke(
        context: Context,
        distro: LinuxDistro,
        onProgress: (Float) -> Unit
    ): Result {
        val extension = if (distro.url.endsWith(".tar.gz")) ".tar.gz" else ".tar.xz"
        val targetFile = File(context.getUnifiedFilesDir(), "downloads/rootfs-${distro.id}$extension")
        val downloaded = DownloadManager.downloadRootfs(distro.url, targetFile, onProgress)
        if (!downloaded) {
            return Result(success = false, failurePhase = FailurePhase.DOWNLOAD)
        }

        val extracted = extractRootfsUseCase(context, targetFile.name, distro.id)
        if (extracted.isFailure) {
            return Result(success = false, failurePhase = FailurePhase.EXTRACT)
        }

        return Result(success = true)
    }
}