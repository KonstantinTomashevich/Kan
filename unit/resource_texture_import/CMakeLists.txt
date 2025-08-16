# TODO: Temporary disabled while we're refactoring everything to new resource build and provision logic.
return ()

register_concrete (resource_texture_import)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE resource_texture)
concrete_require (SCOPE PRIVATE ABSTRACT error image log CONCRETE_INTERFACE resource_pipeline)
setup_reflected_preprocessing ()
