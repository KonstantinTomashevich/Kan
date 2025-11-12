//! kan_universe_world_definition_t

world_name = "example"
scheduler_name = "trivial"

+pipelines {
    name = "update"
    mutator_groups = 
        text_effects,
        text_shaping
}
