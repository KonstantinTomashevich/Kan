register_abstract (precise_time)
abstract_include ("${CMAKE_CURRENT_SOURCE_DIR}")
abstract_require (INTERFACE api_common)
abstract_register_implementation (NAME sdl PARTS precise_time_sdl)
