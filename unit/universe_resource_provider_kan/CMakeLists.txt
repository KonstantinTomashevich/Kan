register_concrete (universe_resource_provider_kan)
concrete_include (PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
universe_concrete_setup_sources (GLOB "*.c")
concrete_implements_abstract (universe_resource_provider)

concrete_require (
        SCOPE PRIVATE
        ABSTRACT
        context_hot_reload_coordination_system log platform precise_time universe_resource_provider virtual_file_system
        CONCRETE_INTERFACE
        context_reflection_system context_resource_pipeline_system context_virtual_file_system resource_index
        serialization universe_resource_provider_reflection
        THIRD_PARTY qsort)

set (KAN_UNIVERSE_RESOURCE_PROVIDER_TEMPORARY_CHUNK_SIZE "4096" CACHE STRING
        "Chunk size for resource provider temporary allocator.")

set (KAN_UNIVERSE_RESOURCE_PROVIDER_SCAN_DIRECTORY_CAPACITY "8" CACHE STRING
        "Initial capacity for scan directory stack for scanning for resources.")

set (KAN_UNIVERSE_RESOURCE_PROVIDER_STRING_REGISTRIES "4" CACHE STRING
        "Initial capacity for loaded string registries array.")

set (KAN_UNIVERSE_RESOURCE_PROVIDER_READ_BUFFER "16384" CACHE STRING
        "Size of buffer for reading files resources except third party ones.")

set (KAN_UNIVERSE_RESOURCE_PROVIDER_TPL_CHUNK "16384" CACHE STRING
        "Size of chunk for third party loading (loading of third party resources).")

set (KAN_UNIVERSE_RESOURCE_PROVIDER_RC_INITIAL_SIZE "8" CACHE STRING
        "Initial size of an arrays used for various operations for runtime compilation.")

set (KAN_UNIVERSE_RESOURCE_PROVIDER_RB_MAX_NAME_LENGTH "256" CACHE STRING
        "Max length of runtime byproduct name. Used for format buffer creation.")

concrete_compile_definitions (
        PRIVATE
        KAN_UNIVERSE_RESOURCE_PROVIDER_TEMPORARY_CHUNK_SIZE=${KAN_UNIVERSE_RESOURCE_PROVIDER_TEMPORARY_CHUNK_SIZE}
        KAN_UNIVERSE_RESOURCE_PROVIDER_SCAN_DIRECTORY_CAPACITY=${KAN_UNIVERSE_RESOURCE_PROVIDER_SCAN_DIRECTORY_CAPACITY}
        KAN_UNIVERSE_RESOURCE_PROVIDER_STRING_REGISTRIES=${KAN_UNIVERSE_RESOURCE_PROVIDER_STRING_REGISTRIES}
        KAN_UNIVERSE_RESOURCE_PROVIDER_READ_BUFFER=${KAN_UNIVERSE_RESOURCE_PROVIDER_READ_BUFFER}
        KAN_UNIVERSE_RESOURCE_PROVIDER_TPL_CHUNK=${KAN_UNIVERSE_RESOURCE_PROVIDER_TPL_CHUNK}
        KAN_UNIVERSE_RESOURCE_PROVIDER_RC_INITIAL_SIZE=${KAN_UNIVERSE_RESOURCE_PROVIDER_RC_INITIAL_SIZE}
        KAN_UNIVERSE_RESOURCE_PROVIDER_RB_MAX_NAME_LENGTH=${KAN_UNIVERSE_RESOURCE_PROVIDER_RB_MAX_NAME_LENGTH})
