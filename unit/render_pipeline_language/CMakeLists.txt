register_concrete (render_pipeline_language)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

set (GENERATION_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/kan/render_pipeline_language/parser.c")
set (GENERATION_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/Generated/parser.c")

add_custom_command (
        OUTPUT "${GENERATION_OUTPUT}"
        DEPENDS "${GENERATION_SOURCE}"
        COMMENT "Generate render_pipeline_language. code from re2c template."
        COMMAND re2c "${GENERATION_SOURCE}" -o "${GENERATION_OUTPUT}")

concrete_highlight_direct ("${GENERATION_SOURCE}")
concrete_sources_direct (
        "${GENERATION_OUTPUT}"
        "${CMAKE_CURRENT_SOURCE_DIR}/kan/render_pipeline_language/compiler.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/kan/render_pipeline_language/emitter.c")

concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE container)
concrete_require (SCOPE PRIVATE ABSTRACT error log reflection THIRD_PARTY SPIRV-Headers::SPIRV-Headers)

c_interface_scanner_setup (GLOB "*.h")
reflection_generator_setup (GLOB "*.h")
register_unit_reflection ()

reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}")

set (KAN_RPL_PARSER_STACK_GROUP_SIZE "1048576 " CACHE STRING
        "Size of a stack group allocator to store all parser allocations.")
set (KAN_RPL_COMPILER_CONTEXT_RESOLVE_STACK "65536 " CACHE STRING
        "Size of a stack group allocator node to store all allocations for compiler context resolve.")
set (KAN_RPL_COMPILER_INSTANCE_RESOLVE_STACK "1048576 " CACHE STRING
        "Size of a stack group allocator node to store all compiler instance data allocated during resolve.")
set (KAN_RPL_COMPILER_INSTANCE_MAX_FLAT_NAME_LENGTH "256 " CACHE STRING
        "Max length for flattened buffer declaration readable name.")
set (KAN_RPL_PARSER_SPIRV_GENERATION_TEMPORARY_SIZE "1048576" CACHE STRING
        "Size of a temporary stack group allocator stack for SPIRV generation algorithm.")

concrete_compile_definitions (
        PRIVATE
        KAN_RPL_PARSER_STACK_GROUP_SIZE=${KAN_RPL_PARSER_STACK_GROUP_SIZE}
        KAN_RPL_COMPILER_CONTEXT_RESOLVE_STACK=${KAN_RPL_COMPILER_CONTEXT_RESOLVE_STACK}
        KAN_RPL_COMPILER_INSTANCE_RESOLVE_STACK=${KAN_RPL_COMPILER_INSTANCE_RESOLVE_STACK}
        KAN_RPL_COMPILER_INSTANCE_MAX_FLAT_NAME_LENGTH=${KAN_RPL_COMPILER_INSTANCE_MAX_FLAT_NAME_LENGTH}
        KAN_RPL_PARSER_SPIRV_GENERATION_TEMPORARY_SIZE=${KAN_RPL_PARSER_SPIRV_GENERATION_TEMPORARY_SIZE})
