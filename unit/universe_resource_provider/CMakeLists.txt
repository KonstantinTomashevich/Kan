register_abstract (universe_resource_provider)
abstract_include ("${CMAKE_CURRENT_SOURCE_DIR}")
abstract_require (CONCRETE_INTERFACE resource_pipeline universe)
abstract_register_implementation (NAME kan PARTS universe_resource_provider_kan universe_resource_provider_reflection)

create_accompanying_reflection_unit (
        FOR_ABSTRACT universe_resource_provider NAME universe_resource_provider_reflection GLOB "*.h")
