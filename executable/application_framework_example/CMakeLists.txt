add_subdirectory (unit)

register_application (application_framework_example)
application_core_include (
        ABSTRACT
        cpu_dispatch=kan cpu_profiler=none error=sdl file_system=platform_default file_system_watcher=user_level
        hash=djb2 log=kan memory=kan memory_profiler=none platform=sdl reflection=kan repository=kan stream=kan
        threading=sdl virtual_file_system=kan workflow=kan
        CONCRETE
        application_framework container context context_application_system context_plugin_system
        context_reflection_system context_update_system context_virtual_file_system resource_index readable_data
        serialization testing universe)

application_core_set_configuration ("${CMAKE_CURRENT_SOURCE_DIR}/configuration/core.rd")
application_core_use_plugin_group (base)

register_application_plugin (NAME base GROUP base)
application_plugin_include (ABSTRACT universe_resource_provider=kan)

register_application_plugin (NAME logic GROUP test)
application_plugin_include (ABSTRACT CONCRETE application_framework_example_logic)

register_application_program (test)
application_program_set_configuration ("${CMAKE_CURRENT_SOURCE_DIR}/configuration/program_test.rd")
application_program_use_plugin_group (test)

application_generate ()
