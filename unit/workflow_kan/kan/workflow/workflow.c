#include <stddef.h>

#include <kan/api_common/alignment.h>
#include <kan/container/dynamic_array.h>
#include <kan/container/fixed_length_bitset.h>
#include <kan/container/hash_storage.h>
#include <kan/container/interned_string.h>
#include <kan/container/stack_group_allocator.h>
#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>
#include <kan/reflection/markup.h>
#include <kan/threading/atomic.h>
#include <kan/threading/conditional_variable.h>
#include <kan/threading/mutex.h>
#include <kan/workflow/workflow.h>

KAN_LOG_DEFINE_CATEGORY (workflow_graph);
KAN_LOG_DEFINE_CATEGORY (workflow_graph_builder);

#if defined(KAN_WORKFLOW_VERIFY)
enum traverse_status_t
{
    TRAVERSE_STATUS_NOT_TRAVERSED,
    TRAVERSE_STATUS_IN_PROGRESS,
    TRAVERSE_STATUS_DONE
};
#endif

struct building_graph_node_t
{
    struct kan_hash_storage_node_t node;

    kan_interned_string_t name;
    kan_workflow_function_t function;
    kan_functor_user_data_t user_data;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t depends_on;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t dependency_of;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t resource_access_population;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t resource_access_view;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_interned_string_t)
    struct kan_dynamic_array_t resource_access_modification;

    struct graph_builder_t *builder;

    kan_instance_size_t intermediate_node_id;
    kan_instance_size_t intermediate_references_count;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_instance_size_t)
    struct kan_dynamic_array_t intermediate_incomes;

    KAN_REFLECTION_DYNAMIC_ARRAY_TYPE (kan_instance_size_t)
    struct kan_dynamic_array_t intermediate_outcomes;

#if defined(KAN_WORKFLOW_VERIFY)
    struct kan_fixed_length_bitset_t *intermediate_access_population;
    struct kan_fixed_length_bitset_t *intermediate_access_view;
    struct kan_fixed_length_bitset_t *intermediate_access_modification;
    struct kan_fixed_length_bitset_t *intermediate_reachability;
    enum traverse_status_t intermediate_traverse_status;
#endif
};

struct graph_builder_t
{
    struct kan_hash_storage_t nodes;
    struct kan_atomic_int_t node_submission_lock;
    kan_allocation_group_t main_group;
    kan_allocation_group_t builder_group;
};

struct workflow_graph_node_t
{
    kan_workflow_function_t function;
    kan_functor_user_data_t user_data;
    kan_cpu_section_t profiler_section;

    struct workflow_graph_header_t *header;
    kan_instance_size_t incomes_count;
    struct kan_atomic_int_t incomes_left;
    kan_cpu_job_t job;

    kan_instance_size_t outcomes_count;
    struct workflow_graph_node_t *outcomes[];
};

struct workflow_graph_header_t
{
    kan_instance_size_t total_nodes_count;
    kan_instance_size_t start_nodes_count;

    struct kan_stack_group_allocator_t temporary_allocator;
    struct kan_atomic_int_t temporary_allocator_lock;

    kan_instance_size_t nodes_left_to_execute;
    kan_mutex_t nodes_left_to_execute_mutex;
    kan_conditional_variable_t nodes_left_to_execute_signal;

    kan_allocation_group_t allocation_group;
    kan_instance_size_t allocation_size;
    struct workflow_graph_node_t *start_nodes[];
};

static bool statics_initialized = false;
static kan_cpu_section_t task_start_section;
static kan_cpu_section_t task_finish_section;

static void ensure_statics_initialized (void)
{
    if (!statics_initialized)
    {
        task_start_section = kan_cpu_section_get ("workflow_task_start");
        task_finish_section = kan_cpu_section_get ("workflow_task_finish");
        statics_initialized = true;
    }
}

#if defined(KAN_WORKFLOW_VERIFY)
struct resource_info_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t name;
    kan_instance_size_t id;
};

static inline kan_instance_size_t query_resource (struct kan_hash_storage_t *hash_storage,
                                                  kan_interned_string_t resource_name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (hash_storage, KAN_HASH_OBJECT_POINTER (resource_name));
    struct resource_info_node_t *node = (struct resource_info_node_t *) bucket->first;
    const struct resource_info_node_t *node_end =
        (struct resource_info_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->name == resource_name)
        {
            return node->id;
        }

        node = (struct resource_info_node_t *) node->node.list_node.next;
    }

    return KAN_INT_MAX (kan_instance_size_t);
}

static inline void register_resource (struct kan_hash_storage_t *hash_storage,
                                      kan_interned_string_t resource_name,
                                      kan_instance_size_t *id_counter,
                                      struct kan_stack_group_allocator_t *temporary_allocator)
{
    if (query_resource (hash_storage, resource_name) != KAN_INT_MAX (kan_instance_size_t))
    {
        return;
    }

    struct resource_info_node_t *node =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (temporary_allocator, struct resource_info_node_t);
    node->node.hash = KAN_HASH_OBJECT_POINTER (resource_name);
    node->name = resource_name;
    node->id = *id_counter;

    kan_hash_storage_update_bucket_count_default (hash_storage, KAN_WORKFLOW_RESOURCE_INITIAL_BUCKETS);
    kan_hash_storage_add (hash_storage, &node->node);
    ++*id_counter;
}

