add_subdirectory (unit)

register_application (application_framework_example_basic)
application_core_include (
        ABSTRACT
        context_hot_reload_coordination_system=default cpu_dispatch=kan cpu_profiler=default error=sdl
        file_system=platform_default file_system_watcher=user_level hash=djb2 log=kan memory=kan memory_profiler=default
        platform=sdl precise_time=sdl reflection=kan repository=kan stream=kan threading=sdl virtual_file_system=kan
        workflow=kan
        CONCRETE
        application_framework container context context_application_system context_plugin_system
        context_reflection_system context_resource_pipeline_system context_universe_world_definition_system
        context_update_system context_virtual_file_system readable_data reflection_helpers resource_index
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
        CONCRETE universe_single_pipeline_scheduler universe_time)

register_application_plugin (NAME logic GROUP test)
application_plugin_include (ABSTRACT CONCRETE application_framework_example_basic_logic)

register_application_program (test)
application_program_set_configuration ("${CMAKE_CURRENT_SOURCE_DIR}/configuration/program_test.rd")
application_program_use_plugin_group (test)
application_program_use_as_test_in_development_mode (ARGUMENTS --test PROPERTIES TIMEOUT 10)
application_program_use_as_test_in_packaged_mode (ARGUMENTS --test PROPERTIES TIMEOUT 10)

register_application_variant (test)
application_variant_add_program (test)
application_variant_add_environment_tag (packaged)

application_generate ()
