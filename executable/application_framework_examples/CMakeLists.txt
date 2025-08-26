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
        context_reflection_system context_universe_world_definition_system context_update_system
        context_virtual_file_system inline_math readable_data reflection_helpers resource_pipeline serialization 
        universe)

application_core_set_configuration ("${CMAKE_CURRENT_SOURCE_DIR}/configuration/core.rd")
application_core_set_platform_configuration_directory ("${CMAKE_CURRENT_SOURCE_DIR}/resource_platform_configuration")
application_core_use_plugin_group (base)

application_set_world_directory ("${CMAKE_CURRENT_SOURCE_DIR}/world")
application_add_development_environment_tag (development)

register_application_plugin (NAME base GROUP base)
application_plugin_include (
        CONCRETE
        render_pipeline_language 
        # TODO: Old modules that are preserved as comments for easier refactor later.
        # resource_material resource_texture universe_object universe_render_foundation
        universe_resource_provider universe_single_pipeline_scheduler universe_time)

# Normally, base plugins should not include import-only things like image parsing.
# However, we use images for visual test results verification, therefore we need them everywhere.
application_plugin_include (ABSTRACT image=stb)

# TODO: Empty right now, but will be populated back when resources are refactored.
# Plugin with resource build only logic.
#
# register_application_plugin (NAME resource_build GROUP resource_build)
# application_plugin_include (CONCRETE resource_texture_import)

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

# TODO: Temporary disabled while we're refactoring everything to new resource build and provision logic.
if (0)

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

endif ()

application_generate ()
