#include <stddef.h>

#include <kan/container/stack_group_allocator.h>
#include <kan/context/reflection_system.h>
#include <kan/cpu_dispatch/job.h>
#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/threading/atomic.h>

KAN_LOG_DEFINE_CATEGORY (reflection_system);

#if defined(_WIN32)
__declspec (dllimport) void kan_reflection_system_register_statics (kan_reflection_registry_t registry);
#else
void kan_reflection_system_register_statics (kan_reflection_registry_t registry);
#endif

struct populate_connection_node_t
{
    struct populate_connection_node_t *next;
    kan_context_system_handle_t other_system;
    kan_context_reflection_populate_t functor;
};

struct generated_connection_node_t
{
    struct generated_connection_node_t *next;
    kan_context_system_handle_t other_system;
    kan_context_reflection_generated_t functor;
};

struct generation_iterate_connection_node_t
{
    struct generation_iterate_connection_node_t *next;
    kan_context_system_handle_t other_system;
    kan_context_reflection_generation_iterate_t functor;
};

struct enum_event_entry_node_t
{
    struct enum_event_entry_node_t *next;
    const struct kan_reflection_enum_t *data;
};

struct struct_event_entry_node_t
{
    struct struct_event_entry_node_t *next;
    const struct kan_reflection_struct_t *data;
};

struct function_event_entry_node_t
{
    struct function_event_entry_node_t *next;
    const struct kan_reflection_function_t *data;
};

struct top_level_meta_node_t
{
    struct top_level_meta_node_t *next;
    kan_interned_string_t top_level_name;
    kan_interned_string_t meta_type_name;
    void *meta;
};

struct lower_level_meta_node_t
{
    struct lower_level_meta_node_t *next;
    kan_interned_string_t top_level_name;
    kan_interned_string_t lower_level_name;
    kan_interned_string_t meta_type_name;
    void *meta;
};

struct generation_iterator_t
{
    struct generation_context_t *generation_context;
    struct enum_event_entry_node_t *current_added_enum;
    struct struct_event_entry_node_t *current_added_struct;
    struct function_event_entry_node_t *current_added_function;
    struct enum_event_entry_node_t *current_changed_enum;
    struct struct_event_entry_node_t *current_changed_struct;
    struct function_event_entry_node_t *current_changed_function;

    struct top_level_meta_node_t *current_added_enum_meta;
    struct lower_level_meta_node_t *current_added_enum_value_meta;
    struct top_level_meta_node_t *current_added_struct_meta;
    struct lower_level_meta_node_t *current_added_struct_field_meta;
    struct top_level_meta_node_t *current_added_function_meta;
    struct lower_level_meta_node_t *current_added_function_argument_meta;
};

struct generation_context_t
{
    struct kan_atomic_int_t this_iteration_submission_lock;
    struct kan_stack_group_allocator_t temporary_allocator;

    struct enum_event_entry_node_t *first_added_enum_this_iteration;
    struct struct_event_entry_node_t *first_added_struct_this_iteration;
    struct function_event_entry_node_t *first_added_function_this_iteration;
    struct enum_event_entry_node_t *first_changed_enum_this_iteration;
    struct struct_event_entry_node_t *first_changed_struct_this_iteration;
    struct function_event_entry_node_t *first_changed_function_this_iteration;

    struct top_level_meta_node_t *first_added_enum_meta_this_iteration;
    struct lower_level_meta_node_t *first_added_enum_value_meta_this_iteration;
    struct top_level_meta_node_t *first_added_struct_meta_this_iteration;
    struct lower_level_meta_node_t *first_added_struct_field_meta_this_iteration;
    struct top_level_meta_node_t *first_added_function_meta_this_iteration;
    struct lower_level_meta_node_t *first_added_function_argument_meta_this_iteration;

    struct enum_event_entry_node_t *first_added_enum_previous_iteration;
    struct struct_event_entry_node_t *first_added_struct_previous_iteration;
    struct function_event_entry_node_t *first_added_function_previous_iteration;
    struct enum_event_entry_node_t *first_changed_enum_previous_iteration;
    struct struct_event_entry_node_t *first_changed_struct_previous_iteration;
    struct function_event_entry_node_t *first_changed_function_previous_iteration;

    struct top_level_meta_node_t *first_added_enum_meta_previous_iteration;
    struct lower_level_meta_node_t *first_added_enum_value_meta_previous_iteration;
    struct top_level_meta_node_t *first_added_struct_meta_previous_iteration;
    struct lower_level_meta_node_t *first_added_struct_field_meta_previous_iteration;
    struct top_level_meta_node_t *first_added_function_meta_previous_iteration;
    struct lower_level_meta_node_t *first_added_function_argument_meta_previous_iteration;
};

