register_abstract (file_system_watcher)
abstract_include ("${CMAKE_CURRENT_SOURCE_DIR}")
abstract_require (INTERFACE api_common file_system)
abstract_register_implementation (NAME user_level PARTS file_system_watcher_user_level)
