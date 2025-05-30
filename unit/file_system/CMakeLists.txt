register_abstract (file_system)
abstract_include ("${CMAKE_CURRENT_SOURCE_DIR}")
abstract_require (INTERFACE api_common stream)

set (KAN_FILE_SYSTEM_MAX_PATH_LENGTH "4096" CACHE STRING "Maximum absolute file path size.")
abstract_compile_definitions (KAN_FILE_SYSTEM_MAX_PATH_LENGTH=${KAN_FILE_SYSTEM_MAX_PATH_LENGTH})

if (UNIX)
    abstract_register_implementation (NAME linux PARTS file_system_linux file_system_common)
    abstract_alias_implementation (ALIAS platform_default SOURCE linux)
elseif (WIN32)
    abstract_register_implementation (NAME win32 PARTS file_system_win32 file_system_common)
    abstract_alias_implementation (ALIAS platform_default SOURCE win32)
endif ()
