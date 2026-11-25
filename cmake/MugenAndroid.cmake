# Android port (docs/android-port, phase P1): arm64-v8a only (D-001), NDK toolchain.
#
# Included from the top-level CMakeLists.txt when ANDROID is set; the Windows build never sees this file.
# P1 scope: the third-party libraries and an empty libmugen.so.
# P2 scope: the engine sources (MUGEN_SOURCES, defined by the top-level CMakeLists.txt) are COMPILED, as the
# object library mugen_core, to prove that they build for arm64. They are not linked into libmugen.so yet: that
# needs the platform layer of P3 (SDL, files, audio, renderers).
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

  add_library(mugen_core OBJECT ${MUGEN_SOURCES} src/platform/platform_posix.c ${_shader_headers})
  # Include order matters: the stubs (GL 1.1, libresample) come before everything else. The SDL 1.2 headers are
  # those of the Windows SDK and the GLEW header is only for model.c; both are P2 stand-ins, see D-013.
  target_include_directories(mugen_core PRIVATE
    src
    src/platform/stubs
    "${_tp}/glew-1.5.4/include"
    "${_tp}/sdk/include/SDL"
    "${_shader_dir}")
  target_compile_definitions(mugen_core PRIVATE MUGEN_PLATFORM_ANDROID=1)
  target_link_libraries(mugen_core PRIVATE lua51 png12 minizip cgltf stb_image ${MUGEN_ZLIB})
  set_target_properties(mugen_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
  # R2: no FMA contraction, never -ffast-math. The original is a signed-char, non-strict-aliasing MSVC binary:
  # char is unsigned on arm64 and clang optimises with strict aliasing, so both are set back (D-012).
  # The pointer <-> int conversions are errors: they were the 64-bit bugs of the 32-bit original.
  target_compile_options(mugen_core PRIVATE
    -ffp-contract=off -fsigned-char -fno-strict-aliasing
    -w -Werror=pointer-to-int-cast -Werror=int-to-pointer-cast -Werror=implicit-function-declaration
    -Werror=incompatible-pointer-types)
endif()

# The (still empty) engine library loaded by the Kotlin app. It links the libraries above so that the
# link of every P1 dependency is exercised; the stub only reports their versions.
add_library(mugen SHARED android/app/src/main/cpp/mugen_android_stub.c)
target_include_directories(mugen PRIVATE src)
target_compile_definitions(mugen PRIVATE MUGEN_PLATFORM_ANDROID=1)
# R2 (docs/android-port/01-analise-do-repo.md): no FMA contraction, never -ffast-math, to stay close to x87 results.
target_compile_options(mugen PRIVATE -ffp-contract=off -Wall)
target_link_libraries(mugen PRIVATE lua51 png12 minizip cgltf stb_image log android ${MUGEN_ZLIB})
target_link_options(mugen PRIVATE ${MUGEN_ANDROID_LINK_FLAGS})
if(MUGEN_ANDROID_BUILD_CORE)
  add_dependencies(mugen mugen_core)   # built with the library, but not linked into it until P3
endif()
