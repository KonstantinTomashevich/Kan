//! kan_resource_platform_configuration_t
parent = ""

+configuration {
    __type = pipeline_instance_platform_configuration_t
    format = PIPELINE_INSTANCE_PLATFORM_FORMAT_SPIRV
}

+configuration {
    __type = kan_resource_texture_platform_configuration_t
    supported_compiled_formats =
        KAN_RESOURCE_TEXTURE_COMPILED_FORMAT_UNCOMPRESSED_R8,
        KAN_RESOURCE_TEXTURE_COMPILED_FORMAT_UNCOMPRESSED_RG16,
        KAN_RESOURCE_TEXTURE_COMPILED_FORMAT_UNCOMPRESSED_RGBA32,
        KAN_RESOURCE_TEXTURE_COMPILED_FORMAT_UNCOMPRESSED_D16,
        KAN_RESOURCE_TEXTURE_COMPILED_FORMAT_UNCOMPRESSED_D32
}
