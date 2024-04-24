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
    kan_workflow_user_data_t user_data;

    /// \meta reflection_dynamic_array_type = "kan_interned_string_t"
    struct kan_dynamic_array_t depends_on;

    /// \meta reflection_dynamic_array_type = "kan_interned_string_t"
    struct kan_dynamic_array_t dependency_of;

    /// \meta reflection_dynamic_array_type = "kan_interned_string_t"
    struct kan_dynamic_array_t resource_insert_access;

    /// \meta reflection_dynamic_array_type = "kan_interned_string_t"
    struct kan_dynamic_array_t resource_write_access;

    /// \meta reflection_dynamic_array_type = "kan_interned_string_t"
    struct kan_dynamic_array_t resource_read_access;

    struct graph_builder_t *builder;

    uint64_t intermediate_node_id;
    uint64_t intermediate_references_count;

    /// \meta reflection_dynamic_array_type = "uint64_t"
    struct kan_dynamic_array_t intermediate_incomes;

    /// \meta reflection_dynamic_array_type = "uint64_t"
    struct kan_dynamic_array_t intermediate_outcomes;

#if defined(KAN_WORKFLOW_VERIFY)
    struct kan_fixed_length_bitset_t *intermediate_insert_access;
    struct kan_fixed_length_bitset_t *intermediate_write_access;
    struct kan_fixed_length_bitset_t *intermediate_read_access;
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
    kan_interned_string_t name;
    kan_workflow_function_t function;
    kan_workflow_user_data_t user_data;

    struct workflow_graph_header_t *header;
    uint64_t incomes_count;
    struct kan_atomic_int_t incomes_left;
    kan_cpu_job_t job;

    uint64_t outcomes_count;
    struct workflow_graph_node_t *outcomes[];
};

struct workflow_graph_header_t
{
    uint64_t total_nodes_count;
    uint64_t start_nodes_count;

    struct kan_stack_group_allocator_t temporary_allocator;
    struct kan_atomic_int_t temporary_allocator_lock;

    uint64_t nodes_left_to_execute;
    kan_mutex_handle_t nodes_left_to_execute_mutex;
    kan_conditional_variable_handle_t nodes_left_to_execute_signal;

    kan_allocation_group_t allocation_group;
    uint64_t allocation_size;
    struct workflow_graph_node_t *start_nodes[];
};

#if defined(KAN_WORKFLOW_VERIFY)
struct resource_info_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t name;
    uint64_t id;
};

static inline uint64_t query_resource (struct kan_hash_storage_t *hash_storage, kan_interned_string_t resource_name)
{
    const struct kan_hash_storage_bucket_t *bucket = kan_hash_storage_query (hash_storage, (uint64_t) resource_name);
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

    return UINT64_MAX;
}

static inline void register_resource (struct kan_hash_storage_t *hash_storage,
                                      kan_interned_string_t resource_name,
                                      uint64_t *id_counter,
                                      struct kan_stack_group_allocator_t *temporary_allocator)
{
    if (query_resource (hash_storage, resource_name) != UINT64_MAX)
    {
        return;
    }

    if (hash_storage->bucket_count * KAN_WORKFLOW_RESOURCE_LOAD_FACTOR < hash_storage->items.size)
    {
        kan_hash_storage_set_bucket_count (hash_storage, hash_storage->bucket_count * 2u);
    }

    struct resource_info_node_t *node =
        KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (temporary_allocator, struct resource_info_node_t);
    node->node.hash = (uint64_t) resource_name;
    node->name = resource_name;
    node->id = *id_counter;

    if (hash_storage->bucket_count * KAN_WORKFLOW_RESOURCE_LOAD_FACTOR <= hash_storage->items.size)
    {
        kan_hash_storage_set_bucket_count (hash_storage, hash_storage->bucket_count * 2u);
    }

    kan_hash_storage_add (hash_storage, &node->node);
    ++*id_counter;
}

