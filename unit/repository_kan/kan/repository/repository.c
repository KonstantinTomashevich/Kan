#define _CRT_SECURE_NO_WARNINGS

#include <memory.h>
#include <stddef.h>

#include <kan/api_common/alignment.h>
#include <kan/container/event_queue.h>
#include <kan/container/hash_storage.h>
#include <kan/container/stack_group_allocator.h>
#include <kan/cpu_dispatch/job.h>
#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/repository/meta.h>
#include <kan/repository/repository.h>
#include <kan/threading/atomic.h>

KAN_LOG_DEFINE_CATEGORY (repository);

#define OBSERVATION_BUFFER_ALIGNMENT _Alignof (uint64_t)
#define OBSERVATION_BUFFER_CHUNK_ALIGNMENT _Alignof (uint64_t)

struct observation_buffer_scenario_chunk_t
{
    uint64_t source_offset;
    uint64_t size;
    uint64_t flags;
};

struct copy_out_t
{
    uint64_t source_offset;
    uint64_t target_offset;
    uint64_t size;
};

struct observation_buffer_definition_t
{
    uint64_t buffer_size;
    void *buffer;

    uint64_t scenario_chunks_count;
    struct observation_buffer_scenario_chunk_t *scenario_chunks;
};

struct observation_event_trigger_t
{
    uint64_t flags;
    struct kan_repository_event_insert_query_t event_insert_query;
    uint64_t buffer_copy_outs_count;
    uint64_t record_copy_outs_count;
    struct copy_out_t copy_outs[];
};

struct observation_event_triggers_definition_t
{
    uint64_t event_triggers_count;
    struct observation_event_trigger_t *event_triggers;
};

struct singleton_storage_node_t
{
    struct kan_hash_storage_node_t node;
    kan_allocation_group_t allocation_group;
    const struct kan_reflection_struct_t *type;
    struct kan_atomic_int_t queries_count;
    void *singleton;

    struct observation_buffer_definition_t observation_buffer;
    struct observation_event_triggers_definition_t observation_events_triggers;

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
    struct kan_atomic_int_t safeguard_access_status;
#endif
};

struct singleton_read_query_t
{
    struct singleton_storage_node_t *storage;
};

_Static_assert (sizeof (struct singleton_read_query_t) <= sizeof (struct kan_repository_singleton_read_query_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct singleton_read_query_t) <= _Alignof (struct kan_repository_singleton_read_query_t),
                "Query alignments match.");

struct singleton_write_query_t
{
    struct singleton_storage_node_t *storage;
};

_Static_assert (sizeof (struct singleton_write_query_t) <= sizeof (struct kan_repository_singleton_write_query_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct singleton_write_query_t) <= _Alignof (struct kan_repository_singleton_write_query_t),
                "Query alignments match.");

struct event_queue_node_t
{
    struct kan_event_queue_node_t node;
    void *event;
};

struct event_storage_node_t
{
    struct kan_hash_storage_node_t node;
    kan_allocation_group_t allocation_group;
    const struct kan_reflection_struct_t *type;
    struct kan_atomic_int_t queries_count;
    struct kan_atomic_int_t single_threaded_operations_lock;
    struct kan_event_queue_t event_queue;

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
    struct kan_atomic_int_t safeguard_access_status;
#endif
};

struct event_insert_query_t
{
    struct event_storage_node_t *storage;
};

_Static_assert (sizeof (struct event_insert_query_t) <= sizeof (struct kan_repository_event_insert_query_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct event_insert_query_t) <= _Alignof (struct kan_repository_event_insert_query_t),
                "Query alignments match.");

struct event_insertion_package_t
{
    struct event_storage_node_t *storage;
    void *event;
};

_Static_assert (sizeof (struct event_insertion_package_t) <= sizeof (struct kan_repository_event_insertion_package_t),
                "Insertion package sizes match.");
_Static_assert (_Alignof (struct event_insertion_package_t) <=
                    _Alignof (struct kan_repository_event_insertion_package_t),
                "Insertion package alignments match.");

struct event_fetch_query_t
{
    struct event_storage_node_t *storage;
    kan_event_queue_iterator_t iterator;
};

_Static_assert (sizeof (struct event_fetch_query_t) <= sizeof (struct kan_repository_event_fetch_query_t),
                "Query sizes match.");
_Static_assert (_Alignof (struct event_fetch_query_t) <= _Alignof (struct kan_repository_event_fetch_query_t),
                "Query alignments match.");

struct event_read_access_t
{
    struct event_storage_node_t *storage;
    kan_event_queue_iterator_t iterator;
};

_Static_assert (sizeof (struct event_read_access_t) <= sizeof (struct kan_repository_event_read_access_t),
                "Read access sizes match.");
_Static_assert (_Alignof (struct event_read_access_t) <= _Alignof (struct kan_repository_event_read_access_t),
                "Read access alignments match.");

enum repository_mode_t
{
    REPOSITORY_MODE_PLANNING,
    REPOSITORY_MODE_SERVING,
};

struct repository_t
{
    struct repository_t *parent;
    struct repository_t *next;
    struct repository_t *first;

    kan_interned_string_t name;
    kan_reflection_registry_t registry;
    kan_allocation_group_t allocation_group;
    enum repository_mode_t mode;

    struct kan_hash_storage_t singleton_storages;
    struct kan_hash_storage_t event_storages;
};

struct migration_context_t
{
    struct kan_stack_group_allocator_t allocator;
    struct kan_cpu_task_list_node_t *task_list;
    kan_interned_string_t task_name;
};

struct record_migration_user_data_t
{
    void **record_pointer;
    kan_allocation_group_t allocation_group;
    kan_bool_t batched_allocation;
    kan_reflection_struct_migrator_t migrator;
    const struct kan_reflection_struct_t *old_type;
    const struct kan_reflection_struct_t *new_type;
};

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
KAN_LOG_DEFINE_CATEGORY (repository_safeguards);

