register_concrete (application_framework_resource_build)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

concrete_require (
        SCOPE PUBLIC
        ABSTRACT file_system precise_time stream virtual_file_system
        CONCRETE_INTERFACE
        context context_plugin_system context_reflection_system resource_pipeline_build serialization)
setup_core_preprocessing ()
