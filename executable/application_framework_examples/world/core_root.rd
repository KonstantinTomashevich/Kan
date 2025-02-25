//! kan_universe_world_definition_t

world_name = "core"
scheduler_name = "single_pipeline_no_time"

+configuration {
    name = "resource_provider"
    +layers {
        data {
            __type = kan_resource_provider_configuration_t
            scan_budget_ns = 2000000
            serve_budget_ns = 2000000
            use_load_only_string_registry = 1
            resource_directory_path = "resources"
        }
    }
}

+configuration {
    name = "render_foundation_material_management"
    +layers {
        data {
            __type = kan_render_material_configuration_t
            preload_materials = 0
        }
    }

    +layers {
        required_tags = packaged
        data {
            __type = kan_render_material_configuration_t
            preload_materials = 1
        }
    }
}

+pipelines {
    name = "update"
    mutator_groups =
        resource_provider,
        render_foundation_root_routine,
        render_foundation_material_management,
        render_foundation_material_instance_management,
        render_foundation_texture_management
}
