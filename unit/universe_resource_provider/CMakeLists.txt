register_concrete (universe_resource_provider)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources (GLOB "*.c")

concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE resource_pipeline_runtime serialization universe)
concrete_require (
        SCOPE PRIVATE
        ABSTRACT context_hot_reload_coordination_system log platform precise_time virtual_file_system
        CONCRETE_INTERFACE context_reflection_system context_virtual_file_system)

setup_reflected_preprocessing ()

set (KAN_UNIVERSE_RESOURCE_PROVIDER_TYPE_HEADER_BUFFER "256" CACHE STRING 
        "Size of an IO buffer for reading type headers.")
set (KAN_UNIVERSE_RESOURCE_PROVIDER_IO_BUFFER "16384" CACHE STRING 
        "Size of an IO buffer for reading actual resources.")
set (KAN_UNIVERSE_RESOURCE_PROVIDER_TEMPORARY_CHUNK_SIZE "4096" CACHE STRING
        "Chunk size for resource provider temporary allocator.")

concrete_compile_definitions (
        PRIVATE
        KAN_UNIVERSE_RESOURCE_PROVIDER_TYPE_HEADER_BUFFER=${KAN_UNIVERSE_RESOURCE_PROVIDER_TYPE_HEADER_BUFFER}
        KAN_UNIVERSE_RESOURCE_PROVIDER_IO_BUFFER=${KAN_UNIVERSE_RESOURCE_PROVIDER_IO_BUFFER}
        KAN_UNIVERSE_RESOURCE_PROVIDER_TEMPORARY_CHUNK_SIZE=${KAN_UNIVERSE_RESOURCE_PROVIDER_TEMPORARY_CHUNK_SIZE})
