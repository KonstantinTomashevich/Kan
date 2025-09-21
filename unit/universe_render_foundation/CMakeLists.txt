register_concrete (universe_render_foundation)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources (GLOB "*.c")

concrete_require (
        SCOPE PUBLIC
        ABSTRACT context_render_backend_system
        CONCRETE_INTERFACE inline_math resource_render_foundation universe universe_object)

concrete_require (
        SCOPE PRIVATE
        ABSTRACT context_hot_reload_coordination_system cpu_profiler log
        CONCRETE_INTERFACE universe_resource_provider)
setup_reflected_preprocessing ()

set (KAN_UNIVERSE_RENDER_FOUNDATION_GIC_BUCKETS "17" CACHE STRING
        "Initial count of buckets for render foundation graph resource management image cache.")

set (KAN_UNIVERSE_RENDER_FOUNDATION_GFBC_BUCKETS "17" CACHE STRING
        "Initial count of buckets for render foundation graph resource management frame buffer cache.")

set (KAN_UNIVERSE_RENDER_FOUNDATION_GTA_INITIAL_STACK "32768" CACHE STRING
        "Size of a stack group item for render foundation graph temporary allocator.")

set (KAN_UNIVERSE_RENDER_FOUNDATION_ATTACHMENTS_MAX_STATIC "8" CACHE STRING
        "Max count of attachment references inside render graph algorithm for static allocation.")

set (KAN_UNIVERSE_RENDER_FOUNDATION_BINDINGS_MAX_STATIC "16" CACHE STRING
        "Max count of parameter set bindings for parameter set layout construction for static allocation.")

set (KAN_UNIVERSE_RENDER_FOUNDATION_NAME_BUFFER_LENGTH "256" CACHE STRING
        "Size of a buffer for forming various backend objects tracking names.")

set (KAN_UNIVERSE_RENDER_FOUNDATION_PASS_PRIORITY "1000" CACHE STRING
        "Loading priority at which render pass information is loaded.")

set (KAN_UNIVERSE_RENDER_FOUNDATION_MATERIAL_BASE_PRIORITY "50" CACHE STRING
        "Loading priority at which materials are loaded if they're not used by material instances yet.")

set (KAN_UNIVERSE_RENDER_FOUNDATION_MATERIAL_USED_PRIORITY "150" CACHE STRING
        "Loading priority at which materials are loaded if they are already needed for some material instance loading.")

set (KAN_UNIVERSE_RENDER_FOUNDATION_MI_PRIORITY "125" CACHE STRING
        "Loading priority at which material instances datas are loaded.")

set (KAN_UNIVERSE_RENDER_FOUNDATION_MI_UPDATES_COUNT "32" CACHE STRING
        "Size of a buffer for material instance parameter set initial bindings.")

set (KAN_UNIVERSE_RENDER_FOUNDATION_TEXTURE_INFO_PRIORITY "200" CACHE STRING
        "Loading priority at which texture information is loaded.")

set (KAN_UNIVERSE_RENDER_FOUNDATION_TEXTURE_DATA_PRIORITY "100" CACHE STRING
        "Loading priority at which texture data is loaded.")

concrete_compile_definitions (
        PRIVATE
        KAN_UNIVERSE_RENDER_FOUNDATION_GIC_BUCKETS=${KAN_UNIVERSE_RENDER_FOUNDATION_GIC_BUCKETS}
        KAN_UNIVERSE_RENDER_FOUNDATION_GFBC_BUCKETS=${KAN_UNIVERSE_RENDER_FOUNDATION_GFBC_BUCKETS}
        KAN_UNIVERSE_RENDER_FOUNDATION_GTA_INITIAL_STACK=${KAN_UNIVERSE_RENDER_FOUNDATION_GTA_INITIAL_STACK}
        KAN_UNIVERSE_RENDER_FOUNDATION_ATTACHMENTS_MAX_STATIC=${KAN_UNIVERSE_RENDER_FOUNDATION_ATTACHMENTS_MAX_STATIC}
        KAN_UNIVERSE_RENDER_FOUNDATION_BINDINGS_MAX_STATIC=${KAN_UNIVERSE_RENDER_FOUNDATION_BINDINGS_MAX_STATIC}
        KAN_UNIVERSE_RENDER_FOUNDATION_NAME_BUFFER_LENGTH=${KAN_UNIVERSE_RENDER_FOUNDATION_NAME_BUFFER_LENGTH}
        KAN_UNIVERSE_RENDER_FOUNDATION_PASS_PRIORITY=${KAN_UNIVERSE_RENDER_FOUNDATION_PASS_PRIORITY}
        KAN_UNIVERSE_RENDER_FOUNDATION_MATERIAL_BASE_PRIORITY=${KAN_UNIVERSE_RENDER_FOUNDATION_MATERIAL_BASE_PRIORITY}
        KAN_UNIVERSE_RENDER_FOUNDATION_MATERIAL_USED_PRIORITY=${KAN_UNIVERSE_RENDER_FOUNDATION_MATERIAL_USED_PRIORITY}
        KAN_UNIVERSE_RENDER_FOUNDATION_MI_PRIORITY=${KAN_UNIVERSE_RENDER_FOUNDATION_MI_PRIORITY}
        KAN_UNIVERSE_RENDER_FOUNDATION_MI_UPDATES_COUNT=${KAN_UNIVERSE_RENDER_FOUNDATION_MI_UPDATES_COUNT}
        KAN_UNIVERSE_RENDER_FOUNDATION_TEXTURE_INFO_PRIORITY=${KAN_UNIVERSE_RENDER_FOUNDATION_TEXTURE_INFO_PRIORITY}
        KAN_UNIVERSE_RENDER_FOUNDATION_TEXTURE_DATA_PRIORITY=${KAN_UNIVERSE_RENDER_FOUNDATION_TEXTURE_DATA_PRIORITY})
