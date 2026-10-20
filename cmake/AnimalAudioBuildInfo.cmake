# Per-plugin build stamp (version + git SHA + date). Each VST3 target gets its own generated header
# under build/generated/build_info/<Target>/animalaudio_build_info.h (version from plugins/<slug>/VERSION).

get_filename_component(AA_CMAKE_MODULE_DIR "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)

function(aa_attach_plugin_build_info target version)
    if(NOT target OR NOT version)
        message(FATAL_ERROR "aa_attach_plugin_build_info: target and version are required")
    endif()

    set(_info_dir "${CMAKE_BINARY_DIR}/generated/build_info/${target}")
    set(_header "${_info_dir}/animalaudio_build_info.h")

    add_custom_target(aa_build_info_${target}
        COMMAND ${CMAKE_COMMAND}
            -DOUT=${_header}
            -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
            -DVERSION=${version}
            -DLOCAL_BUILD=${AA_LOCAL_BUILD}
            -P ${AA_CMAKE_MODULE_DIR}/GenerateBuildInfo.cmake
        COMMENT "Generating build stamp for ${target}"
        VERBATIM)

    execute_process(COMMAND ${CMAKE_COMMAND}
        -DOUT=${_header}
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DVERSION=${version}
        -DLOCAL_BUILD=${AA_LOCAL_BUILD}
        -P ${AA_CMAKE_MODULE_DIR}/GenerateBuildInfo.cmake)

    target_include_directories(${target} BEFORE PRIVATE ${_info_dir})
    add_dependencies(${target} aa_build_info_${target})
endfunction()

# Legacy alias: INTERFACE target so nothing breaks if referenced; plugins no longer use a global header.
add_library(animalaudio_build_info INTERFACE)
add_library(animalaudio::build_info ALIAS animalaudio_build_info)
