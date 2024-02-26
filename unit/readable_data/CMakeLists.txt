register_concrete (readable_data)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

set (GENERATION_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/kan/readable_data/readable_data.c")
set (GENERATION_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/Generated/readable_data.c")

add_custom_command (
        OUTPUT "${GENERATION_OUTPUT}"
        DEPENDS "${GENERATION_SOURCE}"
        COMMENT "Generate readable_data code from re2c template."
        COMMAND re2c "${GENERATION_SOURCE}" -o "${GENERATION_OUTPUT}")

concrete_sources_direct ("${GENERATION_OUTPUT}")
concrete_require (SCOPE PUBLIC ABSTRACT reflection INTERFACE stream)
concrete_require (SCOPE PRIVATE ABSTRACT error log memory CONCRETE_INTERFACE container)
c_interface_scanner_setup (GLOB "*.c" "*.h")

set (KAN_READABLE_DATA_PARSE_INPUT_BUFFER_SIZE "16384" CACHE STRING
        "Size of an input buffer for readable data parser.")
set (KAN_READABLE_DATA_EMIT_FORMATTING_BUFFER_SIZE "128" CACHE STRING
        "Size of a formatting buffer for readable data emitter.")
set (KAN_READABLE_DATA_PARSE_TEMPORARY_ALLOCATOR_SIZE "4096" CACHE STRING
        "Size of one stack for temporary stack group allocator for parsing.")

concrete_compile_definitions (
        PRIVATE
        KAN_READABLE_DATA_PARSE_INPUT_BUFFER_SIZE=${KAN_READABLE_DATA_PARSE_INPUT_BUFFER_SIZE}
        KAN_READABLE_DATA_EMIT_FORMATTING_BUFFER_SIZE=${KAN_READABLE_DATA_EMIT_FORMATTING_BUFFER_SIZE}
        KAN_READABLE_DATA_PARSE_TEMPORARY_ALLOCATOR_SIZE=${KAN_READABLE_DATA_PARSE_TEMPORARY_ALLOCATOR_SIZE})
