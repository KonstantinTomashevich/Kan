register_concrete (cpu_dispatch_kan)
concrete_include (PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PRIVATE ABSTRACT cpu_profiler error memory platform precise_time threading)
concrete_implements_abstract (cpu_dispatch)
c_interface_scanner_setup (GLOB "*.c" "*.h")

if (NOT MSVC)
    concrete_require (SCOPE PRIVATE THIRD_PARTY m)
endif ()

set (KAN_CPU_DISPATCHER_WAIT_CHECK_DELAY_NS "100000" CACHE STRING
        "When waiting for job to be executed, we go to sleep for this duration and then wake up to check again.")

concrete_compile_definitions (
        PRIVATE
        KAN_CPU_DISPATCHER_WAIT_CHECK_DELAY_NS=${KAN_CPU_DISPATCHER_WAIT_CHECK_DELAY_NS})
