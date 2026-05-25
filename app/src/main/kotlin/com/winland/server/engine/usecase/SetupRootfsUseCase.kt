package com.winland.server.engine.usecase

import android.content.Context
import com.winland.server.engine.ChrootInstaller

class SetupRootfsUseCase {
    data class Result(
        val success: Boolean,
        val message: String,
        val stageText: String
    )

    suspend operator fun invoke(context: Context, distroId: String = "ubuntu"): Result {
        val installResult = ChrootInstaller.setupRootfs(context, distroId)
        val latestStatus = ChrootInstaller.getChrootStatus(context, distroId)

        return if (installResult.isSuccess && latestStatus.installed) {
            Result(
                success = true,
                message = "Setup complete. Button switched to Run.",
                stageText = "Setup finished successfully"
            )
        } else {
            val reason = installResult.exceptionOrNull()?.message ?: latestStatus.reason
            Result(
                success = false,
                message = "Setup failed: $reason",
                stageText = "Setup failed"
            )
        }
    }
}