static kan_bool_t traverse_and_verify (struct building_graph_node_t *node, struct building_graph_node_t **id_to_node)
{
    switch (node->intermediate_traverse_status)
    {
    case TRAVERSE_STATUS_NOT_TRAVERSED:
        node->intermediate_traverse_status = TRAVERSE_STATUS_IN_PROGRESS;
        for (uint64_t outcome_index = 0u; outcome_index < node->intermediate_outcomes.size; ++outcome_index)
        {
            const uint64_t outcome_id = ((uint64_t *) node->intermediate_outcomes.data)[outcome_index];
            struct building_graph_node_t *outcome = id_to_node[outcome_id];

            if (!traverse_and_verify (outcome, id_to_node))
            {
                KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR, "- \"%s\"", node->name)
                return KAN_FALSE;
            }

            kan_fixed_length_bitset_set (node->intermediate_reachability, outcome_id, KAN_TRUE);
            kan_fixed_length_bitset_or_assign (node->intermediate_reachability, outcome->intermediate_reachability);
        }

        node->intermediate_traverse_status = TRAVERSE_STATUS_DONE;
        return KAN_TRUE;

    case TRAVERSE_STATUS_IN_PROGRESS:
        KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR, "Caught cycle in workflow graph. Dumping node stack: ")
        KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR, "- \"%s\"", node->name)
        return KAN_FALSE;

    case TRAVERSE_STATUS_DONE:
        return KAN_TRUE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_FALSE;
}

static inline void print_colliding_resources (struct kan_fixed_length_bitset_t *first,
                                              struct kan_fixed_length_bitset_t *second,
                                              struct resource_info_node_t **id_to_resource_node)
{
    KAN_ASSERT (first->items == second->items)
    for (uint64_t item_index = 0u; item_index < first->items; ++item_index)
    {
        const uint64_t intersection = first->data[item_index] & second->data[item_index];
        if (intersection != 0u)
        {
            for (uint64_t bit_index = 0u; bit_index < 64u; ++bit_index)
            {
                if ((intersection & (((uint64_t) 1u) << bit_index)) > 0u)
                {
                    KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR, "  - \"%s\"",
                             id_to_resource_node[item_index * 64u + bit_index]->name)
                }
            }
        }
    }
}

static kan_bool_t graph_builder_verify_intermediate (struct graph_builder_t *builder,
                                                     struct building_graph_node_t **id_to_node)
{
    struct kan_hash_storage_t resource_storage;
    kan_hash_storage_init (&resource_storage, builder->builder_group, KAN_WORKFLOW_RESOURCE_INITIAL_BUCKETS);

    struct kan_stack_group_allocator_t temporary_allocator;
    kan_stack_group_allocator_init (&temporary_allocator, builder->builder_group, KAN_WORKFLOW_VERIFICATION_STACK_SIZE);

    // Register all resource types.
    struct building_graph_node_t *node = (struct building_graph_node_t *) builder->nodes.items.first;
    uint64_t resource_id_counter = 0u;

    while (node)
    {
        for (uint64_t index = 0u; index < node->resource_insert_access.size; ++index)
        {
            register_resource (&resource_storage, ((kan_interned_string_t *) node->resource_insert_access.data)[index],
                               &resource_id_counter, &temporary_allocator);
        }

        for (uint64_t index = 0u; index < node->resource_write_access.size; ++index)
        {
            register_resource (&resource_storage, ((kan_interned_string_t *) node->resource_write_access.data)[index],
                               &resource_id_counter, &temporary_allocator);
        }

        for (uint64_t index = 0u; index < node->resource_read_access.size; ++index)
        {
            register_resource (&resource_storage, ((kan_interned_string_t *) node->resource_read_access.data)[index],
                               &resource_id_counter, &temporary_allocator);
        }

        node = (struct building_graph_node_t *) node->node.list_node.next;
    }

    // Fill id to resource node array.
    struct resource_info_node_t **id_to_resource_node = kan_stack_group_allocator_allocate (
        &temporary_allocator, sizeof (void *) * resource_id_counter, _Alignof (void *));
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
        node->intermediate_insert_access = (struct kan_fixed_length_bitset_t *) kan_stack_group_allocator_allocate (
            &temporary_allocator, kan_fixed_length_bitset_calculate_allocation_size (resource_id_counter),
            _Alignof (struct kan_fixed_length_bitset_t));
        kan_fixed_length_bitset_init (node->intermediate_insert_access, resource_id_counter);

        for (uint64_t index = 0u; index < node->resource_insert_access.size; ++index)
        {
            const uint64_t resource_id = query_resource (
                &resource_storage, ((kan_interned_string_t *) node->resource_insert_access.data)[index]);
            KAN_ASSERT (resource_id < resource_id_counter)
            kan_fixed_length_bitset_set (node->intermediate_insert_access, resource_id, KAN_TRUE);
        }

        node->intermediate_write_access = (struct kan_fixed_length_bitset_t *) kan_stack_group_allocator_allocate (
            &temporary_allocator, kan_fixed_length_bitset_calculate_allocation_size (resource_id_counter),
            _Alignof (struct kan_fixed_length_bitset_t));
        kan_fixed_length_bitset_init (node->intermediate_write_access, resource_id_counter);

