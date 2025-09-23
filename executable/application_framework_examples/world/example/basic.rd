//! kan_universe_world_definition_t

world_name = "example"
scheduler_name = "trivial"

+pipelines {
    name = "update"
    mutators =
        example_basic
}
