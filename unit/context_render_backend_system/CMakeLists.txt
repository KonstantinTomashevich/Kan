register_abstract (context_render_backend_system)
abstract_include ("${CMAKE_CURRENT_SOURCE_DIR}")
abstract_require (CONCRETE_INTERFACE container context context_application_system INTERFACE api_common platform)

if (Vulkan_FOUND)
    abstract_register_implementation (
            NAME vulkan
            PARTS context_render_backend_system_vulkan context_render_backend_system_reflection)
endif ()

c_interface_scanner_setup (GLOB "*.h")
reflection_generator_setup (GLOB "*.h")
reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)

register_concrete (context_render_backend_system_reflection)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}")
concrete_require (SCOPE PUBLIC ABSTRACT context_render_backend_system)
concrete_require (SCOPE PRIVATE ABSTRACT reflection)
register_unit_reflection_with_name (context_render_backend_system)
