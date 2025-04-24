//! kan_resource_material_t
sources = "opaque.rpl", "depth_stencil_common.rpl"

+passes {
    name = g_buffer

    +entry_points {
        stage = KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX
        function_name = g_buffer_vertex_main
    }

    +entry_points {
        stage = KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_FRAGMENT
        function_name = g_buffer_fragment_main
    }

    options {
        +enums {
            name = depth_stencil_mode
            value = "g-buffer lit geometry"
        }
    }
}

+passes {
    name = shadow

    +entry_points {
        stage = KAN_RPL_PIPELINE_STAGE_GRAPHICS_CLASSIC_VERTEX
        function_name = shadow_vertex_main
    }

    options {
        +flags {
            name = shadow_map_reverse_normal
            value = 1
        }
        +enums {
            name = depth_stencil_mode
            value = "shadow"
        }
    }
}
