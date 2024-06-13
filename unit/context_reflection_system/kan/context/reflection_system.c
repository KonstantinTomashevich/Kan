#include <stddef.h>
#include <string.h>

#include <kan/container/stack_group_allocator.h>
#include <kan/context/reflection_system.h>
#include <kan/cpu_dispatch/job.h>
#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/threading/atomic.h>

KAN_LOG_DEFINE_CATEGORY (reflection_system);

#if defined(_WIN32)
__declspec (dllimport) void KAN_CONTEXT_REFLECTION_SYSTEM_REGISTRAR_FUNCTION (kan_reflection_registry_t registry);
#else
void KAN_CONTEXT_REFLECTION_SYSTEM_REGISTRAR_FUNCTION (kan_reflection_registry_t registry);
#endif

struct populate_connection_node_t
{
    struct populate_connection_node_t *next;
    kan_context_system_handle_t other_system;
    kan_context_reflection_populate_t functor;
};

struct finalize_connection_node_t
{
    struct finalize_connection_node_t *next;
    kan_context_system_handle_t other_system;
    kan_context_reflection_finalize_t functor;
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

struct cleanup_connection_node_t
{
    struct cleanup_connection_node_t *next;
    kan_context_system_handle_t other_system;
    kan_context_reflection_cleanup_t functor;
};

struct pre_shutdown_connection_node_t
{
    struct pre_shutdown_connection_node_t *next;
    kan_context_system_handle_t other_system;
    kan_context_reflection_cleanup_t functor;
};

struct reflection_generator_node_t
{
    struct reflection_generator_node_t *next;
    void *instance;
    kan_allocation_group_t instance_group;

    const struct kan_reflection_struct_t *type;
    const struct kan_reflection_function_t *bootstrap_function;
    const struct kan_reflection_function_t *iterate_function;
    const struct kan_reflection_function_t *finalize_function;
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
    struct generation_iterate_connection_node_t *connection_node;
    struct reflection_generator_node_t *generator_node;
};

struct reflection_system_t
{
    struct populate_connection_node_t *first_populate_connection;
    struct finalize_connection_node_t *first_finalize_connection;
    struct generated_connection_node_t *first_generated_connection;
    struct generation_iterate_connection_node_t *first_generation_iterate_connection;
    struct cleanup_connection_node_t *first_cleanup_connection;
    struct pre_shutdown_connection_node_t *first_pre_shutdown_connection;

    kan_allocation_group_t group;
    kan_context_handle_t context;
    kan_reflection_registry_t current_registry;

