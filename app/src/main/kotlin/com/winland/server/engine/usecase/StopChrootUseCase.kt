package com.winland.server.engine.usecase

import android.content.Context
import com.winland.server.engine.ChrootInstaller

class StopChrootUseCase {
    suspend operator fun invoke(context: Context, distroId: String = "ubuntu"): Result<Unit> {
        return ChrootInstaller.stopChroot(context, distroId)
    }
}