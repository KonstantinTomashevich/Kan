register_abstract (application_framework_static_launcher)
abstract_include ("${CMAKE_CURRENT_SOURCE_DIR}")
abstract_require (INTERFACE api_common)
abstract_register_implementation (NAME sdl PARTS application_framework_static_launcher_sdl)
