register_concrete (render_pipeline_language)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

concrete_require (SCOPE PUBLIC ABSTRACT reflection CONCRETE_INTERFACE container)
concrete_require (SCOPE PRIVATE ABSTRACT error log THIRD_PARTY SPIRV-Headers::SPIRV-Headers)

set (KAN_RPL_BUILTIN_HASH_STORAGE_BUCKETS "269" CACHE STRING
        "Fixed count of buckets for compiler builtin functions hash storage.")
set (KAN_RPL_PARSER_STACK_GROUP_SIZE "1048576 " CACHE STRING
        "Size of a stack group allocator to store all parser allocations.")
set (KAN_RPL_INTERMEDIATE_EXPRESSION_STORAGE_SIZE "65536 " CACHE STRING
        "Initial size for expression storage used during building intermediate.")
set (KAN_RPL_INTERMEDIATE_EXPRESSION_LISTS_STORAGE_SIZE "65536 " CACHE STRING
        "Initial size for expression lists storage used during building intermediate.")
set (KAN_RPL_INTERMEDIATE_META_LISTS_STORAGE_SIZE "65536 " CACHE STRING
        "Initial size for meta lists storage used during building intermediate.")
set (KAN_RPL_COMPILER_CONTEXT_RESOLVE_STACK "65536 " CACHE STRING
        "Size of a stack group allocator node to store all allocations for compiler context resolve.")
set (KAN_RPL_COMPILER_INSTANCE_RESOLVE_STACK "1048576 " CACHE STRING
        "Size of a stack group allocator node to store all compiler instance data allocated during resolve.")
set (KAN_RPL_COMPILER_INSTANCE_MAX_FLAT_NAME_LENGTH "256" CACHE STRING
        "Max length for flattened buffer declaration readable name.")
set (KAN_RPL_PARSER_SPIRV_GENERATION_TEMPORARY_SIZE "1048576" CACHE STRING
        "Size of a temporary stack group allocator stack for SPIRV generation algorithm.")
set (KAN_RPL_PARSER_UNIFORM_BUFFER_SIZE_LIMIT "16384" CACHE STRING
        "Compilation validates that uniform buffers are not bigger than this limit.")
set (KAN_RPL_PARSER_PUSH_CONSTANT_BUFFER_SIZE_LIMIT "128" CACHE STRING
        "Compilation validates that push constant buffers are not bigger than this limit.")

concrete_compile_definitions (
        PRIVATE
        KAN_RPL_BUILTIN_HASH_STORAGE_BUCKETS=${KAN_RPL_BUILTIN_HASH_STORAGE_BUCKETS}
        KAN_RPL_PARSER_STACK_GROUP_SIZE=${KAN_RPL_PARSER_STACK_GROUP_SIZE}
        KAN_RPL_INTERMEDIATE_EXPRESSION_STORAGE_SIZE=${KAN_RPL_INTERMEDIATE_EXPRESSION_STORAGE_SIZE}
        KAN_RPL_INTERMEDIATE_EXPRESSION_LISTS_STORAGE_SIZE=${KAN_RPL_INTERMEDIATE_EXPRESSION_LISTS_STORAGE_SIZE}
        KAN_RPL_INTERMEDIATE_META_LISTS_STORAGE_SIZE=${KAN_RPL_INTERMEDIATE_META_LISTS_STORAGE_SIZE}
        KAN_RPL_COMPILER_CONTEXT_RESOLVE_STACK=${KAN_RPL_COMPILER_CONTEXT_RESOLVE_STACK}
        KAN_RPL_COMPILER_INSTANCE_RESOLVE_STACK=${KAN_RPL_COMPILER_INSTANCE_RESOLVE_STACK}
        KAN_RPL_COMPILER_INSTANCE_MAX_FLAT_NAME_LENGTH=${KAN_RPL_COMPILER_INSTANCE_MAX_FLAT_NAME_LENGTH}
        KAN_RPL_PARSER_SPIRV_GENERATION_TEMPORARY_SIZE=${KAN_RPL_PARSER_SPIRV_GENERATION_TEMPORARY_SIZE}
        KAN_RPL_PARSER_UNIFORM_BUFFER_SIZE_LIMIT=${KAN_RPL_PARSER_UNIFORM_BUFFER_SIZE_LIMIT}
        KAN_RPL_PARSER_PUSH_CONSTANT_BUFFER_SIZE_LIMIT=${KAN_RPL_PARSER_PUSH_CONSTANT_BUFFER_SIZE_LIMIT})

concrete_preprocessing_queue_step_apply (COMMAND re2c ARGUMENTS "$$INPUT" -o "$$OUTPUT" FILTER "parser.c$")
concrete_preprocessing_queue_step_preprocess ()
reflection_preprocessor_setup_step (
        DIRECT "kan/render_pipeline_language/compiler.h" "kan/render_pipeline_language/parser.h")
register_unit_reflection ()
