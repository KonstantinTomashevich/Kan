register_abstract (context_hot_reload_coordination_system)
abstract_include ("${CMAKE_CURRENT_SOURCE_DIR}")
abstract_require (ABSTRACT platform CONCRETE_INTERFACE container context INTERFACE api_common)

abstract_register_implementation (
        NAME kan PARTS context_hot_reload_coordination_system_kan context_hot_reload_coordination_system_reflection)
abstract_register_implementation (
        NAME none PARTS context_hot_reload_coordination_system_none context_hot_reload_coordination_system_reflection)

option (KAN_CONTEXT_HOT_RELOAD_COORDINATION_ENABLE_BY_DEFAULT
        "Whether context hot reload coordination system is enabled by default." ON)

if (KAN_CONTEXT_HOT_RELOAD_COORDINATION_ENABLE_BY_DEFAULT)
    abstract_alias_implementation (ALIAS default SOURCE kan)
else ()
    abstract_alias_implementation (ALIAS default SOURCE none)
endif ()

c_interface_scanner_setup (GLOB "*.h")
reflection_generator_setup (GLOB "*.h")
reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)

register_concrete (context_hot_reload_coordination_system_reflection)
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}")
concrete_require (SCOPE PUBLIC ABSTRACT context_hot_reload_coordination_system)
concrete_require (SCOPE PRIVATE ABSTRACT error reflection)
register_unit_reflection_with_name (context_hot_reload_coordination_system)