static kan_bool_t safeguard_singleton_write_access_try_create (struct singleton_storage_node_t *singleton_storage)
{
    while (KAN_TRUE)
    {
        int old_value = kan_atomic_int_get (&singleton_storage->safeguard_access_status);
        if (old_value < 0)
        {
            KAN_LOG (repository_safeguards, KAN_LOG_ERROR,
                     "Singleton type \"%s\". Unable to create write access because read accesses detected.",
                     singleton_storage->type->name);
            return KAN_FALSE;
        }

        int new_value = old_value + 1;
        if (kan_atomic_int_compare_and_set (&singleton_storage->safeguard_access_status, old_value, new_value))
        {
            return KAN_TRUE;
        }
    }
}

static void safeguard_singleton_write_access_destroyed (struct singleton_storage_node_t *singleton_storage)
{
    kan_atomic_int_add (&singleton_storage->safeguard_access_status, -1);
}

static kan_bool_t safeguard_singleton_read_access_try_create (struct singleton_storage_node_t *singleton_storage)
{
    while (KAN_TRUE)
    {
        int old_value = kan_atomic_int_get (&singleton_storage->safeguard_access_status);
        if (old_value > 0)
        {
            KAN_LOG (repository_safeguards, KAN_LOG_ERROR,
                     "Singleton type \"%s\". Unable to create read access because write detected.",
                     singleton_storage->type->name);
            return KAN_FALSE;
        }

        int new_value = old_value - 1;
        if (kan_atomic_int_compare_and_set (&singleton_storage->safeguard_access_status, old_value, new_value))
        {
            return KAN_TRUE;
        }
    }
}

static void safeguard_singleton_read_access_destroyed (struct singleton_storage_node_t *singleton_storage)
{
    kan_atomic_int_add (&singleton_storage->safeguard_access_status, 1);
}

static kan_bool_t safeguard_event_insertion_package_try_create (struct event_storage_node_t *event_storage)
{
    while (KAN_TRUE)
    {
        int old_value = kan_atomic_int_get (&event_storage->safeguard_access_status);
        if (old_value < 0)
        {
            KAN_LOG (repository_safeguards, KAN_LOG_ERROR,
                     "Event type \"%s\". Unable to create event insertion package because existing event read accesses "
                     "detected.",
                     event_storage->type->name);
            return KAN_FALSE;
        }

        int new_value = old_value + 1;
        if (kan_atomic_int_compare_and_set (&event_storage->safeguard_access_status, old_value, new_value))
        {
            return KAN_TRUE;
        }
    }
}

static void safeguard_event_insertion_package_destroyed (struct event_storage_node_t *event_storage)
{
    kan_atomic_int_add (&event_storage->safeguard_access_status, -1);
}

static kan_bool_t safeguard_event_read_access_try_create (struct event_storage_node_t *event_storage)
{
    while (KAN_TRUE)
    {
        int old_value = kan_atomic_int_get (&event_storage->safeguard_access_status);
        if (old_value > 0)
        {
            KAN_LOG (repository_safeguards, KAN_LOG_ERROR,
                     "Event type \"%s\". Unable to create event read access because existing event insertion packages "
                     "detected.",
                     event_storage->type->name);
            return KAN_FALSE;
        }

        int new_value = old_value - 1;
        if (kan_atomic_int_compare_and_set (&event_storage->safeguard_access_status, old_value, new_value))
        {
            return KAN_TRUE;
        }
    }
}

static void safeguard_event_read_access_destroyed (struct event_storage_node_t *event_storage)
{
    kan_atomic_int_add (&event_storage->safeguard_access_status, 1);
}
#endif

static kan_allocation_group_t storage_event_automation_allocation_group (
    kan_allocation_group_t storage_allocation_group)
{
    return kan_allocation_group_get_child (storage_allocation_group, "event_automation");
}

static void apply_copy_outs (uint64_t copy_outs_count, struct copy_out_t *copy_outs, const void *source, void *target)
{
    for (uint64_t index = 0u; index < copy_outs_count; ++index)
    {
        struct copy_out_t *copy_out = &copy_outs[index];
        memcpy ((uint8_t *) target + copy_out->target_offset, (uint8_t *) source + copy_out->source_offset,
                copy_out->size);
    }
}

static void observation_buffer_definition_init (struct observation_buffer_definition_t *definition)
{
    definition->buffer_size = 0u;
    definition->buffer = NULL;
    definition->scenario_chunks_count = 0u;
    definition->scenario_chunks = NULL;
}

static void observation_buffer_definition_import (struct observation_buffer_definition_t *definition, void *record)
{
    if (!definition->buffer)
    {
        return;
    }

    KAN_ASSERT (definition->buffer_size > 0u)
    KAN_ASSERT (definition->scenario_chunks_count > 0u)
    KAN_ASSERT (definition->scenario_chunks)

    uint8_t *output = (uint8_t *) definition->buffer;
    const struct observation_buffer_scenario_chunk_t *chunk = definition->scenario_chunks;
    const struct observation_buffer_scenario_chunk_t *end =
        definition->scenario_chunks + definition->scenario_chunks_count;

    while (chunk != end)
    {
        memcpy (output, (uint8_t *) record + chunk->source_offset, chunk->size);
        output += kan_apply_alignment (chunk->size, OBSERVATION_BUFFER_CHUNK_ALIGNMENT);
        ++chunk;
    }

    KAN_ASSERT ((uint64_t) (output - (uint8_t *) definition->buffer) == definition->buffer_size)
}

static uint64_t observation_buffer_definition_compare (struct observation_buffer_definition_t *definition, void *record)
{
    if (!definition->buffer)
    {
        return 0u;
    }

    uint64_t result = 0u;
    const uint8_t *input = (uint8_t *) definition->buffer;
    const struct observation_buffer_scenario_chunk_t *chunk = definition->scenario_chunks;
    const struct observation_buffer_scenario_chunk_t *end =
        definition->scenario_chunks + definition->scenario_chunks_count;

    while (chunk != end)
    {
        if (memcmp (input, (uint8_t *) record + chunk->source_offset, chunk->size) != 0)
        {
            result |= chunk->flags;
        }

        input += kan_apply_alignment (chunk->size, OBSERVATION_BUFFER_CHUNK_ALIGNMENT);
        ++chunk;
    }

    return result;
}

