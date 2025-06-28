register_concrete (readable_data)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC INTERFACE stream)
concrete_require (SCOPE PRIVATE ABSTRACT error log memory CONCRETE_INTERFACE container)

concrete_preprocessing_queue_step_apply (COMMAND re2c ARGUMENTS "$$INPUT" -o "$$OUTPUT" FILTER "readable_data.c$")
setup_core_preprocessing ()

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
