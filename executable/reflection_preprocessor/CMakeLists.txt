add_subdirectory (unit)
register_executable (reflection_preprocessor)
executable_include (
        SCOPE PUBLIC
        ABSTRACT
        error=sdl file_system=platform_default hash=djb2 log=kan memory=kan memory_profiler=default precise_time=sdl
        stream=kan threading=sdl
        CONCRETE container reflection_preprocessor_main)
executable_verify ()
executable_copy_linked_artefacts ()
