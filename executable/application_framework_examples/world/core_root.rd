//! kan_universe_world_definition_t

world_name = "core"
scheduler_name = "single_pipeline_no_time"

+configuration {
    name = "resource_provider"
    +layers {
        data {
            __type = kan_resource_provider_configuration_t
            serve_budget_ns = 2000000
            resource_directory_path = "resources"
        }
    }
}

+pipelines {
    name = "update"
    mutator_groups =
        resource_provider,
        render_foundation_program_management,
        render_foundation_frame,
        render_foundation_texture_management
}
