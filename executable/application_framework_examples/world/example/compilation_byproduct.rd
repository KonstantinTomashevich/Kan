//! kan_universe_world_definition_t

world_name = "example"
scheduler_name = "single_pipeline"

+pipelines {
    name = "update"
    mutators =
        compilation_byproduct
}
