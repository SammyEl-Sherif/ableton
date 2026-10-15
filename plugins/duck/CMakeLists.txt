set(DUCK_SOURCES
    Source/DuckEditor.cpp
    Source/DuckProcessor.cpp
    Source/dsp/DuckPresets.cpp
    Source/ui/DuckMotif.cpp
    Source/ui/DuckTheme.cpp)

aa_add_plugin(Duck
    PRODUCT_NAME "Duck"
    PLUGIN_CODE Duck
    DESCRIPTION "Tempo-synced volume ducking with an editable curve"
    SOURCES ${DUCK_SOURCES})

target_include_directories(Duck PRIVATE Source)

if(AA_BUILD_TESTS)
    # The standard shaper-plugin suites (shared/testing) run against Duck's own processor and editor.
    # Set AA_SCREENSHOT_DIR to also write the headless UI renders as PNGs.
    add_executable(DuckTests tests/DuckTests.cpp ${DUCK_SOURCES})
    target_include_directories(DuckTests PRIVATE Source)
    target_link_libraries(DuckTests PRIVATE
        animalaudio::testing
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
    catch_discover_tests(DuckTests TEST_PREFIX "duck.")
endif()
