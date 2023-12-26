register_concrete (container)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC ABSTRACT memory threading)
concrete_require (SCOPE PRIVATE ABSTRACT error hash)

set (KAN_CONTAINER_STRING_INTERNING_STACK_SIZE "1048576" CACHE STRING
        "Size of stack instance for string interning logic.")
set (KAN_CONTAINER_STRING_INTERNING_INITIAL_BUCKETS "8192" CACHE STRING
        "Initial count of buckets for interned strings hash storage.")
set (KAN_CONTAINER_STRING_INTERNING_LOAD_FACTOR "4" CACHE STRING "Target count of interned strings per bucket.")
set (KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS "4" CACHE STRING "Maximum supported dimensions for space tree.")

concrete_compile_definitions (
        PRIVATE
        KAN_CONTAINER_STRING_INTERNING_STACK_SIZE=${KAN_CONTAINER_STRING_INTERNING_STACK_SIZE}
        KAN_CONTAINER_STRING_INTERNING_INITIAL_BUCKETS=${KAN_CONTAINER_STRING_INTERNING_INITIAL_BUCKETS}
        KAN_CONTAINER_STRING_INTERNING_LOAD_FACTOR=${KAN_CONTAINER_STRING_INTERNING_LOAD_FACTOR})

concrete_compile_definitions (
        PUBLIC
        KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS=${KAN_CONTAINER_SPACE_TREE_MAX_DIMENSIONS})

c_interface_scanner_setup (GLOB "*.c" "*.h")
