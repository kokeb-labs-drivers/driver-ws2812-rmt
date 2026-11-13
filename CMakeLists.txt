#===============================================================================
# WS2812_RMT driver: root CMake
#===============================================================================
# One file, two build systems:
#
# - ESP-IDF (ESP_PLATFORM): this folder is an ESP-IDF component. Point
#   EXTRA_COMPONENT_DIRS at it, check it out under components/, or add it as a
#   component-manager git dependency. menuconfig reads _config/Kconfig.
# - Any other CMake build (host, STM32, RP2040, nRF, ...): one INTERFACE target,
#   ws2812_rmt, consumed as ws2812_rmt::ws2812_rmt both through add_subdirectory() and
#   through find_package(ws2812_rmt) after install.
#
#   cmake -S . -B build -DWS2812_RMT_BUILD_TESTS=ON
#   cmake --build build && ctest --test-dir build --output-on-failure
#===============================================================================
cmake_minimum_required(VERSION 3.16)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/ws2812_rmt_build_settings.cmake)

#===============================================================================
# ESP-IDF component
#===============================================================================
if(ESP_PLATFORM)
    idf_component_register(
        SRCS ${WS2812_RMT_SOURCE_FILES}
        INCLUDE_DIRS ${WS2812_RMT_PUBLIC_INCLUDE_DIRS}
        REQUIRES ${WS2812_RMT_IDF_REQUIRES}
        KCONFIG ${CMAKE_CURRENT_LIST_DIR}/_config/Kconfig
    )
    if(WS2812_RMT_SOURCE_FILES)
        target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
    else()
        target_compile_features(${COMPONENT_LIB} INTERFACE cxx_std_20)
    endif()
    return()
endif()

project(ws2812_rmt_driver
    VERSION ${WS2812_RMT_VERSION}
    DESCRIPTION "C++20 driver for single-wire addressable RGB and RGBW LEDs (WS2812, SK6812 and compatible) \
over a timed bit stream"
    LANGUAGES CXX)

include(GNUInstallDirs)

# ISO C++20 for every target this project builds (tests, fuzzers, examples), never the GNU dialect
# that CMake picks by default (handbook KB-CPP-001, KB-CPP20-001). CXX_EXTENSIONS does not travel
# through an INTERFACE target, so each folder that builds a target sets it as well.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The installed layout keeps the source tree's inc/ + src/ pair, because ws2812_rmt.hpp includes
# "../src/ws2812_rmt.ipp" relative to itself:
#   <prefix>/include/ws2812_rmt/inc/*.hpp, ws2812_rmt_version.h
#   <prefix>/include/ws2812_rmt/src/ws2812_rmt.ipp
set(WS2812_RMT_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}/ws2812_rmt/inc")

#===============================================================================
# Target
#===============================================================================
add_library(${WS2812_RMT_TARGET_NAME} INTERFACE)
target_include_directories(${WS2812_RMT_TARGET_NAME}
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/inc>
        $<BUILD_INTERFACE:${WS2812_RMT_VERSION_HEADER_DIR}>
        $<INSTALL_INTERFACE:${WS2812_RMT_INSTALL_INCLUDEDIR}>
)
target_compile_features(${WS2812_RMT_TARGET_NAME} INTERFACE cxx_std_20)

# ws2812_rmt::ws2812_rmt everywhere: the alias for add_subdirectory(), EXPORT_NAME for the installed package.
add_library(ws2812_rmt::ws2812_rmt ALIAS ${WS2812_RMT_TARGET_NAME})
set_target_properties(${WS2812_RMT_TARGET_NAME} PROPERTIES EXPORT_NAME ws2812_rmt)

option(WS2812_RMT_ENABLE_WARNINGS "Add -Wall -Wextra -Wpedantic to every target that links ws2812_rmt::ws2812_rmt" OFF)
if(WS2812_RMT_ENABLE_WARNINGS)
    target_compile_options(${WS2812_RMT_TARGET_NAME} INTERFACE -Wall -Wextra -Wpedantic)
endif()

#===============================================================================
# Host tests, documentation snippets, install check (tests/)
#===============================================================================
option(WS2812_RMT_BUILD_TESTS "Build the host tests in tests/" OFF)
option(WS2812_RMT_BUILD_FUZZERS "Also build the libFuzzer targets in tests/fuzz/ (Clang only)" OFF)
if(WS2812_RMT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

#===============================================================================
# Install and package export
#===============================================================================
include(CMakePackageConfigHelpers)

install(TARGETS ${WS2812_RMT_TARGET_NAME} EXPORT ws2812_rmtTargets
    INCLUDES DESTINATION ${WS2812_RMT_INSTALL_INCLUDEDIR})

install(DIRECTORY inc/ DESTINATION ${WS2812_RMT_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")
install(FILES "${WS2812_RMT_VERSION_HEADER}" DESTINATION ${WS2812_RMT_INSTALL_INCLUDEDIR})
install(DIRECTORY src/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ws2812_rmt/src
    FILES_MATCHING PATTERN "*.ipp")

install(EXPORT ws2812_rmtTargets
    FILE ws2812_rmtTargets.cmake
    NAMESPACE ws2812_rmt::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ws2812_rmt)

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/cmake/ws2812_rmtConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ws2812_rmtConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ws2812_rmt)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ws2812_rmtConfigVersion.cmake"
    VERSION ${WS2812_RMT_VERSION}
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ws2812_rmtConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ws2812_rmtConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ws2812_rmt)
