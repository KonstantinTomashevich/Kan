set (GENERATION_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/c_interface_scanner.c")
set (GENERATION_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/Generated/c_interface_scanner.c")

add_custom_command (
        OUTPUT "${GENERATION_OUTPUT}"
        DEPENDS "${GENERATION_SOURCE}"
        COMMENT "Generate c_interface_scanner_main code from re2c template."
        COMMAND re2c "${GENERATION_SOURCE}" -o "${GENERATION_OUTPUT}")

register_concrete (c_interface_scanner_main)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources_direct ("${GENERATION_OUTPUT}")
concrete_require (SCOPE PUBLIC ABSTRACT error memory CONCRETE_INTERFACE container)
