register_concrete (reflection_helpers)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

concrete_require (SCOPE PUBLIC ABSTRACT reflection)
concrete_require (SCOPE PRIVATE ABSTRACT error)
setup_core_preprocessing ()
