//! kan_resource_render_pass_header_t
type = KAN_RENDER_PASS_GRAPHICS

+attachments {
    type = KAN_RENDER_PASS_ATTACHMENT_COLOR
    format = KAN_RENDER_IMAGE_FORMAT_RGBA32_SRGB
    samples = 1
    load_operation = KAN_RENDER_LOAD_OPERATION_CLEAR
    store_operation = KAN_RENDER_STORE_OPERATION_STORE
}

+attachments {
    type = KAN_RENDER_PASS_ATTACHMENT_DEPTH_STENCIL
    format = KAN_RENDER_IMAGE_FORMAT_D32_SFLOAT_S8_UINT
    samples = 1
    load_operation = KAN_RENDER_LOAD_OPERATION_LOAD
    store_operation = KAN_RENDER_STORE_OPERATION_STORE
}

+variants {
    name = default
    sources = "scene_view_pass_parameters.rpl", "lighting.rpl"
}