struct generation_iteration_task_user_data_t
{
    struct generation_iterator_t iterator;
    kan_reflection_registry_t new_registry;
    uint64_t iteration_index;
    struct generation_iterate_connection_node_t *node;
};

struct reflection_system_t
{
    struct populate_connection_node_t *first_populate_connection;
    struct generated_connection_node_t *first_generated_connection;
    struct generation_iterate_connection_node_t *first_generation_iterate_connection;
    kan_allocation_group_t group;
    kan_context_handle_t context;
    kan_reflection_registry_t current_registry;
};

static kan_context_system_handle_t reflection_system_create (kan_allocation_group_t group, void *user_config)
{
    struct reflection_system_t *system = (struct reflection_system_t *) kan_allocate_general (
        group, sizeof (struct reflection_system_t), _Alignof (struct reflection_system_t));
    system->first_populate_connection = NULL;
    system->first_generated_connection = NULL;
    system->first_generation_iterate_connection = NULL;
    system->group = group;
    system->current_registry = KAN_INVALID_REFLECTION_REGISTRY;

    return (kan_context_system_handle_t) system;
}

static void call_generation_iterate_task (uint64_t user_data)
{
    struct generation_iteration_task_user_data_t *data = (struct generation_iteration_task_user_data_t *) user_data;
    data->node->functor (data->node->other_system, data->new_registry,
                         (kan_reflection_system_generation_iterator_t) &data->iterator, data->iteration_index);
}

