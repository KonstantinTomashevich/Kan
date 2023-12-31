register_abstract (workflow)
abstract_include ("${CMAKE_CURRENT_SOURCE_DIR}")
abstract_require (ABSTRACT cpu_dispatch memory_profiler INTERFACE api_common)

abstract_register_implementation (NAME kan PARTS workflow_kan)
c_interface_scanner_setup (GLOB "*.h")
