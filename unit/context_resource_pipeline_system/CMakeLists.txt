register_concrete (context_resource_pipeline_system)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE context resource_pipeline)
concrete_require (
        SCOPE PRIVATE
        ABSTRACT context_hot_reload_coordination_system error file_system log memory precise_time
        CONCRETE_INTERFACE context_reflection_system context_update_system serialization)

register_context_system (NAME resource_pipeline_system_t)
concrete_preprocessing_queue_step_preprocess ()
reflection_preprocessor_setup_step (GLOB "*.h")
register_unit_reflection ()

set (KAN_RESOURCE_PIPELINE_SYSTEM_PC_INITIAL_SIZE "8" CACHE STRING
        "Initial size of loaded platform configuration array.")
set (KAN_RESOURCE_PIPELINE_SYSTEM_IO_BUFFER "4096" CACHE STRING
        "Size of a buffer for buffered reading of platform configurations.")

concrete_compile_definitions (
        PRIVATE
        KAN_RESOURCE_PIPELINE_SYSTEM_PC_INITIAL_SIZE=${KAN_RESOURCE_PIPELINE_SYSTEM_PC_INITIAL_SIZE}
        KAN_RESOURCE_PIPELINE_SYSTEM_IO_BUFFER=${KAN_RESOURCE_PIPELINE_SYSTEM_IO_BUFFER})
