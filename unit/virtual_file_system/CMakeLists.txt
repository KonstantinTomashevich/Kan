register_abstract (virtual_file_system)
abstract_include ("${CMAKE_CURRENT_SOURCE_DIR}")
abstract_require (INTERFACE api_common file_system stream)
abstract_register_implementation (NAME kan PARTS virtual_file_system_kan)
