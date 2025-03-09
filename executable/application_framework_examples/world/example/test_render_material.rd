//! kan_universe_world_definition_t

world_name = "example"
scheduler_name = "single_pipeline"

+pipelines {
    name = "update"
    mutators =
        render_foundation_material_instance_custom_sync,
        test_render_material
}
