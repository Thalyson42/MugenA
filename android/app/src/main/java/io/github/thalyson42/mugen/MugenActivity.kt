package io.github.thalyson42.mugen

import android.os.Bundle
import android.system.Os
import android.util.Log
import org.libsdl.app.SDLActivity

/**
 * P3b (D-002 (A), D-015): the activity SDL2 needs. SDLActivity loads the native libraries, creates the surface and runs
 * `SDL_main` of libmugen.so on its own thread. Order matters: libSDL2.so first (sdl12-compat finds it with dlopen),
 * then libSDL.so (sdl12-compat, the SDL 1.2 API of the engine), then libmugen.so, whose `SDL_main` is the entry point
 * (SDLActivity takes the LAST name of this list as the main shared object).
 */
class MugenActivity : SDLActivity() {
    override fun getLibraries(): Array<String> = arrayOf("SDL2", "SDL", "mugen")

    override fun onCreate(savedInstanceState: Bundle?) {
        // D-017: SDL2 publishes the accelerometer as a joystick by default and the engine opens every joystick (Pads_Init),
        // so tilting the phone counted as pad input. SDL reads this hint from the environment; it must be set before SDL starts.
        try {
            Os.setenv("SDL_ACCELEROMETER_AS_JOYSTICK", "0", true)
        } catch (e: Exception) {
            Log.w("mugen", "setenv SDL_ACCELEROMETER_AS_JOYSTICK failed: $e")
        }
        super.onCreate(savedInstanceState)
    }

    override fun onResume() {
        super.onResume()
        Native.activity = this      // the engine's error dialog (Native.showError) has to appear over this activity
    }
}
