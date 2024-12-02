# In some cases, it is useful to accumulate data during configuration stage and then write it to some file at the end
# of the configuration stage. For example, it could be some header that stores accumulated defines. For this case,
# cumulative file generation API was created. User should register cumulative file and store its content in global
# property. Then, when `cumulative_file_generation_execute` is called, all content will be extracted and saved properly.

# Registers new cumulative file generation.
# Arguments:
# - PATH: Path to save generated file when everything is done.
# - PROPERTY: Global property from which generated file content should be extracted.
function (cumulative_file_generation_register)
    cmake_parse_arguments (ARG "" "PATH;PROPERTY" "" ${ARGV})
    if (DEFINED ARG_UNPARSED_ARGUMENTS OR
            NOT DEFINED ARG_PATH OR
            NOT DEFINED ARG_PROPERTY)
        message (FATAL_ERROR "Incorrect function arguments!")
    endif ()

    get_property (PATHS GLOBAL PROPERTY INTERNAL_CUMULATIVE_FILE_PATHS)
    if (NOT PATHS)
        set (PATHS)
    endif ()

    get_property (PROPERTIES GLOBAL PROPERTY INTERNAL_CUMULATIVE_FILE_PROPERTIES)
    if (NOT PROPERTIES)
        set (PROPERTIES)
    endif ()

    message (STATUS "Registering cumulative file \"${ARG_PATH}\" generation from global property \"${ARG_PROPERTY}\".")
    list (APPEND PATHS "${ARG_PATH}")
    list (APPEND PROPERTIES "${ARG_PROPERTY}")

    set_property (GLOBAL PROPERTY INTERNAL_CUMULATIVE_FILE_PATHS "${PATHS}")
    set_property (GLOBAL PROPERTY INTERNAL_CUMULATIVE_FILE_PROPERTIES "${PROPERTIES}")
endfunction ()

# Called to generate all registered cumulative files. Should be the last command in configuration stage.
function (cumulative_file_generation_execute)
    get_property (PATHS GLOBAL PROPERTY INTERNAL_CUMULATIVE_FILE_PATHS)
    get_property (PROPERTIES GLOBAL PROPERTY INTERNAL_CUMULATIVE_FILE_PROPERTIES)

    if (NOT PATHS OR NOT PROPERTIES)
        return ()
    endif ()

    foreach (PATH PROPERTY IN ZIP_LISTS PATHS PROPERTIES)
        message (STATUS "Generating cumulative file \"${PATH}\" from global property \"${PROPERTY}\".")
        get_property (CONTENT GLOBAL PROPERTY "${PROPERTY}")
        file_write_if_not_equal ("${PATH}" "${CONTENT}")
    endforeach ()
endfunction ()
