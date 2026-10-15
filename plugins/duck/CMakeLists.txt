set(DUCK_SOURCES
    Source/CurveState.cpp
    Source/PluginEditor.cpp
    Source/PluginProcessor.cpp
    Source/dsp/DuckPresets.cpp
    Source/ui/CurveEditor.cpp
    Source/ui/DuckArt.cpp
    Source/ui/DuckKnob.cpp)

aa_add_plugin(Duck
    PRODUCT_NAME "Duck"
    PLUGIN_CODE Duck
    DESCRIPTION "Tempo-synced volume ducking with an editable curve"
    SOURCES ${DUCK_SOURCES})

target_include_directories(Duck PRIVATE Source)

if(AA_BUILD_TESTS)
    # Logic tests (engine, curve state, processor) plus a headless UI render (set AA_SCREENSHOT_DIR to write PNGs).
    add_executable(DuckTests
        tests/DuckEngineTests.cpp
        tests/DuckPresetTests.cpp
        tests/CurveStateTests.cpp
        tests/ProcessorTests.cpp
        tests/ScreenshotTests.cpp
        ${DUCK_SOURCES})
    target_include_directories(DuckTests PRIVATE Source)
    target_compile_definitions(DuckTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(DuckTests PRIVATE
        animalaudio::ui
        animalaudio::dsp
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
        Catch2::Catch2WithMain)
    catch_discover_tests(DuckTests TEST_PREFIX "duck.")
endif()
