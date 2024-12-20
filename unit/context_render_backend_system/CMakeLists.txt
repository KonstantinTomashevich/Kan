register_abstract (context_render_backend_system)
abstract_include ("${CMAKE_CURRENT_SOURCE_DIR}")
abstract_require (CONCRETE_INTERFACE container context context_application_system INTERFACE api_common platform)

if (Vulkan_FOUND)
    abstract_register_implementation (
            NAME vulkan
            PARTS context_render_backend_system_vulkan context_render_backend_system_reflection)
endif ()

create_accompanying_reflection_unit (
        FOR_ABSTRACT context_render_backend_system
        NAME context_render_backend_system_reflection
        GLOB "*.h")
