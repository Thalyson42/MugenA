# Android port (docs/android-port, phase P1): arm64-v8a only (D-001), NDK toolchain.
#
# Included from the top-level CMakeLists.txt when ANDROID is set; the Windows build never sees this file.
# P1 scope: the third-party libraries and an empty libmugen.so. The engine sources (MUGEN_SOURCES) are
# NOT compiled here yet: they need the 64-bit / Win32 portability work of P2 and the platform layer of P3.
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

# TODO(P3, D-002/D-003): SDL layer, SDL_mixer, SDL_ttf (+FreeType), SDL_gfx, libresample, libmpg123.

# The (still empty) engine library loaded by the Kotlin app. It links the libraries above so that the
# link of every P1 dependency is exercised; the stub only reports their versions.
add_library(mugen SHARED android/app/src/main/cpp/mugen_android_stub.c)
target_include_directories(mugen PRIVATE src)
target_compile_definitions(mugen PRIVATE MUGEN_PLATFORM_ANDROID=1)
# R2 (docs/android-port/01-analise-do-repo.md): no FMA contraction, never -ffast-math, to stay close to x87 results.
target_compile_options(mugen PRIVATE -ffp-contract=off -Wall)
target_link_libraries(mugen PRIVATE lua51 png12 minizip cgltf stb_image log android ${MUGEN_ZLIB})
target_link_options(mugen PRIVATE ${MUGEN_ANDROID_LINK_FLAGS})
