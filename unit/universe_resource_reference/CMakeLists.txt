register_abstract (universe_resource_reference)
abstract_include ("${CMAKE_CURRENT_SOURCE_DIR}")
abstract_require (ABSTRACT universe_resource_provider CONCRETE_INTERFACE universe)
abstract_register_implementation (NAME kan PARTS universe_resource_reference_kan universe_resource_reference_reflection)

create_accompanying_reflection_unit (
        FOR_ABSTRACT universe_resource_reference NAME universe_resource_reference_reflection GLOB "*.h")