    kan_allocation_group_t reflection_generator_allocation_group;
    struct reflection_generator_node_t *current_registry_first_generator;
};

static kan_context_system_handle_t reflection_system_create (kan_allocation_group_t group, void *user_config)
{
    struct reflection_system_t *system = (struct reflection_system_t *) kan_allocate_general (
        group, sizeof (struct reflection_system_t), _Alignof (struct reflection_system_t));
    system->first_populate_connection = NULL;
    system->first_finalize_connection = NULL;
    system->first_generated_connection = NULL;
    system->first_generation_iterate_connection = NULL;
    system->first_cleanup_connection = NULL;
    system->first_pre_shutdown_connection = NULL;

    system->group = group;
    system->current_registry = KAN_INVALID_REFLECTION_REGISTRY;

    system->reflection_generator_allocation_group =
        kan_allocation_group_get_child (system->group, "reflection_generators");
    system->current_registry_first_generator = NULL;

    return (kan_context_system_handle_t) system;
}

static void call_generation_iterate_task (uint64_t user_data)
{
    struct generation_iteration_task_user_data_t *data = (struct generation_iteration_task_user_data_t *) user_data;
    if (data->connection_node)
    {
        data->connection_node->functor (data->connection_node->other_system, data->new_registry,
                                        (kan_reflection_system_generation_iterator_t) &data->iterator,
                                        data->iteration_index);
    }
    else
    {
        KAN_ASSERT (data->generator_node)
        if (data->generator_node->iterate_function)
        {
            struct
            {
                void *instance;
                kan_reflection_registry_t registry;
                kan_reflection_system_generation_iterator_t iterator;
                uint64_t iteration_index;
            } arguments = {
                .instance = data->generator_node->instance,
                .registry = data->new_registry,
                .iterator = (kan_reflection_system_generation_iterator_t) &data->iterator,
                .iteration_index = data->iteration_index,
            };

            data->generator_node->iterate_function->call (data->generator_node->iterate_function->call_user_data, NULL,
                                                          &arguments);
        }
    }
}

static void add_to_reflection_generators_if_needed (kan_reflection_registry_t registry,
                                                    const struct kan_reflection_struct_t *type,
                                                    uint64_t current_iterator_for_bootstrap,
                                                    struct reflection_generator_node_t **first_generator_node,
                                                    kan_allocation_group_t allocation_group)
{
    if (strncmp (type->name, "kan_reflection_generator_", 25u) != 0)
    {
        // Not a reflection generator.
        return;
    }

    const char *generator_name_begin = type->name + 25u;
    const char *generator_name_end = generator_name_begin;

    while (*generator_name_end)
    {
        ++generator_name_end;
    }

    if (generator_name_end - generator_name_begin > 2u && *(generator_name_end - 1u) == 't' &&
        *(generator_name_end - 2u) == '_')
    {
        // Remove "_t" suffix.
        generator_name_end -= 2u;
    }

    if (generator_name_begin == generator_name_end)
    {
        // Something wrong with naming, we cannot extract non-zero name.
        return;
    }

    kan_interned_string_t generator_name = kan_char_sequence_intern (generator_name_begin, generator_name_end);
    struct reflection_generator_node_t *node = (struct reflection_generator_node_t *) kan_allocate_batched (
        allocation_group, sizeof (struct reflection_generator_node_t));

    node->next = *first_generator_node;
    *first_generator_node = node;

    node->instance_group = kan_allocation_group_get_child (allocation_group, generator_name);
    node->instance = kan_allocate_general (node->instance_group, type->size, type->alignment);

    if (type->init)
    {
        kan_allocation_group_stack_push (node->instance_group);
        type->init (type->functor_user_data, node->instance);
        kan_allocation_group_stack_pop ();
    }

    node->type = type;
#define NAME_BUFFER_SIZE 256u
    char name_buffer[NAME_BUFFER_SIZE];
    snprintf (name_buffer, NAME_BUFFER_SIZE, "kan_reflection_generator_%s_bootstrap", generator_name);
    node->bootstrap_function = kan_reflection_registry_query_function (registry, kan_string_intern (name_buffer));

    snprintf (name_buffer, NAME_BUFFER_SIZE, "kan_reflection_generator_%s_iterate", generator_name);
    node->iterate_function = kan_reflection_registry_query_function (registry, kan_string_intern (name_buffer));

    snprintf (name_buffer, NAME_BUFFER_SIZE, "kan_reflection_generator_%s_finalize", generator_name);
    node->finalize_function = kan_reflection_registry_query_function (registry, kan_string_intern (name_buffer));
#undef NAME_BUFFER_SIZE

    if (node->bootstrap_function)
    {
        if (node->bootstrap_function->arguments_count != 2u ||
            node->bootstrap_function->arguments[0u].archetype != KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER ||
            node->bootstrap_function->arguments[0u].archetype_struct_pointer.type_name != type->name ||
            node->bootstrap_function->arguments[1u].archetype != KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT ||
            node->bootstrap_function->arguments[1u].size != sizeof (uint64_t))
        {
            KAN_LOG (reflection_system, KAN_LOG_ERROR,
                     "Bootstrap function should have two arguments -- instance pointer and first iteration index. But "
                     "\"%s\" is not compatible with this requirements.",
                     node->bootstrap_function->name)
            node->bootstrap_function = NULL;
        }
    }

    if (node->iterate_function)
    {
        if (node->iterate_function->arguments_count != 4u ||
            node->iterate_function->arguments[0u].archetype != KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER ||
            node->iterate_function->arguments[0u].archetype_struct_pointer.type_name != type->name ||
            node->iterate_function->arguments[1u].archetype != KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT ||
            node->iterate_function->arguments[1u].size != sizeof (uint64_t) ||
            node->iterate_function->arguments[2u].archetype != KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT ||
            node->iterate_function->arguments[2u].size != sizeof (uint64_t) ||
            node->iterate_function->arguments[3u].archetype != KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT ||
            node->iterate_function->arguments[3u].size != sizeof (uint64_t))
        {
            KAN_LOG (reflection_system, KAN_LOG_ERROR,
                     "Iterate function should have 4 arguments -- instance pointer, registry, generation iterator and "
                     "iteration index. But \"%s\" is not compatible with this requirements.",
                     node->iterate_function->name)
            node->iterate_function = NULL;
        }
    }

    if (node->finalize_function)
    {
        if (node->finalize_function->arguments_count != 2u ||
            node->finalize_function->arguments[0u].archetype != KAN_REFLECTION_ARCHETYPE_STRUCT_POINTER ||
            node->finalize_function->arguments[0u].archetype_struct_pointer.type_name != type->name ||
            node->finalize_function->arguments[1u].archetype != KAN_REFLECTION_ARCHETYPE_UNSIGNED_INT ||
            node->finalize_function->arguments[1u].size != sizeof (uint64_t))
        {
            KAN_LOG (reflection_system, KAN_LOG_ERROR,
                     "Finalize function should have two arguments -- instance pointer and registry to finalize. But "
                     "\"%s\" is not compatible with this requirements.",
                     node->finalize_function->name)
            node->finalize_function = NULL;
        }
    }

    if (node->bootstrap_function)
    {
        struct
        {
            void *instance;
            uint64_t first_iteration_index;
        } arguments = {
            .instance = node->instance,
            .first_iteration_index = current_iterator_for_bootstrap,
        };

        node->bootstrap_function->call (node->bootstrap_function->call_user_data, NULL, &arguments);
    }
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

    KAN_LOG (reflection_system, KAN_LOG_INFO, "Collecting initial reflection generators.")
    struct reflection_generator_node_t *first_reflection_generator = NULL;

    kan_reflection_registry_struct_iterator_t struct_iterator =
        kan_reflection_registry_struct_iterator_create (new_registry);
    const struct kan_reflection_struct_t *struct_to_scan;

    while ((struct_to_scan = kan_reflection_registry_struct_iterator_get (struct_iterator)))
    {
        add_to_reflection_generators_if_needed (new_registry, struct_to_scan, 0u, &first_reflection_generator,
                                                system->reflection_generator_allocation_group);
        struct_iterator = kan_reflection_registry_struct_iterator_next (struct_iterator);
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
            add_to_reflection_generators_if_needed (
                new_registry, generation_context.first_added_struct_this_iteration->data, iteration_index,
                &first_reflection_generator, system->reflection_generator_allocation_group);
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
            KAN_CPU_TASK_LIST_USER_STRUCT (
                &list_node, &generation_context.temporary_allocator, task_name, call_generation_iterate_task,
                struct generation_iteration_task_user_data_t,
                {
                    .iterator =
                        (struct generation_iterator_t) {
                            .generation_context = &generation_context,
                            .current_added_enum = generation_context.first_added_enum_previous_iteration,
                            .current_added_struct = generation_context.first_added_struct_previous_iteration,
                            .current_added_function = generation_context.first_added_function_previous_iteration,
                            .current_changed_enum = generation_context.first_changed_enum_previous_iteration,
                            .current_changed_struct = generation_context.first_changed_struct_previous_iteration,
                            .current_changed_function = generation_context.first_changed_function_previous_iteration,

                            .current_added_enum_meta = generation_context.first_added_enum_meta_previous_iteration,
                            .current_added_enum_value_meta =
                                generation_context.first_added_enum_value_meta_previous_iteration,
                            .current_added_struct_meta = generation_context.first_added_struct_meta_previous_iteration,
                            .current_added_struct_field_meta =
                                generation_context.first_added_struct_field_meta_previous_iteration,
                            .current_added_function_meta =
                                generation_context.first_added_function_meta_previous_iteration,
                            .current_added_function_argument_meta =
                                generation_context.first_added_function_argument_meta_previous_iteration,
                        },
                    .new_registry = new_registry,
                    .iteration_index = iteration_index,
                    .connection_node = iterate_node,
                    .generator_node = NULL,
                })
            iterate_node = iterate_node->next;
        }

        struct reflection_generator_node_t *reflection_generator_node = first_reflection_generator;
        while (reflection_generator_node)
        {
            KAN_CPU_TASK_LIST_USER_STRUCT (
                &list_node, &generation_context.temporary_allocator, task_name, call_generation_iterate_task,
                struct generation_iteration_task_user_data_t,
                {
                    .iterator =
                        (struct generation_iterator_t) {
                            .generation_context = &generation_context,
                            .current_added_enum = generation_context.first_added_enum_previous_iteration,
                            .current_added_struct = generation_context.first_added_struct_previous_iteration,
                            .current_added_function = generation_context.first_added_function_previous_iteration,
                            .current_changed_enum = generation_context.first_changed_enum_previous_iteration,
                            .current_changed_struct = generation_context.first_changed_struct_previous_iteration,
                            .current_changed_function = generation_context.first_changed_function_previous_iteration,

                            .current_added_enum_meta = generation_context.first_added_enum_meta_previous_iteration,
                            .current_added_enum_value_meta =
                                generation_context.first_added_enum_value_meta_previous_iteration,
                            .current_added_struct_meta = generation_context.first_added_struct_meta_previous_iteration,
                            .current_added_struct_field_meta =
                                generation_context.first_added_struct_field_meta_previous_iteration,
                            .current_added_function_meta =
                                generation_context.first_added_function_meta_previous_iteration,
                            .current_added_function_argument_meta =
                                generation_context.first_added_function_argument_meta_previous_iteration,
                        },
                    .new_registry = new_registry,
                    .iteration_index = iteration_index,
                    .connection_node = NULL,
                    .generator_node = reflection_generator_node,
                })
            reflection_generator_node = reflection_generator_node->next;
        }

        if (list_node)
        {
            kan_cpu_job_t job = kan_cpu_job_create ();
            kan_cpu_job_dispatch_and_detach_task_list (job, list_node);
            kan_cpu_job_release (job);
            kan_cpu_job_wait (job);
        }

        kan_stack_group_allocator_reset (&generation_context.temporary_allocator);
        ++iteration_index;

    } while (generation_context.first_added_enum_this_iteration ||
             generation_context.first_added_struct_this_iteration ||
             generation_context.first_added_function_this_iteration ||
             generation_context.first_changed_enum_this_iteration ||
             generation_context.first_changed_struct_this_iteration ||
             generation_context.first_changed_function_this_iteration);

