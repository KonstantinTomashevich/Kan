register_concrete (universe_resource_provider_kan)
concrete_include (PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

concrete_require (
        SCOPE PRIVATE
        ABSTRACT log platform universe_resource_provider virtual_file_system
        CONCRETE_INTERFACE
        context_reflection_system context_virtual_file_system resource_index serialization
        universe_resource_provider_reflection
        THIRD_PARTY qsort)

c_interface_scanner_setup (GLOB "*.c")
reflection_generator_setup (GLOB "*.c")
register_unit_reflection ()

reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}")

concrete_implements_abstract (universe_resource_provider)

set (KAN_UNIVERSE_RESOURCE_PROVIDER_TEMPORARY_CHUNK_SIZE "4096" CACHE STRING
        "Chunk size for resource provider temporary allocator.")

set (KAN_UNIVERSE_RESOURCE_PROVIDER_SCAN_DIRECTORY_CAPACITY "8" CACHE STRING
        "Initial capacity for scan directory stack for scanning for resources.")

set (KAN_UNIVERSE_RESOURCE_PROVIDER_STRING_REGISTRIES "4" CACHE STRING
        "Initial capacity for loaded string registries array.")

set (KAN_UNIVERSE_RESOURCE_PROVIDER_TPL_CHUNK "1024" CACHE STRING
        "Size of chunk for third party loading (loading of third party resources).")

concrete_compile_definitions (
        PRIVATE
        KAN_UNIVERSE_RESOURCE_PROVIDER_TEMPORARY_CHUNK_SIZE=${KAN_UNIVERSE_RESOURCE_PROVIDER_TEMPORARY_CHUNK_SIZE}
        KAN_UNIVERSE_RESOURCE_PROVIDER_SCAN_DIRECTORY_CAPACITY=${KAN_UNIVERSE_RESOURCE_PROVIDER_SCAN_DIRECTORY_CAPACITY}
        KAN_UNIVERSE_RESOURCE_PROVIDER_STRING_REGISTRIES=${KAN_UNIVERSE_RESOURCE_PROVIDER_STRING_REGISTRIES}
        KAN_UNIVERSE_RESOURCE_PROVIDER_TPL_CHUNK=${KAN_UNIVERSE_RESOURCE_PROVIDER_TPL_CHUNK})