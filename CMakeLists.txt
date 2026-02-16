cmake_minimum_required(VERSION 3.16)
project(crosspoint-emulator CXX C)

set(CMAKE_CXX_STANDARD 17)

if(MSVC)
  add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# Crosspoint repo path (sibling by default)
if(NOT DEFINED CROSSPOINT_ROOT)
  set(CROSSPOINT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../Crosspoint")
endif()

# Optional: point to a local SDL2 install (e.g. from Downloads or built from source)
# Usage: cmake -DSDL2_ROOT=/path/to/SDL2-2.30.2 ..
if(DEFINED SDL2_ROOT)
  set(CMAKE_PREFIX_PATH "${SDL2_ROOT};${CMAKE_PREFIX_PATH}")
endif()

# Find SDL2: try config first, then pkg-config
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
  message(FATAL_ERROR "SDL2 not found. Need SDL2 (not SDL3). Options:\n"
    "  - Install: brew install sdl2\n"
    "  - Or download SDL2 from https://github.com/libsdl-org/SDL/releases (choose a 2.x release),\n"
    "    extract it, build and install, then: cmake -DSDL2_ROOT=/path/to/SDL2-build ..")
endif()

# Pre-build: generate HTML headers if Crosspoint src exists
if(EXISTS "${CROSSPOINT_ROOT}/scripts/build_html.py")
  find_program(PYTHON_EXE NAMES python3 python)
  if(PYTHON_EXE)
    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E env "PYTHONPATH=" "${PYTHON_EXE}" "${CROSSPOINT_ROOT}/scripts/build_html.py"
      WORKING_DIRECTORY "${CROSSPOINT_ROOT}"
      OUTPUT_QUIET
    )
  endif()
endif()

# Pre-build: generate I18n sources/headers into this repo (sandbox-writable)
set(I18N_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sim/generated/i18n")
if(EXISTS "${CROSSPOINT_ROOT}/scripts/gen_i18n.py")
  if(NOT PYTHON_EXE)
    find_program(PYTHON_EXE NAMES python3 python)
  endif()
  if(PYTHON_EXE)
    file(MAKE_DIRECTORY "${I18N_GEN_DIR}")
    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E env "PYTHONIOENCODING=UTF-8" "PYTHONUTF8=1"
              "${PYTHON_EXE}" "${CROSSPOINT_ROOT}/scripts/gen_i18n.py"
              "${CROSSPOINT_ROOT}/lib/I18n/translations"
              "${I18N_GEN_DIR}"
      WORKING_DIRECTORY "${CROSSPOINT_ROOT}"
      RESULT_VARIABLE I18N_GEN_RESULT
      OUTPUT_QUIET
      ERROR_VARIABLE I18N_GEN_ERROR
    )
    if(NOT I18N_GEN_RESULT EQUAL 0
       AND NOT (EXISTS "${I18N_GEN_DIR}/I18nKeys.h"
                AND EXISTS "${I18N_GEN_DIR}/I18nStrings.h"
                AND EXISTS "${I18N_GEN_DIR}/I18nStrings.cpp"))
      message(FATAL_ERROR "Failed to generate I18n sources using scripts/gen_i18n.py: ${I18N_GEN_ERROR}")
    elseif(NOT I18N_GEN_RESULT EQUAL 0)
      message(WARNING "I18n generator returned non-zero exit code, but generated files exist; continuing.")
    endif()
  endif()
endif()

# Sim HAL and stubs (include stub impls for OTA/HTTP so we don't compile real network code)
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
  sim/src/ota_updater_stub.cpp
  sim/src/http_downloader_stub.cpp
  sim/src/image_to_bmp.cpp
  sim/src/patched/BaseTheme.cpp
  sim/src/patched/LyraTheme.cpp
)

# Crosspoint application sources (all src/*.cpp); exclude network impls we stub in sim
file(GLOB_RECURSE CROSSPOINT_SRC "${CROSSPOINT_ROOT}/src/*.cpp")
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX ".*/network/OtaUpdater\\.cpp$")
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX ".*/network/HttpDownloader\\.cpp$")
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX ".*/components/themes/BaseTheme\\.cpp$")
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX ".*/components/themes/lyra/LyraTheme\\.cpp$")

# Crosspoint libs (exclude hal - we use sim HAL)
file(GLOB CROSSPOINT_LIB_CPP
  "${CROSSPOINT_ROOT}/lib/GfxRenderer/*.cpp"
  "${CROSSPOINT_ROOT}/lib/Epub/*.cpp"
  "${CROSSPOINT_ROOT}/lib/Epub/Epub/*.cpp"
  "${CROSSPOINT_ROOT}/lib/Epub/Epub/*/*.cpp"
  "${CROSSPOINT_ROOT}/lib/Epub/Epub/*/*/*.cpp"
  "${CROSSPOINT_ROOT}/lib/Txt/*.cpp"
  "${CROSSPOINT_ROOT}/lib/Xtc/*.cpp"
  "${CROSSPOINT_ROOT}/lib/Xtc/Xtc/*.cpp"
  "${CROSSPOINT_ROOT}/lib/EpdFont/*.cpp"
  "${CROSSPOINT_ROOT}/lib/Utf8/*.cpp"
  "${CROSSPOINT_ROOT}/lib/FsHelpers/*.cpp"
  "${CROSSPOINT_ROOT}/lib/ZipFile/*.cpp"
  "${CROSSPOINT_ROOT}/lib/miniz/*.c"
  "${CROSSPOINT_ROOT}/lib/expat/*.c"
  "${CROSSPOINT_ROOT}/lib/JpegToBmpConverter/*.cpp"
  "${CROSSPOINT_ROOT}/lib/PngToBmpConverter/*.cpp"
  "${CROSSPOINT_ROOT}/lib/picojpeg/*.c"
  "${CROSSPOINT_ROOT}/lib/I18n/*.cpp"
  "${CROSSPOINT_ROOT}/lib/KOReaderSync/*.cpp"
  "${CROSSPOINT_ROOT}/lib/OpdsParser/*.cpp"
  "${CROSSPOINT_ROOT}/lib/OpdsParser/*/*.cpp"
)
# Exclude hal
list(FILTER CROSSPOINT_LIB_CPP EXCLUDE REGEX ".*/hal/.*")

