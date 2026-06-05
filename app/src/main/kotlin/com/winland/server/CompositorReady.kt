package com.winland.server

import kotlinx.coroutines.CompletableDeferred

object CompositorReady {
    val signal: CompletableDeferred<Unit> = CompletableDeferred()
}
