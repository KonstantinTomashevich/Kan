register_concrete (context_virtual_file_system)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC ABSTRACT virtual_file_system CONCRETE_INTERFACE context)
concrete_require (SCOPE PRIVATE ABSTRACT error log memory)
register_context_system (virtual_file_system_t)
c_interface_scanner_setup (GLOB "*.c" "*.h")
