register_abstract (hash)
abstract_include ("${CMAKE_CURRENT_SOURCE_DIR}")
abstract_require (INTERFACE api_common)
abstract_register_implementation (NAME djb2 PARTS hash_djb2)
