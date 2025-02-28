register_abstract (cpu_dispatch)
abstract_include ("${CMAKE_CURRENT_SOURCE_DIR}")
abstract_require (ABSTRACT platform CONCRETE_INTERFACE container INTERFACE api_common cpu_profiler error)
abstract_register_implementation (NAME kan PARTS cpu_dispatch_kan)
