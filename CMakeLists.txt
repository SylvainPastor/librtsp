cmake_minimum_required(VERSION 3.13)
project(rtsp LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# GStreamer dependencies via pkg-config (imported target)
set(GST_MODULES
  gstreamer-1.0
  gstreamer-app-1.0
  gstreamer-rtsp-server-1.0
)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED IMPORTED_TARGET ${GST_MODULES})

message(STATUS "Using pkg-config: ${PKG_CONFIG_EXECUTABLE}")
message(STATUS "GStreamer dependencies:")
foreach(_mod IN LISTS GST_MODULES)
  message(STATUS "  ${_mod}: ${GST_${_mod}_VERSION}")
  message(STATUS "    prefix:     ${GST_${_mod}_PREFIX}")
  message(STATUS "    includedir: ${GST_${_mod}_INCLUDEDIR}")
  message(STATUS "    libdir:     ${GST_${_mod}_LIBDIR}")
endforeach()

add_library(${PROJECT_NAME} SHARED
  src/app_src_source.cpp
  src/event_loop.cpp
  src/gstreamer.cpp
  src/logger.cpp
  src/pipeline_source.cpp
  src/server.cpp
  src/source.cpp
  src/stream.cpp
  src/test_source.cpp
  src/timer.cpp
)

# Hide symbols by default; only LIBRTSP_API-annotated ones are exported.
set_target_properties(${PROJECT_NAME} PROPERTIES
  CXX_VISIBILITY_PRESET   hidden
  VISIBILITY_INLINES_HIDDEN ON
)

# Generate the export header (defines LIBRTSP_API / LIBRTSP_NO_API /
# LIBRTSP_DEPRECATED ...). The generated file ends up at:
#   <build>/exports/librtsp/export.hpp
include(GenerateExportHeader)
generate_export_header(${PROJECT_NAME}
  BASE_NAME            LIBRTSP
  EXPORT_MACRO_NAME    LIBRTSP_API
  NO_EXPORT_MACRO_NAME LIBRTSP_NO_API
  EXPORT_FILE_NAME     ${CMAKE_CURRENT_BINARY_DIR}/exports/librtsp/export.hpp
)

target_include_directories(${PROJECT_NAME}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/exports>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/librtsp
)

target_link_libraries(${PROJECT_NAME}
  PUBLIC
    PkgConfig::GST
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(${PROJECT_NAME} PRIVATE
    -Werror -Wall -Wextra -Wpedantic
  )
endif()

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

option(LIBRTSP_BUILD_EXAMPLES "Build example programs in examples/" ON)
if(LIBRTSP_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
