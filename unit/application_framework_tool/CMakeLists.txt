register_concrete (application_framework_tool)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (
        SCOPE PUBLIC
        ABSTRACT error log memory reflection
        CONCRETE_INTERFACE context context_plugin_system context_reflection_system)