        for (uint64_t index = 0u; index < node->resource_write_access.size; ++index)
        {
            const uint64_t resource_id =
                query_resource (&resource_storage, ((kan_interned_string_t *) node->resource_write_access.data)[index]);
            KAN_ASSERT (resource_id < resource_id_counter)
            kan_fixed_length_bitset_set (node->intermediate_write_access, resource_id, KAN_TRUE);
        }

        node->intermediate_read_access = (struct kan_fixed_length_bitset_t *) kan_stack_group_allocator_allocate (
            &temporary_allocator, kan_fixed_length_bitset_calculate_allocation_size (resource_id_counter),
            _Alignof (struct kan_fixed_length_bitset_t));
        kan_fixed_length_bitset_init (node->intermediate_read_access, resource_id_counter);

        for (uint64_t index = 0u; index < node->resource_read_access.size; ++index)
        {
            const uint64_t resource_id =
                query_resource (&resource_storage, ((kan_interned_string_t *) node->resource_read_access.data)[index]);
            KAN_ASSERT (resource_id < resource_id_counter)
            kan_fixed_length_bitset_set (node->intermediate_read_access, resource_id, KAN_TRUE);
        }

        node->intermediate_reachability = (struct kan_fixed_length_bitset_t *) kan_stack_group_allocator_allocate (
            &temporary_allocator, kan_fixed_length_bitset_calculate_allocation_size (builder->nodes.items.size),
            _Alignof (struct kan_fixed_length_bitset_t));
        kan_fixed_length_bitset_init (node->intermediate_reachability, builder->nodes.items.size);

        node->intermediate_traverse_status = TRAVERSE_STATUS_NOT_TRAVERSED;
        node = (struct building_graph_node_t *) node->node.list_node.next;
    }

    // Traverse graph, fill reachability and check for cycles.
    node = (struct building_graph_node_t *) builder->nodes.items.first;
    kan_bool_t is_valid = KAN_TRUE;

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
                const kan_bool_t can_be_executed_simultaneously =
                    !kan_fixed_length_bitset_get (first_node->intermediate_reachability,
                                                  second_node->intermediate_node_id) &&
                    !kan_fixed_length_bitset_get (second_node->intermediate_reachability,
                                                  first_node->intermediate_node_id);

                if (can_be_executed_simultaneously)
                {
                    const kan_bool_t read_write_collision = kan_fixed_length_bitset_check_intersection (
                        first_node->intermediate_read_access, second_node->intermediate_write_access);
                    const kan_bool_t write_read_collision = kan_fixed_length_bitset_check_intersection (
                        first_node->intermediate_write_access, second_node->intermediate_read_access);

                    const kan_bool_t read_insert_collision = kan_fixed_length_bitset_check_intersection (
                        first_node->intermediate_read_access, second_node->intermediate_insert_access);
                    const kan_bool_t insert_read_collision = kan_fixed_length_bitset_check_intersection (
                        first_node->intermediate_insert_access, second_node->intermediate_read_access);

                    const kan_bool_t insert_write_collision = kan_fixed_length_bitset_check_intersection (
                        first_node->intermediate_insert_access, second_node->intermediate_write_access);
                    const kan_bool_t write_insert_collision = kan_fixed_length_bitset_check_intersection (
                        first_node->intermediate_write_access, second_node->intermediate_insert_access);

                    const kan_bool_t write_write_collision = kan_fixed_length_bitset_check_intersection (
                        first_node->intermediate_write_access, second_node->intermediate_write_access);

                    if (read_write_collision || write_read_collision || read_insert_collision ||
                        insert_read_collision || insert_write_collision || write_insert_collision ||
                        write_write_collision)
                    {
                        is_valid = KAN_FALSE;
                        KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR,
                                 "Found race collision between nodes \"%s\" and \"%s\", enumerating collisions:",
                                 first_node->name, second_node->name)

                        if (read_write_collision)
                        {
                            KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR,
                                     "- First node reads and second node writes:")

                            print_colliding_resources (first_node->intermediate_read_access,
                                                       second_node->intermediate_write_access, id_to_resource_node);
                        }

                        if (write_read_collision)
                        {
                            KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR,
                                     "- First node writes and second node reads:")

                            print_colliding_resources (first_node->intermediate_write_access,
                                                       second_node->intermediate_read_access, id_to_resource_node);
                        }

                        if (read_insert_collision)
                        {
                            KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR,
                                     "- First node reads and second node inserts:")

                            print_colliding_resources (first_node->intermediate_read_access,
                                                       second_node->intermediate_insert_access, id_to_resource_node);
                        }

                        if (insert_read_collision)
                        {
                            KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR,
                                     "- First node inserts and second node reads:")

                            print_colliding_resources (first_node->intermediate_insert_access,
                                                       second_node->intermediate_read_access, id_to_resource_node);
                        }

                        if (insert_write_collision)
                        {
                            KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR,
                                     "- First node inserts and second node writes:")

                            print_colliding_resources (first_node->intermediate_insert_access,
                                                       second_node->intermediate_write_access, id_to_resource_node);
                        }

                        if (write_insert_collision)
                        {
                            KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR,
                                     "- First node writes and second node inserts:")

                            print_colliding_resources (first_node->intermediate_write_access,
                                                       second_node->intermediate_insert_access, id_to_resource_node);
                        }

                        if (write_write_collision)
                        {
                            KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR,
                                     "- First node writes and second node writes:")

                            print_colliding_resources (first_node->intermediate_write_access,
                                                       second_node->intermediate_write_access, id_to_resource_node);
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

