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

set(COMMON_CPP_FILES
  src/common/event_loop.cpp
  src/common/gstreamer.cpp
  src/common/logger.cpp
  src/common/timer.cpp
)

set(SERVER_CPP_FILES
  src/server/app_src_source.cpp
  src/server/pipeline_source.cpp
  src/server/server.cpp
  src/server/source.cpp
  src/server/stream.cpp
  src/server/test_source.cpp
)

set(CLIENT_CPP_FILES
  src/client/callback_sink.cpp
  src/client/client.cpp
  src/client/frame.cpp
  src/client/pipeline_sink.cpp
  src/client/sink.cpp
)

add_library(${PROJECT_NAME} SHARED
  ${COMMON_CPP_FILES}
  ${SERVER_CPP_FILES}
  ${CLIENT_CPP_FILES}
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

# Install rules: lay out headers under <prefix>/include/librtsp preserving
# the common/ server/ client/ subdirectories, plus the generated
# export.hpp at <prefix>/include/librtsp/export.hpp.
include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/librtsp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.hpp"
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/exports/librtsp/export.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/librtsp
)

# Optional: install a GStreamer debug log function so ERROR-level messages
# (e.g. "Connection refused" from gstrtspconnection) can be retrieved via
# Gstreamer::drain_recent_errors(). When OFF, drain_recent_errors() always
# returns an empty vector and no log function is installed.
option(LIBRTSP_CAPTURE_GST_ERRORS
  "Capture GStreamer ERROR-level messages for Gstreamer::drain_recent_errors()"
  ON)
if(LIBRTSP_CAPTURE_GST_ERRORS)
  target_compile_definitions(${PROJECT_NAME} PRIVATE LIBRTSP_CAPTURE_GST_ERRORS=1)
endif()

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

option(LIBRTSP_BUILD_EXAMPLES "Build example programs in examples/" ON)
if(LIBRTSP_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
