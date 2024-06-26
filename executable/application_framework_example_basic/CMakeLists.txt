add_subdirectory (unit)

# We need to shorten generated file names under Windows.
if (WIN32)
    register_application (afe_basic)
else ()
    register_application (application_framework_example_basic)
endif ()

application_core_include (
        ABSTRACT
        cpu_dispatch=kan cpu_profiler=none error=sdl file_system=platform_default file_system_watcher=user_level
        hash=djb2 log=kan memory=kan memory_profiler=none platform=sdl reflection=kan repository=kan stream=kan
        threading=sdl virtual_file_system=kan workflow=kan
        CONCRETE
        application_framework container context context_application_system context_plugin_system
        context_reflection_system context_universe_world_definition_system context_update_system
        context_virtual_file_system readable_data resource_index resource_pipeline serialization universe)

application_core_set_configuration ("${CMAKE_CURRENT_SOURCE_DIR}/configuration/core.rd")
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
application_program_use_as_test_in_development_mode (ARGUMENTS --test PROPERTIES TIMEOUT 4)
application_program_use_as_test_in_packaged_mode (ARGUMENTS --test PROPERTIES TIMEOUT 4)

register_application_variant (test)
application_variant_add_program (test)
application_variant_add_environment_tag (packaged)

application_generate ()