static inline void add_to_id_array (struct kan_dynamic_array_t *array, uint64_t id)
{
    void *spot = kan_dynamic_array_add_last (array);
    if (!spot)
    {
        kan_dynamic_array_set_capacity (array, array->capacity * 2u);
        spot = kan_dynamic_array_add_last (array);
    }

    KAN_ASSERT (spot)
    *(uint64_t *) spot = id;
}

static inline void remove_from_id_array (struct kan_dynamic_array_t *array, uint64_t id)
{
    for (uint64_t index = 0u; index < array->size; ++index)
    {
        if (((uint64_t *) array->data)[index] == id)
        {
            kan_dynamic_array_remove_swap_at (array, index);
            return;
        }
    }
}

static inline kan_bool_t building_graph_node_is_checkpoint (struct building_graph_node_t *node)
{
    return node->function == NULL;
}

static void building_graph_node_destroy (struct building_graph_node_t *node, kan_bool_t has_intermediate_data)
{
    kan_dynamic_array_shutdown (&node->depends_on);
    kan_dynamic_array_shutdown (&node->dependency_of);
    kan_dynamic_array_shutdown (&node->resource_insert_access);
    kan_dynamic_array_shutdown (&node->resource_write_access);
    kan_dynamic_array_shutdown (&node->resource_read_access);

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
    const struct kan_hash_storage_bucket_t *bucket = kan_hash_storage_query (&builder->nodes, (uint64_t) name);
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
    node->node.hash = (uint64_t) name;

    node->name = name;
    node->function = NULL;
    node->user_data = 0u;

    kan_dynamic_array_init (&node->depends_on, KAN_WORKFLOW_GRAPH_NODE_INFO_ARRAY_INITIAL_CAPACITY,
                            sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t), builder->builder_group);

    kan_dynamic_array_init (&node->dependency_of, KAN_WORKFLOW_GRAPH_NODE_INFO_ARRAY_INITIAL_CAPACITY,
                            sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t), builder->builder_group);

    kan_dynamic_array_init (&node->resource_insert_access, KAN_WORKFLOW_GRAPH_NODE_INFO_ARRAY_INITIAL_CAPACITY,
                            sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t), builder->builder_group);

    kan_dynamic_array_init (&node->resource_write_access, KAN_WORKFLOW_GRAPH_NODE_INFO_ARRAY_INITIAL_CAPACITY,
                            sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t), builder->builder_group);

    kan_dynamic_array_init (&node->resource_read_access, KAN_WORKFLOW_GRAPH_NODE_INFO_ARRAY_INITIAL_CAPACITY,
                            sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t), builder->builder_group);

    node->builder = builder;
    return node;
}

static void graph_builder_submit_node (struct graph_builder_t *builder, struct building_graph_node_t *node)
{
    kan_atomic_int_lock (&builder->node_submission_lock);
    if (builder->nodes.bucket_count * KAN_WORKFLOW_GRAPH_NODES_LOAD_FACTOR < builder->nodes.items.size)
    {
        kan_hash_storage_set_bucket_count (&builder->nodes, builder->nodes.bucket_count * 2u);
    }

    kan_hash_storage_add (&builder->nodes, &node->node);
    kan_atomic_int_unlock (&builder->node_submission_lock);
}

