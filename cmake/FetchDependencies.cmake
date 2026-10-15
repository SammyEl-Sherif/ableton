# Third-party dependencies, fetched at configure time and pinned to exact tags.
# Set FETCHCONTENT_BASE_DIR (or FETCHCONTENT_SOURCE_DIR_JUCE) to reuse a local checkout/cache.
include(FetchContent)

set(AA_JUCE_TAG "8.0.16-security" CACHE STRING "JUCE git tag to fetch")
set(AA_CATCH2_TAG "v3.16.1" CACHE STRING "Catch2 git tag to fetch")

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG ${AA_JUCE_TAG}
    GIT_SHALLOW ON
    GIT_PROGRESS ON)

if(AA_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG ${AA_CATCH2_TAG}
        GIT_SHALLOW ON)
endif()

FetchContent_MakeAvailable(JUCE)

if(AA_BUILD_TESTS)
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(CTest)
    include(Catch)
endif()
