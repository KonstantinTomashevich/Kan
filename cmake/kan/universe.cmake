# Declares utility functions for settings up units that are built upon on universe unit functionality.

# Setups preprocessing queue for concrete unit that uses universe with both universe preprocessor and reflection.
# Arguments:
# - SKIP_REFLECTION_REGISTRATION: Optional flag. If passed, unit reflection is not registered. Mostly used for tests.
function (universe_concrete_preprocessing_queue)
    cmake_parse_arguments (ARG "SKIP_REFLECTION_REGISTRATION" "" "" ${ARGV})
    if (DEFINED ARG_UNPARSED_ARGUMENTS)
        message (FATAL_ERROR "Incorrect function arguments!")
    endif ()

    concrete_preprocessing_queue_step_cushion ()
    reflection_preprocessor_setup_step (GLOB "*.h" "*.c")
    # TODO: We will later rework preprocessing queue setups in a common pattern.

    if (NOT ARG_SKIP_REFLECTION_REGISTRATION)
        register_unit_reflection ()
    endif ()
endfunction ()
