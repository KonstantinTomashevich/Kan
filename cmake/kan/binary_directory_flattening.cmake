# Contains helper function for generation binary directories for binary directory hierarchy flattening.
#
# Main issue with directory hierarchy is that CMake generates target directories as
# "${CMAKE_CURRENT_BINARY_DIRECTORY}/CMakeFiles/${TARGET_NAME}", but target names must be global and unique.
# Therefore, when we creating something like generated reflection target for application plugin, its name looks
# like my_application_plugin_my_plugin_reflections_statics, so the result path would look like
# "${CMAKE_BINARY_DIR}/executable/my_application/CMakeFiles/my_application_plugin_my_plugin_reflections_statics".
# It is still okay when application name is as small as "my_application", but when it gets big like
# "application_framework_example_advanced_compilation", then issues start to appear as application name is duplicated
# and takes lots of space. The same thing usually happens with tests and their runners.
#
# Therefore, we've introduced custom flattened build dirs -- any add_subdirectory can point to
# "${CMAKE_BINARY_DIR}/flattened/${INDEX}/" directory as current binary and store its targets there to avoid
# excessive context duplication in path.

# Returns path to the next available flattened directory to OUTPUT_NAME variable.
function (get_next_flattened_binary_directory OUTPUT_NAME)
    get_property (FLATTENED_INDEX GLOBAL PROPERTY INTERNAL_FLATTENED_COUNT)
    if (NOT FLATTENED_INDEX)
        set (FLATTENED_INDEX 0)
    endif ()

    set (FLATTENED_DIRECTORY "${CMAKE_BINARY_DIR}/flattened/${FLATTENED_INDEX}")
    file (MAKE_DIRECTORY "${FLATTENED_DIRECTORY}")
    set ("${OUTPUT_NAME}" "${FLATTENED_DIRECTORY}" PARENT_SCOPE)

    math (EXPR FLATTENED_INDEX "${FLATTENED_INDEX} + 1")
    set_property (GLOBAL PROPERTY INTERNAL_FLATTENED_COUNT "${FLATTENED_INDEX}")
endfunction ()
