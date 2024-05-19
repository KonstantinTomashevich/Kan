//! kan_universe_world_definition_t

world_name = "test"
scheduler_name = "single_pipeline"

+pipelines {
    name = "update"
    mutators =
        test_mutator
}
