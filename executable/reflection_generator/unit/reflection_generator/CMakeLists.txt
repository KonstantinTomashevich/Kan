register_concrete (reflection_generator_main)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC ABSTRACT error file_system memory CONCRETE_INTERFACE c_interface container)
