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

create_accompanying_reflection_unit (
        FOR_ABSTRACT context_hot_reload_coordination_system
        NAME context_hot_reload_coordination_system_reflection
        GLOB "*.h")
