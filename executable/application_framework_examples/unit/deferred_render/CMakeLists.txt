register_concrete (application_framework_examples_deferred_render)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources (GLOB "*.c")

concrete_require (
        SCOPE PUBLIC
        ABSTRACT file_system image log precise_time
        CONCRETE_INTERFACE
        application_framework context_application_system resource_test_expectation universe universe_render_foundation
        universe_resource_provider universe_time)

register_application_resource_directory ("${CMAKE_CURRENT_SOURCE_DIR}/resources")
setup_reflected_preprocessing ()
