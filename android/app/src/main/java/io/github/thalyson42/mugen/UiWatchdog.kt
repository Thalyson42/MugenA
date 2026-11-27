package io.github.thalyson42.mugen

import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.util.Log
import java.io.File

/**
 * REMOVER-NO-RELEASE (docs/android-port/05-diagnosticos-a-remover.md) item 4.
 * Diagnostic only (P3b M3, D-017). The native watchdog (mugen_watchdog.c) fires when the engine stops presenting frames;
 * the "app is not responding" dialog seen on the A065M came while the engine kept running, so the UI thread was the one
 * stuck. A helper thread pings the main looper once a second; if it does not answer for 3 s (and again at 8 s) this logs,
 * with the tag `mugen-ui`, the Java stack of the main thread and of the SDL thread (its top frame names the JNI call it is
 * blocked in) and the state and wait channel of every thread of the process. It never changes the app's behaviour.
 */
object UiWatchdog {
    private const val TAG = "mugen-ui"

    @Volatile
    private var started = false

    @Volatile
    private var lastPong = SystemClock.uptimeMillis()

    @JvmStatic
    fun start() {
        if (started) return
        started = true
        lastPong = SystemClock.uptimeMillis()
        Thread({ loop() }, "mugen-ui-watchdog").apply {
            isDaemon = true
            start()
        }
    }

    private fun loop() {
        val main = Handler(Looper.getMainLooper())
        var stalledSeconds = 0
        while (true) {
            main.post { lastPong = SystemClock.uptimeMillis() }
            try {
                Thread.sleep(1000)
            } catch (e: InterruptedException) {
                return
            }
            val age = SystemClock.uptimeMillis() - lastPong
            if (age < 3000) {
                if (stalledSeconds >= 3) Log.w(TAG, "UI thread answered again after ~$stalledSeconds s")
                stalledSeconds = 0
                continue
            }
            stalledSeconds++
            if (stalledSeconds == 1 || stalledSeconds == 6) {
                Log.w(TAG, "UI thread has not answered for ${age / 1000} s: dumping")
                dump()
                Log.w(TAG, "dump done")
            }
        }
    }

    private fun dump() {
        try {
            val mainThread = Looper.getMainLooper().thread
            Log.w(TAG, "main thread (${mainThread.name}) state ${mainThread.state}:")
            for (f in mainThread.stackTrace) Log.w(TAG, "  at $f")
            for ((th, frames) in Thread.getAllStackTraces()) {
                if (th === mainThread || !th.name.contains("SDL", ignoreCase = true)) continue
                Log.w(TAG, "thread ${th.name} state ${th.state}:")
                for (f in frames) Log.w(TAG, "  at $f")
            }
            val tasks = File("/proc/self/task").listFiles() ?: return
            for (d in tasks) {
                val comm = File(d, "comm").readTextOrEmpty().trim()
                val stat = File(d, "stat").readTextOrEmpty()
                val state = stat.substringAfterLast(')', "").trim().firstOrNull() ?: '?'
                val wchan = File(d, "wchan").readTextOrEmpty().trim()
                Log.w(TAG, "task ${d.name} ($comm) state $state wchan '$wchan'")
            }
        } catch (e: Exception) {
            Log.w(TAG, "dump failed: $e")
        }
    }

    private fun File.readTextOrEmpty(): String = try {
        readText()
    } catch (e: Exception) {
        ""
    }
}
