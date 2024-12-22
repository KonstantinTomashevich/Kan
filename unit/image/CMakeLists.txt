register_abstract (image)
abstract_include ("${CMAKE_CURRENT_SOURCE_DIR}")
abstract_require (ABSTRACT stream INTERFACE api_common)
abstract_register_implementation (NAME stb PARTS image_stb)
