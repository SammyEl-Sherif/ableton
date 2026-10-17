# Catch2 helpers that exercise the shared plugin plumbing through a plugin's own processor/editor.
# Link it into a plugin's test executable and instantiate the standard suites with
# AA_SHAPER_PLUGIN_TESTS / AA_GAIN_SHAPER_PLUGIN_TESTS / AA_REAL_MOUSE_PLUGIN_TESTS (see plugins/duck/tests).
# Header-only apart from one tiny source (compiled into each test exe) that prepares a bare X server.
add_library(animalaudio_testing INTERFACE)
add_library(animalaudio::testing ALIAS animalaudio_testing)
target_include_directories(animalaudio_testing INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_sources(animalaudio_testing INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src/NativeWindowSupport.cpp)
target_link_libraries(animalaudio_testing INTERFACE animalaudio::ui animalaudio::core animalaudio::dsp
                                                    juce::juce_audio_utils Catch2::Catch2WithMain)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(animalaudio_testing INTERFACE X11)
endif()
target_compile_definitions(animalaudio_testing INTERFACE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_USE_XSHM=0)
