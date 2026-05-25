package com.winland.server.engine.usecase

import android.content.Context
import com.winland.server.engine.ChrootInstaller

class ExtractRootfsUseCase {
    suspend operator fun invoke(context: Context, archiveName: String, distroId: String = "ubuntu"): Result<Unit> {
        return ChrootInstaller.extractRootfs(context, archiveName, distroId)
    }
}