static bool traverse_and_verify (struct building_graph_node_t *node, struct building_graph_node_t **id_to_node)
{
    switch (node->intermediate_traverse_status)
    {
    case TRAVERSE_STATUS_NOT_TRAVERSED:
        node->intermediate_traverse_status = TRAVERSE_STATUS_IN_PROGRESS;
        for (kan_loop_size_t outcome_index = 0u; outcome_index < node->intermediate_outcomes.size; ++outcome_index)
        {
            const kan_instance_size_t outcome_id =
                ((kan_instance_size_t *) node->intermediate_outcomes.data)[outcome_index];
            struct building_graph_node_t *outcome = id_to_node[outcome_id];

            if (!traverse_and_verify (outcome, id_to_node))
            {
                KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR, "- \"%s\"", node->name)
                return false;
            }

            kan_fixed_length_bitset_set (node->intermediate_reachability, outcome_id, true);
            kan_fixed_length_bitset_or_assign (node->intermediate_reachability, outcome->intermediate_reachability);
        }

        node->intermediate_traverse_status = TRAVERSE_STATUS_DONE;
        return true;

    case TRAVERSE_STATUS_IN_PROGRESS:
        KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR, "Caught cycle in workflow graph. Dumping node stack: ")
        KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR, "- \"%s\"", node->name)
        return false;

    case TRAVERSE_STATUS_DONE:
        return true;
    }

    KAN_ASSERT (false)
    return false;
}

static inline void print_colliding_resources (struct kan_fixed_length_bitset_t *first,
                                              struct kan_fixed_length_bitset_t *second,
                                              struct resource_info_node_t **id_to_resource_node)
{
    KAN_ASSERT (first->items == second->items)
    for (kan_loop_size_t item_index = 0u; item_index < first->items; ++item_index)
    {
        const kan_bitset_item_t intersection = first->data[item_index] & second->data[item_index];
        if (intersection != 0u)
        {
            for (kan_loop_size_t bit_index = 0u; bit_index < 64u; ++bit_index)
            {
                if ((intersection & (((kan_bitset_item_t) 1u) << bit_index)) > 0u)
                {
                    KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR, "  - \"%s\"",
                             id_to_resource_node[item_index * 64u + bit_index]->name)
                }
            }
        }
    }
}

static bool graph_builder_verify_intermediate (struct graph_builder_t *builder,
                                               struct building_graph_node_t **id_to_node)
{
    struct kan_hash_storage_t resource_storage;
    kan_hash_storage_init (&resource_storage, builder->builder_group, KAN_WORKFLOW_RESOURCE_INITIAL_BUCKETS);

    struct kan_stack_group_allocator_t temporary_allocator;
    kan_stack_group_allocator_init (&temporary_allocator, builder->builder_group, KAN_WORKFLOW_VERIFICATION_STACK_SIZE);

    // Register all resource types.
    struct building_graph_node_t *node = (struct building_graph_node_t *) builder->nodes.items.first;
    kan_instance_size_t resource_id_counter = 0u;

    while (node)
    {
        for (kan_loop_size_t index = 0u; index < node->resource_access_population.size; ++index)
        {
            register_resource (&resource_storage,
                               ((kan_interned_string_t *) node->resource_access_population.data)[index],
                               &resource_id_counter, &temporary_allocator);
        }

        for (kan_loop_size_t index = 0u; index < node->resource_access_view.size; ++index)
        {
            register_resource (&resource_storage, ((kan_interned_string_t *) node->resource_access_view.data)[index],
                               &resource_id_counter, &temporary_allocator);
        }

        for (kan_loop_size_t index = 0u; index < node->resource_access_modification.size; ++index)
        {
            register_resource (&resource_storage,
                               ((kan_interned_string_t *) node->resource_access_modification.data)[index],
                               &resource_id_counter, &temporary_allocator);
        }

        node = (struct building_graph_node_t *) node->node.list_node.next;
    }

    // Fill id to resource node array.
    struct resource_info_node_t **id_to_resource_node = kan_stack_group_allocator_allocate (
        &temporary_allocator, sizeof (void *) * resource_id_counter, alignof (void *));
    struct resource_info_node_t *resource_node = (struct resource_info_node_t *) resource_storage.items.first;

    while (resource_node)
    {
        id_to_resource_node[resource_node->id] = resource_node;
        resource_node = (struct resource_info_node_t *) resource_node->node.list_node.next;
    }

    // Generate access masks.
    node = (struct building_graph_node_t *) builder->nodes.items.first;

    while (node)
    {
        node->intermediate_access_population = (struct kan_fixed_length_bitset_t *) kan_stack_group_allocator_allocate (
            &temporary_allocator, kan_fixed_length_bitset_calculate_allocation_size (resource_id_counter),
            alignof (struct kan_fixed_length_bitset_t));
        kan_fixed_length_bitset_init (node->intermediate_access_population, resource_id_counter);

        for (kan_loop_size_t index = 0u; index < node->resource_access_population.size; ++index)
        {
            const kan_instance_size_t resource_id = query_resource (
                &resource_storage, ((kan_interned_string_t *) node->resource_access_population.data)[index]);
            KAN_ASSERT (resource_id < resource_id_counter)
            kan_fixed_length_bitset_set (node->intermediate_access_population, resource_id, true);
        }

        node->intermediate_access_view = (struct kan_fixed_length_bitset_t *) kan_stack_group_allocator_allocate (
            &temporary_allocator, kan_fixed_length_bitset_calculate_allocation_size (resource_id_counter),
            alignof (struct kan_fixed_length_bitset_t));
        kan_fixed_length_bitset_init (node->intermediate_access_view, resource_id_counter);

        for (kan_loop_size_t index = 0u; index < node->resource_access_view.size; ++index)
        {
            const kan_instance_size_t resource_id =
                query_resource (&resource_storage, ((kan_interned_string_t *) node->resource_access_view.data)[index]);
            KAN_ASSERT (resource_id < resource_id_counter)
            kan_fixed_length_bitset_set (node->intermediate_access_view, resource_id, true);
        }

        node->intermediate_access_modification =
            (struct kan_fixed_length_bitset_t *) kan_stack_group_allocator_allocate (
                &temporary_allocator, kan_fixed_length_bitset_calculate_allocation_size (resource_id_counter),
                alignof (struct kan_fixed_length_bitset_t));
        kan_fixed_length_bitset_init (node->intermediate_access_modification, resource_id_counter);

        for (kan_loop_size_t index = 0u; index < node->resource_access_modification.size; ++index)
        {
            const kan_instance_size_t resource_id = query_resource (
                &resource_storage, ((kan_interned_string_t *) node->resource_access_modification.data)[index]);
            KAN_ASSERT (resource_id < resource_id_counter)
            kan_fixed_length_bitset_set (node->intermediate_access_modification, resource_id, true);
        }

        node->intermediate_reachability = (struct kan_fixed_length_bitset_t *) kan_stack_group_allocator_allocate (
            &temporary_allocator, kan_fixed_length_bitset_calculate_allocation_size (builder->nodes.items.size),
            alignof (struct kan_fixed_length_bitset_t));
        kan_fixed_length_bitset_init (node->intermediate_reachability, builder->nodes.items.size);

        node->intermediate_traverse_status = TRAVERSE_STATUS_NOT_TRAVERSED;
        node = (struct building_graph_node_t *) node->node.list_node.next;
    }

