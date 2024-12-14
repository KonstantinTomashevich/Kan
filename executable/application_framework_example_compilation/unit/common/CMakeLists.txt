register_concrete (application_framework_example_compilation_common)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
universe_concrete_setup_sources (GLOB "*.c")
concrete_require (SCOPE PUBLIC ABSTRACT reflection)
