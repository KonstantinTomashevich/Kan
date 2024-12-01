register_abstract (cpu_profiler)
abstract_include ("${CMAKE_CURRENT_SOURCE_DIR}")
abstract_require (INTERFACE api_common)

abstract_register_implementation (NAME none PARTS cpu_profiler_none)
abstract_register_implementation (NAME tracy PARTS cpu_profiler_tracy)

option (KAN_CPU_PROFILER_ENABLE_BY_DEFAULT "Whether to enable cpu profiler by default in this configuration." ON)
if (KAN_CPU_PROFILER_ENABLE_BY_DEFAULT)
    abstract_alias_implementation (ALIAS default SOURCE tracy)
else ()
    abstract_alias_implementation (ALIAS default SOURCE none)
endif ()

c_interface_scanner_setup (GLOB "*.h")
