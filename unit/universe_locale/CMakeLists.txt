register_concrete (universe_locale)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources (GLOB "*.c")

concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE resource_locale universe)
concrete_require (SCOPE PRIVATE ABSTRACT cpu_profiler log CONCRETE_INTERFACE universe_resource_provider)
setup_reflected_preprocessing ()

set (KAN_UNIVERSE_LOCALE_LOADING_PRIORITY "300" CACHE STRING
        "Loading priority at which locale and language data is loaded.")

concrete_compile_definitions (
        PRIVATE
        KAN_UNIVERSE_LOCALE_LOADING_PRIORITY=${KAN_UNIVERSE_LOCALE_LOADING_PRIORITY})
