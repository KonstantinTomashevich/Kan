register_abstract (cpu_profiler)
abstract_include ("${CMAKE_CURRENT_SOURCE_DIR}")
abstract_require (INTERFACE api_common)

abstract_register_implementation (NAME none PARTS cpu_profiler_none)
abstract_register_implementation (NAME tracy PARTS cpu_profiler_tracy)
c_interface_scanner_setup (GLOB "*.h")