register_concrete (resource_render_foundation_build)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE resource_render_foundation)
concrete_require (SCOPE PRIVATE ABSTRACT error file_system image log CONCRETE_INTERFACE inline_math resource_pipeline)
setup_reflected_preprocessing ()

set (KAN_RESOURCE_RF_TEXTURE_LOAD_BUFFER "16384" CACHE STRING
        "Size of a buffer used for IO when loading texture image.")

set (KAN_RESOURCE_RF_TEXTURE_DATA_MAX_NAME_LENGTH "256" CACHE STRING
        "Size of a buffer used to format built render foundation texture data resource names.")

set (KAN_RESOURCE_RF_PIPELINE_MAX_NAME_LENGTH "256" CACHE STRING
        "Size of a buffer used to format built render foundation pipeline resource names.")

set (KAN_RESOURCE_RF_MI_TAIL_APPEND_CAPACITY "16" CACHE STRING
        "When material instance buffer tail append happens, reserve capacity for this count of tail appends.")

concrete_compile_definitions (
        PRIVATE
        KAN_RESOURCE_RF_TEXTURE_LOAD_BUFFER=${KAN_RESOURCE_RF_TEXTURE_LOAD_BUFFER}
        KAN_RESOURCE_RF_TEXTURE_DATA_MAX_NAME_LENGTH=${KAN_RESOURCE_RF_TEXTURE_DATA_MAX_NAME_LENGTH}
        KAN_RESOURCE_RF_PIPELINE_MAX_NAME_LENGTH=${KAN_RESOURCE_RF_PIPELINE_MAX_NAME_LENGTH}
        KAN_RESOURCE_RF_MI_TAIL_APPEND_CAPACITY=${KAN_RESOURCE_RF_MI_TAIL_APPEND_CAPACITY})