static void observation_buffer_definition_shutdown (struct observation_buffer_definition_t *definition,
                                                    kan_allocation_group_t allocation_group)
{
    if (definition->buffer)
    {
        kan_free_general (allocation_group, definition->buffer, definition->buffer_size);
        definition->buffer = NULL;
    }

    definition->buffer_size = 0u;
    if (definition->scenario_chunks)
    {
        kan_free_general (allocation_group, definition->scenario_chunks,
                          definition->scenario_chunks_count * sizeof (struct observation_buffer_scenario_chunk_t));
        definition->scenario_chunks = NULL;
    }

    definition->scenario_chunks_count = 0u;
}

static struct observation_event_trigger_t *observation_event_trigger_next (struct observation_event_trigger_t *trigger)
{
    uint8_t *raw_pointer = (uint8_t *) trigger;
    const uint64_t trigger_size =
        sizeof (struct observation_event_trigger_t) +
        (trigger->buffer_copy_outs_count + trigger->record_copy_outs_count) * sizeof (struct copy_out_t);

    return (struct observation_event_trigger_t *) (raw_pointer +
                                                   kan_apply_alignment (trigger_size,
                                                                        _Alignof (struct observation_event_trigger_t)));
}

static void observation_event_triggers_definition_init (struct observation_event_triggers_definition_t *definition)
{
    definition->event_triggers_count = 0u;
    definition->event_triggers = NULL;
}

static void observation_event_triggers_definition_fire (struct observation_event_triggers_definition_t *definition,
                                                        uint64_t flags,
                                                        struct observation_buffer_definition_t *buffer,
                                                        void *record)
{
    if (!definition->event_triggers)
    {
        return;
    }

    KAN_ASSERT (definition->event_triggers_count > 0u)
    KAN_ASSERT (buffer->buffer_size > 0u)
    KAN_ASSERT (buffer->buffer)

    struct observation_event_trigger_t *current_trigger = definition->event_triggers;
    for (uint64_t trigger_index = 0u; trigger_index < definition->event_triggers_count; ++trigger_index)
    {
        if (current_trigger->flags & flags)
        {
            struct kan_repository_event_insertion_package_t package =
                kan_repository_event_insert_query_execute (&current_trigger->event_insert_query);
            void *event = kan_repository_event_insertion_package_get (&package);

            if (event)
            {
                apply_copy_outs (current_trigger->buffer_copy_outs_count, current_trigger->copy_outs, buffer->buffer,
                                 event);
                apply_copy_outs (current_trigger->record_copy_outs_count,
                                 current_trigger->copy_outs + current_trigger->buffer_copy_outs_count, record, event);
                kan_repository_event_insertion_package_submit (&package);
            }
        }

        if (trigger_index != definition->event_triggers_count - 1u)
        {
            current_trigger = observation_event_trigger_next (current_trigger);
        }
    }
}

static void observation_event_triggers_definition_shutdown (struct observation_event_triggers_definition_t *definition,
                                                            kan_allocation_group_t allocation_group)
{
    if (definition->event_triggers)
    {
        struct observation_event_trigger_t *first_trigger = definition->event_triggers;
        struct observation_event_trigger_t *current_trigger = first_trigger;

        for (uint64_t trigger_index = 0u; trigger_index < definition->event_triggers_count; ++trigger_index)
        {
            kan_repository_event_insert_query_shutdown (&current_trigger->event_insert_query);
            current_trigger = observation_event_trigger_next (current_trigger);
        }

        uint64_t triggers_size = (uint8_t *) current_trigger - (uint8_t *) first_trigger;
        kan_free_general (allocation_group, definition->event_triggers, triggers_size);
        definition->event_triggers = NULL;
    }

    definition->event_triggers_count = 0u;
}

static void singleton_storage_node_shutdown_and_free (struct singleton_storage_node_t *node,
                                                      struct repository_t *repository)
{
    KAN_ASSERT (kan_atomic_int_get (&node->queries_count) == 0)
    KAN_ASSERT (node->singleton)

    if (node->type->shutdown)
    {
        node->type->shutdown (node->type->functor_user_data, node->singleton);
    }

    kan_free_general (node->allocation_group, node->singleton, node->type->size);
    observation_buffer_definition_shutdown (&node->observation_buffer, node->allocation_group);
    observation_event_triggers_definition_shutdown (&node->observation_events_triggers, node->allocation_group);

    kan_hash_storage_remove (&repository->singleton_storages, &node->node);
    kan_free_batched (node->allocation_group, node);
}

static void event_queue_node_shutdown_and_free (struct event_queue_node_t *node, struct event_storage_node_t *storage)
{
    KAN_ASSERT (node->event || &node->node == storage->event_queue.next_placeholder)
    if (node->event)
    {
        if (storage->type->shutdown)
        {
            storage->type->shutdown (storage->type->functor_user_data, node->event);
        }

        kan_free_batched (storage->allocation_group, node->event);
    }

    kan_free_batched (storage->allocation_group, node);
}

static void event_storage_node_shutdown_and_free (struct event_storage_node_t *node, struct repository_t *repository)
{
    KAN_ASSERT (kan_atomic_int_get (&node->queries_count) == 0)
    struct event_queue_node_t *queue_node = (struct event_queue_node_t *) node->event_queue.oldest;

    while (queue_node)
    {
        event_queue_node_shutdown_and_free (queue_node, node);
        queue_node = (struct event_queue_node_t *) queue_node->node.next;
    }

    kan_hash_storage_remove (&repository->singleton_storages, &node->node);
    kan_free_batched (node->allocation_group, node);
}