    KAN_LOG (reflection_system, KAN_LOG_INFO, "Calling connected finalization functors.")
    struct finalize_connection_node_t *finalize_node = system->first_finalize_connection;

    while (finalize_node)
    {
        finalize_node->functor (finalize_node->other_system, new_registry);
        finalize_node = finalize_node->next;
    }

    struct reflection_generator_node_t *reflection_generator_node = first_reflection_generator;
    while (reflection_generator_node)
    {
        struct reflection_generator_node_t *next = reflection_generator_node->next;
        if (reflection_generator_node->finalize_function)
        {
            struct
            {
                void *instance;
                kan_reflection_registry_t registry;
            } arguments = {
                .instance = reflection_generator_node->instance,
                .registry = new_registry,
            };

            reflection_generator_node->finalize_function->call (
                reflection_generator_node->finalize_function->call_user_data, NULL, &arguments);
        }

        reflection_generator_node = next;
    }

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

        KAN_LOG (reflection_system, KAN_LOG_INFO, "Migrating patches.")
        kan_reflection_struct_migrator_migrate_patches (migrator, system->current_registry, new_registry);
    }

    while (generated_node)
    {
        generated_node->functor (generated_node->other_system, new_registry, migration_seed, migrator);
        generated_node = generated_node->next;
    }

    if (system->current_registry != KAN_INVALID_REFLECTION_REGISTRY)
    {
        KAN_LOG (reflection_system, KAN_LOG_INFO, "Destroying migration data.")
        kan_reflection_struct_migrator_destroy (migrator);
        kan_reflection_migration_seed_destroy (migration_seed);

        KAN_LOG (reflection_system, KAN_LOG_INFO, "Destroying old reflection registry.")
        kan_reflection_registry_destroy (system->current_registry);

        KAN_LOG (reflection_system, KAN_LOG_INFO, "Calling connected cleanup functors.")
        struct cleanup_connection_node_t *cleanup_node = system->first_cleanup_connection;

        while (cleanup_node)
        {
            cleanup_node->functor (cleanup_node->other_system);
            cleanup_node = cleanup_node->next;
        }

        while (system->current_registry_first_generator)
        {
            struct reflection_generator_node_t *next = system->current_registry_first_generator->next;
            if (system->current_registry_first_generator->type->shutdown)
            {
                system->current_registry_first_generator->type->shutdown (
                    system->current_registry_first_generator->type->functor_user_data,
                    system->current_registry_first_generator->instance);
            }

            kan_free_general (system->current_registry_first_generator->instance_group,
                              system->current_registry_first_generator->instance,
                              system->current_registry_first_generator->type->size);
            kan_free_batched (system->reflection_generator_allocation_group, system->current_registry_first_generator);
            system->current_registry_first_generator = next;
        }
    }

    system->current_registry = new_registry;
    system->current_registry_first_generator = first_reflection_generator;
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
    struct reflection_system_t *system = (struct reflection_system_t *) handle;
    KAN_LOG (reflection_system, KAN_LOG_INFO, "Calling pre-shutdown functors.")
    struct pre_shutdown_connection_node_t *pre_shutdown_node = system->first_pre_shutdown_connection;

    while (pre_shutdown_node)
    {
        pre_shutdown_node->functor (pre_shutdown_node->other_system);
        pre_shutdown_node = pre_shutdown_node->next;
    }

    while (system->current_registry_first_generator)
    {
        struct reflection_generator_node_t *next = system->current_registry_first_generator->next;
        if (system->current_registry_first_generator->type->shutdown)
        {
            system->current_registry_first_generator->type->shutdown (
                system->current_registry_first_generator->type->functor_user_data,
                system->current_registry_first_generator->instance);
        }

        kan_free_general (system->current_registry_first_generator->instance_group,
                          system->current_registry_first_generator->instance,
                          system->current_registry_first_generator->type->size);
        kan_free_batched (system->reflection_generator_allocation_group, system->current_registry_first_generator);
        system->current_registry_first_generator = next;
    }

    if (system->current_registry != KAN_INVALID_REFLECTION_REGISTRY)
    {
        kan_reflection_registry_destroy (system->current_registry);
    }
}

