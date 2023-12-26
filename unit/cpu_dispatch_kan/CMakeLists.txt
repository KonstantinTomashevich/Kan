register_concrete (cpu_dispatch_kan)
concrete_include (PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PRIVATE ABSTRACT cpu_profiler error memory platform threading)
concrete_implements_abstract (cpu_dispatch)
c_interface_scanner_setup (GLOB "*.c" "*.h")

set (KAN_CPU_DISPATCHER_MAX_BACKGROUND_PERCENTAGE "50" CACHE STRING
        "The maximum percentage of global CPU dispatcher threads that execute background tasks.")

concrete_compile_definitions (
        PRIVATE
        KAN_CPU_DISPATCHER_MAX_BACKGROUND_PERCENTAGE=${KAN_CPU_DISPATCHER_MAX_BACKGROUND_PERCENTAGE})
