register_concrete (universe_time)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
universe_concrete_setup_sources (GLOB "*.c")
concrete_require (SCOPE PRIVATE CONCRETE_INTERFACE universe)
