//! kan_application_framework_program_configuration_t

${PLUGINS}

program_world {
    world_name = "test"
    scheduler_name = "plain_update"

    +pipelines {
        name = "update"
        +mutators { name = "test_mutator" }
    }
}
