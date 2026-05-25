package com.winland.server.engine.usecase

import android.content.Context
import com.winland.server.engine.ChrootInstaller

class PrepareRunUseCase {
    data class Result(
        val canLaunch: Boolean,
        val message: String
    )

    suspend operator fun invoke(context: Context, distroId: String = "ubuntu"): Result {
        val currentStatus = ChrootInstaller.getChrootStatus(context, distroId)
        return if (currentStatus.ready) {
            Result(
                canLaunch = true,
                message = "Run started"
            )
        } else {
            Result(
                canLaunch = false,
                message = currentStatus.reason
            )
        }
    }
}