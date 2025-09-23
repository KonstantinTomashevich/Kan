register_concrete (universe_transform)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources (GLOB "*.c")

concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE inline_math universe universe_object)
setup_reflected_preprocessing ()
