register_abstract (memory_profiler)
abstract_include ("${CMAKE_CURRENT_SOURCE_DIR}")
abstract_require (INTERFACE api_common)

abstract_register_implementation (NAME kan PARTS memory_profiler_kan)
abstract_register_implementation (NAME none PARTS memory_profiler_none)

option (KAN_MEMORY_PROFILER_ENABLE_BY_DEFAULT "Whether to enable memory profiler by default in this configuration." ON)
if (KAN_MEMORY_PROFILER_ENABLE_BY_DEFAULT)
    abstract_alias_implementation (ALIAS default SOURCE kan)
else ()
    abstract_alias_implementation (ALIAS default SOURCE none)
endif ()

c_interface_scanner_setup (GLOB "*.h")
