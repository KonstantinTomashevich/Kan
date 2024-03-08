add_subdirectory (unit)
register_executable (c_interface_scanner)
executable_include (
        SCOPE PUBLIC
        ABSTRACT error=sdl file_system=stdcxx hash=djb2 log=kan memory=kan memory_profiler=none stream=kan threading=sdl
        CONCRETE c_interface c_interface_scanner_main container)
executable_verify ()
executable_copy_linked_artefacts ()
