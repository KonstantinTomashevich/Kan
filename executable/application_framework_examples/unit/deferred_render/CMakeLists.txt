# TODO: Temporary disabled while we're refactoring everything to new resource build and provision logic.
return ()

register_concrete (application_framework_examples_deferred_render)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources (GLOB "*.c")

concrete_require (
        SCOPE PUBLIC
        ABSTRACT image log precise_time universe_resource_provider
        CONCRETE_INTERFACE
        application_framework context_application_system context_virtual_file_system universe universe_render_foundation
        universe_time)

register_application_resource_directory ("${CMAKE_CURRENT_SOURCE_DIR}/resources")
setup_reflected_preprocessing ()
