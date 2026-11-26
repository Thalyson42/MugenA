package io.github.thalyson42.mugen

import android.app.Activity
import android.app.ActivityManager
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.opengl.GLES20
import android.opengl.GLSurfaceView
import android.os.Build
import android.os.Bundle
import android.util.Log
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import java.io.File
import kotlin.concurrent.thread
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

/**
 * Diagnostic app. P1: reports the device facts that decision D-010 asks for (Android version, ABI, GLES, Vulkan
 * features, screen, RAM) and the versions of the libraries linked into libmugen.so. P3a: also sets the data directory
 * of the engine (D-005) and runs the self-test of the platform layer. P3b: a button starts MugenActivity (SDLActivity),
 * whose SDL_main writes sdl-probe.txt in the data directory; it is appended to the report on return. The report is shown, logged (tag "mugen-diag")
 * and can be copied to paste into docs/android-port/03-decisoes.md or LOG.md.
 */
class MainActivity : Activity() {
    private lateinit var report: TextView
    private var deviceInfo = ""
    private var glInfo = "GLES: (waiting for the GL context)"
    private var nativeInfo = ""
    private var platformInfo = "P3a self-test: running..."
    private var dataDir = ""
    private var sdlInfo = "P3b SDL 1.2 probe: not run yet (tap \"Run SDL 1.2 probe\")"

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        nativeInfo = try {
            Native.libraryVersions()
        } catch (e: UnsatisfiedLinkError) {
            "libmugen.so failed to load: ${e.message}"
        }
        deviceInfo = collectDeviceInfo()

        // D-005: the game's files live in the app's own folder. getExternalFilesDir is /sdcard/Android/data/<package>/files
        // (needs no permission); the internal folder is only a fallback when there is no external storage.
        dataDir = (getExternalFilesDir(null) ?: filesDir).absolutePath
        val platformReady = try {
            Native.initPlatform(dataDir)
        } catch (e: Throwable) {          // libmugen.so did not load (UnsatisfiedLinkError, ExceptionInInitializerError)
            false
        }
        if (platformReady) {
            thread(name = "p3a-selftest") {
                val result = try { Native.platformSelfTest(cacheDir.absolutePath) } catch (e: Throwable) { "P3a self-test crashed: $e" }
                runOnUiThread {
                    platformInfo = result
                    refresh()
                }
            }
        } else {
            platformInfo = "P3a: initPlatform failed for $dataDir"
        }

