register_abstract (log)
abstract_include ("${CMAKE_CURRENT_SOURCE_DIR}")
abstract_require (CONCRETE_INTERFACE container INTERFACE api_common)
abstract_register_implementation (NAME kan PARTS log_kan)

set (KAN_LOG_DEFAULT_BUFFER_SIZE "1024" CACHE STRING "Default size of buffer for log formatting.")
abstract_compile_definitions (KAN_LOG_DEFAULT_BUFFER_SIZE=${KAN_LOG_DEFAULT_BUFFER_SIZE})
