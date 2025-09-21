register_concrete (application_framework_examples_basic)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources (GLOB "*.c")

concrete_require (
        SCOPE PUBLIC
        ABSTRACT log
        CONCRETE_INTERFACE 
        application_framework context_application_system universe universe_resource_provider universe_time)

register_application_resource_directory ("${CMAKE_CURRENT_SOURCE_DIR}/resources")
setup_reflected_preprocessing ()