static struct repository_t *repository_create (kan_allocation_group_t allocation_group,
                                               struct repository_t *parent,
                                               kan_interned_string_t name,
                                               kan_reflection_registry_t registry)
{
    struct repository_t *repository = (struct repository_t *) kan_allocate_general (
        allocation_group, sizeof (struct repository_t), _Alignof (struct repository_t));
    repository->parent = parent;
    repository->first = NULL;

    if (parent)
    {
        repository->next = parent->first;
        parent->first = repository;
    }
    else
    {
        repository->next = NULL;
    }

    repository->name = name;
    repository->registry = registry;
    repository->allocation_group = allocation_group;
    repository->mode = REPOSITORY_MODE_PLANNING;

    kan_hash_storage_init (&repository->singleton_storages, repository->allocation_group,
                           KAN_REPOSITORY_SINGLETON_STORAGE_INITIAL_BUCKETS);

    kan_hash_storage_init (&repository->event_storages, repository->allocation_group,
                           KAN_REPOSITORY_EVENT_STORAGE_INITIAL_BUCKETS);
    return repository;
}

kan_repository_t kan_repository_create_root (kan_allocation_group_t allocation_group,
                                             kan_reflection_registry_t registry)
{
    return (kan_repository_t) repository_create (allocation_group, NULL, kan_string_intern ("root"), registry);
}

kan_repository_t kan_repository_create_child (kan_repository_t parent, kan_interned_string_t name)
{
    struct repository_t *parent_repository = (struct repository_t *) parent;
    const kan_allocation_group_t allocation_group = kan_allocation_group_get_child (
        kan_allocation_group_get_child (parent_repository->allocation_group, "child_repositories"), name);

    return (kan_repository_t) repository_create (allocation_group, parent_repository, name,
                                                 parent_repository->registry);
}

static void repository_enter_planning_mode_internal (struct repository_t *repository)
{
    KAN_ASSERT (repository->mode == REPOSITORY_MODE_SERVING)
    repository->mode = REPOSITORY_MODE_PLANNING;

    struct singleton_storage_node_t *singleton_storage_node =
        (struct singleton_storage_node_t *) repository->singleton_storages.items.first;

    while (singleton_storage_node)
    {
#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        if (kan_atomic_int_get (&singleton_storage_node->safeguard_access_status) != 0)
        {
            KAN_LOG (repository_safeguard, KAN_LOG_ERROR,
                     "Unsafe switch to planning mode. Singleton \"%s\" is still accessed.",
                     singleton_storage_node->type->name);
        }
#endif

        const kan_allocation_group_t automatic_events_group =
            storage_event_automation_allocation_group (singleton_storage_node->allocation_group);

        observation_buffer_definition_shutdown (&singleton_storage_node->observation_buffer, automatic_events_group);
        observation_event_triggers_definition_shutdown (&singleton_storage_node->observation_events_triggers,
                                                        automatic_events_group);

        singleton_storage_node = (struct singleton_storage_node_t *) singleton_storage_node->node.list_node.next;
    }

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
    struct event_storage_node_t *event_storage_node =
        (struct event_storage_node_t *) repository->event_storages.items.first;

    while (event_storage_node)
    {
        if (kan_atomic_int_get (&event_storage_node->safeguard_access_status) != 0)
        {
            KAN_LOG (repository_safeguard, KAN_LOG_ERROR,
                     "Unsafe switch to planning mode. Events \"%s\" are still accessed.",
                     singleton_storage_node->type->name);
        }

        event_storage_node = (struct event_storage_node_t *) event_storage_node->node.list_node.next;
    }
#endif

    repository = repository->first;
    while (repository)
    {
        repository_enter_planning_mode_internal (repository);
        repository = repository->next;
    }
}

void kan_repository_enter_planning_mode (kan_repository_t root_repository)
{
    struct repository_t *repository = (struct repository_t *) root_repository;
    KAN_ASSERT (!repository->parent)
    repository_enter_planning_mode_internal (repository);
}

static void execute_migration (uint64_t user_data)
{
    struct record_migration_user_data_t *data = (struct record_migration_user_data_t *) user_data;
    void *old_object = *data->record_pointer;

    void *new_object = data->batched_allocation ? kan_allocate_batched (data->allocation_group, data->new_type->size) :
                                                  kan_allocate_general (data->allocation_group, data->new_type->size,
                                                                        data->new_type->alignment);

    if (data->new_type->init)
    {
        data->new_type->init (data->new_type->functor_user_data, new_object);
    }

    kan_reflection_struct_migrator_migrate_instance (data->migrator, data->new_type->name, old_object, new_object);
    if (data->old_type->shutdown)
    {
        data->old_type->shutdown (data->old_type->functor_user_data, old_object);
    }

    if (data->batched_allocation)
    {
        kan_free_batched (data->allocation_group, old_object);
    }
    else
    {
        kan_free_general (data->allocation_group, old_object, data->old_type->size);
    }

    *data->record_pointer = new_object;
}

static struct record_migration_user_data_t *allocate_migration_user_data (struct migration_context_t *context)
{
    return (struct record_migration_user_data_t *) kan_stack_group_allocator_allocate (
        &context->allocator, sizeof (struct record_migration_user_data_t),
        _Alignof (struct record_migration_user_data_t));
}

static void spawn_migration_task (struct migration_context_t *context, struct record_migration_user_data_t *user_data)
{
    struct kan_cpu_task_list_node_t *node = (struct kan_cpu_task_list_node_t *) kan_stack_group_allocator_allocate (
        &context->allocator, sizeof (struct kan_cpu_task_list_node_t), _Alignof (struct kan_cpu_task_list_node_t));

    node->queue = KAN_CPU_DISPATCH_QUEUE_FOREGROUND;
    node->task = (struct kan_cpu_task_t) {
        .name = context->task_name,
        .function = execute_migration,
        .user_data = (uint64_t) user_data,
    };

    node->next = context->task_list;
    context->task_list = node;
}

