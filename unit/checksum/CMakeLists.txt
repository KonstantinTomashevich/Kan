register_abstract (checksum)
abstract_include ("${CMAKE_CURRENT_SOURCE_DIR}")
abstract_require (INTERFACE api_common)
abstract_register_implementation (NAME xxhash PARTS checksum_xxhash)
