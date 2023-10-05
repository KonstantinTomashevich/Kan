# Integrates clang format as custom targets.

find_program (CLANG_FORMAT clang-format)
if (NOT CLANG_FORMAT-NOTFOUND)
    message (STATUS "Found clang-format: \"${CLANG_FORMAT}\".")
    message (STATUS "Generating formatting targets...")

    message (STATUS "    Searching for sources...")
    file (GLOB_RECURSE FILES "executable/*.c" "executable/*.h" "test/*.c" "test/*.h" "unit/*.c" "unit/*.h")

    message (STATUS "    Writing file list...")
    list (JOIN FILES "\n" FILE_LIST_CONTENT)
    file (WRITE "${CMAKE_BINARY_DIR}/clang_format_file_list.txt" "${FILE_LIST_CONTENT}")

    message (STATUS "    Setting up targets...")
    add_custom_target (
            kan_clang_format_check
            COMMENT "Check formatting using clang format."
            COMMAND
            "${CLANG_FORMAT}" --style=file --dry-run --Werror "--files=${CMAKE_BINARY_DIR}/clang_format_file_list.txt")

    add_custom_target (
            kan_clang_format_fix
            COMMENT "Fix formatting using clang format."
            COMMAND
            "${CLANG_FORMAT}" --style=file -i --Werror "--files=${CMAKE_BINARY_DIR}/clang_format_file_list.txt")

else ()
    message (WARNING "clang-format not found, formatting targets won't be created.")
endif ()
