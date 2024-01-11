register_concrete (context)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC ABSTRACT memory_profiler)
concrete_require (SCOPE PRIVATE ABSTRACT error log memory CONCRETE_INTERFACE container)
c_interface_scanner_setup (GLOB "*.c" "*.h")

set (KAN_CONTEXT_SYSTEM_INITIAL_BUCKETS "16" CACHE STRING "Initial count of buckets for systems hash storage.")
set (KAN_CONTEXT_SYSTEM_LOAD_FACTOR "4" CACHE STRING "Target count of systems per bucket.")
set (KAN_CONTEXT_SYSTEM_CONNECTIONS_INITIAL_COUNT "4" CACHE STRING "Initial size for outgoing connections array.")

concrete_compile_definitions (
        PRIVATE
        KAN_CONTEXT_SYSTEM_INITIAL_BUCKETS=${KAN_CONTEXT_SYSTEM_INITIAL_BUCKETS}
        KAN_CONTEXT_SYSTEM_LOAD_FACTOR=${KAN_CONTEXT_SYSTEM_LOAD_FACTOR}
        KAN_CONTEXT_SYSTEM_CONNECTIONS_INITIAL_COUNT=${KAN_CONTEXT_SYSTEM_CONNECTIONS_INITIAL_COUNT})
