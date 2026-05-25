package com.winland.server.engine.usecase

import android.content.Context
import com.winland.server.engine.ChrootInstaller

class RestartChrootUseCase {
    suspend operator fun invoke(context: Context, distroId: String = "ubuntu"): Result<Unit> {
        return ChrootInstaller.restartChroot(context, distroId)
    }
}