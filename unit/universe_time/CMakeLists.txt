register_concrete (universe_time)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources (GLOB "*.c")
concrete_require (SCOPE PRIVATE CONCRETE_INTERFACE universe)
universe_concrete_preprocessing_queue ()
