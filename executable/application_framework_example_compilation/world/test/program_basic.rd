//! kan_universe_world_definition_t

world_name = "basic"
scheduler_name = "single_pipeline"

+pipelines {
    name = "update"
    mutators =
        basic_mutator
}
