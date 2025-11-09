//! kan_resource_material_header_t
sources = "text_material.rpl"

+passes {
    name = text

    +entry_points {
        stage = KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX
        function_name = text_vertex_main
    }

    +entry_points {
        stage = KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_FRAGMENT
        function_name = text_fragment_main
    }
}