kan_workflow_graph_builder_t kan_workflow_graph_builder_create (kan_allocation_group_t group)
{
    kan_allocation_group_t builder_group = kan_allocation_group_get_child (group, "workflow_graph_builder");
    struct graph_builder_t *builder =
        kan_allocate_general (builder_group, sizeof (struct graph_builder_t), _Alignof (struct graph_builder_t));

    kan_hash_storage_init (&builder->nodes, builder_group, KAN_WORKFLOW_GRAPH_NODES_INITIAL_BUCKETS);
    builder->node_submission_lock = kan_atomic_int_init (0);
    builder->main_group = group;
    builder->builder_group = builder_group;
    return (kan_workflow_graph_builder_t) builder;
}

kan_bool_t kan_workflow_graph_builder_register_checkpoint_dependency (kan_workflow_graph_builder_t builder,
                                                                      const char *dependency_checkpoint,
                                                                      const char *dependant_checkpoint)
{
    struct graph_builder_t *builder_data = (struct graph_builder_t *) builder;
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
        return KAN_FALSE;
    }

    add_to_interned_string_array (&dependency_node->dependency_of, interned_dependant_checkpoint);
    return KAN_TRUE;
}

static void shutdown_nodes (struct graph_builder_t *builder,
                            kan_bool_t have_intermediate_data,
                            kan_bool_t have_intermediate_verification_data)
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
    struct graph_builder_t *builder_data = (struct graph_builder_t *) builder;
    if (builder_data->nodes.items.size == 0u)
    {
        KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR, "Caught attempt to finalize empty graph.")
        return KAN_INVALID_WORKFLOW_GRAPH;
    }

    // Create missing checkpoints.
    struct building_graph_node_t *node = (struct building_graph_node_t *) builder_data->nodes.items.first;

    while (node)
    {
        for (uint64_t index = 0u; index < node->depends_on.size; ++index)
        {
            kan_interned_string_t name = ((const kan_interned_string_t *) node->depends_on.data)[index];
            if (!graph_builder_find_node (builder_data, name))
            {
                struct building_graph_node_t *new_node = graph_builder_create_node (builder_data, name);
                // As this node is essentially empty, we do not care if we revisit it, so it is ok to just insert it.
                graph_builder_submit_node (builder_data, new_node);
            }
        }

        for (uint64_t index = 0u; index < node->dependency_of.size; ++index)
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
    uint64_t next_id_to_assign = 0u;

    while (node)
    {
        node->intermediate_node_id = next_id_to_assign++;
        node->intermediate_references_count = 0u;

        kan_dynamic_array_init (&node->intermediate_incomes, KAN_WORKFLOW_GRAPH_NODE_INFO_ARRAY_INITIAL_CAPACITY,
                                sizeof (uint64_t), _Alignof (uint64_t), builder_data->builder_group);

        kan_dynamic_array_init (&node->intermediate_outcomes, KAN_WORKFLOW_GRAPH_NODE_INFO_ARRAY_INITIAL_CAPACITY,
                                sizeof (uint64_t), _Alignof (uint64_t), builder_data->builder_group);

        node = (struct building_graph_node_t *) node->node.list_node.next;
    }

    // Fill incomes and outcomes. Fill id-to-node array along the way.
    node = (struct building_graph_node_t *) builder_data->nodes.items.first;
    struct building_graph_node_t **id_to_node =
        kan_allocate_general (builder_data->builder_group, sizeof (void *) * next_id_to_assign, _Alignof (void *));

    while (node)
    {
        id_to_node[node->intermediate_node_id] = node;
        for (uint64_t index = 0u; index < node->depends_on.size; ++index)
        {
            kan_interned_string_t name = ((const kan_interned_string_t *) node->depends_on.data)[index];
            struct building_graph_node_t *found_node = graph_builder_find_node (builder_data, name);

            KAN_ASSERT (found_node)
            ++found_node->intermediate_references_count;

            add_to_id_array (&found_node->intermediate_outcomes, node->intermediate_node_id);
            add_to_id_array (&node->intermediate_incomes, found_node->intermediate_node_id);
        }

        for (uint64_t index = 0u; index < node->dependency_of.size; ++index)
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

            for (uint64_t outcome_index = 0u; outcome_index < node->intermediate_outcomes.size; ++outcome_index)
            {
                const uint64_t outcome_id = ((uint64_t *) node->intermediate_outcomes.data)[outcome_index];
                struct building_graph_node_t *outcome = id_to_node[outcome_id];
                remove_from_id_array (&outcome->intermediate_incomes, node->intermediate_node_id);
            }

            for (uint64_t income_index = 0u; income_index < node->intermediate_incomes.size; ++income_index)
            {
                const uint64_t income_id = ((uint64_t *) node->intermediate_incomes.data)[income_index];
                struct building_graph_node_t *income = id_to_node[income_id];
                remove_from_id_array (&income->intermediate_outcomes, node->intermediate_node_id);

                for (uint64_t outcome_index = 0u; outcome_index < node->intermediate_outcomes.size; ++outcome_index)
                {
                    const uint64_t outcome_id = ((uint64_t *) node->intermediate_outcomes.data)[outcome_index];
                    struct building_graph_node_t *outcome = id_to_node[outcome_id];
                    add_to_id_array (&outcome->intermediate_incomes, income_id);
                    add_to_id_array (&income->intermediate_outcomes, outcome_id);
                }
            }

            kan_hash_storage_remove (&builder_data->nodes, &node->node);
            building_graph_node_destroy (node, KAN_TRUE);
        }

        node = next;
    }

    if (builder_data->nodes.items.size == 0u)
    {
        KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR,
                 "Caught attempt to finalize graph with only checkpoints and no functional nodes.")
        kan_free_general (builder_data->builder_group, id_to_node, sizeof (void *) * next_id_to_assign);
        return KAN_INVALID_WORKFLOW_GRAPH;
    }

    kan_bool_t is_valid = KAN_TRUE;
    struct workflow_graph_header_t *result_graph = NULL;

