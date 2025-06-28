register_concrete (test_serialization)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC ABSTRACT file_system CONCRETE_INTERFACE serialization testing)
setup_reflected_preprocessing ()

register_shared_library (test_serialization_library)
shared_library_include (
        SCOPE PUBLIC
        ABSTRACT
        cpu_dispatch=kan cpu_profiler=default error=sdl file_system=platform_default hash=djb2 log=kan memory=kan
        memory_profiler=default platform=sdl precise_time=sdl reflection=kan stream=kan threading=sdl
        CONCRETE container serialization readable_data testing test_serialization)

shared_library_verify ()
shared_library_copy_linked_artefacts ()
kan_setup_tests (TEST_UNIT test_serialization TEST_SHARED_LIBRARY test_serialization_library)
