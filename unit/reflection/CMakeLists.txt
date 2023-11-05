register_abstract (reflection)
abstract_include ("${CMAKE_CURRENT_SOURCE_DIR}")
abstract_require (CONCRETE_INTERFACE container INTERFACE api_common)
abstract_register_implementation (NAME kan PARTS reflection_kan)
