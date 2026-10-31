cmake_minimum_required(VERSION 3.21)
project(crosspoint-emulator CXX C)

set(CMAKE_CXX_STANDARD 20)

if(NOT DEFINED CROSSPOINT_ROOT)
  set(CROSSPOINT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../Crosspoint")
endif()
# if(EXISTS ...) is only well-defined for absolute paths; a relative
# CROSSPOINT_ROOT (e.g. -DCROSSPOINT_ROOT=./crosspoint-reader) resolves
# against the process's working directory, not CMAKE_CURRENT_SOURCE_DIR,
# silently breaking every EXISTS check below when configured from build/.
get_filename_component(CROSSPOINT_ROOT "${CROSSPOINT_ROOT}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

if(NOT DEFINED FREEINK_SDK_ROOT)
  set(FREEINK_SDK_ROOT "${CROSSPOINT_ROOT}/freeink-sdk")
endif()
get_filename_component(FREEINK_SDK_ROOT "${FREEINK_SDK_ROOT}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

if(DEFINED SDL2_ROOT)
  set(CMAKE_PREFIX_PATH "${SDL2_ROOT};${CMAKE_PREFIX_PATH}")
endif()

set(SDL2_USE_PKGCONFIG OFF)
find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(SDL2 sdl2)
    if(SDL2_FOUND)
      set(SDL2_USE_PKGCONFIG ON)
    endif()
  endif()
endif()
if(NOT SDL2_FOUND)
  message(FATAL_ERROR "SDL2 not found. Need SDL2 (not SDL3).")
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Report the firmware version the emulator was built from, as platformio.ini defines it
file(STRINGS "${CROSSPOINT_ROOT}/platformio.ini" CROSSPOINT_VERSION_LINE REGEX "^version *= *" LIMIT_COUNT 1)
string(REGEX REPLACE "^version *= *" "" CROSSPOINT_FIRMWARE_VERSION "${CROSSPOINT_VERSION_LINE}")
if(NOT CROSSPOINT_FIRMWARE_VERSION)
  set(CROSSPOINT_FIRMWARE_VERSION "unknown")
endif()

if(EXISTS "${CROSSPOINT_ROOT}/scripts/build_html.py")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "PYTHONPATH=" "${Python3_EXECUTABLE}" "${CROSSPOINT_ROOT}/scripts/build_html.py"
    WORKING_DIRECTORY "${CROSSPOINT_ROOT}"
    OUTPUT_QUIET
  )
endif()

# Generate the I18n string tables (PlatformIO runs this as a pre-build script)
if(EXISTS "${CROSSPOINT_ROOT}/scripts/gen_i18n.py")
  execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${CROSSPOINT_ROOT}/scripts/gen_i18n.py"
      "${CROSSPOINT_ROOT}/lib/I18n/translations" "${CROSSPOINT_ROOT}/lib/I18n/"
    WORKING_DIRECTORY "${CROSSPOINT_ROOT}"
    OUTPUT_QUIET
  )
endif()

set(SIM_SOURCES
  sim/src/main_sim.cpp
  sim/src/sim_display.cpp
  sim/src/sim_gpio.cpp
  sim/src/sim_storage.cpp
  sim/src/sim_spi_bus.cpp
  sim/src/arduino_stub.cpp
  sim/src/esp_stub.cpp
  sim/src/battery_stub.cpp
  sim/src/wifi_stub.cpp
  sim/src/freertos_stub.cpp
  sim/src/mdns_stub.cpp
  sim/src/sim_hal.cpp
  sim/src/base64_stub.cpp
  sim/src/net_stubs.cpp
  sim/src/crosspoint_web_server_stub.cpp
  sim/src/ota_updater_stub.cpp
)

file(GLOB_RECURSE CROSSPOINT_SRC "${CROSSPOINT_ROOT}/src/*.cpp")
# The web server (HTTP/WebDAV) and OTA flashing are built on ESP32-only libraries;
# sim/src stubs them as a server that never starts and an update that never exists.
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX "/network/(CrossPointWebServer|WebDAVHandler|OtaUpdater)\.cpp$")

# Every firmware library, minus the hardware HAL (the sim provides it) and sources
# that are #included by others or only make sense on the device.
file(GLOB_RECURSE CROSSPOINT_LIB_CPP
  "${CROSSPOINT_ROOT}/lib/*.cpp"
  "${CROSSPOINT_ROOT}/lib/*.c"
)
list(FILTER CROSSPOINT_LIB_CPP EXCLUDE REGEX "/lib/[^/]+/test/")
list(FILTER CROSSPOINT_LIB_CPP EXCLUDE REGEX "/lib/hal/")
list(FILTER CROSSPOINT_LIB_CPP EXCLUDE REGEX "/lib/miniz/third_party/")  # built via miniz/src/miniz_impl.c
list(FILTER CROSSPOINT_LIB_CPP EXCLUDE REGEX "/lib/expat/xmltok_(impl|ns)\.c$")  # included by xmltok.c
list(FILTER CROSSPOINT_LIB_CPP EXCLUDE REGEX "/lib/Epub/BookKey\.cpp$")  # DRM key wrapping needs wolfSSL; see sim/src/net_stubs.cpp

