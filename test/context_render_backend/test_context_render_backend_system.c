#include <stdio.h>

#include <kan/context/render_backend_system.h>
#include <kan/platform/application.h>
#include <kan/testing/testing.h>

KAN_TEST_CASE (temp)
{
    kan_platform_application_init ();
    kan_context_handle_t context =
        kan_context_create (kan_allocation_group_get_child (kan_allocation_group_root (), "context"));

    struct kan_render_backend_system_config_t render_backend_config = {
        .disable_render = KAN_FALSE,
        .application_info_name = kan_string_intern ("Kan autotest"),
        .version_major = 1u,
        .version_minor = 0u,
        .version_patch = 0u,
    };

    KAN_TEST_CHECK (kan_context_request_system (context, KAN_CONTEXT_APPLICATION_SYSTEM_NAME, NULL))
    KAN_TEST_CHECK (
        kan_context_request_system (context, KAN_CONTEXT_RENDER_BACKEND_SYSTEM_NAME, &render_backend_config))

    kan_context_assembly (context);
    kan_context_system_handle_t application_system = kan_context_query (context, KAN_CONTEXT_APPLICATION_SYSTEM_NAME);
    kan_context_system_handle_t render_backend_system =
        kan_context_query (context, KAN_CONTEXT_RENDER_BACKEND_SYSTEM_NAME);

    struct kan_render_supported_devices_t *devices = kan_render_backend_system_get_devices (render_backend_system);
    printf ("Devices (%lu):\n", (unsigned long) devices->supported_device_count);
    kan_render_device_id_t picked_device;
    uint64_t picked_device_index = UINT64_MAX;

    for (uint64_t index = 0u; index < devices->supported_device_count; ++index)
    {
        printf ("  - name: %s\n    device_type: %lu\n    memory_type: %lu\n", devices->devices[index].name,
                (unsigned long) devices->devices[index].device_type,
                (unsigned long) devices->devices[index].memory_type);

        if (picked_device_index == UINT64_MAX ||
            devices->devices[picked_device_index].device_type != KAN_RENDER_DEVICE_TYPE_DISCRETE_GPU)
        {
            picked_device = devices->devices[index].id;
            picked_device_index = index;
        }
    }

    kan_render_backend_system_select_device (render_backend_system, picked_device);
#define TEST_FRAMES 1000u

    for (uint64_t frame = 0u; frame < TEST_FRAMES; ++frame)
    {
        kan_application_system_sync_in_main_thread (application_system);
    }

#undef TEST_FRAMES
    kan_context_destroy (context);
    kan_platform_application_shutdown ();
}
