register_abstract (memory_profiler)
abstract_include ("${CMAKE_CURRENT_SOURCE_DIR}")
abstract_require (INTERFACE api_common)

abstract_register_implementation (NAME kan PARTS memory_profiler_kan)
abstract_register_implementation (NAME none PARTS memory_profiler_none)
c_interface_scanner_setup (GLOB "*.h")
