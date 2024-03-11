register_abstract (file_system)
abstract_include ("${CMAKE_CURRENT_SOURCE_DIR}")
abstract_require (INTERFACE api_common stream)
c_interface_scanner_setup (GLOB "*.h")

set (KAN_FILE_SYSTEM_MAX_PATH_LENGTH "4096" CACHE STRING "Maximum absolute file path size.")
abstract_compile_definitions (KAN_FILE_SYSTEM_MAX_PATH_LENGTH=${KAN_FILE_SYSTEM_MAX_PATH_LENGTH})

if (UNIX)
    abstract_register_implementation (NAME platform_default PARTS file_system_linux file_system_common)
endif ()
