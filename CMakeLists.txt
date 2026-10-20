cmake_minimum_required(VERSION 3.22)

project(AnimalAudio VERSION 0.0.0 LANGUAGES C CXX
    DESCRIPTION "Animal Audio monorepo (per-plugin semver in plugins/<name>/VERSION)")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(AA_BUILD_TESTS "Build unit tests (Catch2)" ON)
option(AA_BUILD_TOOLS "Build developer tools (e.g. headless UI screenshot renderers)" OFF)
option(AA_BUILD_STANDALONE "Also build Standalone apps (handy for local debugging)" OFF)
option(AA_COPY_PLUGINS_AFTER_BUILD "Install VST3 into the user plugin folder after build" OFF)
option(AA_LOCAL_BUILD "Suffix product name with Local (dev installs; release scripts turn this OFF)" OFF)
set(AA_PLUGINS "" CACHE STRING "Semicolon-separated plugin folder names to build (empty = all in plugins/)")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)
include(FetchDependencies)
include(AnimalAudioPlugin)
include(AnimalAudioBuildInfo)

add_subdirectory(shared)

file(GLOB AA_PLUGIN_DIRS LIST_DIRECTORIES true RELATIVE ${CMAKE_CURRENT_LIST_DIR}/plugins
     ${CMAKE_CURRENT_LIST_DIR}/plugins/*)
foreach(plugin_dir IN LISTS AA_PLUGIN_DIRS)
    if(NOT EXISTS ${CMAKE_CURRENT_LIST_DIR}/plugins/${plugin_dir}/CMakeLists.txt)
        continue()
    endif()
    if(AA_PLUGINS AND NOT plugin_dir IN_LIST AA_PLUGINS)
        continue()
    endif()
    message(STATUS "Animal Audio plugin: ${plugin_dir}")
    add_subdirectory(plugins/${plugin_dir})
endforeach()
