register_concrete (application_framework_resource_tool)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC ABSTRACT memory reflection CONCRETE_INTERFACE container context)
concrete_require (
        SCOPE PRIVATE
        ABSTRACT file_system log
        CONCRETE_INTERFACE context_plugin_system context_reflection_system context_resource_pipeline_system
        serialization)

concrete_preprocessing_queue_step_preprocess ()
reflection_preprocessor_setup_step (GLOB "*.h")
register_unit_reflection ()

set (KAN_RESOURCE_PROJECT_IO_BUFFER "4096" CACHE STRING "Size of a buffer for buffered reading of resource projects.")

concrete_compile_definitions (
        PRIVATE
        KAN_RESOURCE_PROJECT_IO_BUFFER=${KAN_RESOURCE_PROJECT_IO_BUFFER})
