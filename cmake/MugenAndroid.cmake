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
add_subdirectory("${_tp}/SDL2" "${CMAKE_CURRENT_BINARY_DIR}/SDL2")
target_link_options(SDL2 PRIVATE ${MUGEN_ANDROID_LINK_FLAGS})

# sdl12-compat: one C file. Its own CMakeLists.txt (vendored for reference) is not used: it wants find_package(SDL2) and
# installs; this is what it does for the library on Linux-like systems. The SDL 1.2 headers are in include/SDL.
add_library(SDL12 SHARED "${_tp}/sdl12-compat/src/SDL12_compat.c")
set_target_properties(SDL12 PROPERTIES OUTPUT_NAME SDL)            # libSDL.so
# SDL12_compat.c includes the SDL 2 headers by name (SDL.h): they must come first for this one target.
target_include_directories(SDL12 PRIVATE "${_tp}/SDL2/include")
target_include_directories(SDL12 INTERFACE "${_tp}/sdl12-compat/include/SDL")   # what the engine includes (M1c)
target_compile_definitions(SDL12 PRIVATE _REENTRANT SDL_DISABLE_IMMINTRIN_H)
target_compile_options(SDL12 PRIVATE -w)
target_link_libraries(SDL12 PRIVATE ${CMAKE_DL_LIBS})
target_link_options(SDL12 PRIVATE ${MUGEN_ANDROID_LINK_FLAGS})
set_target_properties(SDL12 PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_dependencies(SDL12 SDL2)   # libSDL2.so has to be in the APK next to it

# TODO(P3b M2, D-003): SDL_mixer, SDL_ttf (+FreeType), SDL_gfx, libresample, libmpg123.

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

# The real engine files that need nothing but the platform layer, plus the globals and callbacks they expect from
# the rest of the engine (TEMPORARY: mugen_p3a_support.c goes away in P3b, when mugen_core is linked instead).
add_library(mugen_p3a STATIC
  src/timer.c src/log.c src/file.c src/textring.c src/util.c
  android/app/src/main/cpp/mugen_p3a_support.c)
target_include_directories(mugen_p3a PRIVATE ${MUGEN_ANDROID_ENGINE_INCLUDES})
target_compile_definitions(mugen_p3a PRIVATE MUGEN_PLATFORM_ANDROID=1)
target_compile_options(mugen_p3a PRIVATE ${MUGEN_ANDROID_ENGINE_FLAGS} -w)
target_link_libraries(mugen_p3a PUBLIC mugen_platform lua51 png12 minizip cgltf stb_image ${MUGEN_ZLIB})
set_target_properties(mugen_p3a PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---- P2: the engine sources, compiled for arm64 (not linked yet) ---------------------------------------------
option(MUGEN_ANDROID_BUILD_CORE "Compile the engine sources (mugen_core) as part of the Android build" ON)
if(MUGEN_ANDROID_BUILD_CORE)
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

# The engine library loaded by the Kotlin app. P3a: it links the platform layer and the engine subset above, and exposes
# them through JNI (initialisation, self-test, error dialog); the rest of the engine joins in P3b.
# P3b M1b: SDL_main (mugen_sdl_probe.c, no SDL header) runs the SDL 1.2 mode test (mugen_sdl12_probe.c). That test is an
# object library of its own with ONLY the sdl12-compat headers on its include path: the SDL 1.2 and SDL 2 headers share
# file names, so a file sees one family or the other, never both. M1c replaces both with the engine's own SDL_main.
add_library(mugen_sdl12_probe OBJECT android/app/src/main/cpp/mugen_sdl12_probe.c)
target_include_directories(mugen_sdl12_probe PRIVATE "${_tp}/sdl12-compat/include/SDL" android/app/src/main/cpp)
target_compile_options(mugen_sdl12_probe PRIVATE -ffp-contract=off -fsigned-char -Wall -Wextra -Wno-unused-parameter)
set_target_properties(mugen_sdl12_probe PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(mugen SHARED
  android/app/src/main/cpp/mugen_android_stub.c
  android/app/src/main/cpp/mugen_selftest.c
  android/app/src/main/cpp/mugen_sdl_probe.c
  $<TARGET_OBJECTS:mugen_sdl12_probe>)
target_include_directories(mugen PRIVATE ${MUGEN_ANDROID_ENGINE_INCLUDES})
target_compile_definitions(mugen PRIVATE MUGEN_PLATFORM_ANDROID=1)
# R2 (docs/android-port/01-analise-do-repo.md) and D-012/D-014: the engine flags, -fsigned-char included, on libmugen.so too.
target_compile_options(mugen PRIVATE ${MUGEN_ANDROID_ENGINE_FLAGS} -Wall)
# SDL: libmugen.so links libSDL.so (sdl12-compat, the SDL 1.2 API) and NOT libSDL2.so. Both export the same SDL_* names with
# different ABIs, and a symbol is bound in the order of the NEEDED entries: with libSDL2.so in the list, the engine's SDL_Init,
# SDL_PollEvent, SDL_MapRGB... would bind to SDL 2 (D-015, item 5 amended). libSDL2.so is still built and packaged (add_dependencies),
# loaded first by SDLActivity, and found by the compat library with dlopen. LINK_ONLY: no SDL headers leak into libmugen's files.
target_link_libraries(mugen PRIVATE mugen_p3a mugen_platform lua51 png12 minizip cgltf stb_image
  $<LINK_ONLY:SDL12> log android ${MUGEN_ZLIB})
add_dependencies(mugen SDL2 SDL12)
target_link_options(mugen PRIVATE ${MUGEN_ANDROID_LINK_FLAGS})
if(MUGEN_ANDROID_BUILD_CORE)
  add_dependencies(mugen mugen_core)   # built with the library, but not linked into it until P3b
endif()
