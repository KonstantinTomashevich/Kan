//! kan_resource_material_instance_t

material = test_material

+tail_append {
    tail_name = presets
    +parameters {
        name = modifier_1
        type = KAN_RPL_META_VARIABLE_TYPE_F4
        value_f4 { x = 1.0 y = 1.0 z = 1.0 w = 1.0 }
    }
    +parameters {
        name = modifier_2
        type = KAN_RPL_META_VARIABLE_TYPE_F4
        value_f4 { x = 0.0 y = 0.0 z = 0.0 w = 0.0 }
    }
}

+tail_append {
    tail_name = presets
    +parameters {
        name = modifier_1
        type = KAN_RPL_META_VARIABLE_TYPE_F4
        value_f4 { x = 0.0 y = 0.0 z = 0.0 w = 0.0 }
    }
    +parameters {
        name = modifier_2
        type = KAN_RPL_META_VARIABLE_TYPE_F4
        value_f4 { x = 1.0 y = 1.0 z = 1.0 w = 1.0 }
    }
}

+tail_append {
    tail_name = presets
    +parameters {
        name = modifier_1
        type = KAN_RPL_META_VARIABLE_TYPE_F4
        value_f4 { x = 0.5 y = 0.5 z = 0.5 w = 0.5 }
    }
    +parameters {
        name = modifier_2
        type = KAN_RPL_META_VARIABLE_TYPE_F4
        value_f4 { x = 0.5 y = 0.5 z = 0.5 w = 0.5 }
    }
}

+tail_append {
    tail_name = grids
    +parameters {
        name = thickness
        type = KAN_RPL_META_VARIABLE_TYPE_F1
        value_f1 = 0.01
    }
    +parameters {
        name = separator
        type = KAN_RPL_META_VARIABLE_TYPE_F1
        value_f1 = 0.1
    }
}

+tail_append {
    tail_name = grids
    +parameters {
        name = thickness
        type = KAN_RPL_META_VARIABLE_TYPE_F1
        value_f1 = 0.01
    }
    +parameters {
        name = separator
        type = KAN_RPL_META_VARIABLE_TYPE_F1
        value_f1 = 0.2
    }
}

+tail_append {
    tail_name = grids
    +parameters {
        name = thickness
        type = KAN_RPL_META_VARIABLE_TYPE_F1
        value_f1 = 0.01
    }
    +parameters {
        name = separator
        type = KAN_RPL_META_VARIABLE_TYPE_F1
        value_f1 = 0.5
    }
}

+instanced_parameters {
    name = preset_index
    type = KAN_RPL_META_VARIABLE_TYPE_I1
    value_i1 = 0
}

+instanced_parameters {
    name = grid_index
    type = KAN_RPL_META_VARIABLE_TYPE_I1
    value_i1 = 0
}

+images {
    name = texture_1_color
    texture = bricks_1
    sampler {
        mag_filter = KAN_RENDER_FILTER_MODE_NEAREST
        min_filter = KAN_RENDER_FILTER_MODE_NEAREST
        mip_map_mode = KAN_RENDER_MIP_MAP_MODE_NEAREST
        address_mode_u = KAN_RENDER_ADDRESS_MODE_REPEAT
        address_mode_v = KAN_RENDER_ADDRESS_MODE_REPEAT
        address_mode_w = KAN_RENDER_ADDRESS_MODE_REPEAT
    }
}

+images {
    name = texture_2_color
    texture = bricks_2
    sampler {
        mag_filter = KAN_RENDER_FILTER_MODE_NEAREST
        min_filter = KAN_RENDER_FILTER_MODE_NEAREST
        mip_map_mode = KAN_RENDER_MIP_MAP_MODE_NEAREST
        address_mode_u = KAN_RENDER_ADDRESS_MODE_REPEAT
        address_mode_v = KAN_RENDER_ADDRESS_MODE_REPEAT
        address_mode_w = KAN_RENDER_ADDRESS_MODE_REPEAT
    }
}