    // Traverse graph, fill reachability and check for cycles.
    node = (struct building_graph_node_t *) builder->nodes.items.first;
    bool is_valid = true;

    while (node)
    {
        is_valid = traverse_and_verify (node, id_to_node);
        if (!is_valid)
        {
            break;
        }

        node = (struct building_graph_node_t *) node->node.list_node.next;
    }

    if (is_valid)
    {
        struct building_graph_node_t *first_node = (struct building_graph_node_t *) builder->nodes.items.first;
        while (first_node)
        {
            struct building_graph_node_t *second_node =
                (struct building_graph_node_t *) first_node->node.list_node.next;
            while (second_node)
            {
                const bool can_be_executed_simultaneously =
                    !kan_fixed_length_bitset_get (first_node->intermediate_reachability,
                                                  second_node->intermediate_node_id) &&
                    !kan_fixed_length_bitset_get (second_node->intermediate_reachability,
                                                  first_node->intermediate_node_id);

                if (can_be_executed_simultaneously)
                {
                    const bool view_modification_collision = kan_fixed_length_bitset_check_intersection (
                        first_node->intermediate_access_view, second_node->intermediate_access_modification);
                    const bool modification_view_collision = kan_fixed_length_bitset_check_intersection (
                        first_node->intermediate_access_modification, second_node->intermediate_access_view);

                    const bool view_population_collision = kan_fixed_length_bitset_check_intersection (
                        first_node->intermediate_access_view, second_node->intermediate_access_population);
                    const bool population_view_collision = kan_fixed_length_bitset_check_intersection (
                        first_node->intermediate_access_population, second_node->intermediate_access_view);

                    const bool population_modification_collision = kan_fixed_length_bitset_check_intersection (
                        first_node->intermediate_access_population, second_node->intermediate_access_modification);
                    const bool modification_population_collision = kan_fixed_length_bitset_check_intersection (
                        first_node->intermediate_access_modification, second_node->intermediate_access_population);

                    const bool modification_modification_collision = kan_fixed_length_bitset_check_intersection (
                        first_node->intermediate_access_modification, second_node->intermediate_access_modification);

                    if (view_modification_collision || modification_view_collision || view_population_collision ||
                        population_view_collision || population_modification_collision ||
                        modification_population_collision || modification_modification_collision)
                    {
                        is_valid = false;
                        KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR,
                                 "Found race collision between nodes \"%s\" and \"%s\", enumerating collisions:",
                                 first_node->name, second_node->name)

                        if (view_modification_collision)
                        {
                            KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR,
                                     "- First node has view access and second node has modification access:")

                            print_colliding_resources (first_node->intermediate_access_view,
                                                       second_node->intermediate_access_modification,
                                                       id_to_resource_node);
                        }

                        if (modification_view_collision)
                        {
                            KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR,
                                     "- First node has modification access and second node has view access:")

                            print_colliding_resources (first_node->intermediate_access_modification,
                                                       second_node->intermediate_access_view, id_to_resource_node);
                        }

                        if (view_population_collision)
                        {
                            KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR,
                                     "- First node has view access and second node has population access:")

                            print_colliding_resources (first_node->intermediate_access_view,
                                                       second_node->intermediate_access_population,
                                                       id_to_resource_node);
                        }

                        if (population_view_collision)
                        {
                            KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR,
                                     "- First node has population access and second node has view access:")

                            print_colliding_resources (first_node->intermediate_access_population,
                                                       second_node->intermediate_access_view, id_to_resource_node);
                        }

                        if (population_modification_collision)
                        {
                            KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR,
                                     "- First node has population access and second node has modification access:")

                            print_colliding_resources (first_node->intermediate_access_population,
                                                       second_node->intermediate_access_modification,
                                                       id_to_resource_node);
                        }

                        if (modification_population_collision)
                        {
                            KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR,
                                     "- First node has modification access and second node has population access:")

                            print_colliding_resources (first_node->intermediate_access_modification,
                                                       second_node->intermediate_access_population,
                                                       id_to_resource_node);
                        }

                        if (modification_modification_collision)
                        {
                            KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR,
                                     "- First node has modification access and second node has modification access:")

                            print_colliding_resources (first_node->intermediate_access_modification,
                                                       second_node->intermediate_access_modification,
                                                       id_to_resource_node);
                        }
                    }
                }

                second_node = (struct building_graph_node_t *) second_node->node.list_node.next;
            }

            first_node = (struct building_graph_node_t *) first_node->node.list_node.next;
        }
    }

    kan_stack_group_allocator_shutdown (&temporary_allocator);
    kan_hash_storage_shutdown (&resource_storage);
    return is_valid;
}
#endif

