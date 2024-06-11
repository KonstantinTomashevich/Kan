//! kan_universe_world_definition_t

world_name = "core"
scheduler_name = "single_pipeline_no_time"

+configuration {
    name = "resource_provider"
    +variants {
        required_tags = development
        data {
            __type = kan_resource_provider_configuration_t
            scan_budget_ns = 2000000
            load_budget_ns = 2000000
            add_wait_time_ns = 100000000
            modify_wait_time_ns = 100000000
            use_load_only_string_registry = 1
            observe_file_system = 1
            resource_directory_path = "resources"
        }
    }

    +variants {
        data {
            __type = kan_resource_provider_configuration_t
            scan_budget_ns = 2000000
            load_budget_ns = 2000000
            add_wait_time_ns = 100000000
            modify_wait_time_ns = 100000000
            use_load_only_string_registry = 1
            observe_file_system = 0
            resource_directory_path = "resources"
        }
    }
}

+pipelines {
    name = "update"
    mutator_groups =
        resource_provider
}
