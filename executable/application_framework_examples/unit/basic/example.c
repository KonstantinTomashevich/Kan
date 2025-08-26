#include <application_framework_examples_basic_api.h>

#include <stdio.h>
#include <string.h>

#include <kan/context/all_system_names.h>
#include <kan/context/application_framework_system.h>
#include <kan/context/application_system.h>
#include <kan/log/logging.h>
#include <kan/resource_pipeline/meta.h>
#include <kan/universe/macro.h>
#include <kan/universe/universe.h>
#include <kan/universe_resource_provider/universe_resource_provider.h>
#include <kan/universe_time/universe_time.h>

KAN_LOG_DEFINE_CATEGORY (application_framework_examples_basic);
KAN_USE_STATIC_INTERNED_IDS

struct basic_data_type_t
{
    kan_serialized_size_t x;
    kan_serialized_size_t y;
};

KAN_REFLECTION_STRUCT_META (basic_data_type_t)
APPLICATION_FRAMEWORK_EXAMPLES_BASIC_API struct kan_resource_type_meta_t basic_data_type_resource_type = {
    .flags = KAN_RESOURCE_TYPE_ROOT,
    .version = CUSHION_START_NS_X64,
    .move = NULL,
    .reset = NULL,
};

struct example_basic_singleton_t
{
    kan_application_system_window_t window_handle;
    bool test_usage_added;
    kan_resource_usage_id_t test_usage_id;
};

APPLICATION_FRAMEWORK_EXAMPLES_BASIC_API void example_basic_singleton_init (struct example_basic_singleton_t *instance)
{
    instance->window_handle = KAN_HANDLE_SET_INVALID (kan_application_system_window_t);
    instance->test_usage_added = false;
}

struct example_basic_state_t
{
    KAN_UM_GENERATE_STATE_QUERIES (example_basic)
    KAN_UM_BIND_STATE (example_basic, state)

    kan_context_system_t application_system_handle;
    kan_context_system_t application_framework_system_handle;

    bool test_mode;
    bool test_passed;
    bool test_asset_loaded;
    kan_instance_size_t test_frames_count;
};

APPLICATION_FRAMEWORK_EXAMPLES_BASIC_API KAN_UM_MUTATOR_DEPLOY (example_basic)
{
    kan_static_interned_ids_ensure_initialized ();
    kan_context_t context = kan_universe_get_context (universe);

    state->application_system_handle = kan_context_query (context, KAN_CONTEXT_APPLICATION_SYSTEM_NAME);
    state->application_framework_system_handle =
        kan_context_query (context, KAN_CONTEXT_APPLICATION_FRAMEWORK_SYSTEM_NAME);

    if (KAN_HANDLE_IS_VALID (state->application_framework_system_handle))
    {
        state->test_mode =
            kan_application_framework_system_get_arguments_count (state->application_framework_system_handle) == 2 &&
            strcmp (kan_application_framework_system_get_arguments (state->application_framework_system_handle)[1],
                    "--test") == 0;
    }
    else
    {
        state->test_mode = false;
    }

    state->test_passed = true;
    state->test_asset_loaded = false;
    state->test_frames_count = 0u;
}

APPLICATION_FRAMEWORK_EXAMPLES_BASIC_API KAN_UM_MUTATOR_EXECUTE (example_basic)
{
    KAN_UMI_SINGLETON_WRITE (singleton, example_basic_singleton_t)

    if (!KAN_HANDLE_IS_VALID (singleton->window_handle))
    {
        singleton->window_handle =
            kan_application_system_window_create (state->application_system_handle, "Title placeholder", 600u, 400u,
                                                  KAN_PLATFORM_WINDOW_FLAG_SUPPORTS_VULKAN);
        kan_application_system_window_raise (state->application_system_handle, singleton->window_handle);
    }

    if (!singleton->test_usage_added)
    {
        KAN_UMI_SINGLETON_READ (provider, kan_resource_provider_singleton_t)
        KAN_UMO_INDEXED_INSERT (usage, kan_resource_usage_t)
        {
            usage->usage_id = kan_next_resource_usage_id (provider);
            usage->type = KAN_STATIC_INTERNED_ID_GET (basic_data_type_t);
            usage->name = KAN_STATIC_INTERNED_ID_GET (test);
            usage->priority = 0u;
            singleton->test_usage_id = usage->usage_id;
        }

        singleton->test_usage_added = true;
    }

    kan_instance_size_t x = 0;
    kan_instance_size_t y = 0;

    {
        const kan_interned_string_t name = KAN_STATIC_INTERNED_ID_GET (test);
        KAN_UMI_RESOURCE_RETRIEVE_IF_LOADED (loaded, basic_data_type_t, &name)

        if (loaded)
        {
            state->test_asset_loaded = true;
            x = loaded->x;
            y = loaded->y;

            if (x != 3u || y != 5u)
            {
                state->test_passed = false;
                KAN_LOG (application_framework_examples_basic, KAN_LOG_INFO, "Unexpected x or y.")
            }
        }
    }

#define TITLE_BUFFER_SIZE 256u
    char buffer[TITLE_BUFFER_SIZE];

    {
        KAN_UMI_SINGLETON_READ (time, kan_time_singleton_t)
        snprintf (buffer, TITLE_BUFFER_SIZE, "Visual time: %.3f seconds. Visual delta: %.3f seconds. X: %llu. Y: %llu.",
                  (float) (time->visual_time_ns) / 1e9f, (float) (time->visual_delta_ns) / 1e9f, (unsigned long long) x,
                  (unsigned long long) y);
    }

    kan_application_system_window_set_title (state->application_system_handle, singleton->window_handle, buffer);
#undef TITLE_BUFFER_SIZE

    if (state->test_mode)
    {
        if (30u < ++state->test_frames_count)
        {
            KAN_LOG (application_framework_examples_basic, KAN_LOG_INFO, "Shutting down...")
            if (!state->test_asset_loaded)
            {
                state->test_passed = false;
                KAN_LOG (application_framework_examples_basic, KAN_LOG_ERROR, "Failed to load asset.")
            }

            KAN_ASSERT (KAN_HANDLE_IS_VALID (state->application_framework_system_handle))
            if (state->test_passed)
            {
                kan_application_framework_system_request_exit (state->application_framework_system_handle, 0);
            }
            else
            {
                kan_application_framework_system_request_exit (state->application_framework_system_handle, 1);
            }
        }
    }
}
