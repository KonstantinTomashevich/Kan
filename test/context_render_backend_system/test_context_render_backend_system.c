#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <kan/context/render_backend_system.h>
#include <kan/platform/application.h>
#include <kan/platform/precise_time.h>
#include <kan/testing/testing.h>

KAN_TEST_CASE (temp)
{
    kan_platform_application_init ();
    kan_context_handle_t context =
        kan_context_create (kan_allocation_group_get_child (kan_allocation_group_root (), "context"));

    struct kan_render_backend_system_config_t render_backend_config = {
        .disable_render = KAN_FALSE,
        .prefer_vsync = KAN_FALSE,
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
    kan_render_context_t render_context = kan_render_backend_system_get_render_context (render_backend_system);

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

    kan_application_system_window_handle_t window_handle = kan_application_system_window_create (
        application_system, "Kan context_render_backend test window", 800u, 600u,
        kan_render_get_required_window_flags () | KAN_PLATFORM_WINDOW_FLAG_RESIZABLE);
    kan_render_backend_system_create_surface (render_backend_system, window_handle, kan_string_intern ("test_surface"));

    kan_render_frame_lifetime_buffer_allocator_t frame_lifetime_buffer_allocator =
        kan_render_frame_lifetime_buffer_allocator_create (render_context, KAN_RENDER_BUFFER_TYPE_STORAGE,
                                                           16u * 1024u * 1024u,
                                                           kan_string_intern ("test_frame_lifetime_buffer"));

    kan_render_buffer_t stub_buffers[] = {KAN_INVALID_RENDER_BUFFER, KAN_INVALID_RENDER_BUFFER,
                                          KAN_INVALID_RENDER_BUFFER, KAN_INVALID_RENDER_BUFFER};

    uint8_t stub_buffer_initial_data[1024u * sizeof (stub_buffers) / sizeof (stub_buffers[0u])];
    memset (stub_buffer_initial_data, 0, sizeof (stub_buffer_initial_data));

    uint8_t stub_buffer_changed_data[1024u * sizeof (stub_buffers) / sizeof (stub_buffers[0u])];
    memset (stub_buffer_changed_data, 42, sizeof (stub_buffer_initial_data));

    uint8_t stub_buffer_changed_again_data[1024u * sizeof (stub_buffers) / sizeof (stub_buffers[0u])];
    memset (stub_buffer_changed_again_data, 123, sizeof (stub_buffer_initial_data));

#define TEST_IMAGE_WIDTH 1024u
#define TEST_IMAGE_HEIGHT 1024u
#define TEST_IMAGE_DEPTH 1u
#define TEST_IMAGE_MIPS 8u
    uint8_t test_image_first_mip_data[4u * TEST_IMAGE_WIDTH * TEST_IMAGE_HEIGHT & TEST_IMAGE_DEPTH];
    memset (test_image_first_mip_data, 255, sizeof (test_image_first_mip_data));

    struct kan_render_image_description_t test_image_description = {
        .type = KAN_RENDER_IMAGE_TYPE_COLOR_2D,
        .color_format = KAN_RENDER_IMAGE_COLOR_FORMAT_RGBA32_SRGB,
        .width = TEST_IMAGE_WIDTH,
        .height = TEST_IMAGE_HEIGHT,
        .depth = TEST_IMAGE_DEPTH,
        .mips = TEST_IMAGE_MIPS,
        .render_target = KAN_FALSE,
        .supports_sampling = KAN_TRUE,
        .tracking_name = kan_string_intern ("test_image"),
    };

    kan_render_image_t test_image = kan_render_image_create (render_context, &test_image_description);
    kan_render_image_upload_data (test_image, 0u, test_image_first_mip_data);
    kan_render_image_request_mip_generation (test_image, 0u, TEST_IMAGE_MIPS - 1u);

    for (uint64_t frame = 0u; frame < TEST_FRAMES; ++frame)
    {
        switch (frame % 4u)
        {
        case 0u:
            for (uint64_t index = 0u; index < sizeof (stub_buffers) / sizeof (stub_buffers[0u]); ++index)
            {
                if (stub_buffers[index] != KAN_INVALID_RENDER_BUFFER)
                {
                    kan_render_buffer_destroy (stub_buffers[index]);
                }

                stub_buffers[index] =
                    kan_render_buffer_create (render_context, KAN_RENDER_BUFFER_TYPE_STORAGE, (index + 1u) * 1024u,
                                              stub_buffer_initial_data, kan_string_intern ("stub_buffer"));
            }

            break;

        case 1u:
            for (uint64_t index = 0u; index < sizeof (stub_buffers) / sizeof (stub_buffers[0u]); ++index)
            {
                void *memory = kan_render_buffer_patch (stub_buffers[index], 0u, (index + 1u) * 1024u);
                KAN_TEST_ASSERT (memory)
                memcpy (memory, stub_buffer_changed_data, (index + 1u) * 1024u);
            }

            break;

        case 2u:
            for (uint64_t index = 0u; index < sizeof (stub_buffers) / sizeof (stub_buffers[0u]); ++index)
            {
                void *memory = kan_render_buffer_patch (stub_buffers[index], 0u, (index + 1u) * 1024u);
                KAN_TEST_ASSERT (memory)
                memcpy (memory, stub_buffer_changed_again_data, (index + 1u) * 1024u);
            }

            break;

        case 3u:
            break;
        }

        struct kan_render_allocated_slice_t slice = kan_render_frame_lifetime_buffer_allocator_allocate (
            frame_lifetime_buffer_allocator, 1024u, _Alignof (float));
        KAN_TEST_ASSERT (slice.buffer != KAN_INVALID_RENDER_BUFFER)
        void *memory = kan_render_buffer_patch (slice.buffer, slice.slice_offset, 1024u);
        KAN_TEST_ASSERT (memory)
        memcpy (memory, stub_buffer_changed_again_data, 1024u);

        kan_application_system_sync_in_main_thread (application_system);
        kan_render_backend_system_next_frame (render_backend_system);

        // Sleep to avoid exiting too fast.
        kan_platform_sleep (1000000u);
    }

    kan_application_system_prepare_for_destroy_in_main_thread (application_system);

#undef TEST_FRAMES
    kan_context_destroy (context);
    kan_platform_application_shutdown ();
}
