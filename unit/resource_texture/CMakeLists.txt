register_concrete (resource_texture)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE container)
concrete_require (SCOPE PRIVATE ABSTRACT error log CONCRETE_INTERFACE inline_math resource_pipeline)

concrete_preprocessing_queue_step_preprocess ()
reflection_preprocessor_setup_step (GLOB "*.c" "*.h")
register_unit_reflection ()

set (KAN_RESOURCE_TEXTURE_BYPRODUCT_MAX_NAME_LENGTH "256" CACHE STRING
        "Size of a buffer used to format compiled texture byproducts (compiled data's) names.")

concrete_compile_definitions (
        PRIVATE
        KAN_RESOURCE_TEXTURE_BYPRODUCT_MAX_NAME_LENGTH=${KAN_RESOURCE_TEXTURE_BYPRODUCT_MAX_NAME_LENGTH})