static void repository_migrate_internal (struct repository_t *repository,
                                         struct migration_context_t *context,
                                         kan_reflection_registry_t new_registry,
                                         kan_reflection_migration_seed_t migration_seed,
                                         kan_reflection_struct_migrator_t migrator)
{
    KAN_ASSERT (repository->mode == REPOSITORY_MODE_PLANNING)
    struct singleton_storage_node_t *singleton_storage_node =
        (struct singleton_storage_node_t *) repository->singleton_storages.items.first;

    while (singleton_storage_node)
    {
        struct singleton_storage_node_t *next =
            (struct singleton_storage_node_t *) singleton_storage_node->node.list_node.next;

        const struct kan_reflection_struct_t *old_type = singleton_storage_node->type;
        const struct kan_reflection_struct_t *new_type =
            kan_reflection_registry_query_struct (new_registry, old_type->name);
        singleton_storage_node->type = new_type;

        const struct kan_reflection_struct_migration_seed_t *seed =
            kan_reflection_migration_seed_get_for_struct (migration_seed, old_type->name);

        switch (seed->status)
        {
        case KAN_REFLECTION_MIGRATION_NEEDED:
        {
            struct record_migration_user_data_t *user_data = allocate_migration_user_data (context);
            *user_data = (struct record_migration_user_data_t) {
                .record_pointer = &singleton_storage_node->singleton,
                .allocation_group = singleton_storage_node->allocation_group,
                .batched_allocation = KAN_FALSE,
                .migrator = migrator,
                .old_type = old_type,
                .new_type = new_type,
            };

            spawn_migration_task (context, user_data);
            break;
        }

        case KAN_REFLECTION_MIGRATION_NOT_NEEDED:
            // No migration, therefore don't need to do anything.
            break;

        case KAN_REFLECTION_MIGRATION_REMOVED:
            singleton_storage_node_shutdown_and_free (singleton_storage_node, repository);
            break;
        }

        singleton_storage_node = next;
    }

    struct event_storage_node_t *event_storage_node =
        (struct event_storage_node_t *) repository->event_storages.items.first;

    while (event_storage_node)
    {
        struct event_storage_node_t *next = (struct event_storage_node_t *) event_storage_node->node.list_node.next;

        const struct kan_reflection_struct_t *old_type = event_storage_node->type;
        const struct kan_reflection_struct_t *new_type =
            kan_reflection_registry_query_struct (new_registry, old_type->name);
        event_storage_node->type = new_type;

        const struct kan_reflection_struct_migration_seed_t *seed =
            kan_reflection_migration_seed_get_for_struct (migration_seed, old_type->name);

        switch (seed->status)
        {
        case KAN_REFLECTION_MIGRATION_NEEDED:
        {
            struct event_queue_node_t *node = (struct event_queue_node_t *) event_storage_node->event_queue.oldest;
            while (&node->node != event_storage_node->event_queue.next_placeholder)
            {
                struct record_migration_user_data_t *user_data = allocate_migration_user_data (context);
                *user_data = (struct record_migration_user_data_t) {
                    .record_pointer = &node->event,
                    .allocation_group = event_storage_node->allocation_group,
                    .batched_allocation = KAN_TRUE,
                    .migrator = migrator,
                    .old_type = old_type,
                    .new_type = new_type,
                };

                spawn_migration_task (context, user_data);
                node = (struct event_queue_node_t *) node->node.next;
            }

            break;
        }

        case KAN_REFLECTION_MIGRATION_NOT_NEEDED:
            // No migration, therefore don't need to do anything.
            break;

        case KAN_REFLECTION_MIGRATION_REMOVED:
            event_storage_node_shutdown_and_free (event_storage_node, repository);
            break;
        }

        event_storage_node = next;
    }

    repository->registry = new_registry;
    repository = repository->first;

    while (repository)
    {
        repository_migrate_internal (repository, context, new_registry, migration_seed, migrator);
        repository = repository->next;
    }
}

void kan_repository_migrate (kan_repository_t root_repository,
                             kan_reflection_registry_t new_registry,
                             kan_reflection_migration_seed_t migration_seed,
                             kan_reflection_struct_migrator_t migrator)
{
    struct repository_t *repository = (struct repository_t *) root_repository;
    KAN_ASSERT (!repository->parent)
    kan_cpu_job_t job = kan_cpu_job_create ();
    KAN_ASSERT (job != KAN_INVALID_CPU_JOB)

    struct migration_context_t context;
    kan_stack_group_allocator_init (&context.allocator,
                                    kan_allocation_group_get_child (repository->allocation_group, "migration"),
                                    KAN_REPOSITORY_MIGRATION_STACK_INITIAL_SIZE);

    context.task_list = NULL;
    context.task_name = kan_string_intern ("repository_migration_instance");
    repository_migrate_internal (repository, &context, new_registry, migration_seed, migrator);

    if (context.task_list)
    {
        kan_cpu_job_dispatch_task_list (job, context.task_list);
        while (context.task_list)
        {
            KAN_ASSERT (context.task_list->dispatch_handle != KAN_INVALID_CPU_TASK_HANDLE)
            kan_cpu_task_detach (context.task_list->dispatch_handle);
            context.task_list = context.task_list->next;
        }
    }

    kan_cpu_job_wait (job);
    kan_stack_group_shutdown (&context.allocator);
}

static struct singleton_storage_node_t *query_singleton_storage_across_hierarchy (struct repository_t *repository,
                                                                                  kan_interned_string_t type_name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&repository->singleton_storages, (uint64_t) type_name);
    struct singleton_storage_node_t *node = (struct singleton_storage_node_t *) bucket->first;
    const struct singleton_storage_node_t *end =
        (struct singleton_storage_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != end)
    {
        if (node->type->name == type_name)
        {
            return node;
        }

        node = (struct singleton_storage_node_t *) node->node.list_node.next;
    }

    return repository->parent ? query_singleton_storage_across_hierarchy (repository->parent, type_name) : NULL;
}

