register_abstract (memory)
abstract_include ("${CMAKE_CURRENT_SOURCE_DIR}")
abstract_require (ABSTRACT memory_profiler INTERFACE api_common)
abstract_register_implementation (NAME kan PARTS memory_kan)
