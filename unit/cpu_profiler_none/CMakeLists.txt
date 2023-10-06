register_concrete (cpu_profiler_none)
concrete_include (PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_implements_abstract (cpu_profiler)
