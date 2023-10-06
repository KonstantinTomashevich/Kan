# Declares global build options for Kan project and applies their values.

include_guard (GLOBAL)

option (KAN_ENABLE_COVERAGE "Add compile and link time flags, that enable code coverage reporting." OFF)
option (KAN_TREAT_WARNINGS_AS_ERRORS "Enables \"treat warnings as errors\" compiler policy for all targets." ON)

# We can not add common compile options here, because they would affect third party libraries compilation.
# Therefore every Kan root source directory must call this function to setup compile options locally.
function (add_common_compile_options)
    if (KAN_ENABLE_COVERAGE)
        if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "^.*Clang$")
            add_compile_options (-fprofile-instr-generate -fcoverage-mapping)
        else ()
            add_compile_options (--coverage)
            add_link_options (--coverage)
        endif ()
    endif ()

    if (KAN_TREAT_WARNINGS_AS_ERRORS)
        if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "^.*Clang$")
            if (MSVC)
                add_compile_options (/W4 /WX)
            else ()
                add_compile_options (-pedantic)
            endif ()

            add_compile_options (
                    # We silence unused parameter warnings, because it is troublesome
                    # to silence them manually for every compiler.
                    -Wno-unused-parameter
                    # Zero length arrays greatly increase readability for classes and structs with dynamic sizes.
                    -Wno-zero-length-array)

        elseif (MSVC)
            add_compile_options (
                    /W4
                    /WX
                    # We silence unused parameter warnings, because it is troublesome
                    # to silence them manually for every compiler.
                    /wd4100
                    # Zero length arrays greatly increase readability for classes and structs with dynamic sizes.
                    /wd4200
                    # Currently we're okay with assignments on conditional expressions.
                    /wd4706)
        else ()
            add_compile_options (
                    -Wall
                    -Wextra
                    -Werror
                    # We silence unused parameter warnings, because it is troublesome
                    # to silence them manually for every compiler.
                    -Wno-unused-parameter
                    # Zero length arrays greatly increase readability for classes and structs with dynamic sizes.
                    -Wno-zero-length-array)
        endif ()
    endif ()
endfunction ()

# Position independent code should be generated when one shared library depends on another shared library.
set (CMAKE_POSITION_INDEPENDENT_CODE ON)