static inline void add_to_interned_string_array (struct kan_dynamic_array_t *array, kan_interned_string_t string)
{
    void *spot = kan_dynamic_array_add_last (array);
    if (!spot)
    {
        kan_dynamic_array_set_capacity (array, array->capacity * 2u);
        spot = kan_dynamic_array_add_last (array);
    }

    KAN_ASSERT (spot)
    *(kan_interned_string_t *) spot = string;
}

static inline void add_to_id_array (struct kan_dynamic_array_t *array, kan_instance_size_t id)
{
    void *spot = kan_dynamic_array_add_last (array);
    if (!spot)
    {
        kan_dynamic_array_set_capacity (array, array->capacity * 2u);
        spot = kan_dynamic_array_add_last (array);
    }

    KAN_ASSERT (spot)
    *(kan_instance_size_t *) spot = id;
}

static inline void remove_from_id_array (struct kan_dynamic_array_t *array, kan_instance_size_t id)
{
    for (kan_loop_size_t index = 0u; index < array->size; ++index)
    {
        if (((kan_instance_size_t *) array->data)[index] == id)
        {
            kan_dynamic_array_remove_swap_at (array, index);
            return;
        }
    }
}

static inline bool building_graph_node_is_checkpoint (struct building_graph_node_t *node)
{
    return node->function == NULL;
}

static void building_graph_node_destroy (struct building_graph_node_t *node, bool has_intermediate_data)
{
    kan_dynamic_array_shutdown (&node->depends_on);
    kan_dynamic_array_shutdown (&node->dependency_of);
    kan_dynamic_array_shutdown (&node->resource_access_population);
    kan_dynamic_array_shutdown (&node->resource_access_view);
    kan_dynamic_array_shutdown (&node->resource_access_modification);

    if (has_intermediate_data)
    {
        kan_dynamic_array_shutdown (&node->intermediate_incomes);
        kan_dynamic_array_shutdown (&node->intermediate_outcomes);
    }

    kan_free_batched (node->builder->builder_group, node);
}

static struct building_graph_node_t *graph_builder_find_node (struct graph_builder_t *builder,
                                                              kan_interned_string_t name)
{
    const struct kan_hash_storage_bucket_t *bucket =
        kan_hash_storage_query (&builder->nodes, KAN_HASH_OBJECT_POINTER (name));
    struct building_graph_node_t *node = (struct building_graph_node_t *) bucket->first;
    const struct building_graph_node_t *node_end =
        (struct building_graph_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->name == name)
        {
            return node;
        }

        node = (struct building_graph_node_t *) node->node.list_node.next;
    }

    return NULL;
}

static struct building_graph_node_t *graph_builder_create_node (struct graph_builder_t *builder,
                                                                kan_interned_string_t name)
{
    struct building_graph_node_t *node =
        kan_allocate_batched (builder->builder_group, sizeof (struct building_graph_node_t));
    node->node.hash = KAN_HASH_OBJECT_POINTER (name);

    node->name = name;
    node->function = NULL;
    node->user_data = 0u;

    kan_dynamic_array_init (&node->depends_on, KAN_WORKFLOW_GRAPH_NODE_INFO_ARRAY_INITIAL_CAPACITY,
                            sizeof (kan_interned_string_t), alignof (kan_interned_string_t), builder->builder_group);

    kan_dynamic_array_init (&node->dependency_of, KAN_WORKFLOW_GRAPH_NODE_INFO_ARRAY_INITIAL_CAPACITY,
                            sizeof (kan_interned_string_t), alignof (kan_interned_string_t), builder->builder_group);

    kan_dynamic_array_init (&node->resource_access_population, KAN_WORKFLOW_GRAPH_NODE_INFO_ARRAY_INITIAL_CAPACITY,
                            sizeof (kan_interned_string_t), alignof (kan_interned_string_t), builder->builder_group);

    kan_dynamic_array_init (&node->resource_access_view, KAN_WORKFLOW_GRAPH_NODE_INFO_ARRAY_INITIAL_CAPACITY,
                            sizeof (kan_interned_string_t), alignof (kan_interned_string_t), builder->builder_group);

    kan_dynamic_array_init (&node->resource_access_modification, KAN_WORKFLOW_GRAPH_NODE_INFO_ARRAY_INITIAL_CAPACITY,
                            sizeof (kan_interned_string_t), alignof (kan_interned_string_t), builder->builder_group);

    node->builder = builder;
    return node;
}

static void graph_builder_submit_node (struct graph_builder_t *builder, struct building_graph_node_t *node)
{
    kan_atomic_int_lock (&builder->node_submission_lock);
    kan_hash_storage_update_bucket_count_default (&builder->nodes, KAN_WORKFLOW_GRAPH_NODES_INITIAL_BUCKETS);
    kan_hash_storage_add (&builder->nodes, &node->node);
    kan_atomic_int_unlock (&builder->node_submission_lock);
}

