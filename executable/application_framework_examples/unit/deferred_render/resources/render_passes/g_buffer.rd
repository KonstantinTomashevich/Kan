//! kan_resource_render_pass_header_t
type = KAN_RENDER_PASS_GRAPHICS

// Position+specular modifier buffer.
+attachments {
    type = KAN_RENDER_PASS_ATTACHMENT_COLOR
    format = KAN_RENDER_IMAGE_FORMAT_RGBA128_SFLOAT
    samples = 1
    load_operation = KAN_RENDER_LOAD_OPERATION_CLEAR
    store_operation = KAN_RENDER_STORE_OPERATION_STORE
}

// Normal+shininess buffer.
+attachments {
    type = KAN_RENDER_PASS_ATTACHMENT_COLOR
    format = KAN_RENDER_IMAGE_FORMAT_RGBA128_SFLOAT
    samples = 1
    load_operation = KAN_RENDER_LOAD_OPERATION_CLEAR
    store_operation = KAN_RENDER_STORE_OPERATION_STORE
}

// Diffuse buffer.
+attachments {
    type = KAN_RENDER_PASS_ATTACHMENT_COLOR
    format = KAN_RENDER_IMAGE_FORMAT_RGBA32_SRGB
    samples = 1
    load_operation = KAN_RENDER_LOAD_OPERATION_CLEAR
    store_operation = KAN_RENDER_STORE_OPERATION_STORE
}

// Depth buffer.
+attachments {
    type = KAN_RENDER_PASS_ATTACHMENT_DEPTH_STENCIL
    format = KAN_RENDER_IMAGE_FORMAT_D32_SFLOAT_S8_UINT
    samples = 1
    load_operation = KAN_RENDER_LOAD_OPERATION_CLEAR
    store_operation = KAN_RENDER_STORE_OPERATION_STORE
}

+variants {
    name = default
    sources = "scene_view_pass_parameters.rpl", "g_buffer_pass.rpl"
}
