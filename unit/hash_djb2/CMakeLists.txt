register_concrete (hash_djb2)
concrete_include (PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_implements_abstract (hash)
c_interface_scanner_setup (GLOB "*.c" "*.h")
