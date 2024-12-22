register_concrete (application_framework_example_compilation_state)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources (GLOB "*.c")

concrete_require (
        SCOPE PUBLIC
        ABSTRACT log universe_resource_provider
        CONCRETE_INTERFACE application_framework context_application_system universe
        THIRD_PARTY qsort)

register_application_resource_directory ("${CMAKE_CURRENT_SOURCE_DIR}/resources")
universe_concrete_preprocessing_queue ()
