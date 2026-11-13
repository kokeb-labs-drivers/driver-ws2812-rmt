#===============================================================================
# WS2812_RMT driver: build settings
#===============================================================================
# Read by the root CMakeLists.txt in both of its modes (plain CMake and the
# ESP-IDF component): target name, version, include directories, sources and
# ESP-IDF requirements. This file is the single source
# of the version; tests/CMakeLists.txt checks every other copy against it.
#===============================================================================

include_guard(GLOBAL)

set(WS2812_RMT_TARGET_NAME "ws2812_rmt")

#===============================================================================
# Version (single source of truth)
#===============================================================================
set(WS2812_RMT_VERSION_MAJOR 0)
set(WS2812_RMT_VERSION_MINOR 1)
set(WS2812_RMT_VERSION_PATCH 0)
set(WS2812_RMT_VERSION "${WS2812_RMT_VERSION_MAJOR}.${WS2812_RMT_VERSION_MINOR}.${WS2812_RMT_VERSION_PATCH}")

#===============================================================================
# Generated version header (build tree, never the source tree)
#===============================================================================
set(WS2812_RMT_VERSION_TEMPLATE "${CMAKE_CURRENT_LIST_DIR}/../inc/ws2812_rmt_version.h.in")
set(WS2812_RMT_VERSION_HEADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/ws2812_rmt_generated")
set(WS2812_RMT_VERSION_HEADER "${WS2812_RMT_VERSION_HEADER_DIR}/ws2812_rmt_version.h")

# The directory must exist before ESP-IDF's idf_component_register() checks the include paths.
file(MAKE_DIRECTORY "${WS2812_RMT_VERSION_HEADER_DIR}")
configure_file("${WS2812_RMT_VERSION_TEMPLATE}" "${WS2812_RMT_VERSION_HEADER}" @ONLY)

#===============================================================================
# Public include directories, sources, ESP-IDF requirements
#===============================================================================
set(WS2812_RMT_PUBLIC_INCLUDE_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../inc"
    "${WS2812_RMT_VERSION_HEADER_DIR}"
)

# Header-only: the template implementation is src/ws2812_rmt.ipp, included by inc/ws2812_rmt.hpp.
set(WS2812_RMT_SOURCE_FILES)

set(WS2812_RMT_IDF_REQUIRES)
