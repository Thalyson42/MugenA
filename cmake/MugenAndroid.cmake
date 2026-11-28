# Android port (docs/android-port, phase P1): arm64-v8a only (D-001), NDK toolchain.
#
# Included from the top-level CMakeLists.txt when ANDROID is set; the Windows build never sees this file.
# P1 scope: the third-party libraries and an empty libmugen.so.
# P2 scope: the engine sources (MUGEN_SOURCES, defined by the top-level CMakeLists.txt) are COMPILED, as the
# object library mugen_core, to prove that they build for arm64. They are not linked into libmugen.so yet: that
# needs SDL, SDL_mixer & co. (P3b, D-003) and the renderers.
# P3a scope: the platform layer without SDL (src/platform/*.c as the static library mugen_platform) is linked into
# libmugen.so, together with a small subset of the real engine files (mugen_p3a) so that the app can run them on the
# device: timer.c, log.c, file.c, textring.c, util.c. See D-014.
# GLEW, the import libraries generated from third_party/sdk/def/*.def and the opengl32/glu32/winmm/...
# system libraries are Windows-only and are not part of this path.

set(_tp "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

# Android's page size may be 16 KB on newer devices/Play requirements; NDK r27 does not align for it by default.
set(MUGEN_ANDROID_LINK_FLAGS "-Wl,-z,max-page-size=16384")

function(mugen_android_third_party target)
  target_compile_options(${target} PRIVATE -w)   # old code: silence warnings that are not ours to fix
  set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
endfunction()

# zlib: the original links zlib 1.2.3 from zlib1.dll. On Android the NDK's system libz (and its header) is used;
# the 1.2.3 header in third_party/sdk/include is deliberately NOT on the include path. See D-003.
find_library(MUGEN_ZLIB z REQUIRED)

# Lua 5.1.4
file(GLOB _lua_sources "${_tp}/lua-5.1.4/src/*.c")
add_library(lua51 STATIC ${_lua_sources})
target_include_directories(lua51 PUBLIC "${_tp}/lua-5.1.4/src")
target_compile_definitions(lua51 PRIVATE LUA_USE_POSIX)
# Lua 5.1 keeps a string's characters right after its TString header (svalue = (TString*)o + 1). Bionic's FORTIFY
# computes the object size from the header alone and aborts "strchr: prevented read past end of buffer" in
# lgc.c traversetable (found on the A065M, symbolized in CI). A false positive of the layout trick: no FORTIFY for Lua.
target_compile_options(lua51 PRIVATE -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0)
mugen_android_third_party(lua51)

# libpng 1.2.38 (against the system zlib). The MMX assembler of 1.2.x is x86-only; PUBLIC so that the
# engine's own png.h sees the same png_struct layout as the library.
file(GLOB _png_sources "${_tp}/libpng-1.2.38/png*.c")
list(FILTER _png_sources EXCLUDE REGEX "pngtest\\.c$")
add_library(png12 STATIC ${_png_sources})
target_include_directories(png12 PUBLIC "${_tp}/libpng-1.2.38")
target_compile_definitions(png12 PUBLIC PNG_NO_ASSEMBLER_CODE PNG_NO_MMX_CODE)
target_link_libraries(png12 PUBLIC ${MUGEN_ZLIB})
mugen_android_third_party(png12)

# minizip "unzip 0.15"
add_library(minizip STATIC "${_tp}/minizip-0.15/unzip.c")
target_include_directories(minizip PUBLIC "${_tp}/minizip-0.15")
target_link_libraries(minizip PUBLIC ${MUGEN_ZLIB})
mugen_android_third_party(minizip)

# cgltf 1.14 and stb_image 2.30 (mugen-unleashed additions)
add_library(cgltf STATIC "${_tp}/cgltf-1.14/cgltf.c")
target_include_directories(cgltf PUBLIC "${_tp}/cgltf-1.14")
mugen_android_third_party(cgltf)

add_library(stb_image STATIC "${_tp}/stb/stb_image.c")
target_include_directories(stb_image PUBLIC "${_tp}/stb")
mugen_android_third_party(stb_image)

# ---- P3b (M1a): SDL2 + sdl12-compat (D-002 (A), D-015) -----------------------------------------------------------
# libSDL2.so is the real SDL2 (it hosts the JNI side of the Java SDLActivity); libSDL.so is sdl12-compat, the SDL 1.2 API
# the engine is written against; the compat library finds libSDL2.so at run time with dlopen (patched name, D-015).
# SDL2 is built C-only: HIDAPI (the only C++ part, and the only user of the HIDDevice* natives) is off, and the matching
# line of SDLActivity.java is patched (third_party/SDL2/README-mugen.md). Controllers still arrive through Android's own
# joystick path.
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_HIDAPI OFF CACHE BOOL "" FORCE)
set(SDL_HIDAPI_JOYSTICK OFF CACHE BOOL "" FORCE)
set(SDL_VIRTUAL_JOYSTICK OFF CACHE BOOL "" FORCE)
set(SDL_CCACHE OFF CACHE BOOL "" FORCE)
# No internal sanity checks (D-018): with them on ("auto" is on for a non-NDEBUG build) SDL2 shows an "Assertion Failed" dialog
# for harmless conditions (add_capture_device: HasCaptureSupport, at audio init), and when one fires on the UI thread (Android's
# audio-device callback) the dialog waits for the very thread that is blocked: ANR, "not responding" (seen on the A065M).
set(SDL_ASSERTIONS "disabled" CACHE STRING "" FORCE)
add_subdirectory("${_tp}/SDL2" "${CMAKE_CURRENT_BINARY_DIR}/SDL2")
# The cache variable above was not enough on the device (the assertion still fired in the e52a2b9 APK): force the level on
# the SDL2 target itself (SDL_assert.h honours SDL_ASSERT_LEVEL first).
target_compile_definitions(SDL2 PRIVATE SDL_ASSERT_LEVEL=0)
target_link_options(SDL2 PRIVATE ${MUGEN_ANDROID_LINK_FLAGS})

