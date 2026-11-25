package io.github.thalyson42.mugen

import org.libsdl.app.SDLActivity

/**
 * P3b (D-002 (A), D-015): the activity SDL2 needs. SDLActivity loads the native libraries, creates the surface and runs
 * `SDL_main` of libmugen.so on its own thread. Order matters: libSDL2.so first (sdl12-compat finds it with dlopen),
 * then libSDL.so (sdl12-compat, the SDL 1.2 API of the engine), then libmugen.so, whose `SDL_main` is the entry point
 * (SDLActivity takes the LAST name of this list as the main shared object).
 */
class MugenActivity : SDLActivity() {
    override fun getLibraries(): Array<String> = arrayOf("SDL2", "SDL", "mugen")
}
