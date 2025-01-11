//! kan_render_graph_pass_resource_t
type = KAN_RENDER_PASS_GRAPHICS

+attachments {
    type = KAN_RENDER_PASS_ATTACHMENT_COLOR
    format = KAN_RENDER_IMAGE_FORMAT_SURFACE
    samples = 1
    load_operation = KAN_RENDER_LOAD_OPERATION_CLEAR
    store_operation = KAN_RENDER_STORE_OPERATION_STORE
}

+attachments {
    type = KAN_RENDER_PASS_ATTACHMENT_DEPTH
    format = KAN_RENDER_IMAGE_FORMAT_D32_SFLOAT
    samples = 1
    load_operation = KAN_RENDER_LOAD_OPERATION_CLEAR
    store_operation = KAN_RENDER_STORE_OPERATION_STORE
}