#if defined(KAN_WORKFLOW_VERIFY)
    is_valid = graph_builder_verify_intermediate (builder_data, id_to_node);
#endif

    if (is_valid)
    {
        uint64_t start_nodes_count = 0u;
        uint64_t body_size = 0u;

        // Calculate data for workflow allocation.
        node = (struct building_graph_node_t *) builder_data->nodes.items.first;

        while (node)
        {
            if (node->intermediate_incomes.size == 0u)
            {
                ++start_nodes_count;
            }

            body_size = kan_apply_alignment (
                body_size + sizeof (struct workflow_graph_node_t) + sizeof (void *) * node->intermediate_outcomes.size,
                _Alignof (struct workflow_graph_node_t));
            node = (struct building_graph_node_t *) node->node.list_node.next;
        }

        if (start_nodes_count > 0u)
        {
            _Static_assert (_Alignof (struct workflow_graph_header_t) == _Alignof (struct workflow_graph_node_t),
                            "Workflow header and body have matching alignment.");

            const uint64_t header_size =
                kan_apply_alignment (sizeof (struct workflow_graph_header_t) + sizeof (void *) * start_nodes_count,
                                     sizeof (struct workflow_graph_header_t));
            const uint64_t graph_size = header_size + body_size;

            result_graph = (struct workflow_graph_header_t *) kan_allocate_general (
                builder_data->main_group, graph_size, _Alignof (struct workflow_graph_header_t));

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
                builder_data->builder_group, sizeof (void *) * next_id_to_assign, _Alignof (void *));

            // Fill basic data about built nodes and fill id to built nodes array. Assign start nodes.
            node = (struct building_graph_node_t *) builder_data->nodes.items.first;
            uint64_t next_start_node_index = 0u;
            uint8_t *nodes_base = ((uint8_t *) result_graph) + header_size;
            uint64_t node_offset = 0u;

            while (node)
            {
                struct workflow_graph_node_t *built_node = (struct workflow_graph_node_t *) (nodes_base + node_offset);
                id_to_built_node[node->intermediate_node_id] = built_node;

                if (node->intermediate_incomes.size == 0u)
                {
                    result_graph->start_nodes[next_start_node_index++] = built_node;
                }

                built_node->name = node->name;
                built_node->function = node->function;
                built_node->user_data = node->user_data;

                built_node->header = result_graph;
                built_node->incomes_count = node->intermediate_incomes.size;
                built_node->incomes_left = kan_atomic_int_init ((int) built_node->incomes_count);
                built_node->outcomes_count = node->intermediate_outcomes.size;

                node_offset = kan_apply_alignment (node_offset + sizeof (struct workflow_graph_node_t) +
                                                       sizeof (void *) * node->intermediate_outcomes.size,
                                                   _Alignof (struct workflow_graph_node_t));
                node = (struct building_graph_node_t *) node->node.list_node.next;
            }

            // Fill outcomes in built nodes.
            node = (struct building_graph_node_t *) builder_data->nodes.items.first;
            node_offset = 0u;

            while (node)
            {
                struct workflow_graph_node_t *built_node = (struct workflow_graph_node_t *) (nodes_base + node_offset);
                for (uint64_t index = 0u; index < node->intermediate_outcomes.size; ++index)
                {
                    built_node->outcomes[index] =
                        id_to_built_node[((uint64_t *) node->intermediate_outcomes.data)[index]];
                }

                node_offset = kan_apply_alignment (node_offset + sizeof (struct workflow_graph_node_t) +
                                                       sizeof (void *) * node->intermediate_outcomes.size,
                                                   _Alignof (struct workflow_graph_node_t));
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
    shutdown_nodes (builder_data, KAN_TRUE, KAN_TRUE);
    kan_hash_storage_init (&builder_data->nodes, builder_data->builder_group, KAN_WORKFLOW_GRAPH_NODES_INITIAL_BUCKETS);
    return result_graph ? (kan_workflow_graph_t) result_graph : KAN_INVALID_WORKFLOW_GRAPH;
}

void kan_workflow_graph_builder_destroy (kan_workflow_graph_builder_t builder)
{
    struct graph_builder_t *builder_data = (struct graph_builder_t *) builder;
    shutdown_nodes (builder_data, KAN_FALSE, KAN_FALSE);
    kan_free_general (builder_data->builder_group, builder_data, sizeof (struct graph_builder_t));
}

kan_workflow_graph_node_t kan_workflow_graph_node_create (kan_workflow_graph_builder_t builder, const char *name)
{
    struct graph_builder_t *builder_data = (struct graph_builder_t *) builder;
    kan_interned_string_t interned_name = kan_string_intern (name);
    struct building_graph_node_t *node = graph_builder_create_node (builder_data, interned_name);
    return (kan_workflow_graph_node_t) node;
}

void kan_workflow_graph_node_set_function (kan_workflow_graph_node_t node,
                                           kan_workflow_function_t function,
                                           kan_workflow_user_data_t user_data)
{
    struct building_graph_node_t *node_data = (struct building_graph_node_t *) node;
    node_data->function = function;
    node_data->user_data = user_data;
}

void kan_workflow_graph_node_insert_resource (kan_workflow_graph_node_t node, const char *resource_name)
{
    struct building_graph_node_t *node_data = (struct building_graph_node_t *) node;
    add_to_interned_string_array (&node_data->resource_insert_access, kan_string_intern (resource_name));
}

void kan_workflow_graph_node_write_resource (kan_workflow_graph_node_t node, const char *resource_name)
{
    struct building_graph_node_t *node_data = (struct building_graph_node_t *) node;
    add_to_interned_string_array (&node_data->resource_write_access, kan_string_intern (resource_name));
}

void kan_workflow_graph_node_read_resource (kan_workflow_graph_node_t node, const char *resource_name)
{
    struct building_graph_node_t *node_data = (struct building_graph_node_t *) node;
    add_to_interned_string_array (&node_data->resource_read_access, kan_string_intern (resource_name));
}

void kan_workflow_graph_node_depend_on (kan_workflow_graph_node_t node, const char *name)
{
    struct building_graph_node_t *node_data = (struct building_graph_node_t *) node;
    add_to_interned_string_array (&node_data->depends_on, kan_string_intern (name));
}

void kan_workflow_graph_node_make_dependency_of (kan_workflow_graph_node_t node, const char *name)
{
    struct building_graph_node_t *node_data = (struct building_graph_node_t *) node;
    add_to_interned_string_array (&node_data->dependency_of, kan_string_intern (name));
}

kan_bool_t kan_workflow_graph_node_submit (kan_workflow_graph_node_t node)
{
    struct building_graph_node_t *node_data = (struct building_graph_node_t *) node;
    if (building_graph_node_is_checkpoint (node_data))
    {
        KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR,
                 "Failed to submit workflow node \"%s\" as it has no function and therefore simulates checkpoint.",
                 node_data->name)
        return KAN_FALSE;
    }

    if (graph_builder_find_node (node_data->builder, node_data->name))
    {
        KAN_LOG (workflow_graph_builder, KAN_LOG_ERROR,
                 "Failed to submit workflow node \"%s\" as there is already node with the same name.", node_data->name)
        return KAN_FALSE;
    }

    graph_builder_submit_node (node_data->builder, node_data);
    return KAN_TRUE;
}

