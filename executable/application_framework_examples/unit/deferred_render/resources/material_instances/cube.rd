//! kan_resource_material_instance_t

material = opaque_object

+samplers {
    name = texture_sampler
    sampler {
        mag_filter = KAN_RENDER_FILTER_MODE_LINEAR
        min_filter = KAN_RENDER_FILTER_MODE_LINEAR
        mip_map_mode = KAN_RENDER_MIP_MAP_MODE_NEAREST
        address_mode_u = KAN_RENDER_ADDRESS_MODE_REPEAT
        address_mode_v = KAN_RENDER_ADDRESS_MODE_REPEAT
        address_mode_w = KAN_RENDER_ADDRESS_MODE_REPEAT
        anisotropy_enabled = 1
        anisotropy_max = 2.0
    }
}

+parameters {
    name = specular_and_shininess
    type = KAN_RPL_META_VARIABLE_TYPE_F4
    value_f4 {
        x = 0.3
        y = 16.0
        z = 0.0
        w = 0.0
    }
}

+images {
    name = texture_color
    texture = bricks
}