        // targetSdk 35 draws edge to edge: without this the buttons sit under the status bar (the P1 "Copy report" bug,
        // not yet confirmed on the device).
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            fitsSystemWindows = true
        }
        val errorButton = Button(this).apply {
            text = "Test error dialog"
            setOnClickListener { thread(name = "p3a-error") { Native.testError() } }
        }
        val sdlButton = Button(this).apply {
            text = "Run SDL 1.2 probe (P3b M1b, 15 s)"
            setOnClickListener {
                // no stale report: if the probe dies before writing, the report says so instead of showing an old run
                if (dataDir.isNotEmpty()) File(dataDir, "sdl-probe.txt").delete()
                sdlInfo = "P3b SDL probe: no report written (the probe crashed or did not start; see logcat -s mugen-sdl)"
                startActivity(Intent(this@MainActivity, MugenActivity::class.java))
            }
        }
        val copy = Button(this).apply {
            text = "Copy report"
            setOnClickListener {
                val cm = getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                cm.setPrimaryClip(ClipData.newPlainText("mugen-diag", fullReport()))
                Toast.makeText(this@MainActivity, "Copied", Toast.LENGTH_SHORT).show()
            }
        }
        report = TextView(this).apply {
            setTextIsSelectable(true)
            setPadding(24, 24, 24, 24)
            typeface = android.graphics.Typeface.MONOSPACE
        }
        val scroll = ScrollView(this).apply { addView(report) }

        // A tiny GLSurfaceView only to obtain a GLES context and read glGetString.
        val gl = GLSurfaceView(this).apply {
            setEGLContextClientVersion(3)
            setRenderer(object : GLSurfaceView.Renderer {
                override fun onSurfaceCreated(unused: GL10?, config: EGLConfig?) {
                    val size = IntArray(1)
                    GLES20.glGetIntegerv(GLES20.GL_MAX_TEXTURE_SIZE, size, 0)
                    val info = "GL_VERSION: ${GLES20.glGetString(GLES20.GL_VERSION)}\n" +
                        "GL_RENDERER: ${GLES20.glGetString(GLES20.GL_RENDERER)}\n" +
                        "GL_VENDOR: ${GLES20.glGetString(GLES20.GL_VENDOR)}\n" +
                        "GL_MAX_TEXTURE_SIZE: ${size[0]}\n" +
                        "GL_EXTENSIONS: ${GLES20.glGetString(GLES20.GL_EXTENSIONS)}"
                    runOnUiThread {
                        glInfo = info
                        refresh()
                    }
                }
                override fun onSurfaceChanged(unused: GL10?, w: Int, h: Int) {}
                override fun onDrawFrame(unused: GL10?) {}
            })
            renderMode = GLSurfaceView.RENDERMODE_WHEN_DIRTY
        }

        root.addView(copy)
        root.addView(errorButton)
        root.addView(sdlButton)
        root.addView(gl, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 4))
        root.addView(scroll, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f))
        setContentView(root)
        refresh()
    }

    private fun refresh() {
        val text = fullReport()
        report.text = text
        Log.i("mugen-diag", text)
    }

    private fun fullReport() =
        "$deviceInfo\n\n$glInfo\n\n--- libmugen.so ---\n$nativeInfo\n\n--- data directory (D-005) ---\n$dataDir\n" +
            "Copy the M.U.G.E.N 1.1b1 files here (data, chars, stages, sound, font...).\n\n$platformInfo\n\n--- P3b SDL probe ---\n$sdlInfo"

    override fun onResume() {
        super.onResume()
        Native.activity = this
        // The probe's SDL_main leaves its report in the data directory; show the latest one (if the activity ran).
        if (dataDir.isNotEmpty()) {
            val f = File(dataDir, "sdl-probe.txt")
            if (f.exists()) {
                sdlInfo = try { f.readText() } catch (e: Exception) { "sdl-probe.txt unreadable: $e" }
                refresh()
            }
        }
    }

    override fun onPause() {
        if (Native.activity === this) Native.activity = null
        super.onPause()
    }

    private fun getprop(name: String): String = try {
        val p = ProcessBuilder("getprop", name).redirectErrorStream(true).start()
        p.inputStream.bufferedReader().readText().trim()
    } catch (e: Exception) {
        "?"
    }

    @Suppress("DEPRECATION")
    private fun collectDeviceInfo(): String {
        val sb = StringBuilder()
        sb.appendLine("Device: ${Build.MANUFACTURER} ${Build.MODEL} (${Build.DEVICE})")
        if (Build.VERSION.SDK_INT >= 31) sb.appendLine("SoC: ${Build.SOC_MANUFACTURER} ${Build.SOC_MODEL}")
        sb.appendLine("Android: ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})")
        sb.appendLine("ro.product.cpu.abi: ${getprop("ro.product.cpu.abi")}")
        sb.appendLine("SUPPORTED_ABIS: ${Build.SUPPORTED_ABIS.joinToString()}")

        val am = getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
        val mem = ActivityManager.MemoryInfo().also { am.getMemoryInfo(it) }
        sb.appendLine("RAM: total ${mem.totalMem shr 20} MB, available ${mem.availMem shr 20} MB")
        sb.appendLine("reqGlEsVersion (system): 0x${Integer.toHexString(am.deviceConfigurationInfo.reqGlEsVersion)}")

        val vk = packageManager.systemAvailableFeatures
            .filter { it.name?.startsWith("android.hardware.vulkan") == true }
            .joinToString { "${it.name}=0x${Integer.toHexString(it.version)}" }
        sb.appendLine("Vulkan features: ${vk.ifEmpty { "none reported" }}")

        val dm = resources.displayMetrics
        val refresh = windowManager.defaultDisplay.refreshRate
        sb.append("Display: ${dm.widthPixels}x${dm.heightPixels} px, ${dm.densityDpi} dpi, $refresh Hz")
        return sb.toString()
    }
}
