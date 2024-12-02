add_subdirectory (unit)
register_executable (reflection_generator)
executable_include (
        SCOPE PUBLIC
        ABSTRACT
        error=sdl file_system=platform_default hash=djb2 log=kan memory=kan memory_profiler=default precise_time=sdl
        stream=kan threading=sdl
        CONCRETE c_interface container reflection_generator_main)
executable_verify ()
executable_copy_linked_artefacts ()