static void reflection_system_disconnect (kan_context_system_handle_t handle)
{
}

static void reflection_system_destroy (kan_context_system_handle_t handle)
{
    struct reflection_system_t *system = (struct reflection_system_t *) handle;
    KAN_ASSERT (!system->first_populate_connection)
    KAN_ASSERT (!system->first_finalize_connection)
    KAN_ASSERT (!system->first_generated_connection)
    KAN_ASSERT (!system->first_generation_iterate_connection)
    KAN_ASSERT (!system->first_cleanup_connection)
    KAN_ASSERT (!system->first_pre_shutdown_connection)
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

void kan_reflection_system_connect_on_finalize (kan_context_system_handle_t reflection_system,
                                                kan_context_system_handle_t other_system,
                                                kan_context_reflection_finalize_t functor)
{
    CONNECT (finalize);
}

void kan_reflection_system_disconnect_on_finalize (kan_context_system_handle_t reflection_system,
                                                   kan_context_system_handle_t other_system)
{
    DISCONNECT (finalize)
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

void kan_reflection_system_connect_on_cleanup (kan_context_system_handle_t reflection_system,
                                               kan_context_system_handle_t other_system,
                                               kan_context_reflection_cleanup_t functor)
{
    CONNECT (cleanup);
}

void kan_reflection_system_disconnect_on_cleanup (kan_context_system_handle_t reflection_system,
                                                  kan_context_system_handle_t other_system)
{
    DISCONNECT (pre_shutdown)
}

void kan_reflection_system_connect_on_pre_shutdown (kan_context_system_handle_t reflection_system,
                                                    kan_context_system_handle_t other_system,
                                                    kan_context_reflection_pre_shutdown_t functor)
{
    CONNECT (pre_shutdown);
}

void kan_reflection_system_disconnect_on_pre_shutdown (kan_context_system_handle_t reflection_system,
                                                       kan_context_system_handle_t other_system) {
    DISCONNECT (pre_shutdown)}

#undef CONNECT
#undef DISCONNECT

kan_reflection_registry_t kan_reflection_system_get_registry (kan_context_system_handle_t reflection_system)
{
    return ((struct reflection_system_t *) reflection_system)->current_registry;
}

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
