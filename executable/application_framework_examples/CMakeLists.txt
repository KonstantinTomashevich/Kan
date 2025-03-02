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
        application_framework_examples_icon_resource application_framework_examples_platform_configuration
        render_pipeline_language resource_material resource_texture universe_object universe_render_foundation
        universe_single_pipeline_scheduler universe_time)

# Plugin with import-only logic.

register_application_plugin (NAME import GROUP import)
application_plugin_include (
        ABSTRACT image=stb
        CONCRETE application_framework_examples_icon_import resource_texture_import)

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

# Compilation basic example.

register_application_plugin (NAME example_compilation_basic GROUP example_compilation_basic)
application_plugin_include (ABSTRACT CONCRETE application_framework_examples_compilation_basic)

register_application_program (example_compilation_basic)
application_program_set_configuration ("${CMAKE_CURRENT_SOURCE_DIR}/configuration/example_compilation_basic.rd")
application_program_use_plugin_group (example_compilation_basic)
application_program_use_as_test_in_development_mode (ARGUMENTS --test PROPERTIES TIMEOUT 10)
application_program_use_as_test_in_packaged_mode (ARGUMENTS --test PROPERTIES TIMEOUT 10)

register_application_variant (example_compilation_basic)
application_variant_add_program (example_compilation_basic)
application_variant_add_environment_tag (packaged)

# Compilation byproduct example.

register_application_plugin (NAME example_compilation_byproduct GROUP example_compilation_byproduct)
application_plugin_include (ABSTRACT CONCRETE application_framework_examples_compilation_byproduct)

register_application_program (example_compilation_byproduct)
application_program_set_configuration ("${CMAKE_CURRENT_SOURCE_DIR}/configuration/example_compilation_byproduct.rd")
application_program_use_plugin_group (example_compilation_byproduct)
application_program_use_as_test_in_development_mode (ARGUMENTS --test PROPERTIES TIMEOUT 10)
application_program_use_as_test_in_packaged_mode (ARGUMENTS --test PROPERTIES TIMEOUT 10)

register_application_variant (example_compilation_byproduct)
application_variant_add_program (example_compilation_byproduct)
application_variant_add_environment_tag (packaged)

# Compilation state example.

register_application_plugin (NAME example_compilation_state GROUP example_compilation_state)
application_plugin_include (ABSTRACT CONCRETE application_framework_examples_compilation_state)

register_application_program (example_compilation_state)
application_program_set_configuration ("${CMAKE_CURRENT_SOURCE_DIR}/configuration/example_compilation_state.rd")
application_program_use_plugin_group (example_compilation_state)
application_program_use_as_test_in_development_mode (ARGUMENTS --test PROPERTIES TIMEOUT 10)
application_program_use_as_test_in_packaged_mode (ARGUMENTS --test PROPERTIES TIMEOUT 10)

register_application_variant (example_compilation_state)
application_variant_add_program (example_compilation_state)
application_variant_add_environment_tag (packaged)

# Import example.

register_application_plugin (NAME example_import GROUP example_import)
application_plugin_include (ABSTRACT CONCRETE application_framework_examples_import)

register_application_program (example_import)
application_program_set_configuration ("${CMAKE_CURRENT_SOURCE_DIR}/configuration/example_import.rd")
application_program_use_plugin_group (example_import)
application_program_use_as_test_in_development_mode (ARGUMENTS --test PROPERTIES TIMEOUT 20)
application_program_use_as_test_in_packaged_mode (ARGUMENTS --test PROPERTIES TIMEOUT 10)

register_application_variant (example_import)
application_variant_add_program (example_import)
application_variant_add_environment_tag (packaged)

# Test render foundation temporary examples.
register_application_plugin (NAME example_test_render_graph GROUP example_test_render_graph)
application_plugin_include (ABSTRACT CONCRETE application_framework_examples_test_render_graph)

register_application_program (example_test_render_graph)
application_program_set_configuration ("${CMAKE_CURRENT_SOURCE_DIR}/configuration/example_test_render_graph.rd")
application_program_use_plugin_group (example_test_render_graph)
application_program_use_as_test_in_development_mode (ARGUMENTS --test PROPERTIES TIMEOUT 20 LABELS GRAPHICS)
application_program_use_as_test_in_packaged_mode (ARGUMENTS --test PROPERTIES TIMEOUT 10 LABELS GRAPHICS)

register_application_variant (example_test_render_graph)
application_variant_add_program (example_test_render_graph)
application_variant_add_environment_tag (packaged)

register_application_plugin (NAME example_test_render_texture GROUP example_test_render_texture)
application_plugin_include (ABSTRACT CONCRETE application_framework_examples_test_render_texture)

register_application_program (example_test_render_texture)
application_program_set_configuration ("${CMAKE_CURRENT_SOURCE_DIR}/configuration/example_test_render_texture.rd")
application_program_use_plugin_group (example_test_render_texture)
application_program_use_as_test_in_development_mode (ARGUMENTS --test PROPERTIES TIMEOUT 20 LABELS GRAPHICS)
application_program_use_as_test_in_packaged_mode (ARGUMENTS --test PROPERTIES TIMEOUT 10 LABELS GRAPHICS)

register_application_variant (example_test_render_texture)
application_variant_add_program (example_test_render_texture)
application_variant_add_environment_tag (packaged)

register_application_plugin (NAME example_test_render_material GROUP example_test_render_material)
application_plugin_include (ABSTRACT CONCRETE application_framework_examples_test_render_material)

register_application_program (example_test_render_material)
application_program_set_configuration ("${CMAKE_CURRENT_SOURCE_DIR}/configuration/example_test_render_material.rd")
application_program_use_plugin_group (example_test_render_material)
application_program_use_as_test_in_development_mode (ARGUMENTS --test PROPERTIES TIMEOUT 20000 LABELS GRAPHICS)
application_program_use_as_test_in_packaged_mode (ARGUMENTS --test PROPERTIES TIMEOUT 10 LABELS GRAPHICS)

register_application_variant (example_test_render_material)
application_variant_add_program (example_test_render_material)
application_variant_add_environment_tag (packaged)

application_generate ()

application_get_resource_importer_target_name (RESOURCE_IMPORTER)
application_get_resource_project_path (RESOURCE_PROJECT)

add_custom_target (
        application_framework_examples_import_import_icons
        COMMAND ${RESOURCE_IMPORTER} "${RESOURCE_PROJECT}" "--rule" "import_icons"
        COMMENT "Run resource import for application_framework_examples_import.")

add_custom_target (
        application_framework_examples_test_render_texture_import
        COMMAND ${RESOURCE_IMPORTER} "${RESOURCE_PROJECT}" "--rule" "import_render_test_textures"
        COMMENT "Run resource import for application_framework_examples_test_render_texture.")

add_custom_target (
        application_framework_examples_test_render_material_import
        COMMAND ${RESOURCE_IMPORTER} "${RESOURCE_PROJECT}" "--rule" "import_render_test_textures_for_material"
        COMMENT "Run resource import for application_framework_examples_test_render_material.")

# Make import always required as dependency of resource build.
# IT IS NOT RECOMMENDED. It is just for the sake of testing.
# Variant naming is not public in application framework as variants shouldn't be addressed like that.
# But here we're doing it for testing in order to always execute resource importer as part of tests here.
add_dependencies (
        application_framework_examples_variant_example_import_build_resources
        application_framework_examples_import_import_icons)