kan_workflow_graph_builder_t kan_workflow_graph_builder_create (kan_allocation_group_t group)
{
    ensure_statics_initialized ();
    kan_allocation_group_t builder_group = kan_allocation_group_get_child (group, "workflow_graph_builder");
    struct graph_builder_t *builder =
        kan_allocate_general (builder_group, sizeof (struct graph_builder_t), alignof (struct graph_builder_t));

    kan_hash_storage_init (&builder->nodes, builder_group, KAN_WORKFLOW_GRAPH_NODES_INITIAL_BUCKETS);
    builder->node_submission_lock = kan_atomic_int_init (0);
    builder->main_group = group;
    builder->builder_group = builder_group;
    return KAN_HANDLE_SET (kan_workflow_graph_builder_t, builder);
}

bool kan_workflow_graph_builder_register_checkpoint_dependency (kan_workflow_graph_builder_t builder,
                                                                const char *dependency_checkpoint,
                                                                const char *dependant_checkpoint)
{
    struct graph_builder_t *builder_data = KAN_HANDLE_GET (builder);
    kan_interned_string_t interned_dependency_checkpoint = kan_string_intern (dependency_checkpoint);
    kan_interned_string_t interned_dependant_checkpoint = kan_string_intern (dependant_checkpoint);

    struct building_graph_node_t *dependency_node =
        graph_builder_find_node (builder_data, interned_dependency_checkpoint);

    if (!dependency_node)
    {
        dependency_node = graph_builder_create_node (builder_data, interned_dependency_checkpoint);
        graph_builder_submit_node (builder_data, dependency_node);
    }

    if (!building_graph_node_is_checkpoint (dependency_node))
    {
        KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR,
                 "Caught attempt to register checkpoint dependency where dependency \"%s\" is not a checkpoint.",
                 interned_dependency_checkpoint)
        return false;
    }

    add_to_interned_string_array (&dependency_node->dependency_of, interned_dependant_checkpoint);
    return true;
}

static void shutdown_nodes (struct graph_builder_t *builder,
                            bool have_intermediate_data,
                            bool have_intermediate_verification_data)
{
    struct building_graph_node_t *node = (struct building_graph_node_t *) builder->nodes.items.first;
    while (node)
    {
        struct building_graph_node_t *next = (struct building_graph_node_t *) node->node.list_node.next;
        building_graph_node_destroy (node, have_intermediate_data);
        node = next;
    }

    kan_hash_storage_shutdown (&builder->nodes);
}

kan_workflow_graph_t kan_workflow_graph_builder_finalize (kan_workflow_graph_builder_t builder)
{
    struct graph_builder_t *builder_data = KAN_HANDLE_GET (builder);
    if (builder_data->nodes.items.size == 0u)
    {
        KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR, "Caught attempt to finalize empty graph.")
        return KAN_HANDLE_SET_INVALID (kan_workflow_graph_t);
    }

    // Create missing checkpoints.
    struct building_graph_node_t *node = (struct building_graph_node_t *) builder_data->nodes.items.first;

    while (node)
    {
        for (kan_loop_size_t index = 0u; index < node->depends_on.size; ++index)
        {
            kan_interned_string_t name = ((const kan_interned_string_t *) node->depends_on.data)[index];
            if (!graph_builder_find_node (builder_data, name))
            {
                struct building_graph_node_t *new_node = graph_builder_create_node (builder_data, name);
                // As this node is essentially empty, we do not care if we revisit it, so it is ok to just insert it.
                graph_builder_submit_node (builder_data, new_node);
            }
        }

        for (kan_loop_size_t index = 0u; index < node->dependency_of.size; ++index)
        {
            kan_interned_string_t name = ((const kan_interned_string_t *) node->dependency_of.data)[index];
            if (!graph_builder_find_node (builder_data, name))
            {
                struct building_graph_node_t *new_node = graph_builder_create_node (builder_data, name);
                // As this node is essentially empty, we do not care if we revisit it, so it is ok to just insert it.
                graph_builder_submit_node (builder_data, new_node);
            }
        }

        node = (struct building_graph_node_t *) node->node.list_node.next;
    }

    // Initialize non-verification intermediates.
    node = (struct building_graph_node_t *) builder_data->nodes.items.first;
    kan_instance_size_t next_id_to_assign = 0u;

    while (node)
    {
        node->intermediate_node_id = next_id_to_assign++;
        node->intermediate_references_count = 0u;

        kan_dynamic_array_init (&node->intermediate_incomes, KAN_WORKFLOW_GRAPH_NODE_INFO_ARRAY_INITIAL_CAPACITY,
                                sizeof (kan_instance_size_t), alignof (kan_instance_size_t),
                                builder_data->builder_group);

        kan_dynamic_array_init (&node->intermediate_outcomes, KAN_WORKFLOW_GRAPH_NODE_INFO_ARRAY_INITIAL_CAPACITY,
                                sizeof (kan_instance_size_t), alignof (kan_instance_size_t),
                                builder_data->builder_group);

        node = (struct building_graph_node_t *) node->node.list_node.next;
    }

    // Fill incomes and outcomes. Fill id-to-node array along the way.
    node = (struct building_graph_node_t *) builder_data->nodes.items.first;
    struct building_graph_node_t **id_to_node =
        kan_allocate_general (builder_data->builder_group, sizeof (void *) * next_id_to_assign, alignof (void *));

    while (node)
    {
        id_to_node[node->intermediate_node_id] = node;
        for (kan_loop_size_t index = 0u; index < node->depends_on.size; ++index)
        {
            kan_interned_string_t name = ((const kan_interned_string_t *) node->depends_on.data)[index];
            struct building_graph_node_t *found_node = graph_builder_find_node (builder_data, name);

            KAN_ASSERT (found_node)
            ++found_node->intermediate_references_count;

            add_to_id_array (&found_node->intermediate_outcomes, node->intermediate_node_id);
            add_to_id_array (&node->intermediate_incomes, found_node->intermediate_node_id);
        }

        for (kan_loop_size_t index = 0u; index < node->dependency_of.size; ++index)
        {
            kan_interned_string_t name = ((const kan_interned_string_t *) node->dependency_of.data)[index];
            struct building_graph_node_t *found_node = graph_builder_find_node (builder_data, name);

            KAN_ASSERT (found_node)
            ++found_node->intermediate_references_count;

            add_to_id_array (&node->intermediate_outcomes, found_node->intermediate_node_id);
            add_to_id_array (&found_node->intermediate_incomes, node->intermediate_node_id);
        }

        node = (struct building_graph_node_t *) node->node.list_node.next;
    }

    // Remove all checkpoints from graph.
    node = (struct building_graph_node_t *) builder_data->nodes.items.first;

    while (node)
    {
        struct building_graph_node_t *next = (struct building_graph_node_t *) node->node.list_node.next;
        if (building_graph_node_is_checkpoint (node))
        {
            if (node->intermediate_references_count <= 1u)
            {
                KAN_LOG (workflow_graph_builder, KAN_LOG_WARNING,
                         "Checkpoint \"%s\" is only referenced once. Misspelling or redundant checkpoint?", node->name)
            }

            for (kan_loop_size_t outcome_index = 0u; outcome_index < node->intermediate_outcomes.size; ++outcome_index)
            {
                const kan_instance_size_t outcome_id =
                    ((kan_instance_size_t *) node->intermediate_outcomes.data)[outcome_index];
                struct building_graph_node_t *outcome = id_to_node[outcome_id];
                remove_from_id_array (&outcome->intermediate_incomes, node->intermediate_node_id);
            }

            for (kan_loop_size_t income_index = 0u; income_index < node->intermediate_incomes.size; ++income_index)
            {
                const kan_instance_size_t income_id =
                    ((kan_instance_size_t *) node->intermediate_incomes.data)[income_index];
                struct building_graph_node_t *income = id_to_node[income_id];
                remove_from_id_array (&income->intermediate_outcomes, node->intermediate_node_id);

                for (kan_loop_size_t outcome_index = 0u; outcome_index < node->intermediate_outcomes.size;
                     ++outcome_index)
                {
                    const kan_instance_size_t outcome_id =
                        ((kan_instance_size_t *) node->intermediate_outcomes.data)[outcome_index];
                    struct building_graph_node_t *outcome = id_to_node[outcome_id];
                    add_to_id_array (&outcome->intermediate_incomes, income_id);
                    add_to_id_array (&income->intermediate_outcomes, outcome_id);
                }
            }

            kan_hash_storage_remove (&builder_data->nodes, &node->node);
            building_graph_node_destroy (node, true);
        }

        node = next;
    }

    if (builder_data->nodes.items.size == 0u)
    {
        KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR,
                 "Caught attempt to finalize graph with only checkpoints and no functional nodes.")
        kan_free_general (builder_data->builder_group, id_to_node, sizeof (void *) * next_id_to_assign);
        return KAN_HANDLE_SET_INVALID (kan_workflow_graph_t);
    }

    bool is_valid = true;
    struct workflow_graph_header_t *result_graph = NULL;

