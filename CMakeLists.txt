cmake_minimum_required(VERSION 3.16)
project(crosspoint-emulator CXX C)

set(CMAKE_CXX_STANDARD 20)

# Crosspoint repo path (try common sibling names by default)
if(NOT DEFINED CROSSPOINT_ROOT)
  set(_crosspoint_root_candidates
    "${CMAKE_CURRENT_SOURCE_DIR}/../Crosspoint"
    "${CMAKE_CURRENT_SOURCE_DIR}/../crosspoint-reader"
    "${CMAKE_CURRENT_SOURCE_DIR}/../crosspoint-reader/crosspoint-reader"
  )
  foreach(_crosspoint_candidate IN LISTS _crosspoint_root_candidates)
    if(EXISTS "${_crosspoint_candidate}/src/main.cpp")
      set(CROSSPOINT_ROOT "${_crosspoint_candidate}")
      break()
    endif()
  endforeach()
endif()
if(NOT DEFINED CROSSPOINT_ROOT)
  set(CROSSPOINT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../Crosspoint")
endif()
file(TO_CMAKE_PATH "${CROSSPOINT_ROOT}" CROSSPOINT_ROOT)

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
  if(WIN32)
    find_program(CROSSPOINT_PYTHON NAMES python py)
  else()
    find_program(CROSSPOINT_PYTHON NAMES python3 python)
  endif()
endif()

if(EXISTS "${CROSSPOINT_ROOT}/scripts/build_html.py" AND CROSSPOINT_PYTHON)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "PYTHONPATH=" "${CROSSPOINT_PYTHON}" "${CROSSPOINT_ROOT}/scripts/build_html.py"
    WORKING_DIRECTORY "${CROSSPOINT_ROOT}"
    OUTPUT_QUIET
  )
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
  sim/src/obfuscation_utils_stub.cpp
  sim/src/uzlib_checksums_stub.cpp
  sim/src/webdav_handler_stub.cpp
)

# Crosspoint application sources (all src/*.cpp); exclude network impls we stub in sim
file(GLOB_RECURSE CROSSPOINT_SRC CONFIGURE_DEPENDS "${CROSSPOINT_ROOT}/src/*.cpp")
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX ".*/network/OtaUpdater\\.cpp$")
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX ".*/network/HttpDownloader\\.cpp$")
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX ".*/network/WebDAVHandler\\.cpp$")

# Crosspoint libs (exclude hal - we use sim HAL)
file(GLOB CROSSPOINT_LIB_CPP CONFIGURE_DEPENDS
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
  "${CROSSPOINT_ROOT}/lib/I18n/*.cpp"
  "${CROSSPOINT_ROOT}/lib/Logging/*.cpp"
  "${CROSSPOINT_ROOT}/lib/InflateReader/*.cpp"
  "${CROSSPOINT_ROOT}/lib/PngToBmpConverter/*.cpp"
  "${CROSSPOINT_ROOT}/lib/ZipFile/*.cpp"
  "${CROSSPOINT_ROOT}/lib/XmlParserUtils/*.cpp"
  "${CROSSPOINT_ROOT}/lib/Serialization/*.cpp"
  "${CROSSPOINT_ROOT}/lib/miniz/*.c"
  "${CROSSPOINT_ROOT}/lib/expat/*.c"
  "${CROSSPOINT_ROOT}/lib/uzlib/src/*.c"
  "${CROSSPOINT_ROOT}/lib/JpegToBmpConverter/*.cpp"
  "${CROSSPOINT_ROOT}/lib/picojpeg/*.c"
  "${CROSSPOINT_ROOT}/lib/KOReaderSync/*.cpp"
  "${CROSSPOINT_ROOT}/lib/OpdsParser/*.cpp"
  "${CROSSPOINT_ROOT}/lib/OpdsParser/*/*.cpp"
)
# Exclude hal
list(FILTER CROSSPOINT_LIB_CPP EXCLUDE REGEX ".*/hal/.*")

# ArduinoJson from PlatformIO libs (if present)
set(ARDUINO_JSON_ROOT "${CROSSPOINT_ROOT}/.pio/libdeps/default/ArduinoJson")
set(JPEGDEC_ROOT "${CROSSPOINT_ROOT}/.pio/libdeps/default/JPEGDEC")
set(PNGDEC_ROOT "${CROSSPOINT_ROOT}/.pio/libdeps/default/PNGdec")
if(EXISTS "${ARDUINO_JSON_ROOT}/src/ArduinoJson.h")
  file(GLOB ARDUINO_JSON_SOURCES CONFIGURE_DEPENDS "${ARDUINO_JSON_ROOT}/src/*.cpp")
  list(APPEND CROSSPOINT_LIB_CPP ${ARDUINO_JSON_SOURCES})
endif()
if(EXISTS "${JPEGDEC_ROOT}/src/JPEGDEC.h")
  file(GLOB JPEGDEC_SOURCES CONFIGURE_DEPENDS "${JPEGDEC_ROOT}/src/*.cpp")
  list(APPEND CROSSPOINT_LIB_CPP ${JPEGDEC_SOURCES})
