set(__NAME_UPPER___SOURCES
    Source/PluginEditor.cpp
    Source/PluginProcessor.cpp)

aa_add_plugin(__NAME__
    PRODUCT_NAME "__NAME__"
    PLUGIN_CODE __CODE__
    DESCRIPTION "__DESCRIPTION__"
    SOURCES ${__NAME_UPPER___SOURCES})

target_include_directories(__NAME__ PRIVATE Source)

if(AA_BUILD_TESTS)
    # Pure-DSP tests: no JUCE needed, so they build and run in seconds.
    add_executable(__NAME__Tests tests/GainTests.cpp)
    target_include_directories(__NAME__Tests PRIVATE Source)
    target_link_libraries(__NAME__Tests PRIVATE animalaudio::dsp Catch2::Catch2WithMain)
    catch_discover_tests(__NAME__Tests TEST_PREFIX "__NAME_LOWER__.")
endif()
