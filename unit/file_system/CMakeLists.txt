register_abstract (file_system)
abstract_include ("${CMAKE_CURRENT_SOURCE_DIR}")
abstract_require (INTERFACE api_common stream)
abstract_register_implementation (NAME stdcxx PARTS file_system_stdcxx)
