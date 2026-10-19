# Build stamp (version + git short SHA + date) for every plugin.
#
# `aa_build_info` runs on every build (it is cheap and only rewrites the header when something changed), so the
# stamp reflects the commit that was actually compiled, not the one that was checked out at configure time.
# `animalaudio::build_info` carries the include directory and the build-order dependency; animalaudio::core links it,
# so every plugin and test target gets it without doing anything. Only shared/core/src/BuildStamp.cpp includes the
# generated header.
set(AA_BUILD_INFO_DIR "${CMAKE_BINARY_DIR}/generated")
set(AA_BUILD_INFO_HEADER "${AA_BUILD_INFO_DIR}/animalaudio_build_info.h")

add_custom_target(aa_build_info ALL
    COMMAND ${CMAKE_COMMAND}
        -DOUT=${AA_BUILD_INFO_HEADER}
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DVERSION=${PROJECT_VERSION}
        -DLOCAL_BUILD=${AA_LOCAL_BUILD}
        -P ${CMAKE_CURRENT_LIST_DIR}/GenerateBuildInfo.cmake
    COMMENT "Generating build stamp"
    VERBATIM)

# The header must exist for IDE indexing and for configure-only runs.
execute_process(COMMAND ${CMAKE_COMMAND}
    -DOUT=${AA_BUILD_INFO_HEADER} -DSOURCE_DIR=${CMAKE_SOURCE_DIR} -DVERSION=${PROJECT_VERSION}
    -DLOCAL_BUILD=${AA_LOCAL_BUILD}
    -P ${CMAKE_CURRENT_LIST_DIR}/GenerateBuildInfo.cmake)

add_library(animalaudio_build_info INTERFACE)
add_library(animalaudio::build_info ALIAS animalaudio_build_info)
target_include_directories(animalaudio_build_info INTERFACE ${AA_BUILD_INFO_DIR})
add_dependencies(animalaudio_build_info aa_build_info)
