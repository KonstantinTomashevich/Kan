//! kan_resource_material_header_t
sources = "directional_light.rpl", "lights.rpl", "depth_stencil_common.rpl"

+passes {
    name = lighting

    +entry_points {
        stage = KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX
        function_name = lighting_vertex_main
    }

    +entry_points {
        stage = KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_FRAGMENT
        function_name = lighting_fragment_main
    }

    options {
        +enums {
            name = depth_stencil_mode
            value = "directional light"
        }
    }
}