# ArduinoJson from PlatformIO libs (if present)
set(ARDUINO_JSON_ROOT "${CROSSPOINT_ROOT}/.pio/libdeps/default/ArduinoJson")
if(EXISTS "${ARDUINO_JSON_ROOT}/src/ArduinoJson.h")
  file(GLOB ARDUINO_JSON_SOURCES "${ARDUINO_JSON_ROOT}/src/*.cpp")
  list(APPEND CROSSPOINT_LIB_CPP ${ARDUINO_JSON_SOURCES})
endif()

# Ensure exactly one I18nStrings.cpp is linked (avoid duplicate symbols).
list(FILTER CROSSPOINT_LIB_CPP EXCLUDE REGEX ".*/I18nStrings\\.cpp$")
if(EXISTS "${I18N_GEN_DIR}/I18nStrings.cpp")
  list(APPEND CROSSPOINT_LIB_CPP "${I18N_GEN_DIR}/I18nStrings.cpp")
elseif(EXISTS "${CROSSPOINT_ROOT}/lib/I18n/I18nStrings.cpp")
  list(APPEND CROSSPOINT_LIB_CPP "${CROSSPOINT_ROOT}/lib/I18n/I18nStrings.cpp")
endif()

add_executable(crosspoint_emulator
  ${SIM_SOURCES}
  ${CROSSPOINT_SRC}
  ${CROSSPOINT_LIB_CPP}
)

target_include_directories(crosspoint_emulator PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/sim/include
  ${I18N_GEN_DIR}
  ${CROSSPOINT_ROOT}/src
  ${CROSSPOINT_ROOT}/lib
  ${CROSSPOINT_ROOT}/lib/GfxRenderer
  ${CROSSPOINT_ROOT}/lib/Epub
  ${CROSSPOINT_ROOT}/lib/Txt
  ${CROSSPOINT_ROOT}/lib/Xtc
  ${CROSSPOINT_ROOT}/lib/EpdFont
  ${CROSSPOINT_ROOT}/lib/Utf8
  ${CROSSPOINT_ROOT}/lib/Logging
  ${CROSSPOINT_ROOT}/lib/FsHelpers
  ${CROSSPOINT_ROOT}/lib/ZipFile
  ${CROSSPOINT_ROOT}/lib/Serialization
  ${CROSSPOINT_ROOT}/lib/JpegToBmpConverter
  ${CROSSPOINT_ROOT}/lib/PngToBmpConverter
  ${CROSSPOINT_ROOT}/lib/I18n
  ${CROSSPOINT_ROOT}/lib/KOReaderSync
  ${CROSSPOINT_ROOT}/lib/OpdsParser
  ${CROSSPOINT_ROOT}/lib/miniz
  ${CROSSPOINT_ROOT}/lib/expat
  ${CROSSPOINT_ROOT}/lib/picojpeg
  ${CROSSPOINT_ROOT}/lib/Epub/Epub
  ${CROSSPOINT_ROOT}/lib/Epub/Epub/parsers
  ${CROSSPOINT_ROOT}/lib/Epub/Epub/hyphenation
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

target_compile_definitions(crosspoint_emulator PRIVATE
  CROSSPOINT_EMULATED=1
  CROSSPOINT_EMULATOR_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
  PROGMEM=
  EINK_DISPLAY_SINGLE_BUFFER_MODE=1
  CROSSPOINT_VERSION="0.16.0-emu"
  MINIZ_NO_ZLIB_COMPATIBLE_NAMES=1
  DISABLE_FS_H_WARNING=1
  XML_GE=0
  XML_CONTEXT_BYTES=1024
  USE_UTF8_LONG_NAMES=1
)

if(MSVC)
  target_compile_features(crosspoint_emulator PRIVATE cxx_std_20)
  target_compile_options(crosspoint_emulator PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:/FImsvc_compat.h>
    $<$<COMPILE_LANGUAGE:CXX>:/permissive>
  )
  target_compile_definitions(crosspoint_emulator PRIVATE
    strcasecmp=_stricmp
    strncasecmp=_strnicmp
  )
endif()

# main_sim.cpp provides main() and calls setup()/loop() from Crosspoint main.cpp
# So we must not link a second main - Crosspoint main.cpp does not define main on host
