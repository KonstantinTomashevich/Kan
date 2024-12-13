register_concrete (context_resource_pipeline_system)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE context resource_pipeline)
concrete_require (
        SCOPE PRIVATE
        ABSTRACT context_hot_reload_coordination_system error file_system log memory precise_time
        CONCRETE_INTERFACE context_reflection_system context_update_system serialization)

register_context_system (NAME resource_pipeline_system_t)
c_interface_scanner_setup (GLOB "*.h")
reflection_generator_setup (GLOB "*.h")
register_unit_reflection ()

reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}")

set (KAN_RESOURCE_PIPELINE_SYSTEM_PC_INITIAL_SIZE "8" CACHE STRING
        "Initial size of loaded platform configuration array.")
set (KAN_RESOURCE_PIPELINE_SYSTEM_IO_BUFFER "4096" CACHE STRING
        "Size of a buffer for buffered reading of platform configurations.")

concrete_compile_definitions (
        PRIVATE
        KAN_RESOURCE_PIPELINE_SYSTEM_PC_INITIAL_SIZE=${KAN_RESOURCE_PIPELINE_SYSTEM_PC_INITIAL_SIZE}
        KAN_RESOURCE_PIPELINE_SYSTEM_IO_BUFFER=${KAN_RESOURCE_PIPELINE_SYSTEM_IO_BUFFER})
