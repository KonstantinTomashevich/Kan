register_concrete (error_sdl)
concrete_include (PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (
        SCOPE PRIVATE
        ABSTRACT hash log threading
        CONCRETE_INTERFACE container
        THIRD_PARTY debug_break SDL3::SDL3)
concrete_implements_abstract (error)

set (KAN_ERROR_INTERACTIVE_CRITICAL_MESSAGE_MAX_LENGTH "1024" CACHE STRING
        "Max length for interactive critical error message box.")

concrete_compile_definitions (
        PRIVATE KAN_ERROR_INTERACTIVE_CRITICAL_MESSAGE_MAX_LENGTH=${KAN_ERROR_INTERACTIVE_CRITICAL_MESSAGE_MAX_LENGTH})
c_interface_scanner_setup (GLOB "*.c" "*.h")