static void reflection_system_generate (struct reflection_system_t *system)
{
    KAN_LOG (reflection_system, KAN_LOG_INFO, "Starting reflection registry generation.")
    kan_reflection_registry_t new_registry = kan_reflection_registry_create ();
    KAN_LOG (reflection_system, KAN_LOG_INFO, "Population reflection registry with static information.")
    kan_reflection_system_register_statics (new_registry);

    KAN_LOG (reflection_system, KAN_LOG_INFO, "Calling connected population functors.")
    struct populate_connection_node_t *populate_node = system->first_populate_connection;

    while (populate_node)
    {
        populate_node->functor (populate_node->other_system, new_registry);
        populate_node = populate_node->next;
    }

    KAN_LOG (reflection_system, KAN_LOG_INFO, "Starting generation iteration.")
    uint64_t iteration_index = 0u;
    const kan_interned_string_t task_name = kan_string_intern ("reflection_system_generation_iterate");

    struct generation_context_t generation_context;
    generation_context.this_iteration_submission_lock = kan_atomic_int_init (0);
    kan_stack_group_allocator_init (&generation_context.temporary_allocator, system->group,
                                    KAN_REFLECTION_SYSTEM_GENERATION_STACK_SIZE);

    generation_context.first_added_enum_this_iteration = NULL;
    generation_context.first_added_struct_this_iteration = NULL;
    generation_context.first_added_function_this_iteration = NULL;
    generation_context.first_changed_enum_this_iteration = NULL;
    generation_context.first_changed_struct_this_iteration = NULL;
    generation_context.first_changed_function_this_iteration = NULL;

    generation_context.first_added_enum_meta_this_iteration = NULL;
    generation_context.first_added_enum_value_meta_this_iteration = NULL;
    generation_context.first_added_struct_meta_this_iteration = NULL;
    generation_context.first_added_struct_field_meta_this_iteration = NULL;
    generation_context.first_added_function_meta_this_iteration = NULL;
    generation_context.first_added_function_argument_meta_this_iteration = NULL;

    generation_context.first_added_enum_previous_iteration = NULL;
    generation_context.first_added_struct_previous_iteration = NULL;
    generation_context.first_added_function_previous_iteration = NULL;
    generation_context.first_changed_enum_previous_iteration = NULL;
    generation_context.first_changed_struct_previous_iteration = NULL;
    generation_context.first_changed_function_previous_iteration = NULL;

    generation_context.first_added_enum_meta_previous_iteration = NULL;
    generation_context.first_added_enum_value_meta_previous_iteration = NULL;
    generation_context.first_added_struct_meta_previous_iteration = NULL;
    generation_context.first_added_struct_field_meta_previous_iteration = NULL;
    generation_context.first_added_function_meta_previous_iteration = NULL;
    generation_context.first_added_function_argument_meta_previous_iteration = NULL;

    do
    {
        KAN_LOG (reflection_system, KAN_LOG_INFO, "Running generation iteration %lu.", (unsigned long) iteration_index)
        generation_context.first_added_enum_previous_iteration = generation_context.first_added_enum_this_iteration;
        generation_context.first_added_struct_previous_iteration = generation_context.first_added_struct_this_iteration;
        generation_context.first_added_function_previous_iteration =
            generation_context.first_added_function_this_iteration;
        generation_context.first_changed_enum_previous_iteration = generation_context.first_changed_enum_this_iteration;
        generation_context.first_changed_struct_previous_iteration =
            generation_context.first_changed_struct_this_iteration;
        generation_context.first_changed_function_previous_iteration =
            generation_context.first_changed_function_this_iteration;

        generation_context.first_added_enum_meta_previous_iteration =
            generation_context.first_added_enum_meta_this_iteration;
        generation_context.first_added_enum_value_meta_previous_iteration =
            generation_context.first_added_enum_value_meta_this_iteration;
        generation_context.first_added_struct_meta_previous_iteration =
            generation_context.first_added_struct_meta_this_iteration;
        generation_context.first_added_struct_field_meta_previous_iteration =
            generation_context.first_added_struct_field_meta_this_iteration;
        generation_context.first_added_function_meta_previous_iteration =
            generation_context.first_added_function_meta_this_iteration;
        generation_context.first_added_function_argument_meta_previous_iteration =
            generation_context.first_added_function_argument_meta_this_iteration;

        while (generation_context.first_added_enum_this_iteration)
        {
            kan_reflection_registry_add_enum (new_registry, generation_context.first_added_enum_this_iteration->data);
            generation_context.first_added_enum_this_iteration =
                generation_context.first_added_enum_this_iteration->next;
        }

        while (generation_context.first_added_struct_this_iteration)
        {
            kan_reflection_registry_add_struct (new_registry,
                                                generation_context.first_added_struct_this_iteration->data);
            generation_context.first_added_struct_this_iteration =
                generation_context.first_added_struct_this_iteration->next;
        }

        while (generation_context.first_added_function_this_iteration)
        {
            kan_reflection_registry_add_function (new_registry,
                                                  generation_context.first_added_function_this_iteration->data);
            generation_context.first_added_function_this_iteration =
                generation_context.first_added_function_this_iteration->next;
        }

        while (generation_context.first_added_enum_meta_this_iteration)
        {
            kan_reflection_registry_add_enum_meta (
                new_registry, generation_context.first_added_enum_meta_this_iteration->top_level_name,
                generation_context.first_added_enum_meta_this_iteration->meta_type_name,
                generation_context.first_added_enum_meta_this_iteration->meta);
            generation_context.first_added_enum_meta_this_iteration =
                generation_context.first_added_enum_meta_this_iteration->next;
        }

        while (generation_context.first_added_enum_value_meta_this_iteration)
        {
            kan_reflection_registry_add_enum_value_meta (
                new_registry, generation_context.first_added_enum_value_meta_this_iteration->top_level_name,
                generation_context.first_added_enum_value_meta_this_iteration->lower_level_name,
                generation_context.first_added_enum_value_meta_this_iteration->meta_type_name,
                generation_context.first_added_enum_value_meta_this_iteration->meta);
            generation_context.first_added_enum_value_meta_this_iteration =
                generation_context.first_added_enum_value_meta_this_iteration->next;
        }

        while (generation_context.first_added_struct_meta_this_iteration)
        {
            kan_reflection_registry_add_struct_meta (
                new_registry, generation_context.first_added_struct_meta_this_iteration->top_level_name,
                generation_context.first_added_struct_meta_this_iteration->meta_type_name,
                generation_context.first_added_struct_meta_this_iteration->meta);
            generation_context.first_added_struct_meta_this_iteration =
                generation_context.first_added_struct_meta_this_iteration->next;
        }

        while (generation_context.first_added_struct_field_meta_this_iteration)
        {
            kan_reflection_registry_add_struct_field_meta (
                new_registry, generation_context.first_added_struct_field_meta_this_iteration->top_level_name,
                generation_context.first_added_struct_field_meta_this_iteration->lower_level_name,
                generation_context.first_added_struct_field_meta_this_iteration->meta_type_name,
                generation_context.first_added_struct_field_meta_this_iteration->meta);
            generation_context.first_added_struct_field_meta_this_iteration =
                generation_context.first_added_struct_field_meta_this_iteration->next;
        }

        while (generation_context.first_added_function_meta_this_iteration)
        {
            kan_reflection_registry_add_function_meta (
                new_registry, generation_context.first_added_function_meta_this_iteration->top_level_name,
                generation_context.first_added_function_meta_this_iteration->meta_type_name,
                generation_context.first_added_function_meta_this_iteration->meta);
            generation_context.first_added_function_meta_this_iteration =
                generation_context.first_added_function_meta_this_iteration->next;
        }

        while (generation_context.first_added_function_argument_meta_this_iteration)
        {
            kan_reflection_registry_add_function_argument_meta (
                new_registry, generation_context.first_added_function_argument_meta_this_iteration->top_level_name,
                generation_context.first_added_function_argument_meta_this_iteration->lower_level_name,
                generation_context.first_added_function_argument_meta_this_iteration->meta_type_name,
                generation_context.first_added_function_argument_meta_this_iteration->meta);
            generation_context.first_added_function_argument_meta_this_iteration =
                generation_context.first_added_function_argument_meta_this_iteration->next;
        }

        generation_context.first_changed_enum_this_iteration = NULL;
        generation_context.first_changed_struct_this_iteration = NULL;
        generation_context.first_changed_function_this_iteration = NULL;

        struct kan_cpu_task_list_node_t *list_node = NULL;
        struct generation_iterate_connection_node_t *iterate_node = system->first_generation_iterate_connection;

        while (iterate_node)
        {
            struct kan_cpu_task_list_node_t *next_node = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
                &generation_context.temporary_allocator, struct kan_cpu_task_list_node_t);

            struct generation_iteration_task_user_data_t *user_data = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
                &generation_context.temporary_allocator, struct generation_iteration_task_user_data_t);

            user_data->iterator = (struct generation_iterator_t) {
                .generation_context = &generation_context,
                .current_added_enum = generation_context.first_added_enum_previous_iteration,
                .current_added_struct = generation_context.first_added_struct_previous_iteration,
                .current_added_function = generation_context.first_added_function_previous_iteration,
                .current_changed_enum = generation_context.first_changed_enum_previous_iteration,
                .current_changed_struct = generation_context.first_changed_struct_previous_iteration,
                .current_changed_function = generation_context.first_changed_function_previous_iteration,

                .current_added_enum_meta = generation_context.first_added_enum_meta_previous_iteration,
                .current_added_enum_value_meta = generation_context.first_added_enum_value_meta_previous_iteration,
                .current_added_struct_meta = generation_context.first_added_struct_meta_previous_iteration,
                .current_added_struct_field_meta = generation_context.first_added_struct_field_meta_previous_iteration,
                .current_added_function_meta = generation_context.first_added_function_meta_previous_iteration,
                .current_added_function_argument_meta =
                    generation_context.first_added_function_argument_meta_previous_iteration,
            };

            user_data->new_registry = new_registry;
            user_data->iteration_index = iteration_index;
            user_data->node = iterate_node;

            next_node->task = (struct kan_cpu_task_t) {
                .name = task_name,
                .function = call_generation_iterate_task,
                .user_data = (uint64_t) user_data,
            };

            next_node->queue = KAN_CPU_DISPATCH_QUEUE_FOREGROUND;
            next_node->next = list_node;
            list_node = next_node;

            iterate_node = iterate_node->next;
        }

        if (list_node)
        {
            kan_cpu_job_t job = kan_cpu_job_create ();
            kan_cpu_job_dispatch_and_detach_task_list (job, list_node);
            kan_cpu_job_release (job);
            kan_cpu_job_wait (job);
        }

        ++iteration_index;

    } while (generation_context.first_added_enum_this_iteration ||
             generation_context.first_added_struct_this_iteration ||
             generation_context.first_added_function_this_iteration ||
             generation_context.first_changed_enum_this_iteration ||
             generation_context.first_changed_struct_this_iteration ||
             generation_context.first_changed_function_this_iteration);

    KAN_LOG (reflection_system, KAN_LOG_INFO, "Generation finished.")
    kan_stack_group_allocator_shutdown (&generation_context.temporary_allocator);

    KAN_LOG (reflection_system, KAN_LOG_INFO, "Running generation callbacks.")
    struct generated_connection_node_t *generated_node = system->first_generated_connection;

    kan_reflection_migration_seed_t migration_seed = KAN_INVALID_REFLECTION_MIGRATION_SEED;
    kan_reflection_struct_migrator_t migrator = KAN_INVALID_REFLECTION_STRUCT_MIGRATOR;

    if (system->current_registry != KAN_INVALID_REFLECTION_REGISTRY)
    {
        KAN_LOG (reflection_system, KAN_LOG_INFO, "Creating migration data.")
        migration_seed = kan_reflection_migration_seed_build (system->current_registry, new_registry);
        migrator = kan_reflection_struct_migrator_build (migration_seed);
    }

    while (generated_node)
    {
        generated_node->functor (generated_node->other_system, new_registry, migration_seed, migrator);
        generated_node = generated_node->next;
    }

    if (system->current_registry != KAN_INVALID_REFLECTION_REGISTRY)
    {
        KAN_LOG (reflection_system, KAN_LOG_INFO, "Migrating patches.")
        kan_reflection_struct_migrator_migrate_patches (migrator, system->current_registry, new_registry);

        KAN_LOG (reflection_system, KAN_LOG_INFO, "Destroying migration data.")
        kan_reflection_struct_migrator_destroy (migrator);
        kan_reflection_migration_seed_destroy (migration_seed);

        KAN_LOG (reflection_system, KAN_LOG_INFO, "Destroying old reflection registry.")
        kan_reflection_registry_destroy (system->current_registry);
    }

    system->current_registry = new_registry;
    KAN_LOG (reflection_system, KAN_LOG_INFO, "Generation routine finished successfully.")
}

