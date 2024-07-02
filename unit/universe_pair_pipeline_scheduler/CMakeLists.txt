register_concrete (universe_pair_pipeline_scheduler)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
universe_concrete_setup_sources (GLOB "*.c")
concrete_require (SCOPE PRIVATE ABSTRACT platform CONCRETE_INTERFACE universe universe_time)
