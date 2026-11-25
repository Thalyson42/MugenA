package io.github.thalyson42.mugen

import android.app.Activity
import android.app.AlertDialog
import android.os.Looper
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

/** JNI entry points of libmugen.so (P1: library versions; P3a: platform layer). */
object Native {
    init {
        System.loadLibrary("mugen")
    }

    /** The activity in the foreground, if any; set by MainActivity. The error dialog needs one. */
    @Volatile
    @JvmField
    var activity: Activity? = null

    /** Versions of the statically linked libraries, one per line. */
    @JvmStatic
    external fun libraryVersions(): String

    /**
     * D-005: makes [dataDir] the data directory of the engine (it becomes the current directory, so "data/mugen.cfg"
     * and mugen.log are relative to it), sends the engine's stdout/stderr to logcat and installs the error dialog.
     */
    @JvmStatic
    external fun initPlatform(dataDir: String): Boolean

    /** Runs the P3a self-test of the platform layer; returns its report. Takes about 3 s: call it off the UI thread. */
    @JvmStatic
    external fun platformSelfTest(): String

    /** Raises the engine's error screen with a sample text. Call it off the UI thread: it waits for the dialog. */
    @JvmStatic
    external fun testError()

    /**
     * Called from native code (Error_ShowDialog -> Mugen_ShowError) on the engine's thread: shows [text] in a dialog
     * and returns once the user dismisses it, like the modal dialog of the Windows build.
     */
    @JvmStatic
    fun showError(title: String, text: String) {
        val act = activity ?: return
        if (Looper.myLooper() == Looper.getMainLooper()) return   // would deadlock; the text is in logcat already
        val dismissed = CountDownLatch(1)
        act.runOnUiThread {
            try {
                AlertDialog.Builder(act)
                    .setTitle(title)
                    .setMessage(text)
                    .setCancelable(false)
                    .setPositiveButton("OK") { _, _ -> dismissed.countDown() }
                    .show()
            } catch (e: Exception) {
                dismissed.countDown()          // activity finishing: do not block the engine forever
            }
        }
        dismissed.await(10, TimeUnit.MINUTES)
    }
}
