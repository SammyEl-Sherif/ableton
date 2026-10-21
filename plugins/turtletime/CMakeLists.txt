set(TURTLETIME_SOURCES
    Source/TurtleTimeEditor.cpp
    Source/TurtleTimeProcessor.cpp
    Source/dsp/TurtleTimeEngine.cpp
    Source/ui/TurtleTimeTheme.cpp
    Source/ui/TurtleTimeMotif.cpp
    Source/ui/TurtleTimeWidgets.cpp)

aa_add_plugin(TurtleTime
    PRODUCT_NAME "TurtleTime"
    PLUGIN_CODE Turt
    DESCRIPTION "Tempo-synced half-time, reverse, and beat repeat"
    SOURCES ${TURTLETIME_SOURCES})

target_include_directories(TurtleTime PRIVATE Source)

if(AA_BUILD_TESTS)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" _turtletime_version)
    string(STRIP "${_turtletime_version}" _turtletime_version)
    add_executable(TurtleTimeTests tests/TurtleTimeTests.cpp ${TURTLETIME_SOURCES})
    target_include_directories(TurtleTimeTests PRIVATE Source)
    aa_attach_plugin_build_info(TurtleTimeTests "${_turtletime_version}")
    target_link_libraries(TurtleTimeTests PRIVATE
        animalaudio::testing
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
    catch_discover_tests(TurtleTimeTests TEST_PREFIX "turtletime." PROPERTIES RESOURCE_LOCK native_display)
endif()
