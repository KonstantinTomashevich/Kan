//! kan_universe_world_definition_t

world_name = "verify_code_hot_reload"
scheduler_name = "verify_code_hot_reload"

+pipelines {
    name = "verify_code_hot_reload_update"
    mutators =
        verify_code_hot_reload
}