void kan_workflow_graph_node_destroy (kan_workflow_graph_node_t node)
{
    building_graph_node_destroy ((struct building_graph_node_t *) node, KAN_FALSE);
}

static void workflow_task_finish_function (uint64_t user_data);

static void workflow_task_execute_function (uint64_t user_data);

static void workflow_task_start_function (uint64_t user_data)
{
    struct workflow_graph_node_t *node = (struct workflow_graph_node_t *) user_data;
    node->job = kan_cpu_job_create ();
    kan_cpu_job_set_completion_task (node->job,
                                     (struct kan_cpu_task_t) {
                                         .name = node->name,
                                         .function = workflow_task_finish_function,
                                         .user_data = user_data,
                                     },
                                     KAN_CPU_DISPATCH_QUEUE_FOREGROUND);

    kan_cpu_task_handle_t task_handle = kan_cpu_job_dispatch_task (node->job,
                               (struct kan_cpu_task_t) {
                                   .name = node->name,
                                   .function = workflow_task_execute_function,
                                   .user_data = user_data,
                               },
                               KAN_CPU_DISPATCH_QUEUE_FOREGROUND);

    if (task_handle != KAN_INVALID_CPU_TASK_HANDLE)
    {
        kan_cpu_task_detach (task_handle);
    }
}

