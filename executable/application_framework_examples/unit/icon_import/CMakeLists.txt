register_concrete (application_framework_examples_icon_import)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

concrete_require (
        SCOPE PUBLIC
        ABSTRACT image log reflection
        CONCRETE_INTERFACE application_framework_examples_icon_resource container resource_pipeline)
setup_reflected_preprocessing ()
