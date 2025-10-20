# Declares global build options for Kan project and applies their values.

include_guard (GLOBAL)

option (KAN_ENABLE_ADDRESS_SANITIZER "Add compile and link time flags, that enable address sanitizing." OFF)
option (KAN_ENABLE_THREAD_SANITIZER "Add compile and link time flags, that enable thread sanitizing." OFF)
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

    if (KAN_ENABLE_ADDRESS_SANITIZER)
        if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "^.*Clang$")
            add_compile_options (-fsanitize=address -fno-omit-frame-pointer)
            # We need to also link asan as adding just flags is not enough for some reason.
            link_libraries (asan)
        else ()
            message (FATAL_ERROR "Currently, address sanitizing is supported only under clang.")
        endif ()
    endif ()

    if (KAN_ENABLE_THREAD_SANITIZER)
        if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "^.*Clang$")
            add_compile_options (-fsanitize=thread)
            # We need to also link tsan as adding just flags is not enough for some reason.
            link_libraries (tsan)
        else ()
            message (FATAL_ERROR "Currently, thread sanitizing is supported only under clang.")
        endif ()
    endif ()

    if (KAN_TREAT_WARNINGS_AS_ERRORS)
        if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "^.*Clang$")
            if (MSVC)
                add_compile_options (/W4 /WX)

                # Path is non-portable due to how MSVC preprocessor behaves.
                add_compile_options (-Wno-nonportable-include-path)
            else ()
                add_compile_options (-Wall -Wextra -Werror -pedantic)
            endif ()

            add_compile_options (
                    # Actually, field with flexible array extension can be last field in a struct,
                    # but clang ignores this fact and treats it as errors.
                    -Wno-flexible-array-extensions
                    # We're using GNU-like preprocessor, therefore this warning is going to show up.
                    -Wno-gnu-line-marker
                    # As we're using preprocessing stack, parentheses from macro will trigger this warning.
                    -Wno-parentheses-equality
                    # We use our custom pragmas for code preprocessing.
                    -Wno-unknown-pragmas
                    # GCC and Clang only recognize static inline functions from headers when GCC-specific preprocessor
                    # markup is enabled. When usual #line markup is enabled, it treats them as unused and raises
                    # this warning, therefore making it impossible to use Cushion.
                    -Wno-unused-function
                    # We silence unused parameter warnings, because it is troublesome
                    # to silence them manually for every compiler.
                    -Wno-unused-parameter
                    # Zero length arrays greatly increase readability for classes and structs with dynamic sizes.
                    -Wno-zero-length-array)

        elseif (MSVC)
            add_compile_options (
                    /W4
                    /WX
                    # We use our custom pragmas for code preprocessing.
                    /wd4068
                    # We silence unused parameter warnings, because it is troublesome
                    # to silence them manually for every compiler.
                    /wd4100
                    # We're using unnamed typed for type punning to be more explicit.
                    /wd4116
                    # Zero length arrays greatly increase readability for classes and structs with dynamic sizes.
                    /wd4200
                    # Unfortunately, in VS2022 it is impossible for some reason to disable this warning locally in
                    # specific code fragment. Therefore, we disable it globally.
                    /wd4702
                    # Currently we're okay with assignments on conditional expressions.
                    /wd4706)
        else ()
            add_compile_options (
                    -Wall
                    -Wextra
                    -Werror
                    # We're aware that fallthrough is default implicit behavior and we use it in some cases.
                    -Wno-implicit-fallthrough
                    # It only triggered once and it was false-positive alarm, therefore we disable it.
                    -Wno-stringop-truncation
                    # GCC and Clang only recognize static inline functions from headers when GCC-specific preprocessor
                    # markup is enabled. When usual #line markup is enabled, it treats them as unused and raises
                    # this warning, therefore making it impossible to use Cushion.
                    -Wno-unused-function
                    # We silence unused parameter warnings, because it is troublesome
                    # to silence them manually for every compiler.
                    -Wno-unused-parameter
                    # We use our custom pragmas for code preprocessing.
                    -Wno-unknown-pragmas
                    # For some reason gcc just ignores when this warnings are muted through pragma macro,
                    # therefore we're forced to disable it globally.
                    -Wno-unused-variable
                    -Wno-unused-but-set-variable
                    # Zero length arrays greatly increase readability for classes and structs with dynamic sizes.
                    -Wno-zero-length-array)
        endif ()
    endif ()
endfunction ()

find_package (Vulkan)
if (NOT Vulkan_FOUND)
    message (WARNING "Unable to find Vulkan SDK! Targets that depend on it will be skipped.")
endif ()

find_package (ICU COMPONENTS uc)
if (NOT ICU_FOUND)
    message (WARNING "Unable to find ICU! Targets that depend on it will be skipped.")
endif ()

# Position independent code should be generated when one shared library depends on another shared library.
set (CMAKE_POSITION_INDEPENDENT_CODE ON)

# We use custom export pragma in order to detect exported symbols during preprocessing.
set (UNIT_FRAMEWORK_API_MACRO_EXPORT_PREFIX "_Pragma (\"kan_export\")")
