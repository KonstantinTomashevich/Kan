add_subdirectory (unit)
register_executable (application_framework_resource_builder)
executable_include (
        SCOPE PUBLIC
        ABSTRACT
        cpu_dispatch=kan cpu_profiler=none error=sdl file_system=platform_default file_system_watcher=user_level
        hash=djb2 log=kan memory=kan memory_profiler=none platform=sdl reflection=kan stream=kan threading=sdl
        virtual_file_system=kan
        CONCRETE
        application_framework_resource_builder_main container context context_plugin_system context_reflection_system
        context_update_system readable_data resource_index resource_pipeline serialization)

generate_artefact_context_data ()
generate_artefact_reflection_data ()
executable_verify ()
executable_copy_linked_artefacts ()
