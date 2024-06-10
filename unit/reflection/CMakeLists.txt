register_abstract (reflection)
abstract_include ("${CMAKE_CURRENT_SOURCE_DIR}")
# error abstract unit is required for generated reflection.
abstract_require (ABSTRACT error CONCRETE_INTERFACE container INTERFACE api_common)
abstract_register_implementation (NAME kan PARTS reflection_kan)
c_interface_scanner_setup (GLOB "*.h")