static void reflection_system_connect (kan_context_system_handle_t handle, kan_context_handle_t context)
{
    struct reflection_system_t *system = (struct reflection_system_t *) handle;
    system->context = context;
}

static void reflection_system_connected_init (kan_context_system_handle_t handle)
{
    reflection_system_generate ((struct reflection_system_t *) handle);
}

static void reflection_system_connected_shutdown (kan_context_system_handle_t handle)
{
}

static void reflection_system_disconnect (kan_context_system_handle_t handle)
{
}

static void reflection_system_destroy (kan_context_system_handle_t handle)
{
    struct reflection_system_t *system = (struct reflection_system_t *) handle;
    KAN_ASSERT (!system->first_populate_connection)
    KAN_ASSERT (!system->first_generated_connection)
    KAN_ASSERT (!system->first_generation_iterate_connection)

    if (system->current_registry != KAN_INVALID_REFLECTION_REGISTRY)
    {
        kan_reflection_registry_destroy (system->current_registry);
    }

    kan_free_general (system->group, system, sizeof (struct kan_context_system_api_t));
}

CONTEXT_REFLECTION_SYSTEM_API struct kan_context_system_api_t KAN_CONTEXT_SYSTEM_API_NAME (reflection_system_t) = {
    .name = KAN_CONTEXT_REFLECTION_SYSTEM_NAME,
    .create = reflection_system_create,
    .connect = reflection_system_connect,
    .connected_init = reflection_system_connected_init,
    .connected_shutdown = reflection_system_connected_shutdown,
    .disconnect = reflection_system_disconnect,
    .destroy = reflection_system_destroy,
};