#if defined(KAN_WORKFLOW_VERIFY)
    is_valid = graph_builder_verify_intermediate (builder_data, id_to_node);
#endif

    if (is_valid)
    {
        kan_instance_size_t start_nodes_count = 0u;
        kan_instance_size_t body_size = 0u;

        // Calculate data for workflow allocation.
        node = (struct building_graph_node_t *) builder_data->nodes.items.first;

        while (node)
        {
            if (node->intermediate_incomes.size == 0u)
            {
                ++start_nodes_count;
            }

            body_size = (kan_instance_size_t) kan_apply_alignment (
                body_size + sizeof (struct workflow_graph_node_t) + sizeof (void *) * node->intermediate_outcomes.size,
                alignof (struct workflow_graph_node_t));
            node = (struct building_graph_node_t *) node->node.list_node.next;
        }

        if (start_nodes_count > 0u)
        {
            static_assert (alignof (struct workflow_graph_header_t) == alignof (struct workflow_graph_node_t),
                           "Workflow header and body have matching alignment.");

            const kan_instance_size_t header_size = (kan_instance_size_t) kan_apply_alignment (
                sizeof (struct workflow_graph_header_t) + sizeof (void *) * start_nodes_count,
                sizeof (struct workflow_graph_header_t));
            const kan_instance_size_t graph_size = header_size + body_size;

            result_graph = (struct workflow_graph_header_t *) kan_allocate_general (
                builder_data->main_group, graph_size, alignof (struct workflow_graph_header_t));

            result_graph->total_nodes_count = builder_data->nodes.items.size;
            result_graph->start_nodes_count = start_nodes_count;

            kan_stack_group_allocator_init (&result_graph->temporary_allocator, builder_data->main_group,
                                            KAN_WORKFLOW_EXECUTION_STACK_SIZE);
            result_graph->temporary_allocator_lock = kan_atomic_int_init (0);

            result_graph->nodes_left_to_execute_mutex = kan_mutex_create ();
            result_graph->nodes_left_to_execute_signal = kan_conditional_variable_create ();

            result_graph->allocation_group = builder_data->main_group;
            result_graph->allocation_size = graph_size;

            struct workflow_graph_node_t **id_to_built_node = (struct workflow_graph_node_t **) kan_allocate_general (
                builder_data->builder_group, sizeof (void *) * next_id_to_assign, alignof (void *));

            // Fill basic data about built nodes and fill id to built nodes array. Assign start nodes.
            node = (struct building_graph_node_t *) builder_data->nodes.items.first;
            kan_instance_size_t next_start_node_index = 0u;
            uint8_t *nodes_base = ((uint8_t *) result_graph) + header_size;
            kan_instance_size_t node_offset = 0u;

            while (node)
            {
                struct workflow_graph_node_t *built_node = (struct workflow_graph_node_t *) (nodes_base + node_offset);
                id_to_built_node[node->intermediate_node_id] = built_node;

                if (node->intermediate_incomes.size == 0u)
                {
                    result_graph->start_nodes[next_start_node_index++] = built_node;
                }

                built_node->function = node->function;
                built_node->user_data = node->user_data;
                built_node->profiler_section = kan_cpu_section_get (node->name);

                built_node->header = result_graph;
                built_node->incomes_count = node->intermediate_incomes.size;
                built_node->incomes_left = kan_atomic_int_init ((int) built_node->incomes_count);
                built_node->outcomes_count = node->intermediate_outcomes.size;

                node_offset =
                    (kan_instance_size_t) kan_apply_alignment (node_offset + sizeof (struct workflow_graph_node_t) +
                                                                   sizeof (void *) * node->intermediate_outcomes.size,
                                                               alignof (struct workflow_graph_node_t));
                node = (struct building_graph_node_t *) node->node.list_node.next;
            }

            // Fill outcomes in built nodes.
            node = (struct building_graph_node_t *) builder_data->nodes.items.first;
            node_offset = 0u;

            while (node)
            {
                struct workflow_graph_node_t *built_node = (struct workflow_graph_node_t *) (nodes_base + node_offset);
                for (kan_loop_size_t index = 0u; index < node->intermediate_outcomes.size; ++index)
                {
                    built_node->outcomes[index] =
                        id_to_built_node[((kan_instance_size_t *) node->intermediate_outcomes.data)[index]];
                }

                node_offset =
                    (kan_instance_size_t) kan_apply_alignment (node_offset + sizeof (struct workflow_graph_node_t) +
                                                                   sizeof (void *) * node->intermediate_outcomes.size,
                                                               alignof (struct workflow_graph_node_t));
                node = (struct building_graph_node_t *) node->node.list_node.next;
            }

            kan_free_general (builder_data->builder_group, id_to_built_node, sizeof (void *) * next_id_to_assign);
        }
        else
        {
            KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR,
                     "Graph has no start nodes. Perhaps, there is a cycle somewhere?")
        }
    }

    kan_free_general (builder_data->builder_group, id_to_node, sizeof (void *) * next_id_to_assign);
    shutdown_nodes (builder_data, true, true);
    kan_hash_storage_init (&builder_data->nodes, builder_data->builder_group, KAN_WORKFLOW_GRAPH_NODES_INITIAL_BUCKETS);
    return result_graph ? KAN_HANDLE_SET (kan_workflow_graph_t, result_graph) :
                          KAN_HANDLE_SET_INVALID (kan_workflow_graph_t);
}

