cmake_minimum_required (VERSION 3.22)

# Universal (Apple Silicon + Intel) by default on macOS. Must be set before project().
if (CMAKE_HOST_APPLE AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
    set (CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
endif ()
set (CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

project (DetroitHallelujah VERSION 1.0.0 LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set_property (GLOBAL PROPERTY USE_FOLDERS ON)

option (HB_BUILD_PLUGINS "Build the plug-ins (downloads JUCE)" ON)
option (HB_BUILD_TESTS   "Build the DSP unit tests" ON)
option (HB_COPY_AFTER_BUILD "Install plug-ins into the system plug-in folders after building" OFF)
set (HB_JUCE_TAG  "8.0.12" CACHE STRING "JUCE release tag to download")
set (HB_JUCE_PATH ""       CACHE PATH   "Use a local JUCE checkout instead of downloading")
set (HB_AAX_SDK_PATH ""    CACHE PATH   "Path to the Avid AAX SDK (enables the AAX format)")

set (HB_SHARED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shared")

# ---- DSP tests (no JUCE needed) ------------------------------------------------------
if (HB_BUILD_TESTS)
    enable_testing ()
    add_executable (dsp_tests tests/dsp_tests.cpp)
    target_include_directories (dsp_tests PRIVATE "${HB_SHARED_DIR}/dsp")
    add_test (NAME dsp_tests COMMAND dsp_tests)
endif ()

# ---- Plug-ins ---------------------------------------------------------------------------
if (HB_BUILD_PLUGINS)
    if (HB_JUCE_PATH)
        add_subdirectory ("${HB_JUCE_PATH}" JUCE EXCLUDE_FROM_ALL)
    else ()
        include (FetchContent)
        FetchContent_Declare (JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG        ${HB_JUCE_TAG}
            GIT_SHALLOW    TRUE
            GIT_PROGRESS   TRUE)
        FetchContent_MakeAvailable (JUCE)
    endif ()

    if (HB_AAX_SDK_PATH)
        juce_set_aax_sdk_path ("${HB_AAX_SDK_PATH}")
    endif ()

    set (HB_FORMATS VST3)
    if (APPLE)
        list (APPEND HB_FORMATS AU)
    endif ()
    if (HB_AAX_SDK_PATH)
        list (APPEND HB_FORMATS AAX)
    endif ()

    add_subdirectory (plugins/HallelujahBlue)
    add_subdirectory (plugins/HallelujahDuck)
endif ()
