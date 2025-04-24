//! kan_resource_render_pass_t
type = KAN_RENDER_PASS_GRAPHICS

// Depth buffer.
+attachments {
    type = KAN_RENDER_PASS_ATTACHMENT_DEPTH
    format = KAN_RENDER_IMAGE_FORMAT_D16_UNORM
    samples = 1
    load_operation = KAN_RENDER_LOAD_OPERATION_CLEAR
    store_operation = KAN_RENDER_STORE_OPERATION_STORE
}

+variants {
    sources = "shadow_pass_parameters.rpl"
}
