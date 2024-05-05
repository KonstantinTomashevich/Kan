register_concrete (cpu_profiler_tracy)
concrete_include (PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PRIVATE ABSTRACT threading CONCRETE_INTERFACE container THIRD_PARTY Tracy::TracyClient)
concrete_implements_abstract (cpu_profiler)
c_interface_scanner_setup (GLOB "*.c" "*.h")

set (KAN_CPU_PROFILER_TRACY_INITIAL_SECTION_BUCKETS "64" CACHE STRING
        "Initial number of buckets for CPU profiling sections.")
set (KAN_CPU_PROFILER_TRACY_SECTION_LOAD_FACTOR "4" CACHE STRING
        "Target count of CPU profiling sections per bucket.")

concrete_compile_definitions (
        PRIVATE
        KAN_CPU_PROFILER_TRACY_INITIAL_SECTION_BUCKETS=${KAN_CPU_PROFILER_TRACY_INITIAL_SECTION_BUCKETS}
        KAN_CPU_PROFILER_TRACY_SECTION_LOAD_FACTOR=${KAN_CPU_PROFILER_TRACY_SECTION_LOAD_FACTOR})

# Fixup for Unix build -- we need to link to stdc++
if (UNIX)
    concrete_require (SCOPE PRIVATE THIRD_PARTY stdc++)
endif ()