endif()
if(EXISTS "${PNGDEC_ROOT}/src/PNGdec.h")
  file(GLOB PNGDEC_CPP_SOURCES CONFIGURE_DEPENDS "${PNGDEC_ROOT}/src/*.cpp")
  file(GLOB PNGDEC_C_SOURCES CONFIGURE_DEPENDS "${PNGDEC_ROOT}/src/*.c")
  list(APPEND CROSSPOINT_LIB_CPP ${PNGDEC_CPP_SOURCES} ${PNGDEC_C_SOURCES})
endif()

list(FILTER CROSSPOINT_LIB_CPP EXCLUDE REGEX ".*/Serialization/ObfuscationUtils\\.cpp$")

add_executable(crosspoint_emulator
  ${SIM_SOURCES}
  ${CROSSPOINT_SRC}
  ${CROSSPOINT_LIB_CPP}
)

target_include_directories(crosspoint_emulator PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/sim/include
  ${CROSSPOINT_ROOT}/src
  ${CROSSPOINT_ROOT}/lib
  ${CROSSPOINT_ROOT}/lib/GfxRenderer
  ${CROSSPOINT_ROOT}/lib/Epub
  ${CROSSPOINT_ROOT}/lib/Txt
  ${CROSSPOINT_ROOT}/lib/Xtc
  ${CROSSPOINT_ROOT}/lib/EpdFont
  ${CROSSPOINT_ROOT}/lib/Utf8
  ${CROSSPOINT_ROOT}/lib/FsHelpers
  ${CROSSPOINT_ROOT}/lib/I18n
  ${CROSSPOINT_ROOT}/lib/Logging
  ${CROSSPOINT_ROOT}/lib/InflateReader
  ${CROSSPOINT_ROOT}/lib/PngToBmpConverter
  ${CROSSPOINT_ROOT}/lib/ZipFile
  ${CROSSPOINT_ROOT}/lib/XmlParserUtils
  ${CROSSPOINT_ROOT}/lib/Serialization
  ${CROSSPOINT_ROOT}/lib/JpegToBmpConverter
  ${CROSSPOINT_ROOT}/lib/KOReaderSync
  ${CROSSPOINT_ROOT}/lib/OpdsParser
  ${CROSSPOINT_ROOT}/lib/miniz
  ${CROSSPOINT_ROOT}/lib/expat
  ${CROSSPOINT_ROOT}/lib/picojpeg
  ${CROSSPOINT_ROOT}/lib/uzlib/src
  ${CROSSPOINT_ROOT}/lib/Epub/Epub
  ${CROSSPOINT_ROOT}/lib/Epub/Epub/parsers
  ${CROSSPOINT_ROOT}/lib/Epub/Epub/hyphenation
)

if(EXISTS "${ARDUINO_JSON_ROOT}/src")
  target_include_directories(crosspoint_emulator PRIVATE ${ARDUINO_JSON_ROOT}/src)
endif()
if(EXISTS "${JPEGDEC_ROOT}/src")
  target_include_directories(crosspoint_emulator PRIVATE ${JPEGDEC_ROOT}/src)
endif()
if(EXISTS "${PNGDEC_ROOT}/src")
  target_include_directories(crosspoint_emulator PRIVATE ${PNGDEC_ROOT}/src)
endif()

if(SDL2_USE_PKGCONFIG)
  target_include_directories(crosspoint_emulator PRIVATE ${SDL2_INCLUDE_DIRS})
  target_link_libraries(crosspoint_emulator PRIVATE ${SDL2_LIBRARIES})
  target_link_directories(crosspoint_emulator PRIVATE ${SDL2_LIBRARY_DIRS})
else()
  target_link_libraries(crosspoint_emulator PRIVATE SDL2::SDL2)
endif()

if(WIN32)
  target_link_libraries(crosspoint_emulator PRIVATE winhttp)
  target_compile_options(crosspoint_emulator PRIVATE /FI"${CMAKE_CURRENT_SOURCE_DIR}/sim/include/msvc_compat.h")
endif()

target_compile_definitions(crosspoint_emulator PRIVATE
  CROSSPOINT_EMULATED=1
  ENABLE_SERIAL_LOG=1
  LOG_LEVEL=2
  PROGMEM=
  EINK_DISPLAY_SINGLE_BUFFER_MODE=1
  CROSSPOINT_VERSION="0.16.0-emu"
  MINIZ_NO_ZLIB_COMPATIBLE_NAMES=1
  DISABLE_FS_H_WARNING=1
  XML_GE=0
  XML_CONTEXT_BYTES=1024
  USE_UTF8_LONG_NAMES=1
  RTC_NOINIT_ATTR=
)

if(WIN32 AND DEFINED SDL2_ROOT AND EXISTS "${SDL2_ROOT}/bin/SDL2.dll")
  add_custom_command(TARGET crosspoint_emulator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${SDL2_ROOT}/bin/SDL2.dll"
      "$<TARGET_FILE_DIR:crosspoint_emulator>/SDL2.dll")
endif()

# main_sim.cpp provides main() and calls setup()/loop() from Crosspoint main.cpp
# So we must not link a second main - Crosspoint main.cpp does not define main on host