kan_repository_singleton_storage_t kan_repository_singleton_storage_open (kan_repository_t repository,
                                                                          kan_interned_string_t type_name)
{
    struct repository_t *repository_data = (struct repository_t *) repository;
    KAN_ASSERT (repository_data->mode == REPOSITORY_MODE_PLANNING)
    struct singleton_storage_node_t *storage = query_singleton_storage_across_hierarchy (repository_data, type_name);

    if (!storage)
    {
        const struct kan_reflection_struct_t *singleton_type =
            kan_reflection_registry_query_struct (repository_data->registry, type_name);

        if (!singleton_type)
        {
            KAN_LOG (repository, KAN_LOG_ERROR, "Singleton type \"%s\" not found.", type_name);
            return KAN_INVALID_REPOSITORY_SINGLETON_STORAGE;
        }

        const kan_allocation_group_t storage_allocation_group = kan_allocation_group_get_child (
            kan_allocation_group_get_child (repository_data->allocation_group, "singletons"), type_name);

        storage = (struct singleton_storage_node_t *) kan_allocate_batched (storage_allocation_group,
                                                                            sizeof (struct singleton_storage_node_t));

        storage->node.hash = (uint64_t) type_name;
        storage->allocation_group = storage_allocation_group;
        storage->type = singleton_type;
        storage->queries_count = kan_atomic_int_init (0);

        storage->singleton =
            kan_allocate_general (storage_allocation_group, storage->type->size, storage->type->alignment);

        if (storage->type->init)
        {
            storage->type->init (storage->type->functor_user_data, storage->singleton);
        }

        observation_buffer_definition_init (&storage->observation_buffer);
        observation_event_triggers_definition_init (&storage->observation_events_triggers);

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        storage->safeguard_access_status = kan_atomic_int_init (0);
#endif

        if (repository_data->singleton_storages.bucket_count * KAN_REPOSITORY_SINGLETON_STORAGE_LOAD_FACTOR >=
            repository_data->singleton_storages.items.size)
        {
            kan_hash_storage_set_bucket_count (&repository_data->singleton_storages,
                                               repository_data->singleton_storages.bucket_count * 2u);
        }

        kan_hash_storage_add (&repository_data->singleton_storages, &storage->node);
    }

    return (kan_repository_singleton_storage_t) storage;
}

void kan_repository_singleton_read_query_init (struct kan_repository_singleton_read_query_t *query,
                                               kan_repository_singleton_storage_t storage)
{
    struct singleton_storage_node_t *storage_data = (struct singleton_storage_node_t *) storage;
    kan_atomic_int_add (&storage_data->queries_count, 1);
    *(struct singleton_read_query_t *) query = (struct singleton_read_query_t) {.storage = storage_data};
}

kan_repository_singleton_read_access_t kan_repository_singleton_read_query_execute (
    struct kan_repository_singleton_read_query_t *query)
{
    struct singleton_read_query_t *query_data = (struct singleton_read_query_t *) query;
    KAN_ASSERT (query_data->storage)

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
    if (!safeguard_singleton_read_access_try_create (query_data->storage))
    {
        return (kan_repository_singleton_read_access_t) NULL;
    }
#endif

    return (kan_repository_singleton_read_access_t) query_data->storage;
}

const void *kan_repository_singleton_read_access_resolve (kan_repository_singleton_read_access_t access)
{
    struct singleton_storage_node_t *storage = (struct singleton_storage_node_t *) access;
    return storage ? storage->singleton : NULL;
}

void kan_repository_singleton_read_access_close (kan_repository_singleton_read_access_t access)
{
#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
    struct singleton_storage_node_t *storage = (struct singleton_storage_node_t *) access;
    if (storage)
    {
        safeguard_singleton_read_access_destroyed (storage);
    }
#endif
}

void kan_repository_singleton_read_query_shutdown (struct kan_repository_singleton_read_query_t *query)
{
    struct singleton_read_query_t *query_data = (struct singleton_read_query_t *) query;
    if (query_data->storage)
    {
        kan_atomic_int_add (&query_data->storage->queries_count, -1);
        query_data->storage = NULL;
    }
}

void kan_repository_singleton_write_query_init (struct kan_repository_singleton_write_query_t *query,
                                                kan_repository_singleton_storage_t storage)
{
    struct singleton_storage_node_t *storage_data = (struct singleton_storage_node_t *) storage;
    kan_atomic_int_add (&storage_data->queries_count, 1);
    *(struct singleton_write_query_t *) query = (struct singleton_write_query_t) {.storage = storage_data};
}

kan_repository_singleton_write_access_t kan_repository_singleton_write_query_execute (
    struct kan_repository_singleton_write_query_t *query)
{
    struct singleton_write_query_t *query_data = (struct singleton_write_query_t *) query;
    KAN_ASSERT (query_data->storage)

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
    if (!safeguard_singleton_write_access_try_create (query_data->storage))
    {
        return (kan_repository_singleton_write_access_t) NULL;
    }
#endif

    observation_buffer_definition_import (&query_data->storage->observation_buffer, query_data->storage->singleton);
    return (kan_repository_singleton_write_access_t) query_data->storage;
}

void *kan_repository_singleton_write_access_resolve (kan_repository_singleton_write_access_t access)
{
    struct singleton_storage_node_t *storage = (struct singleton_storage_node_t *) access;
    return storage ? storage->singleton : NULL;
}

void kan_repository_singleton_write_access_close (kan_repository_singleton_write_access_t access)
{
    struct singleton_storage_node_t *storage = (struct singleton_storage_node_t *) access;
    if (storage)
    {
        const uint64_t change_flags =
            observation_buffer_definition_compare (&storage->observation_buffer, storage->singleton);
        if (change_flags != 0u)
        {
            observation_event_triggers_definition_fire (&storage->observation_events_triggers, change_flags,
                                                        &storage->observation_buffer, storage->singleton);
        }

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        safeguard_singleton_write_access_destroyed (storage);
#endif
    }
}

void kan_repository_singleton_write_query_shutdown (struct kan_repository_singleton_write_query_t *query)
{
    struct singleton_write_query_t *query_data = (struct singleton_write_query_t *) query;
    if (query_data->storage)
    {
        kan_atomic_int_add (&query_data->storage->queries_count, -1);
        query_data->storage = NULL;
    }
}

