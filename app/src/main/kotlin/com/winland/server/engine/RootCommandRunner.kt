package com.winland.server.engine

import android.util.Log
import com.topjohnwu.superuser.CallbackList
import com.topjohnwu.superuser.Shell
import java.io.BufferedReader
import java.io.InterruptedIOException
import java.io.InputStream
import java.io.InputStreamReader
import java.io.OutputStream
import java.io.OutputStreamWriter
import java.io.PrintWriter
import java.util.concurrent.CancellationException
import java.util.concurrent.Executors
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.TimeoutException
import java.util.concurrent.atomic.AtomicReference
import kotlin.coroutines.resume
import kotlinx.coroutines.suspendCancellableCoroutine

object RootCommandRunner {

    private const val TAG = "RootCommandRunner"

    private class RootCommandFailedException(exitCode: Int) :
        IllegalStateException("root command failed with exit code $exitCode")

    private val callbackExecutor = Executors.newCachedThreadPool()

    private class StreamingList(
        private val onLine: (String) -> Unit
    ) : CallbackList<String>(callbackExecutor) {
        override fun onAddElement(element: String) {
            safeEmit(onLine, element)
        }
    }

    private fun safeEmit(callback: (String) -> Unit, line: String) {
        try {
            callback(line)
        } catch (t: Throwable) {
            Log.e(TAG, "Reader callback crashed; dropping line", t)
        }
    }

    private class ManagedInteractiveSession(
        private val writer: PrintWriter,
        private val closeAction: () -> Unit
    ) {
        fun send(command: String) {
            writer.println(command)
            writer.flush()
        }

        fun close() {
            try {
                writer.close()
            } catch (_: Exception) {
            }
            closeAction()
        }
    }

    class InteractiveSession private constructor(
        private val delegate: ManagedInteractiveSession
    ) {
        fun send(command: String) {
            delegate.send(command)
        }

        fun close() {
            delegate.close()
        }

        companion object {
            fun create(
                onStdout: (String) -> Unit,
                onStderr: (String) -> Unit,
                onExit: () -> Unit = {}
            ): InteractiveSession {
                return try {
                    InteractiveSession(createWithLibSu(onStdout, onStderr, onExit))
                } catch (_: Exception) {
                    InteractiveSession(createWithProcess(onStdout, onStderr, onExit))
                }
            }

            private fun createWithLibSu(
                onStdout: (String) -> Unit,
                onStderr: (String) -> Unit,
                onExit: () -> Unit
            ): ManagedInteractiveSession {
                val shell = Shell.Builder.create().build("su")
                val executor = Executors.newFixedThreadPool(3)
                val closeLatch = CountDownLatch(1)
                val writerRef = AtomicReference<PrintWriter?>(null)
                val initLatch = CountDownLatch(1)
                val failureRef = AtomicReference<Exception?>(null)

                executor.execute {
                    try {
                        shell.execTask(object : Shell.Task {
                            override fun run(
                                stdin: OutputStream,
                                stdout: InputStream,
                                stderr: InputStream
                            ) {
                                val writer = PrintWriter(OutputStreamWriter(stdin))
                                writerRef.set(writer)
                                startReader(stdout, onStdout)
                                startReader(stderr, onStderr)
                                initLatch.countDown()
                                closeLatch.await()
                            }
                        })
                    } catch (e: Exception) {
                        failureRef.set(e)
                        initLatch.countDown()
                    } finally {
                        try {
                            shell.waitAndClose()
                        } catch (_: Exception) {
                        }
                        onExit()
                        executor.shutdown()
                    }
                }

                initLatch.await()
                failureRef.get()?.let { throw it }
                val writer = writerRef.get() ?: throw IllegalStateException("interactive shell writer not initialized")

                return ManagedInteractiveSession(
                    writer = writer,
                    closeAction = {
                        closeLatch.countDown()
                    }
                )
            }

            private fun createWithProcess(
                onStdout: (String) -> Unit,
                onStderr: (String) -> Unit,
                onExit: () -> Unit
            ): ManagedInteractiveSession {
                val process = Runtime.getRuntime().exec("su")
                val writer = PrintWriter(process.outputStream)
                val executor = Executors.newFixedThreadPool(3)

                executor.execute {
                    try {
                        process.inputStream.bufferedReader().useLines { lines ->
                            lines.forEach { safeEmit(onStdout, it) }
                        }
                    } catch (t: Throwable) {
                        Log.e(TAG, "stdout reader crashed", t)
                    }
                }

                executor.execute {
                    try {
                        process.errorStream.bufferedReader().useLines { lines ->
                            lines.forEach { safeEmit(onStderr, it) }
                        }
                    } catch (t: Throwable) {
                        Log.e(TAG, "stderr reader crashed", t)
                    }
                }

                executor.execute {
                    try {
                        process.waitFor()
                    } finally {
                        onExit()
                        executor.shutdown()
                    }
                }

                return ManagedInteractiveSession(
                    writer = writer,
                    closeAction = {
                        process.destroy()
                    }
                )
            }

            private fun startReader(
                inputStream: InputStream,
                onLine: (String) -> Unit
            ) {
                callbackExecutor.execute {
                    try {
                        BufferedReader(InputStreamReader(inputStream)).useLines { lines ->
                            lines.forEach { safeEmit(onLine, it) }
                        }
                    } catch (t: Throwable) {
                        Log.e(TAG, "interactive reader crashed", t)
                    }
                }
            }
        }
    }

