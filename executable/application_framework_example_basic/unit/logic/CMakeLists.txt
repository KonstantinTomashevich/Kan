register_concrete (application_framework_example_basic_logic)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
universe_concrete_setup_sources (GLOB "*.c")

concrete_require (
        SCOPE PUBLIC
        ABSTRACT log universe_resource_provider
        CONCRETE_INTERFACE application_framework context_application_system universe universe_time
        INTERFACE application_framework_example_basic_resources)