#define CONNECT(TYPE)                                                                                                  \
    struct reflection_system_t *system = (struct reflection_system_t *) reflection_system;                             \
    struct TYPE##_connection_node_t *node = (struct TYPE##_connection_node_t *) kan_allocate_batched (                 \
        system->group, sizeof (struct TYPE##_connection_node_t));                                                      \
    node->other_system = other_system;                                                                                 \
    node->functor = functor;                                                                                           \
    node->next = system->first_##TYPE##_connection;                                                                    \
    system->first_##TYPE##_connection = node

#define DISCONNECT(TYPE)                                                                                               \
    struct reflection_system_t *system = (struct reflection_system_t *) reflection_system;                             \
    struct TYPE##_connection_node_t *node = system->first_##TYPE##_connection;                                         \
                                                                                                                       \
    while (node && node->other_system == other_system)                                                                 \
    {                                                                                                                  \
        struct TYPE##_connection_node_t *next = node->next;                                                            \
        kan_free_batched (system->group, node);                                                                        \
        system->first_##TYPE##_connection = next;                                                                      \
        node = next;                                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    while (node)                                                                                                       \
    {                                                                                                                  \
        struct TYPE##_connection_node_t *next = node->next;                                                            \
        if (next && next->other_system == other_system)                                                                \
        {                                                                                                              \
            node->next = next->next;                                                                                   \
            kan_free_batched (system->group, next);                                                                    \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            node = next;                                                                                               \
        }                                                                                                              \
    }

void kan_reflection_system_connect_on_populate (kan_context_system_handle_t reflection_system,
                                                kan_context_system_handle_t other_system,
                                                kan_context_reflection_populate_t functor)
{
    CONNECT (populate);
}

void kan_reflection_system_disconnect_on_populate (kan_context_system_handle_t reflection_system,
                                                   kan_context_system_handle_t other_system)
{
    DISCONNECT (populate)
}

void kan_reflection_system_connect_on_generation_iterate (kan_context_system_handle_t reflection_system,
                                                          kan_context_system_handle_t other_system,
                                                          kan_context_reflection_generation_iterate_t functor)
{
    CONNECT (generation_iterate);
}

void kan_reflection_system_disconnect_on_generation_iterate (kan_context_system_handle_t reflection_system,
                                                             kan_context_system_handle_t other_system)
{
    DISCONNECT (generation_iterate)
}

void kan_reflection_system_connect_on_generated (kan_context_system_handle_t reflection_system,
                                                 kan_context_system_handle_t other_system,
                                                 kan_context_reflection_generated_t functor)
{
    CONNECT (generated);
}

void kan_reflection_system_disconnect_on_generated (kan_context_system_handle_t reflection_system,
                                                    kan_context_system_handle_t other_system)
{
    DISCONNECT (generated)
}

#undef CONNECT
#undef DISCONNECT

void kan_reflection_system_invalidate (kan_context_system_handle_t reflection_system)
{
    reflection_system_generate ((struct reflection_system_t *) reflection_system);
}

#define ITERATOR_NEXT(TYPE)                                                                                            \
    struct generation_iterator_t *iterator_data = (struct generation_iterator_t *) iterator;                           \
    if (iterator_data->current_##TYPE)                                                                                 \
    {                                                                                                                  \
        kan_interned_string_t result = iterator_data->current_##TYPE->data->name;                                      \
        iterator_data->current_##TYPE = iterator_data->current_##TYPE->next;                                           \
        return result;                                                                                                 \
    }                                                                                                                  \
                                                                                                                       \
    return NULL

kan_interned_string_t kan_reflection_system_generation_iterator_next_added_enum (
    kan_reflection_system_generation_iterator_t iterator)
{
    ITERATOR_NEXT (added_enum);
}

kan_interned_string_t kan_reflection_system_generation_iterator_next_added_struct (
    kan_reflection_system_generation_iterator_t iterator)
{
    ITERATOR_NEXT (added_struct);
}

kan_interned_string_t kan_reflection_system_generation_iterator_next_added_function (
    kan_reflection_system_generation_iterator_t iterator)
{
    ITERATOR_NEXT (added_function);
}

kan_interned_string_t kan_reflection_system_generation_iterator_next_changed_enum (
    kan_reflection_system_generation_iterator_t iterator)
{
    ITERATOR_NEXT (changed_enum);
}

kan_interned_string_t kan_reflection_system_generation_iterator_next_changed_struct (
    kan_reflection_system_generation_iterator_t iterator)
{
    ITERATOR_NEXT (changed_struct);
}

kan_interned_string_t kan_reflection_system_generation_iterator_next_changed_function (
    kan_reflection_system_generation_iterator_t iterator)
{
    ITERATOR_NEXT (changed_function);
}

#undef ITERATOR_NEXT

#define META_ITERATOR_NEXT_TOP_LEVEL(TYPE)                                                                             \
    struct generation_iterator_t *iterator_data = (struct generation_iterator_t *) iterator;                           \
    if (iterator_data->current_##TYPE##_meta)                                                                          \
    {                                                                                                                  \
        struct kan_reflection_system_##TYPE##_meta_t result = {                                                        \
            iterator_data->current_##TYPE##_meta->top_level_name,                                                      \
            iterator_data->current_##TYPE##_meta->meta_type_name,                                                      \
        };                                                                                                             \
                                                                                                                       \
        iterator_data->current_##TYPE##_meta = iterator_data->current_##TYPE##_meta->next;                             \
        return result;                                                                                                 \
    }                                                                                                                  \
                                                                                                                       \
    return (struct kan_reflection_system_##TYPE##_meta_t)                                                              \
    {                                                                                                                  \
        NULL, NULL                                                                                                     \
    }

#define META_ITERATOR_NEXT_LOWER_LEVEL(TYPE)                                                                           \
    struct generation_iterator_t *iterator_data = (struct generation_iterator_t *) iterator;                           \
    if (iterator_data->current_##TYPE##_meta)                                                                          \
    {                                                                                                                  \
        struct kan_reflection_system_##TYPE##_meta_t result = {                                                        \
            iterator_data->current_##TYPE##_meta->top_level_name,                                                      \
            iterator_data->current_##TYPE##_meta->lower_level_name,                                                    \
            iterator_data->current_##TYPE##_meta->meta_type_name,                                                      \
        };                                                                                                             \
                                                                                                                       \
        iterator_data->current_##TYPE##_meta = iterator_data->current_##TYPE##_meta->next;                             \
        return result;                                                                                                 \
    }                                                                                                                  \
                                                                                                                       \
    return (struct kan_reflection_system_##TYPE##_meta_t)                                                              \
    {                                                                                                                  \
        NULL, NULL, NULL                                                                                               \
    }

struct kan_reflection_system_added_enum_meta_t kan_reflection_system_generation_iterator_next_added_enum_meta (
    kan_reflection_system_generation_iterator_t iterator)
{
    META_ITERATOR_NEXT_TOP_LEVEL (added_enum);
}

struct kan_reflection_system_added_enum_value_meta_t
kan_reflection_system_generation_iterator_next_added_enum_value_meta (
    kan_reflection_system_generation_iterator_t iterator)
{
    META_ITERATOR_NEXT_LOWER_LEVEL (added_enum_value);
}

struct kan_reflection_system_added_struct_meta_t kan_reflection_system_generation_iterator_next_added_struct_meta (
    kan_reflection_system_generation_iterator_t iterator)
{
    META_ITERATOR_NEXT_TOP_LEVEL (added_struct);
}

struct kan_reflection_system_added_struct_field_meta_t
kan_reflection_system_generation_iterator_next_added_struct_field_meta (
    kan_reflection_system_generation_iterator_t iterator)
{
    META_ITERATOR_NEXT_LOWER_LEVEL (added_struct_field);
}

struct kan_reflection_system_added_function_meta_t kan_reflection_system_generation_iterator_next_added_function_meta (
    kan_reflection_system_generation_iterator_t iterator)
{
    META_ITERATOR_NEXT_TOP_LEVEL (added_function);
}

struct kan_reflection_system_added_function_argument_meta_t
kan_reflection_system_generation_iterator_next_added_function_argument_meta (
    kan_reflection_system_generation_iterator_t iterator)
{
    META_ITERATOR_NEXT_LOWER_LEVEL (added_function_argument);
}

#undef META_ITERATOR_NEXT_TOP_LEVEL
#undef META_ITERATOR_NEXT_LOW_LEVEL

#define APPEND_EVENT(EVENT, ENTITY)                                                                                    \
    struct generation_iterator_t *iterator_data = (struct generation_iterator_t *) iterator;                           \
    kan_atomic_int_lock (&iterator_data->generation_context->this_iteration_submission_lock);                          \
                                                                                                                       \
    struct ENTITY##_event_entry_node_t *node = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (                              \
        &iterator_data->generation_context->temporary_allocator, struct ENTITY##_event_entry_node_t);                  \
                                                                                                                       \
    node->data = data;                                                                                                 \
    node->next = iterator_data->generation_context->first_##EVENT##_##ENTITY##_this_iteration;                         \
    iterator_data->generation_context->first_##EVENT##_##ENTITY##_this_iteration = node;                               \
    kan_atomic_int_unlock (&iterator_data->generation_context->this_iteration_submission_lock)

void kan_reflection_system_generation_iterator_add_enum (kan_reflection_system_generation_iterator_t iterator,
                                                         const struct kan_reflection_enum_t *data)
{
    APPEND_EVENT (added, enum);
}

void kan_reflection_system_generation_iterator_add_struct (kan_reflection_system_generation_iterator_t iterator,
                                                           const struct kan_reflection_struct_t *data)
{
    APPEND_EVENT (added, struct);
}

void kan_reflection_system_generation_iterator_add_function (kan_reflection_system_generation_iterator_t iterator,
                                                             const struct kan_reflection_function_t *data)
{
    APPEND_EVENT (added, function);
}

void kan_reflection_system_generation_iterator_change_enum (kan_reflection_system_generation_iterator_t iterator,
                                                            const struct kan_reflection_enum_t *data)
{
    APPEND_EVENT (changed, enum);
}

void kan_reflection_system_generation_iterator_change_struct (kan_reflection_system_generation_iterator_t iterator,
                                                              const struct kan_reflection_struct_t *data)
{
    APPEND_EVENT (changed, struct);
}

void kan_reflection_system_generation_iterator_change_function (kan_reflection_system_generation_iterator_t iterator,
                                                                const struct kan_reflection_function_t *data)
{
    APPEND_EVENT (changed, function);
}

#undef APPEND_EVENT

#define ADD_META_EVENT_TOP_LEVEL(EVENT, ENTITY)                                                                        \
    struct generation_iterator_t *iterator_data = (struct generation_iterator_t *) iterator;                           \
    kan_atomic_int_lock (&iterator_data->generation_context->this_iteration_submission_lock);                          \
    struct top_level_meta_node_t *node = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (                                    \
        &iterator_data->generation_context->temporary_allocator, struct top_level_meta_node_t);                        \
                                                                                                                       \
    node->top_level_name = ENTITY##_name;                                                                              \
    node->meta_type_name = meta_type_name;                                                                             \
    node->meta = meta;                                                                                                 \
    node->next = iterator_data->generation_context->first_##EVENT##_##ENTITY##_meta_this_iteration;                    \
    iterator_data->generation_context->first_##EVENT##_##ENTITY##_meta_this_iteration = node;                          \
    kan_atomic_int_unlock (&iterator_data->generation_context->this_iteration_submission_lock)

#define ADD_META_EVENT_LOWER_LEVEL(EVENT, ENTITY, LOWER_ENTITY)                                                        \
    struct generation_iterator_t *iterator_data = (struct generation_iterator_t *) iterator;                           \
    kan_atomic_int_lock (&iterator_data->generation_context->this_iteration_submission_lock);                          \
    struct lower_level_meta_node_t *node = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (                                  \
        &iterator_data->generation_context->temporary_allocator, struct lower_level_meta_node_t);                      \
                                                                                                                       \
    node->top_level_name = ENTITY##_name;                                                                              \
    node->lower_level_name = LOWER_ENTITY##_name;                                                                      \
    node->meta_type_name = meta_type_name;                                                                             \
    node->meta = meta;                                                                                                 \
    node->next = iterator_data->generation_context->first_##EVENT##_##ENTITY##_##LOWER_ENTITY##_meta_this_iteration;   \
    iterator_data->generation_context->first_##EVENT##_##ENTITY##_##LOWER_ENTITY##_meta_this_iteration = node;         \
    kan_atomic_int_unlock (&iterator_data->generation_context->this_iteration_submission_lock)

void kan_reflection_system_generation_iterator_add_enum_meta (kan_reflection_system_generation_iterator_t iterator,
                                                              kan_interned_string_t enum_name,
                                                              kan_interned_string_t meta_type_name,
                                                              void *meta)
{
    ADD_META_EVENT_TOP_LEVEL (added, enum);
}

void kan_reflection_system_generation_iterator_add_enum_value_meta (
    kan_reflection_system_generation_iterator_t iterator,
    kan_interned_string_t enum_name,
    kan_interned_string_t value_name,
    kan_interned_string_t meta_type_name,
    void *meta)
{
    ADD_META_EVENT_LOWER_LEVEL (added, enum, value);
}

void kan_reflection_system_generation_iterator_add_struct_meta (kan_reflection_system_generation_iterator_t iterator,
                                                                kan_interned_string_t struct_name,
                                                                kan_interned_string_t meta_type_name,
                                                                void *meta)
{
    ADD_META_EVENT_TOP_LEVEL (added, struct);
}

void kan_reflection_system_generation_iterator_add_struct_field_meta (
    kan_reflection_system_generation_iterator_t iterator,
    kan_interned_string_t struct_name,
    kan_interned_string_t field_name,
    kan_interned_string_t meta_type_name,
    void *meta)
{
    ADD_META_EVENT_LOWER_LEVEL (added, struct, field);
}

void kan_reflection_system_generation_iterator_add_function_meta (kan_reflection_system_generation_iterator_t iterator,
                                                                  kan_interned_string_t function_name,
                                                                  kan_interned_string_t meta_type_name,
                                                                  void *meta)
{
    ADD_META_EVENT_TOP_LEVEL (added, function);
}

void kan_reflection_system_generation_iterator_add_function_argument_meta (
    kan_reflection_system_generation_iterator_t iterator,
    kan_interned_string_t function_name,
    kan_interned_string_t argument_name,
    kan_interned_string_t meta_type_name,
    void *meta)
{
    ADD_META_EVENT_LOWER_LEVEL (added, function, argument);
}

#undef ADD_META_EVENT_TOP_LEVEL
#undef ADD_META_EVENT_LOWER_LEVEL