    suspend fun execute(
        command: String,
        timeout: Long,
        timeUnit: TimeUnit,
        onStdout: (String) -> Unit = {},
        onStderr: (String) -> Unit = {}
    ): Result<Unit> {
        val libSuResult = executeWithLibSu(command, timeout, timeUnit, onStdout, onStderr)
        val libSuFailure = libSuResult.exceptionOrNull()
        if (
            libSuResult.isSuccess ||
            libSuFailure is RootCommandFailedException ||
            libSuFailure is TimeoutException
        ) {
            return libSuResult
        }

        return suspendCancellableCoroutine { cont ->
            val processRef = AtomicReference<Process?>(null)

            val worker = Thread {
                try {
                    val process = Runtime.getRuntime().exec("su")
                    processRef.set(process)

                    val reader = process.inputStream.bufferedReader()
                    val errorReader = process.errorStream.bufferedReader()
                    val readersDone = CountDownLatch(2)

                    val stdoutThread = Thread {
                        try {
                            var line: String?
                            while (reader.readLine().also { line = it } != null) {
                                safeEmit(onStdout, line ?: "")
                            }
                        } catch (_: InterruptedIOException) {
                        } catch (_: Exception) {
                        } finally {
                            try {
                                reader.close()
                            } catch (_: Exception) {
                            }
                            readersDone.countDown()
                        }
                    }

                    val stderrThread = Thread {
                        try {
                            var line: String?
                            while (errorReader.readLine().also { line = it } != null) {
                                safeEmit(onStderr, line ?: "")
                            }
                        } catch (_: InterruptedIOException) {
                        } catch (_: Exception) {
                        } finally {
                            try {
                                errorReader.close()
                            } catch (_: Exception) {
                            }
                            readersDone.countDown()
                        }
                    }

                    stdoutThread.start()
                    stderrThread.start()

                    val writer = PrintWriter(process.outputStream)
                    writer.println(command)
                    writer.println("exit")
                    writer.flush()
                    writer.close()

                    val finished = process.waitFor(timeout, timeUnit)
                    if (!finished) {
                        process.destroyForcibly()
                    }

                    readersDone.await(2, TimeUnit.SECONDS)
                    val exitCode = if (finished) process.exitValue() else -1
                    val result = if (finished && exitCode == 0) {
                        Result.success(Unit)
                    } else {
                        Result.failure(IllegalStateException("root command failed with exit code $exitCode"))
                    }

                    if (cont.isActive) {
                        cont.resume(result)
                    }
                } catch (e: Exception) {
                    if (cont.isActive) {
                        cont.resume(Result.failure(e))
                    }
                }
            }

            cont.invokeOnCancellation {
                processRef.get()?.destroyForcibly()
                worker.interrupt()
            }

            worker.start()
        }
    }

    private suspend fun executeWithLibSu(
        command: String,
        timeout: Long,
        timeUnit: TimeUnit,
        onStdout: (String) -> Unit,
        onStderr: (String) -> Unit
    ): Result<Unit> {
        return suspendCancellableCoroutine { cont ->
            val futureRef = AtomicReference<java.util.concurrent.Future<Shell.Result>?>(null)

            val worker = Thread {
                try {
                    val stdout = StreamingList(onStdout)
                    val stderr = StreamingList(onStderr)
                    val future = Shell.cmd(command).to(stdout, stderr).enqueue()
                    futureRef.set(future)

                    val result = future.get(timeout, timeUnit)
                    val outcome = if (result.isSuccess) {
                        Result.success(Unit)
                    } else {
                        Result.failure(RootCommandFailedException(result.code))
                    }

                    if (cont.isActive) {
                        cont.resume(outcome)
                    }
                } catch (e: CancellationException) {
                    if (cont.isActive) {
                        cont.resume(Result.failure(e))
                    }
                } catch (e: Exception) {
                    if (cont.isActive) {
                        cont.resume(Result.failure(e))
                    }
                }
            }

            cont.invokeOnCancellation {
                futureRef.get()?.cancel(true)
                worker.interrupt()
            }

            worker.start()
        }
    }

    fun executeBlocking(
        command: String,
        timeout: Long,
        timeUnit: TimeUnit
    ): Boolean {
        try {
            val result = Shell.cmd(command).enqueue().get(timeout, timeUnit)
            return result.isSuccess
        } catch (_: Exception) {
        }

        return try {
            val process = Runtime.getRuntime().exec("su")
            val writer = PrintWriter(process.outputStream)
            writer.println(command)
            writer.println("exit")
            writer.flush()
            writer.close()

            val finished = process.waitFor(timeout, timeUnit)
            if (!finished) {
                process.destroyForcibly()
                return false
            }

            process.exitValue() == 0
        } catch (_: Exception) {
            false
        }
    }
}