static struct event_storage_node_t *query_event_storage_across_hierarchy (struct repository_t *repository,
                                                                          kan_interned_string_t type_name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&repository->event_storages, (uint64_t) type_name);
    struct event_storage_node_t *node = (struct event_storage_node_t *) bucket->first;
    const struct event_storage_node_t *end = (struct event_storage_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != end)
    {
        if (node->type->name == type_name)
        {
            return node;
        }

        node = (struct event_storage_node_t *) node->node.list_node.next;
    }

    return repository->parent ? query_event_storage_across_hierarchy (repository->parent, type_name) : NULL;
}

static struct event_queue_node_t *event_queue_node_allocate (kan_allocation_group_t storage_allocation_group)
{
    struct event_queue_node_t *node = (struct event_queue_node_t *) kan_allocate_batched (
        storage_allocation_group, sizeof (struct event_queue_node_t));
    node->event = NULL;
    return node;
}

kan_repository_event_storage_t kan_repository_event_storage_open (kan_repository_t repository,
                                                                  kan_interned_string_t type_name)
{
    struct repository_t *repository_data = (struct repository_t *) repository;
    KAN_ASSERT (repository_data->mode == REPOSITORY_MODE_PLANNING)
    struct event_storage_node_t *storage = query_event_storage_across_hierarchy (repository_data, type_name);

    if (!storage)
    {
        const struct kan_reflection_struct_t *event_type =
            kan_reflection_registry_query_struct (repository_data->registry, type_name);

        if (!event_type)
        {
            KAN_LOG (repository, KAN_LOG_ERROR, "Event type \"%s\" not found.", type_name);
            return KAN_INVALID_REPOSITORY_EVENT_STORAGE;
        }

        const kan_allocation_group_t storage_allocation_group = kan_allocation_group_get_child (
            kan_allocation_group_get_child (repository_data->allocation_group, "events"), type_name);

        storage = (struct event_storage_node_t *) kan_allocate_batched (storage_allocation_group,
                                                                        sizeof (struct event_storage_node_t));

        storage->node.hash = (uint64_t) type_name;
        storage->allocation_group = storage_allocation_group;
        storage->type = event_type;
        storage->single_threaded_operations_lock = kan_atomic_int_init (0);
        storage->queries_count = kan_atomic_int_init (0);
        kan_event_queue_init (&storage->event_queue, &event_queue_node_allocate (storage_allocation_group)->node);

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        storage->safeguard_access_status = kan_atomic_int_init (0);
#endif

        if (repository_data->event_storages.bucket_count * KAN_REPOSITORY_EVENT_STORAGE_LOAD_FACTOR >=
            repository_data->event_storages.items.size)
        {
            kan_hash_storage_set_bucket_count (&repository_data->event_storages,
                                               repository_data->event_storages.bucket_count * 2u);
        }

        kan_hash_storage_add (&repository_data->event_storages, &storage->node);
    }

    return (kan_repository_event_storage_t) storage;
}

void kan_repository_event_insert_query_init (struct kan_repository_event_insert_query_t *query,
                                             kan_repository_event_storage_t storage)
{
    struct event_storage_node_t *storage_data = (struct event_storage_node_t *) storage;
    kan_atomic_int_add (&storage_data->queries_count, 1);
    *(struct event_insert_query_t *) query = (struct event_insert_query_t) {.storage = storage_data};
}

struct kan_repository_event_insertion_package_t kan_repository_event_insert_query_execute (
    struct kan_repository_event_insert_query_t *query)
{
    struct event_insert_query_t *query_data = (struct event_insert_query_t *) query;
    KAN_ASSERT (query_data->storage)

    struct event_insertion_package_t package;
    package.storage = query_data->storage;

    // No subscribers -- no need to create event. Initial subscribers are fetch queries,
    // therefore if there are any queries, there always will be more than zero iterators.
    if (kan_atomic_int_get (&query_data->storage->event_queue.total_iterators) == 0)
    {
        package.event = NULL;
        return *(struct kan_repository_event_insertion_package_t *) &package;
    }

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
    if (!safeguard_event_insertion_package_try_create (query_data->storage))
    {
        package.event = NULL;
        return *(struct kan_repository_event_insertion_package_t *) &package;
    }
#endif

    package.event = kan_allocate_batched (query_data->storage->allocation_group, query_data->storage->type->size);
    if (query_data->storage->type->init)
    {
        query_data->storage->type->init (query_data->storage->type->functor_user_data, package.event);
    }

    return *(struct kan_repository_event_insertion_package_t *) &package;
}

void *kan_repository_event_insertion_package_get (struct kan_repository_event_insertion_package_t *package)
{
    return ((struct event_insertion_package_t *) package)->event;
}

void kan_repository_event_insertion_package_undo (struct kan_repository_event_insertion_package_t *package)
{
    struct event_insertion_package_t *package_data = (struct event_insertion_package_t *) package;
    KAN_ASSERT (package_data->storage)
    KAN_ASSERT (package_data->event)

    if (package_data->storage->type->shutdown)
    {
        package_data->storage->type->shutdown (package_data->storage->type->functor_user_data, package_data->event);
    }

    kan_free_batched (package_data->storage->allocation_group, package_data->event);

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
    safeguard_event_insertion_package_destroyed (package_data->storage);
#endif
}

void kan_repository_event_insertion_package_submit (struct kan_repository_event_insertion_package_t *package)
{
    struct event_insertion_package_t *package_data = (struct event_insertion_package_t *) package;
    KAN_ASSERT (package_data->storage)
    KAN_ASSERT (package_data->event)

    kan_atomic_int_lock (&package_data->storage->single_threaded_operations_lock);
    struct event_queue_node_t *node =
        (struct event_queue_node_t *) kan_event_queue_submit_begin (&package_data->storage->event_queue);

    KAN_ASSERT (node);
    node->event = package_data->event;

    kan_event_queue_submit_end (&package_data->storage->event_queue,
                                &event_queue_node_allocate (package_data->storage->allocation_group)->node);
    kan_atomic_int_unlock (&package_data->storage->single_threaded_operations_lock);

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
    safeguard_event_insertion_package_destroyed (package_data->storage);
#endif
}

