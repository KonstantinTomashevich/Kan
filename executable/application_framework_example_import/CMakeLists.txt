add_subdirectory (unit)

register_application (application_framework_example_import)
application_core_include (
        ABSTRACT
        cpu_dispatch=kan cpu_profiler=default error=sdl file_system=platform_default file_system_watcher=user_level
        hash=djb2 log=kan memory=kan memory_profiler=default platform=sdl precise_time=sdl reflection=kan repository=kan
        stream=kan threading=sdl virtual_file_system=kan workflow=kan
        CONCRETE
        application_framework container context context_application_system context_plugin_system
        context_reflection_system context_universe_world_definition_system context_update_system
        context_virtual_file_system readable_data reflection_helpers resource_index resource_pipeline serialization
        universe)

application_core_set_configuration ("${CMAKE_CURRENT_SOURCE_DIR}/configuration/core.rd")
application_core_set_resource_platform_configuration (
        "${CMAKE_CURRENT_SOURCE_DIR}/configuration/default_platform_configuration.rd")
application_core_use_plugin_group (base)

application_set_world_directory ("${CMAKE_CURRENT_SOURCE_DIR}/world")
application_add_development_environment_tag (development)

register_application_plugin (NAME base GROUP base)
application_plugin_include (
        ABSTRACT universe_resource_provider=kan
        CONCRETE application_framework_example_import_icon universe_single_pipeline_scheduler universe_time)

register_application_plugin (NAME logic GROUP test)
application_plugin_include (CONCRETE application_framework_example_import_logic)

register_application_plugin (NAME import GROUP import)
application_plugin_include (ABSTRACT image=stb CONCRETE application_framework_example_import_icon_import)

register_application_program (test)
application_program_set_configuration ("${CMAKE_CURRENT_SOURCE_DIR}/configuration/program_test.rd")
application_program_use_plugin_group (test)
# These tests create windows and spamming windows is both not good visually and might brick some environments.
application_program_use_as_test_in_development_mode (ARGUMENTS --test PROPERTIES TIMEOUT 10 RUN_SERIAL ON)
application_program_use_as_test_in_packaged_mode (ARGUMENTS --test PROPERTIES TIMEOUT 10 RUN_SERIAL ON)

register_application_variant (test)
application_variant_add_program (test)
application_variant_add_environment_tag (packaged)

application_generate ()

# Make import always required as dependency of resource build.
# IT IS NOT RECOMMENDED. It is just for the sake of testing.

application_get_resource_importer_target_name (RESOURCE_IMPORTER)
application_get_resource_project_path (RESOURCE_PROJECT)

add_custom_target (
        application_framework_example_import_import_icons
        COMMAND ${RESOURCE_IMPORTER} "${RESOURCE_PROJECT}" "--rule" "import_icons"
        COMMENT "Run resource import for application_framework_example_import.")

# Variant naming is not public in application framework as variants shouldn't be addressed like that.
# But here we're doing it for testing in order to always execute resource importer as part of tests here.
add_dependencies (
        application_framework_example_import_variant_test_build_resources
        application_framework_example_import_import_icons)
