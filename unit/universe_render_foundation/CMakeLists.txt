register_concrete (universe_render_foundation)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources (GLOB "*.c")

concrete_require (
        SCOPE PUBLIC
        ABSTRACT context_render_backend_system
        CONCRETE_INTERFACE inline_math universe universe_object)

concrete_require (
        SCOPE PRIVATE
        ABSTRACT cpu_profiler precise_time universe_resource_provider
        CONCRETE_INTERFACE resource_pipeline resource_texture)

concrete_require (SCOPE PRIVATE ABSTRACT log CONCRETE_INTERFACE universe_time)
universe_concrete_preprocessing_queue ()

set (KAN_UNIVERSE_RENDER_FOUNDATION_GIC_BUCKETS "17" CACHE STRING
        "Initial count of buckets for render foundation graph resource management image cache.")

set (KAN_UNIVERSE_RENDER_FOUNDATION_GFBC_BUCKETS "17" CACHE STRING
        "Initial count of buckets for render foundation graph resource management frame buffer cache.")

set (KAN_UNIVERSE_RENDER_FOUNDATION_GTA_INITIAL_STACK "8192" CACHE STRING
        "Size of a stack group item for render foundation graph temporary allocator.")

set (KAN_UNIVERSE_RENDER_FOUNDATION_ATTACHMENTS_MAX_STATIC "8" CACHE STRING
        "Max count of attachment references inside render graph algorithm for static allocation.")

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
        KAN_UNIVERSE_RENDER_FOUNDATION_TEXTURE_INFO_PRIORITY=${KAN_UNIVERSE_RENDER_FOUNDATION_TEXTURE_INFO_PRIORITY}
        KAN_UNIVERSE_RENDER_FOUNDATION_TEXTURE_DATA_PRIORITY=${KAN_UNIVERSE_RENDER_FOUNDATION_TEXTURE_DATA_PRIORITY})

