register_concrete (context_application_system)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC ABSTRACT platform CONCRETE_INTERFACE container context)
concrete_require (SCOPE PRIVATE ABSTRACT cpu_profiler error memory)
register_context_system (NAME application_system_t)
c_interface_scanner_setup (GLOB "*.c" "*.h")

set (KAN_APPLICATION_SYSTEM_OPERATION_STACK_SIZE "1024" CACHE STRING
        "Size of a stack chunk for application system operations allocation.")

concrete_compile_definitions (
        PRIVATE
        KAN_APPLICATION_SYSTEM_OPERATION_STACK_SIZE=${KAN_APPLICATION_SYSTEM_OPERATION_STACK_SIZE})
