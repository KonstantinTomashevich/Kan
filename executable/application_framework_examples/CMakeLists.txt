add_subdirectory (unit)

register_application (application_framework_examples)
application_core_include (
        ABSTRACT
        context_hot_reload_coordination_system=default context_render_backend_system=vulkan cpu_dispatch=kan
        cpu_profiler=default error=sdl file_system=platform_default file_system_watcher=user_level hash=djb2 log=kan
        memory=kan memory_profiler=default platform=sdl precise_time=sdl reflection=kan repository=kan stream=kan
        threading=sdl virtual_file_system=kan workflow=kan
        CONCRETE
        application_framework container context context_application_system context_plugin_system
        context_reflection_system context_resource_pipeline_system context_universe_world_definition_system
        context_update_system context_virtual_file_system inline_math readable_data reflection_helpers resource_index
        resource_pipeline serialization universe)

application_core_set_configuration ("${CMAKE_CURRENT_SOURCE_DIR}/configuration/core.rd")
application_core_set_resource_platform_configuration (
        "${CMAKE_CURRENT_SOURCE_DIR}/configuration/default_platform_configuration.rd")
application_core_use_plugin_group (base)

application_set_world_directory ("${CMAKE_CURRENT_SOURCE_DIR}/world")
application_add_development_environment_tag (development)

register_application_plugin (NAME base GROUP base)
application_plugin_include (
        ABSTRACT universe_resource_provider=kan
        CONCRETE
        render_pipeline_language resource_material resource_texture universe_object universe_render_foundation
        universe_single_pipeline_scheduler universe_time)

# Normally, base plugins should not include import-only things like image parsing.
# However, we use images for visual test results verification, therefore we need them everywhere.
application_plugin_include (ABSTRACT image=stb)

# Plugin with import-only logic.

register_application_plugin (NAME import GROUP import)
application_plugin_include (CONCRETE resource_texture_import)

# Basic example.

register_application_plugin (NAME example_basic GROUP example_basic)
application_plugin_include (ABSTRACT CONCRETE application_framework_examples_basic)

register_application_program (example_basic)
application_program_set_configuration ("${CMAKE_CURRENT_SOURCE_DIR}/configuration/example_basic.rd")
application_program_use_plugin_group (example_basic)
application_program_use_as_test_in_development_mode (ARGUMENTS --test PROPERTIES TIMEOUT 20)
application_program_use_as_test_in_packaged_mode (ARGUMENTS --test PROPERTIES TIMEOUT 10)

register_application_variant (example_basic)
application_variant_add_program (example_basic)
application_variant_add_environment_tag (packaged)

# Greatly simplified deferred render example on top of render foundation unit.
register_application_plugin (NAME example_deferred_render GROUP example_deferred_render)
application_plugin_include (ABSTRACT CONCRETE application_framework_examples_deferred_render)

register_application_program (example_deferred_render)
application_program_set_configuration ("${CMAKE_CURRENT_SOURCE_DIR}/configuration/example_deferred_render.rd")
application_program_use_plugin_group (example_deferred_render)
application_program_use_as_test_in_development_mode (ARGUMENTS --test PROPERTIES TIMEOUT 20 LABELS GRAPHICS)
application_program_use_as_test_in_packaged_mode (ARGUMENTS --test PROPERTIES TIMEOUT 10 LABELS GRAPHICS)

register_application_variant (example_deferred_render)
application_variant_add_program (example_deferred_render)
application_variant_add_environment_tag (packaged)

application_generate ()

application_get_resource_importer_target_name (RESOURCE_IMPORTER)
application_get_resource_project_path (RESOURCE_PROJECT)

add_custom_target (
        application_framework_examples_deferred_render_import
        COMMAND ${RESOURCE_IMPORTER} "${RESOURCE_PROJECT}" "--rule" "import_render_textures_for_deferred_render"
        COMMENT "Run resource import for application_framework_examples_deferred_render.")