void kan_workflow_graph_builder_destroy (kan_workflow_graph_builder_t builder)
{
    struct graph_builder_t *builder_data = KAN_HANDLE_GET (builder);
    shutdown_nodes (builder_data, false, false);
    kan_free_general (builder_data->builder_group, builder_data, sizeof (struct graph_builder_t));
}

kan_workflow_graph_node_t kan_workflow_graph_node_create (kan_workflow_graph_builder_t builder, const char *name)
{
    struct graph_builder_t *builder_data = KAN_HANDLE_GET (builder);
    kan_interned_string_t interned_name = kan_string_intern (name);
    struct building_graph_node_t *node = graph_builder_create_node (builder_data, interned_name);
    return KAN_HANDLE_SET (kan_workflow_graph_node_t, node);
}

void kan_workflow_graph_node_set_function (kan_workflow_graph_node_t node,
                                           kan_workflow_function_t function,
                                           kan_functor_user_data_t user_data)
{
    struct building_graph_node_t *node_data = KAN_HANDLE_GET (node);
    node_data->function = function;
    node_data->user_data = user_data;
}

void kan_workflow_graph_node_register_access (kan_workflow_graph_node_t node,
                                              const char *resource_name,
                                              enum kan_workflow_resource_access_class_t access_class)
{
    struct building_graph_node_t *node_data = KAN_HANDLE_GET (node);
    switch (access_class)
    {
    case KAN_WORKFLOW_RESOURCE_ACCESS_CLASS_POPULATION:
        add_to_interned_string_array (&node_data->resource_access_population, kan_string_intern (resource_name));
        break;

    case KAN_WORKFLOW_RESOURCE_ACCESS_CLASS_VIEW:
        add_to_interned_string_array (&node_data->resource_access_view, kan_string_intern (resource_name));
        break;

    case KAN_WORKFLOW_RESOURCE_ACCESS_CLASS_MODIFICATION:
        add_to_interned_string_array (&node_data->resource_access_modification, kan_string_intern (resource_name));
        break;
    }
}

void kan_workflow_graph_node_depend_on (kan_workflow_graph_node_t node, const char *name)
{
    struct building_graph_node_t *node_data = KAN_HANDLE_GET (node);
    add_to_interned_string_array (&node_data->depends_on, kan_string_intern (name));
}

void kan_workflow_graph_node_make_dependency_of (kan_workflow_graph_node_t node, const char *name)
{
    struct building_graph_node_t *node_data = KAN_HANDLE_GET (node);
    add_to_interned_string_array (&node_data->dependency_of, kan_string_intern (name));
}