# sdl12-compat: one C file. Its own CMakeLists.txt (vendored for reference) is not used: it wants find_package(SDL2) and
# installs; this is what it does for the library on Linux-like systems. The SDL 1.2 headers are in include/SDL.
add_library(SDL12 SHARED "${_tp}/sdl12-compat/src/SDL12_compat.c")
set_target_properties(SDL12 PROPERTIES OUTPUT_NAME SDL)            # libSDL.so
# SDL12_compat.c includes the SDL 2 headers by name (SDL.h): they must come first for this one target.
target_include_directories(SDL12 PRIVATE "${_tp}/SDL2/include")
target_compile_definitions(SDL12 PRIVATE SDL_ASSERT_LEVEL=0)   # same reason as SDL_ASSERTIONS above (D-018)
target_include_directories(SDL12 INTERFACE "${_tp}/sdl12-compat/include/SDL")   # what the engine includes (M1c)
target_compile_definitions(SDL12 PRIVATE _REENTRANT SDL_DISABLE_IMMINTRIN_H)
target_compile_options(SDL12 PRIVATE -w)
target_link_libraries(SDL12 PRIVATE ${CMAKE_DL_LIBS})
target_link_options(SDL12 PRIVATE ${MUGEN_ANDROID_LINK_FLAGS})
set_target_properties(SDL12 PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_dependencies(SDL12 SDL2)   # libSDL2.so has to be in the APK next to it

# ---- P4a (D-003, D-019): SDL_mixer 1.2 and libresample, vendored by tools/vendor-p4a.sh ----------------------------------
# SDL_mixer is the SDL 1.2 library (the engine passes it SDL 1.2 SDL_RWops/Mix_Chunk), so it is built here against the
# sdl12-compat headers and links nothing but libSDL.so's symbols, resolved at run time like the engine's own. Only WAV/AIFF/VOC
# and the music front end are compiled in: no MP3/Ogg/MIDI/MOD back end (the engine decodes MP3 itself with libmpg123, P4b).
# Until tools/vendor-p4a.sh has been run (third_party/SDL_mixer and third_party/libresample absent) the build falls back to the
# temporary stubs of mugen_d003_stubs.c, so the tree keeps building.
set(MUGEN_AUDIO_REAL OFF)
if(EXISTS "${_tp}/SDL_mixer/mixer.c" AND EXISTS "${_tp}/libresample/src/resample.c")
  set(MUGEN_AUDIO_REAL ON)
  add_library(SDL_mixer STATIC
    "${_tp}/SDL_mixer/mixer.c" "${_tp}/SDL_mixer/music.c" "${_tp}/SDL_mixer/effects_internal.c"
    "${_tp}/SDL_mixer/effect_position.c" "${_tp}/SDL_mixer/effect_stereoreverse.c"
    "${_tp}/SDL_mixer/load_aiff.c" "${_tp}/SDL_mixer/load_voc.c")
  target_include_directories(SDL_mixer PUBLIC "${_tp}/SDL_mixer")
  target_include_directories(SDL_mixer PRIVATE "${_tp}/sdl12-compat/include/SDL")
  target_compile_definitions(SDL_mixer PRIVATE _REENTRANT)
  mugen_android_third_party(SDL_mixer)

  # libresample 0.1.5 (mono, upstream). Its symbols are renamed up_resample_*: the engine-facing API of Elecbyte's
  # libresample (mugen_resample.c, multi-channel, resample_init/reset) uses the original names.
  add_library(libresample_up STATIC
    "${_tp}/libresample/src/filterkit.c" "${_tp}/libresample/src/resample.c" "${_tp}/libresample/src/resamplesubs.c")
  target_include_directories(libresample_up PRIVATE "${_tp}/libresample")
  target_compile_definitions(libresample_up PRIVATE
    resample_open=up_resample_open resample_process=up_resample_process resample_close=up_resample_close
    resample_dup=up_resample_dup resample_get_filter_width=up_resample_get_filter_width)
  mugen_android_third_party(libresample_up)
else()
  message(WARNING "P4a: third_party/SDL_mixer or third_party/libresample missing: run tools/vendor-p4a.sh; "
                  "building with the temporary audio stubs (no sound).")
endif()
# TODO(P4b, D-003): libmpg123 (BGM). TODO(P4c, D-003): SDL_ttf (+FreeType), SDL_gfx zoomSurface.

# Compile flags of every engine source (mugen_core, mugen_p3a, mugen_platform, libmugen.so). R2: no FMA contraction,
# never -ffast-math. The original is a signed-char, non-strict-aliasing MSVC binary: char is unsigned on arm64 and clang
# optimises with strict aliasing, so both are set back (D-012). -fsigned-char must be on EVERY target whose code
# touches engine data, libmugen.so included (P3a, D-014). The pointer <-> int conversions are errors: they were the
# 64-bit bugs of the 32-bit original.
set(MUGEN_ANDROID_ENGINE_FLAGS
  -ffp-contract=off -fsigned-char -fno-strict-aliasing
  -Werror=pointer-to-int-cast -Werror=int-to-pointer-cast -Werror=implicit-function-declaration
  -Werror=incompatible-pointer-types)

# Include order matters: the stubs (GL 1.1, libresample) come before everything else. The SDL 1.2 headers are those of
# sdl12-compat (M1c; the Windows SDK ones and sdl_config_posix.h of D-013 are gone from this path); SDL_mixer.h and SDL_ttf.h
# are copies in src/platform/stubs/SDL_ext until D-003 (M2). The GLEW header is only for model.c, a P2 stand-in (D-013).
set(MUGEN_ANDROID_ENGINE_INCLUDES
  src
  src/platform/stubs
  src/platform/stubs/SDL_ext
  "${_tp}/glew-1.5.4/include"
  "${_tp}/sdl12-compat/include/SDL")
if(MUGEN_AUDIO_REAL)
  # the real SDL_mixer.h (with Mix_QuerySpecEx) shadows the stub copy in SDL_ext (D-019)
  list(INSERT MUGEN_ANDROID_ENGINE_INCLUDES 0 "${_tp}/SDL_mixer")
  # the engine-facing libresample API on top of the upstream mono library (P4a, D-019)
  add_library(mugen_resample STATIC android/app/src/main/cpp/mugen_resample.c)
  target_include_directories(mugen_resample PRIVATE ${MUGEN_ANDROID_ENGINE_INCLUDES})
  target_compile_definitions(mugen_resample PRIVATE MUGEN_PLATFORM_ANDROID=1)
  target_compile_options(mugen_resample PRIVATE ${MUGEN_ANDROID_ENGINE_FLAGS} -Wall)
  target_link_libraries(mugen_resample PUBLIC libresample_up)
  set_target_properties(mugen_resample PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

# ---- P3a: the platform layer (no SDL) --------------------------------------------------------------------------
add_library(mugen_platform STATIC
  src/platform/platform_posix.c
  src/platform/platform_path.c
  src/platform/platform_app.c)
target_include_directories(mugen_platform PRIVATE ${MUGEN_ANDROID_ENGINE_INCLUDES})
target_compile_definitions(mugen_platform PRIVATE MUGEN_PLATFORM_ANDROID=1)
target_compile_options(mugen_platform PRIVATE ${MUGEN_ANDROID_ENGINE_FLAGS} -Wall)
target_link_libraries(mugen_platform PRIVATE log)
set_target_properties(mugen_platform PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---- the engine sources (P2: compiled for arm64; P3b M2: linked into libmugen.so) -----------------------------
if(TRUE)
  # The GLSL programs of the 3D models, embedded as C strings (the same rule as the Windows build).
  set(_shader_dir "${CMAKE_CURRENT_BINARY_DIR}/shaders")
  set(_shader_headers)
  foreach(_shader model.vert model.frag shadow.vert shadow.frag env.vert env_panorama.frag env_filter.frag)
    string(REPLACE "." "_" _shader_var "g_shader_${_shader}")
    set(_shader_out "${_shader_dir}/${_shader}.h")
    add_custom_command(
      OUTPUT "${_shader_out}"
      COMMAND "${CMAKE_COMMAND}" -DIN=${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/${_shader} -DOUT=${_shader_out}
              -DNAME=${_shader_var} -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedShader.cmake"
      DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/${_shader}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedShader.cmake"
      COMMENT "Embedding shader ${_shader}"
      VERBATIM)
    list(APPEND _shader_headers "${_shader_out}")
  endforeach()

  add_library(mugen_core OBJECT ${MUGEN_SOURCES} ${_shader_headers})
  target_include_directories(mugen_core PRIVATE ${MUGEN_ANDROID_ENGINE_INCLUDES} "${_shader_dir}")
  target_compile_definitions(mugen_core PRIVATE MUGEN_PLATFORM_ANDROID=1)
  target_link_libraries(mugen_core PRIVATE lua51 png12 minizip cgltf stb_image ${MUGEN_ZLIB})
  set_target_properties(mugen_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
  target_compile_options(mugen_core PRIVATE ${MUGEN_ANDROID_ENGINE_FLAGS} -w)
endif()

# The engine library loaded by the Kotlin app. P3b M2: it links the whole engine (mugen_core), the platform layer and the JNI
# entry points; the exported SDL_main is the engine's (src/sdlmain.c), which runs the SDL 1.2 mode test of M1b
# (mugen_sdl_probe.c -> mugen_sdl12_probe.c) instead when the app asks for it. The mode test is an object library of its own
# with ONLY the sdl12-compat headers on its include path (the SDL 1.2 and SDL 2 headers share file names). Until the real
# libraries arrive, mugen_d003_stubs.c (SDL_mixer, SDL_ttf, SDL_gfx zoomSurface, libresample; D-003) and mugen_gl_stubs.c (desktop
# GL 1.1 and GLEW; D-013, P6/P7) let the engine link and start, without sound, TrueType fonts or an OpenGL renderer.
add_library(mugen_sdl12_probe OBJECT android/app/src/main/cpp/mugen_sdl12_probe.c)
target_include_directories(mugen_sdl12_probe PRIVATE "${_tp}/sdl12-compat/include/SDL" android/app/src/main/cpp)
target_compile_options(mugen_sdl12_probe PRIVATE -ffp-contract=off -fsigned-char -Wall -Wextra -Wno-unused-parameter)
set_target_properties(mugen_sdl12_probe PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(mugen SHARED
  android/app/src/main/cpp/mugen_android_stub.c
  android/app/src/main/cpp/mugen_watchdog.c
  android/app/src/main/cpp/mugen_selftest.c
  android/app/src/main/cpp/mugen_sdl_probe.c
  android/app/src/main/cpp/mugen_d003_stubs.c
  android/app/src/main/cpp/mugen_gl_stubs.c
  android/app/src/main/cpp/mugen_spritediag.c
  android/app/src/main/cpp/mugen_perf.c
  $<TARGET_OBJECTS:mugen_core>
  $<TARGET_OBJECTS:mugen_sdl12_probe>)
target_include_directories(mugen PRIVATE ${MUGEN_ANDROID_ENGINE_INCLUDES})
target_compile_definitions(mugen PRIVATE MUGEN_PLATFORM_ANDROID=1)
# R2 (docs/android-port/01-analise-do-repo.md) and D-012/D-014: the engine flags, -fsigned-char included, on libmugen.so too.
target_compile_options(mugen PRIVATE ${MUGEN_ANDROID_ENGINE_FLAGS} -Wall)
# SDL: libmugen.so links libSDL.so (sdl12-compat, the SDL 1.2 API) and NOT libSDL2.so. Both export the same SDL_* names with
# different ABIs, and a symbol is bound in the order of the NEEDED entries: with libSDL2.so in the list, the engine's SDL_Init,
# SDL_PollEvent, SDL_MapRGB... would bind to SDL 2 (D-015, item 5 amended). libSDL2.so is still built and packaged (add_dependencies),
# loaded first by SDLActivity, and found by the compat library with dlopen. LINK_ONLY: no SDL headers leak into libmugen's files.
target_link_libraries(mugen PRIVATE mugen_platform lua51 png12 minizip cgltf stb_image
  $<LINK_ONLY:SDL12> log android ${MUGEN_ZLIB})
if(MUGEN_AUDIO_REAL)
  # P4a: the real SDL_mixer and libresample; mugen_d003_stubs.c then leaves their functions out (D-019)
  target_link_libraries(mugen PRIVATE SDL_mixer mugen_resample)
  target_compile_definitions(mugen PRIVATE MUGEN_AUDIO_REAL=1)
endif()
add_dependencies(mugen SDL2 SDL12)
target_link_options(mugen PRIVATE ${MUGEN_ANDROID_LINK_FLAGS})
# REMOVER-NO-RELEASE (docs/android-port/05-diagnosticos-a-remover.md) item 1.
# Diagnostic (P3b M3, D-017): mugen_spritediag.c logs tiny sprites drawn by the System renderer; delete with the dashes' cause found.
foreach(_w Sprite_DrawSoft8 Sprite_DrawSoft Gfx_SoftClear Gfx_SoftDrawRect Gfx_SoftDrawRectAdd Gfx_SoftDrawRectSub
        Gfx_SoftDrawRectBlend Gfx_SoftBlendRect Gfx_SoftFillRect Gfx_SoftFillRectAdd Gfx_SoftFillRectSub
        Gfx_SoftFillRectHalfAdd Gfx_SoftHLine Gfx_SoftVLine Gfx_SoftHLineAdd Gfx_SoftHLineHalfAdd Gfx_SoftHLineSub
        Gfx_SoftVLineAdd Gfx_SoftVLineHalfAdd Gfx_SoftVLineSub Gfx_SoftPutPixel Gfx_SoftAddPixel Gfx_SoftInvertPixel
        Gfx_SoftDrawLine Gfx_SoftDrawLineAdd BgElem_DrawParallaxSoft BgElem_DrawParallaxSoftMasked SDL_UpperBlit
        SDL_FillRect SDL_UpdateRect SDL_Flip)
    target_link_options(mugen PRIVATE "-Wl,--wrap=${_w}")
endforeach()
