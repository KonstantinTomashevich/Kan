#include <stddef.h>
#include <stdlib.h>

#include <kan/api_common/core_types.h>
#include <kan/container/hash_storage.h>
#include <kan/container/interned_string.h>
#include <kan/cpu_profiler/markup.h>
#include <kan/memory/allocation.h>
#include <kan/threading/atomic.h>

#include <tracy/TracyC.h>

void kan_cpu_stage_separator (void)
{
    ___tracy_emit_frame_mark (NULL);
}

struct section_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t name;
    struct ___tracy_source_location_data location;
};

static kan_bool_t section_storage_ready = KAN_FALSE;
static struct kan_atomic_int_t section_storage_lock = {.value = 0};
static struct kan_hash_storage_t section_storage;

kan_cpu_section_t kan_cpu_section_get (const char *name)
{
    kan_atomic_int_lock (&section_storage_lock);
    if (!section_storage_ready)
    {
        kan_hash_storage_init (&section_storage, KAN_ALLOCATION_GROUP_IGNORE,
                               KAN_CPU_PROFILER_TRACY_INITIAL_SECTION_BUCKETS);
        section_storage_ready = KAN_TRUE;
    }

    kan_interned_string_t interned_name = kan_string_intern (name);
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&section_storage, KAN_HASH_OBJECT_POINTER (interned_name));
    struct section_node_t *node = (struct section_node_t *) bucket->first;
    const struct section_node_t *end = (struct section_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != end)
    {
        if (node->name == interned_name)
        {
            kan_atomic_int_unlock (&section_storage_lock);
            return KAN_HANDLE_SET (kan_cpu_section_t, node);
        }

        node = (struct section_node_t *) node->node.list_node.next;
    }

    struct section_node_t *new_node = kan_allocate_general (KAN_ALLOCATION_GROUP_IGNORE, sizeof (struct section_node_t),
                                                            _Alignof (struct section_node_t));
    new_node->node.hash = KAN_HASH_OBJECT_POINTER (interned_name);
    new_node->name = interned_name;
    new_node->location.name = name;
    new_node->location.function = NULL;
    new_node->location.file = NULL;
    new_node->location.line = 0u;
    new_node->location.color = ((rand () % 255u) << 24u) | ((rand () % 255u) << 16u) | ((rand () % 255u) << 8u);

    kan_hash_storage_update_bucket_count_default (&section_storage, KAN_CPU_PROFILER_TRACY_INITIAL_SECTION_BUCKETS);
    kan_hash_storage_add (&section_storage, &new_node->node);
    kan_atomic_int_unlock (&section_storage_lock);
    return KAN_HANDLE_SET (kan_cpu_section_t, new_node);
}

void kan_cpu_section_set_color (kan_cpu_section_t section, uint32_t rgba_color)
{
    struct section_node_t *section_data = KAN_HANDLE_GET (section);
    section_data->location.color = rgba_color;
}

_Static_assert (sizeof (struct kan_cpu_section_execution_t) >= sizeof (struct ___tracy_c_zone_context),
                "Check that kan_cpu_section_execution_t can hold tracy zone data.");

void kan_cpu_section_execution_init (struct kan_cpu_section_execution_t *execution, kan_cpu_section_t section)
{
    struct section_node_t *section_data = KAN_HANDLE_GET (section);
    *(struct ___tracy_c_zone_context *) execution = ___tracy_emit_zone_begin (&section_data->location, 1);
}

void kan_cpu_section_execution_shutdown (struct kan_cpu_section_execution_t *execution)
{
    ___tracy_emit_zone_end (*(struct ___tracy_c_zone_context *) execution);
}
