# Pure C++17, no JUCE dependency: unit-testable and safe to reuse in any plugin.
add_library(animalaudio_dsp INTERFACE)
add_library(animalaudio::dsp ALIAS animalaudio_dsp)
target_include_directories(animalaudio_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(animalaudio_dsp INTERFACE cxx_std_17)

if(AA_BUILD_TESTS)
    add_executable(animalaudio_dsp_tests
        tests/ShapeCurveTests.cpp
        tests/SmoothingTests.cpp
        tests/TempoSyncTests.cpp)
    target_link_libraries(animalaudio_dsp_tests PRIVATE animalaudio::dsp Catch2::Catch2WithMain)
    catch_discover_tests(animalaudio_dsp_tests TEST_PREFIX "dsp.")
endif()
