# Host tests of the Android platform layer (phase P3a). Not part of the Windows or the Android build:
#   cmake -S tests/platform -B build/platform-tests && cmake --build build/platform-tests && ctest --test-dir build/platform-tests --output-on-failure
# They compile the real engine files timer.c, log.c, file.c (+ textring.c, util.c) and src/platform/*.c for the host
# (Linux), with MUGEN_PLATFORM_ANDROID so that the POSIX side of the platform layer is the one under test.
cmake_minimum_required(VERSION 3.16)
project(mugen_platform_tests C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
add_executable(platform_tests
  test_main.c
  "${ROOT}/src/platform/platform_posix.c"
  "${ROOT}/src/platform/platform_path.c"
  "${ROOT}/src/platform/platform_app.c"
  "${ROOT}/src/timer.c"
  "${ROOT}/src/log.c"
  "${ROOT}/src/file.c"
  "${ROOT}/src/textring.c"
  "${ROOT}/src/util.c")
target_include_directories(platform_tests PRIVATE
  "${ROOT}/src" "${ROOT}/src/platform/stubs" "${ROOT}/third_party/glew-1.5.4/include" "${ROOT}/third_party/sdk/include/SDL"
  "${ROOT}/third_party/minizip-0.15" "${ROOT}/third_party/lua-5.1.4/src" "${ROOT}/third_party/libpng-1.2.38"
  "${ROOT}/third_party/cgltf-1.14" "${ROOT}/third_party/stb")
target_compile_definitions(platform_tests PRIVATE MUGEN_PLATFORM_ANDROID=1)
# The same flags as mugen_core (D-012): the engine is a signed-char, non-strict-aliasing, no-FMA program.
target_compile_options(platform_tests PRIVATE -ffp-contract=off -fsigned-char -fno-strict-aliasing -w)
target_link_libraries(platform_tests PRIVATE pthread m)

enable_testing()
add_test(NAME platform_tests COMMAND platform_tests)