# HAL sources that run unchanged on top of the sim's stand-ins (SDCardManager/FsFile
# emulation, host clock, absent frontlight/IMU). sim/src provides the rest of the HAL.
foreach(HAL_SRC HalStorage HalClock HalMemory HalFrontlight HalTiltSensor)
  list(APPEND CROSSPOINT_LIB_CPP "${CROSSPOINT_ROOT}/lib/hal/${HAL_SRC}.cpp")
endforeach()

# FreeInk SDK libraries from platformio.ini's lib_deps that are plain logic (no radio or
# peripheral drivers). ContentProtection binds to the firmware's miniz
# (CONTENT_EXTERNAL_MINIZ) and its wolfSSL backend is replaced by sim/include/WolfsslCrypto.h.
set(FREEINK_LIBS ui/FreeInkUI network/JsonSax network/CatalogList network/Opds book/ContentProtection
    hardware/MemoryManager font/FreeInkFont)
foreach(FREEINK_LIB ${FREEINK_LIBS})
  file(GLOB_RECURSE FREEINK_LIB_SOURCES
    "${FREEINK_SDK_ROOT}/libs/${FREEINK_LIB}/src/*.cpp"
    "${FREEINK_SDK_ROOT}/libs/${FREEINK_LIB}/src/*.c"
  )
  list(APPEND CROSSPOINT_LIB_CPP ${FREEINK_LIB_SOURCES})
endforeach()
list(FILTER CROSSPOINT_LIB_CPP EXCLUDE REGEX "/ContentProtection/src/(vendor/|WolfsslCrypto\.cpp$)")
set(FREEINK_EXTRA_INCLUDE_DIRS
  "${FREEINK_SDK_ROOT}/libs/font/FreeInkFont/third_party/stb"
  "${FREEINK_SDK_ROOT}/libs/font/FreeInkFont/third_party/freetype/include"
)

