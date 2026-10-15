set(__NAME_UPPER___SOURCES
    Source/__NAME__Editor.cpp
    Source/__NAME__Processor.cpp
    Source/dsp/__NAME__Presets.cpp
    Source/ui/__NAME__Theme.cpp)

aa_add_plugin(__NAME__
    PRODUCT_NAME "__NAME__"
    PLUGIN_CODE __CODE__
    DESCRIPTION "__DESCRIPTION__"
    SOURCES ${__NAME_UPPER___SOURCES})

target_include_directories(__NAME__ PRIVATE Source)

if(AA_BUILD_TESTS)
    # The standard shaper-plugin suites (shared/testing) run against this plugin's own processor and editor.
    # Set AA_SCREENSHOT_DIR to also write the headless UI renders as PNGs.
    add_executable(__NAME__Tests tests/__NAME__Tests.cpp ${__NAME_UPPER___SOURCES})
    target_include_directories(__NAME__Tests PRIVATE Source)
    target_link_libraries(__NAME__Tests PRIVATE
        animalaudio::testing
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
    catch_discover_tests(__NAME__Tests TEST_PREFIX "__NAME_LOWER__.")
endif()
