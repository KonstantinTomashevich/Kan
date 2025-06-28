register_concrete (universe_render_foundation)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources (GLOB "*.c")

concrete_require (
        SCOPE PUBLIC
        ABSTRACT context_render_backend_system
        CONCRETE_INTERFACE inline_math resource_material universe universe_object)

concrete_require (
        SCOPE PRIVATE
        ABSTRACT context_hot_reload_coordination_system cpu_profiler precise_time universe_resource_provider
        CONCRETE_INTERFACE resource_pipeline resource_texture)

concrete_require (SCOPE PRIVATE ABSTRACT log CONCRETE_INTERFACE universe_time)
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

set (KAN_UNIVERSE_RENDER_FOUNDATION_NAME_BUFFER_LENGTH "128" CACHE STRING
        "Size of a buffer for forming various backend objects tracking names.")

set (KAN_UNIVERSE_RENDER_FOUNDATION_TEXTURE_INFO_PRIORITY "200" CACHE STRING
        "Loading priority at which texture information is loaded.")

set (KAN_UNIVERSE_RENDER_FOUNDATION_TEXTURE_DATA_PRIORITY "100" CACHE STRING
        "Loading priority at which texture data is loaded.")

set (KAN_UNIVERSE_RENDER_FOUNDATION_MATERIAL_INFO_PRIORITY "200" CACHE STRING
        "Loading priority at which material information is loaded.")

set (KAN_UNIVERSE_RENDER_FOUNDATION_MATERIAL_DATA_PRIORITY "150" CACHE STRING
        "Loading priority at which material data (family and pipelines) is loaded.")

set (KAN_UNIVERSE_RENDER_FOUNDATION_MATERIAL_PSC "8" CACHE STRING
        "Start capacity of pipelines array for loaded material. Later array is resized to its actual size.")

set (KAN_UNIVERSE_RENDER_FOUNDATION_MI_PRIORITY "150" CACHE STRING
        "Loading priority at which material instances are loaded.")

set (KAN_UNIVERSE_RENDER_FOUNDATION_MI_TEMPORARY_DATA_SIZE "32768" CACHE STRING
        "Size of a static buffer for building material instance buffers.")

set (KAN_UNIVERSE_RENDER_FOUNDATION_MI_UPDATES_COUNT "32" CACHE STRING
        "Size of a buffer for material instance parameter set initial bindings.")

concrete_compile_definitions (
        PRIVATE
        KAN_UNIVERSE_RENDER_FOUNDATION_GIC_BUCKETS=${KAN_UNIVERSE_RENDER_FOUNDATION_GIC_BUCKETS}
        KAN_UNIVERSE_RENDER_FOUNDATION_GFBC_BUCKETS=${KAN_UNIVERSE_RENDER_FOUNDATION_GFBC_BUCKETS}
        KAN_UNIVERSE_RENDER_FOUNDATION_GTA_INITIAL_STACK=${KAN_UNIVERSE_RENDER_FOUNDATION_GTA_INITIAL_STACK}
        KAN_UNIVERSE_RENDER_FOUNDATION_ATTACHMENTS_MAX_STATIC=${KAN_UNIVERSE_RENDER_FOUNDATION_ATTACHMENTS_MAX_STATIC}
        KAN_UNIVERSE_RENDER_FOUNDATION_BINDINGS_MAX_STATIC=${KAN_UNIVERSE_RENDER_FOUNDATION_BINDINGS_MAX_STATIC}
        KAN_UNIVERSE_RENDER_FOUNDATION_NAME_BUFFER_LENGTH=${KAN_UNIVERSE_RENDER_FOUNDATION_NAME_BUFFER_LENGTH}
        KAN_UNIVERSE_RENDER_FOUNDATION_TEXTURE_INFO_PRIORITY=${KAN_UNIVERSE_RENDER_FOUNDATION_TEXTURE_INFO_PRIORITY}
        KAN_UNIVERSE_RENDER_FOUNDATION_TEXTURE_DATA_PRIORITY=${KAN_UNIVERSE_RENDER_FOUNDATION_TEXTURE_DATA_PRIORITY}
        KAN_UNIVERSE_RENDER_FOUNDATION_MATERIAL_INFO_PRIORITY=${KAN_UNIVERSE_RENDER_FOUNDATION_MATERIAL_INFO_PRIORITY}
        KAN_UNIVERSE_RENDER_FOUNDATION_MATERIAL_DATA_PRIORITY=${KAN_UNIVERSE_RENDER_FOUNDATION_MATERIAL_DATA_PRIORITY}
        KAN_UNIVERSE_RENDER_FOUNDATION_MATERIAL_PSC=${KAN_UNIVERSE_RENDER_FOUNDATION_MATERIAL_PSC}
        KAN_UNIVERSE_RENDER_FOUNDATION_MI_PRIORITY=${KAN_UNIVERSE_RENDER_FOUNDATION_MI_PRIORITY}
        KAN_UNIVERSE_RENDER_FOUNDATION_MI_TEMPORARY_DATA_SIZE=${KAN_UNIVERSE_RENDER_FOUNDATION_MI_TEMPORARY_DATA_SIZE}
        KAN_UNIVERSE_RENDER_FOUNDATION_MI_UPDATES_COUNT=${KAN_UNIVERSE_RENDER_FOUNDATION_MI_UPDATES_COUNT})

option (KAN_UNIVERSE_RENDER_FOUNDATION_VALIDATION_ENABLED "Whether render foundation data validation is enabled." ON)
if (KAN_UNIVERSE_RENDER_FOUNDATION_VALIDATION_ENABLED)
    concrete_compile_definitions (PRIVATE KAN_UNIVERSE_RENDER_FOUNDATION_VALIDATION_ENABLED)
endif ()