void kan_repository_event_insert_query_shutdown (struct kan_repository_event_insert_query_t *query)
{
    struct event_insert_query_t *query_data = (struct event_insert_query_t *) query;
    if (query_data->storage)
    {
        kan_atomic_int_add (&query_data->storage->queries_count, -1);
        query_data->storage = NULL;
    }
}

void kan_repository_event_fetch_query_init (struct kan_repository_event_fetch_query_t *query,
                                            kan_repository_event_storage_t storage)
{
    struct event_storage_node_t *storage_data = (struct event_storage_node_t *) storage;
    kan_atomic_int_add (&storage_data->queries_count, 1);

    *(struct event_fetch_query_t *) query = (struct event_fetch_query_t) {
        .storage = storage_data,
        .iterator = kan_event_queue_iterator_create (&storage_data->event_queue),
    };
}

struct kan_repository_event_read_access_t kan_repository_event_fetch_query_next (
    struct kan_repository_event_fetch_query_t *query)
{
    struct event_fetch_query_t *query_data = (struct event_fetch_query_t *) query;
    KAN_ASSERT (query_data->storage)
    struct event_read_access_t access;
    access.storage = query_data->storage;

    if (!kan_event_queue_iterator_get (&query_data->storage->event_queue, query_data->iterator))
    {
        access.iterator = KAN_INVALID_EVENT_QUEUE_ITERATOR;
        return *(struct kan_repository_event_read_access_t *) &access;
    }

#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
    if (!safeguard_event_read_access_try_create (query_data->storage))
    {
        access.iterator = KAN_INVALID_EVENT_QUEUE_ITERATOR;
        return *(struct kan_repository_event_read_access_t *) &access;
    }
#endif

    access.iterator = query_data->iterator;
    query_data->iterator =
        kan_event_queue_iterator_create_next (&query_data->storage->event_queue, query_data->iterator);
    return *(struct kan_repository_event_read_access_t *) &access;
}

const void *kan_repository_event_read_access_resolve (struct kan_repository_event_read_access_t *access)
{
    struct event_read_access_t *access_data = (struct event_read_access_t *) access;
    KAN_ASSERT (access_data->storage)

    if (access_data->iterator != KAN_INVALID_EVENT_QUEUE_ITERATOR)
    {
        struct event_queue_node_t *node = (struct event_queue_node_t *) kan_event_queue_iterator_get (
            &access_data->storage->event_queue, access_data->iterator);

        if (node)
        {
            KAN_ASSERT (node->event);
            return node->event;
        }
    }

    return NULL;
}

void kan_repository_event_read_access_close (struct kan_repository_event_read_access_t *access)
{
    struct event_read_access_t *access_data = (struct event_read_access_t *) access;
    KAN_ASSERT (access_data->storage)

    if (access_data->iterator != KAN_INVALID_EVENT_QUEUE_ITERATOR)
    {
#if defined(KAN_REPOSITORY_SAFEGUARDS_ENABLED)
        safeguard_event_read_access_destroyed (access_data->storage);
#endif

        const kan_bool_t should_attempt_cleanup =
            kan_event_queue_iterator_destroy (&access_data->storage->event_queue, access_data->iterator);

        if (should_attempt_cleanup)
        {
            kan_atomic_int_lock (&access_data->storage->single_threaded_operations_lock);
            struct event_queue_node_t *oldest =
                (struct event_queue_node_t *) kan_event_queue_clean_oldest (&access_data->storage->event_queue);

            while (oldest)
            {
                event_queue_node_shutdown_and_free (oldest, access_data->storage);
                oldest =
                    (struct event_queue_node_t *) kan_event_queue_clean_oldest (&access_data->storage->event_queue);
            }

            kan_atomic_int_unlock (&access_data->storage->single_threaded_operations_lock);
        }
    }
}

void kan_repository_event_fetch_query_shutdown (struct kan_repository_event_fetch_query_t *query)
{
    struct event_fetch_query_t *query_data = (struct event_fetch_query_t *) query;
    if (query_data->storage)
    {
        kan_atomic_int_add (&query_data->storage->queries_count, -1);
        query_data->storage = NULL;
    }
}

void kan_repository_enter_serving_mode (kan_repository_t root_repository)
{
    // TODO: Implement. Do not forget about tree hierarchy and storages in parent repositories (especially events).
    // TODO: Clean storages with no queries.
    // TODO: Tasks below might take some time and can be executed in parallel. Attach serving mode enter to cpu jobs?
    // TODO: Connect automatic event storages with singleton and indexed storages. Build copy-outs and such things.
}

static void repository_destroy_internal (struct repository_t *repository)
{
    KAN_ASSERT (repository->mode == REPOSITORY_MODE_PLANNING)
    while (repository->first)
    {
        repository_destroy_internal (repository->first);
    }

    while (repository->singleton_storages.items.first)
    {
        singleton_storage_node_shutdown_and_free (
            (struct singleton_storage_node_t *) repository->singleton_storages.items.first, repository);
    }

    while (repository->event_storages.items.first)
    {
        event_storage_node_shutdown_and_free ((struct event_storage_node_t *) repository->event_storages.items.first,
                                              repository);
    }

    kan_hash_storage_shutdown (&repository->singleton_storages);
    kan_hash_storage_shutdown (&repository->event_storages);

    if (repository->parent)
    {
        if (repository->parent->first == repository)
        {
            repository->parent->first = repository->next;
        }
        else
        {
            struct repository_t *sibling = repository->parent->first;
            while (sibling->next != repository)
            {
                sibling = sibling->next;
                KAN_ASSERT (sibling)
            }

            sibling->next = repository->next;
        }
    }
}

void kan_repository_destroy (kan_repository_t repository)
{
    struct repository_t *repository_data = (struct repository_t *) repository;
    repository_destroy_internal (repository_data);
}