# ArduinoJson (header-only). Reuse the PlatformIO copy if present, otherwise fetch the pinned version.
set(ARDUINO_JSON_ROOT "${CROSSPOINT_ROOT}/.pio/libdeps/default/ArduinoJson")
if(NOT EXISTS "${ARDUINO_JSON_ROOT}/src/ArduinoJson.h")
  include(FetchContent)
  FetchContent_Declare(ArduinoJson
    GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
    GIT_TAG v7.4.2
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(ArduinoJson)
  set(ARDUINO_JSON_ROOT "${arduinojson_SOURCE_DIR}")
endif()

# Image decoders, pinned and patched as in Crosspoint's platformio.ini. SOURCE_SUBDIR
# points nowhere so FetchContent only downloads them; their sources are added below.
include(FetchContent)
FetchContent_Declare(PNGdec
  GIT_REPOSITORY https://github.com/bitbank2/PNGdec.git
  GIT_TAG 1.1.6
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR _none
)
FetchContent_Declare(JPEGDEC
  GIT_REPOSITORY https://github.com/bitbank2/JPEGDEC.git
  GIT_TAG 86282979224c8a32fd51e091ed5a35b0c699a52b
  PATCH_COMMAND "${CMAKE_COMMAND}" -DPATCH_DIR=${CROSSPOINT_ROOT}/scripts/jpegdec_patches
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/apply_patches.cmake"
  SOURCE_SUBDIR _none
)
FetchContent_Declare(AnimatedGIF
  GIT_REPOSITORY https://github.com/bitbank2/AnimatedGIF.git
  GIT_TAG 67cfacafe12ef4926c7713342c57d2a4af040e5e
  PATCH_COMMAND "${Python3_EXECUTABLE}" "${CROSSPOINT_ROOT}/scripts/patch_animatedgif.py" <SOURCE_DIR>
  SOURCE_SUBDIR _none
)
FetchContent_MakeAvailable(PNGdec JPEGDEC AnimatedGIF)
file(GLOB IMAGE_LIB_SOURCES
  "${pngdec_SOURCE_DIR}/src/*.c" "${pngdec_SOURCE_DIR}/src/*.cpp"
  "${jpegdec_SOURCE_DIR}/src/*.c" "${jpegdec_SOURCE_DIR}/src/*.cpp"
  "${animatedgif_SOURCE_DIR}/src/*.c" "${animatedgif_SOURCE_DIR}/src/*.cpp"
)
list(APPEND CROSSPOINT_LIB_CPP ${IMAGE_LIB_SOURCES})
set(IMAGE_LIB_INCLUDE_DIRS "${pngdec_SOURCE_DIR}/src" "${jpegdec_SOURCE_DIR}/src" "${animatedgif_SOURCE_DIR}/src")

add_executable(crosspoint_emulator
  ${SIM_SOURCES}
  ${CROSSPOINT_SRC}
  ${CROSSPOINT_LIB_CPP}
)

file(GLOB LIB_DIRS LIST_DIRECTORIES true "${CROSSPOINT_ROOT}/lib/*")
file(GLOB FREEINK_INCLUDE_DIRS "${FREEINK_SDK_ROOT}/libs/*/*/include")
file(GLOB LIB_PUBLIC_INCLUDE_DIRS "${CROSSPOINT_ROOT}/lib/*/include")
set(LIB_INCLUDES "${CROSSPOINT_ROOT}/src" "${CROSSPOINT_ROOT}/lib")
foreach(LIB_DIR ${LIB_DIRS})
  if(NOT IS_DIRECTORY "${LIB_DIR}")
    continue()
  endif()
  # lib/Memory/memory.h would shadow the C library's <memory.h> on case-insensitive
  # filesystems (Windows, default macOS), so only C++ sources see that directory.
  if(LIB_DIR MATCHES "/Memory/?$")
    list(APPEND LIB_INCLUDES "$<$<COMPILE_LANGUAGE:CXX>:${LIB_DIR}>")
  else()
    list(APPEND LIB_INCLUDES "${LIB_DIR}")
  endif()
endforeach()

target_include_directories(crosspoint_emulator PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/sim/include
  ${LIB_INCLUDES}
  ${CROSSPOINT_ROOT}/lib/Epub/Epub
  ${CROSSPOINT_ROOT}/lib/Epub/Epub/parsers
  ${CROSSPOINT_ROOT}/lib/Epub/Epub/hyphenation
  ${CROSSPOINT_ROOT}/lib/uzlib/src
  ${CROSSPOINT_ROOT}/lib/miniz/src
  ${LIB_PUBLIC_INCLUDE_DIRS}
  ${IMAGE_LIB_INCLUDE_DIRS}
  ${FREEINK_INCLUDE_DIRS}
  ${FREEINK_EXTRA_INCLUDE_DIRS}
)

if(EXISTS "${ARDUINO_JSON_ROOT}/src")
  target_include_directories(crosspoint_emulator PRIVATE ${ARDUINO_JSON_ROOT}/src)
endif()

if(SDL2_USE_PKGCONFIG)
  target_include_directories(crosspoint_emulator PRIVATE ${SDL2_INCLUDE_DIRS})
  target_link_libraries(crosspoint_emulator PRIVATE ${SDL2_LIBRARIES})
  target_link_directories(crosspoint_emulator PRIVATE ${SDL2_LIBRARY_DIRS})
else()
  target_link_libraries(crosspoint_emulator PRIVATE SDL2::SDL2)
endif()

# MinGW: link its C++ runtime statically so the executable only needs SDL2.dll
if(MINGW)
  target_link_options(crosspoint_emulator PRIVATE -static-libgcc -static-libstdc++
    -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive,-Bdynamic)
endif()

# Windows finds DLLs next to the executable: put SDL2.dll there so the build runs as is
if(WIN32 AND NOT SDL2_USE_PKGCONFIG)
  add_custom_command(TARGET crosspoint_emulator POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different $<TARGET_RUNTIME_DLLS:crosspoint_emulator> $<TARGET_FILE_DIR:crosspoint_emulator>
    COMMAND_EXPAND_LISTS
  )
endif()

target_compile_definitions(crosspoint_emulator PRIVATE
  CROSSPOINT_EMULATED=1
  ARDUINO=10812
  MAX_WIDTH=3072
  PNG_MAX_BUFFERED_PIXELS=16416
  FREEINK_DEVICE_X4=1
  PROGMEM=
  EINK_DISPLAY_SINGLE_BUFFER_MODE=1
  CROSSPOINT_VERSION="${CROSSPOINT_FIRMWARE_VERSION}-emu"
  DISABLE_FS_H_WARNING=1
  XML_GE=0
  XML_CONTEXT_BYTES=0
  DESTRUCTOR_CLOSES_FILE=1
  CONTENT_EXTERNAL_MINIZ=1
  FREEINK_FONT_ENABLE_AUTOHINT=1
  USE_UTF8_LONG_NAMES=1
  ENABLE_SERIAL_LOG=1
  LOG_LEVEL=2
)

target_compile_options(crosspoint_emulator PRIVATE
  "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/sim/include/sim_platform_compat.h"
)
# Clang flags HalFile::flush() in the firmware's HalStorage.h (it overrides Print::flush
# without `override`); the device builds with GCC, which does not warn.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(crosspoint_emulator PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-inconsistent-missing-override>)
endif()