static void workflow_task_execute_function (uint64_t user_data)
{
    struct workflow_graph_node_t *node = (struct workflow_graph_node_t *) user_data;
    node->function (node->job, node->user_data);
}

static void workflow_task_finish_function (uint64_t user_data)
{
    struct workflow_graph_node_t *node = (struct workflow_graph_node_t *) user_data;
    kan_cpu_job_detach (node->job);
    node->incomes_left = kan_atomic_int_init ((int) node->incomes_count);
    struct kan_cpu_task_list_node_t *first_list_node = NULL;

    for (uint64_t outcome_index = 0u; outcome_index < node->outcomes_count; ++outcome_index)
    {
        struct workflow_graph_node_t *outcome = node->outcomes[outcome_index];
        if (kan_atomic_int_add (&outcome->incomes_left, -1) == 1)
        {
            kan_atomic_int_lock (&node->header->temporary_allocator_lock);
            struct kan_cpu_task_list_node_t *list_node = KAN_STACK_GROUP_ALLOCATOR_ALLOCATE_TYPED (
                &node->header->temporary_allocator, struct kan_cpu_task_list_node_t);
            kan_atomic_int_unlock (&node->header->temporary_allocator_lock);

            list_node->task = (struct kan_cpu_task_t) {
                .name = outcome->name,
                .function = workflow_task_start_function,
                .user_data = (uint64_t) outcome,
            };

            list_node->queue = KAN_CPU_DISPATCH_QUEUE_FOREGROUND;
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
    const kan_bool_t signal = node->header->nodes_left_to_execute == 0u;
    kan_mutex_unlock (node->header->nodes_left_to_execute_mutex);

    if (signal)
    {
        kan_conditional_variable_signal_one (node->header->nodes_left_to_execute_signal);
    }
}

void kan_workflow_graph_execute (kan_workflow_graph_t graph)
{
    struct workflow_graph_header_t *graph_header = (struct workflow_graph_header_t *) graph;
    graph_header->nodes_left_to_execute = graph_header->total_nodes_count;
    KAN_ASSERT (graph_header->start_nodes_count > 0u)
    struct kan_cpu_task_list_node_t *first_list_node = NULL;

    for (uint64_t start_index = 0u; start_index < graph_header->start_nodes_count; ++start_index)
    {
        struct workflow_graph_node_t *start = graph_header->start_nodes[start_index];
        KAN_CPU_TASK_LIST_USER_VALUE (&first_list_node, &graph_header->temporary_allocator, start->name,
                                      workflow_task_start_function, FOREGROUND, start)
    }

    kan_cpu_task_dispatch_list (first_list_node);
    while (first_list_node)
    {
        kan_cpu_task_detach (first_list_node->dispatch_handle);
        first_list_node = first_list_node->next;
    }

    kan_mutex_lock (graph_header->nodes_left_to_execute_mutex);
    while (KAN_TRUE)
    {
        const kan_bool_t signaled = graph_header->nodes_left_to_execute == 0u;
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
    struct workflow_graph_header_t *graph_header = (struct workflow_graph_header_t *) graph;
    kan_stack_group_allocator_shutdown (&graph_header->temporary_allocator);
    kan_mutex_destroy (graph_header->nodes_left_to_execute_mutex);
    kan_conditional_variable_destroy (graph_header->nodes_left_to_execute_signal);
    kan_free_general (graph_header->allocation_group, graph_header, graph_header->allocation_size);
}