bool kan_workflow_graph_node_submit (kan_workflow_graph_node_t node)
{
    struct building_graph_node_t *node_data = KAN_HANDLE_GET (node);
    if (building_graph_node_is_checkpoint (node_data))
    {
        KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR,
                 "Failed to submit workflow node \"%s\" as it has no function and therefore simulates checkpoint.",
                 node_data->name)
        return false;
    }

    if (graph_builder_find_node (node_data->builder, node_data->name))
    {
        KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR,
                 "Failed to submit workflow node \"%s\" as there is already node with the same name.", node_data->name)
        return false;
    }

    graph_builder_submit_node (node_data->builder, node_data);
    return true;
}

void kan_workflow_graph_node_destroy (kan_workflow_graph_node_t node)
{
    building_graph_node_destroy (KAN_HANDLE_GET (node), false);
}

static void workflow_task_finish_function (kan_functor_user_data_t user_data);

static void workflow_task_execute_function (kan_functor_user_data_t user_data);

static void workflow_task_start_function (kan_functor_user_data_t user_data)
{
    struct workflow_graph_node_t *node = (struct workflow_graph_node_t *) user_data;
    node->job = kan_cpu_job_create ();
    kan_cpu_job_set_completion_task (node->job, (struct kan_cpu_task_t) {
                                                    .function = workflow_task_finish_function,
                                                    .user_data = user_data,
                                                    .profiler_section = task_finish_section,
                                                });

    kan_cpu_task_t task_handle = kan_cpu_job_dispatch_task (node->job, (struct kan_cpu_task_t) {
                                                                           .function = workflow_task_execute_function,
                                                                           .user_data = user_data,
                                                                           .profiler_section = node->profiler_section,
                                                                       });

    if (KAN_HANDLE_IS_VALID (task_handle))
    {
        kan_cpu_task_detach (task_handle);
    }
}

static void workflow_task_execute_function (kan_functor_user_data_t user_data)
{
    struct workflow_graph_node_t *node = (struct workflow_graph_node_t *) user_data;
    node->function (node->job, node->user_data);
}

static void workflow_task_finish_function (kan_functor_user_data_t user_data)
{
    struct workflow_graph_node_t *node = (struct workflow_graph_node_t *) user_data;
    kan_cpu_job_detach (node->job);
    node->incomes_left = kan_atomic_int_init ((int) node->incomes_count);
    struct kan_cpu_task_list_node_t *first_list_node = NULL;

    for (kan_loop_size_t outcome_index = 0u; outcome_index < node->outcomes_count; ++outcome_index)
    {
        struct workflow_graph_node_t *outcome = node->outcomes[outcome_index];
        if (kan_atomic_int_add (&outcome->incomes_left, -1) == 1)
        {
            kan_atomic_int_lock (&node->header->temporary_allocator_lock);
            struct kan_cpu_task_list_node_t *list_node = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
                &node->header->temporary_allocator, struct kan_cpu_task_list_node_t);
            kan_atomic_int_unlock (&node->header->temporary_allocator_lock);

            list_node->task = (struct kan_cpu_task_t) {
                .function = workflow_task_start_function,
                .user_data = (kan_functor_user_data_t) outcome,
                .profiler_section = task_start_section,
            };

            list_node->next = first_list_node;
            first_list_node = list_node;
        }
    }

    if (first_list_node)
    {
        kan_cpu_task_dispatch_list (first_list_node);
        while (first_list_node)
        {
            kan_cpu_task_detach (first_list_node->dispatch_handle);
            first_list_node = first_list_node->next;
        }
    }

    kan_mutex_lock (node->header->nodes_left_to_execute_mutex);
    --node->header->nodes_left_to_execute;
    const bool signal = node->header->nodes_left_to_execute == 0u;
    kan_mutex_unlock (node->header->nodes_left_to_execute_mutex);

    if (signal)
    {
        kan_conditional_variable_signal_one (node->header->nodes_left_to_execute_signal);
    }
}

void kan_workflow_graph_execute (kan_workflow_graph_t graph)
{
    struct workflow_graph_header_t *graph_header = KAN_HANDLE_GET (graph);
    graph_header->nodes_left_to_execute = graph_header->total_nodes_count;
    KAN_ASSERT (graph_header->start_nodes_count > 0u)
    struct kan_cpu_task_list_node_t *first_list_node = NULL;

    for (kan_loop_size_t start_index = 0u; start_index < graph_header->start_nodes_count; ++start_index)
    {
        struct workflow_graph_node_t *start = graph_header->start_nodes[start_index];
        KAN_CPU_TASK_LIST_USER_VALUE (&first_list_node, &graph_header->temporary_allocator,
                                      workflow_task_start_function, task_start_section, start)
    }

    kan_cpu_task_dispatch_list (first_list_node);
    while (first_list_node)
    {
        kan_cpu_task_detach (first_list_node->dispatch_handle);
        first_list_node = first_list_node->next;
    }

    kan_mutex_lock (graph_header->nodes_left_to_execute_mutex);
    while (true)
    {
        const bool signaled = graph_header->nodes_left_to_execute == 0u;
        if (signaled)
        {
            kan_mutex_unlock (graph_header->nodes_left_to_execute_mutex);
            break;
        }

        kan_conditional_variable_wait (graph_header->nodes_left_to_execute_signal,
                                       graph_header->nodes_left_to_execute_mutex);
    }

    kan_stack_group_allocator_reset (&graph_header->temporary_allocator);
}

void kan_workflow_graph_destroy (kan_workflow_graph_t graph)
{
    struct workflow_graph_header_t *graph_header = KAN_HANDLE_GET (graph);
    kan_stack_group_allocator_shutdown (&graph_header->temporary_allocator);
    kan_mutex_destroy (graph_header->nodes_left_to_execute_mutex);
    kan_conditional_variable_destroy (graph_header->nodes_left_to_execute_signal);
    kan_free_general (graph_header->allocation_group, graph_header, graph_header->allocation_size);
}
