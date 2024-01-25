#include <stddef.h>
#include <string.h>

#include <kan/container/dynamic_array.h>
#include <kan/container/hash_storage.h>
#include <kan/container/interned_string.h>
#include <kan/context/context.h>
#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/memory/allocation.h>

KAN_LOG_DEFINE_CATEGORY (context);

enum context_state_t
{
    CONTEXT_STATE_COLLECTING_REQUESTS = 0u,
    CONTEXT_STATE_CREATION,
    CONTEXT_STATE_CONNECTION,
    CONTEXT_STATE_CONNECTED_INITIALIZATION,
    CONTEXT_STATE_READY,
    CONTEXT_STATE_CONNECTED_SHUTDOWN,
    CONTEXT_STATE_DISCONNECTION,
    CONTEXT_STATE_DESTRUCTION,
};

struct system_instance_node_t
{
    struct kan_hash_storage_node_t node;
    kan_interned_string_t name;
    kan_context_system_handle_t instance;
    struct kan_context_system_api_t *api;
    void *user_config;
    kan_bool_t initialized;
    uint64_t connection_references_to_others;
    uint64_t initialization_references_to_me;

    /// \meta reflection_dynamic_array_type = "struct system_instance_node_t *"
    struct kan_dynamic_array_t initialization_references_to_others;

    /// \meta reflection_dynamic_array_type = "struct system_instance_node_t *"
    struct kan_dynamic_array_t connection_references_to_me;
};

struct operation_stack_item_t
{
    struct operation_stack_item_t *next;
    struct system_instance_node_t *system;
};

struct context_t
{
    enum context_state_t state;
    struct kan_hash_storage_t systems;
    struct operation_stack_item_t *operation_stack_top;
    kan_allocation_group_t group;
};

static inline kan_bool_t node_array_contains (struct kan_dynamic_array_t *array, struct system_instance_node_t *node)
{
    for (uint64_t index = 0u; index < array->size; ++index)
    {
        if (((struct system_instance_node_t **) array->data)[index] == node)
        {
            return KAN_TRUE;
        }
    }

    return KAN_FALSE;
}

static inline void node_array_add (struct kan_dynamic_array_t *array, struct system_instance_node_t *node)
{
    void *spot = kan_dynamic_array_add_last (array);
    if (!spot)
    {
        kan_dynamic_array_set_capacity (array, array->capacity * 2u);
        spot = kan_dynamic_array_add_last (array);
    }

    *((struct system_instance_node_t **) spot) = node;
}

static struct system_instance_node_t *context_query_system (struct context_t *context,
                                                            kan_interned_string_t system_name)
{
    const struct kan_hash_storage_bucket_t *bucket = kan_hash_storage_query (&context->systems, (uint64_t) system_name);
    struct system_instance_node_t *node = (struct system_instance_node_t *) bucket->first;
    const struct system_instance_node_t *node_end =
        (struct system_instance_node_t *) (bucket->last ? bucket->last->next : NULL);

    while (node != node_end)
    {
        if (node->name == system_name)
        {
            return node;
        }

        node = (struct system_instance_node_t *) node->node.list_node.next;
    }

    return NULL;
}

static inline void context_push_operation (struct context_t *context, struct system_instance_node_t *system)
{
    struct operation_stack_item_t *item = kan_allocate_batched (context->group, sizeof (struct operation_stack_item_t));
    item->system = system;
    item->next = context->operation_stack_top;
    context->operation_stack_top = item;
}

static inline void context_pop_operation (struct context_t *context)
{
    KAN_ASSERT (context->operation_stack_top)
    struct operation_stack_item_t *next = context->operation_stack_top->next;
    kan_free_batched (context->group, context->operation_stack_top);
    context->operation_stack_top = next;
}

static void context_initialize_system (struct context_t *context, struct system_instance_node_t *node)
{
    if (node->instance == KAN_INVALID_CONTEXT_SYSTEM_HANDLE || node->initialized)
    {
        return;
    }

    for (uint64_t connected_index = 0u; connected_index < node->connection_references_to_me.size; ++connected_index)
    {
        context_initialize_system (
            context, ((struct system_instance_node_t **) node->connection_references_to_me.data)[connected_index]);
    }

    KAN_LOG (context, KAN_LOG_INFO, "Begin system \"%s\" initialization.", node->name)
    context_push_operation (context, node);
    node->api->connected_init (node->instance);
    context_pop_operation (context);
    node->initialized = KAN_TRUE;
    KAN_LOG (context, KAN_LOG_INFO, "End system \"%s\" initialization.", node->name)
}

static void context_shutdown_system (struct context_t *context, struct system_instance_node_t *node)
{
    if (node->instance == KAN_INVALID_CONTEXT_SYSTEM_HANDLE || !node->initialized ||
        node->initialization_references_to_me > 0u || node->connection_references_to_others > 0u)
    {
        return;
    }

    KAN_LOG (context, KAN_LOG_INFO, "Begin system \"%s\" shutdown.", node->name)
    context_push_operation (context, node);
    node->api->connected_shutdown (node->instance);
    context_pop_operation (context);
    node->initialized = KAN_FALSE;
    KAN_LOG (context, KAN_LOG_INFO, "End system \"%s\" shutdown.", node->name)

    for (uint64_t initialized_index = 0u; initialized_index < node->initialization_references_to_others.size;
         ++initialized_index)
    {
        struct system_instance_node_t *other_node =
            ((struct system_instance_node_t **) node->initialization_references_to_others.data)[initialized_index];
        --other_node->initialization_references_to_me;
        context_shutdown_system (context, other_node);
    }

    for (uint64_t connected_index = 0u; connected_index < node->connection_references_to_me.size; ++connected_index)
    {
        struct system_instance_node_t *other_node =
            ((struct system_instance_node_t **) node->connection_references_to_me.data)[connected_index];
        --other_node->connection_references_to_others;
        context_shutdown_system (context, other_node);
    }
}

#if defined(_WIN32)
__declspec (dllimport) extern uint64_t KAN_CONTEXT_SYSTEM_COUNT_NAME;
__declspec (dllimport) extern struct kan_context_system_api_t *KAN_CONTEXT_SYSTEM_ARRAY_NAME[];
__declspec (dllimport) void KAN_CONTEXT_SYSTEM_ARRAY_INITIALIZER_NAME (void);
#else
extern uint64_t KAN_CONTEXT_SYSTEM_COUNT_NAME;
extern struct kan_context_system_api_t *KAN_CONTEXT_SYSTEM_ARRAY_NAME[];
void KAN_CONTEXT_SYSTEM_ARRAY_INITIALIZER_NAME (void);
#endif

kan_context_handle_t kan_context_create (kan_allocation_group_t group)
{
    KAN_CONTEXT_SYSTEM_ARRAY_INITIALIZER_NAME ();
    struct context_t *context = kan_allocate_general (group, sizeof (struct context_t), _Alignof (struct context_t));
    context->state = CONTEXT_STATE_COLLECTING_REQUESTS;
    kan_hash_storage_init (&context->systems, group, KAN_CONTEXT_SYSTEM_INITIAL_BUCKETS);
    context->operation_stack_top = NULL;
    context->group = group;
    return (kan_context_handle_t) context;
}

kan_bool_t kan_context_request_system (kan_context_handle_t handle, const char *system_name, void *user_config)
{
    struct context_t *context = (struct context_t *) handle;
    KAN_ASSERT (context->state == CONTEXT_STATE_COLLECTING_REQUESTS)
    const kan_interned_string_t interned_system_name = kan_string_intern (system_name);

    if (context_query_system (context, interned_system_name))
    {
        KAN_LOG (context, KAN_LOG_ERROR, "Caught duplicate request for system \"%s\"", system_name)
        return KAN_FALSE;
    }

    struct kan_context_system_api_t *api = NULL;
    for (uint64_t api_index = 0u; api_index < KAN_CONTEXT_SYSTEM_COUNT_NAME; ++api_index)
    {
        if (strcmp (KAN_CONTEXT_SYSTEM_ARRAY_NAME[api_index]->name, system_name) == 0)
        {
            api = KAN_CONTEXT_SYSTEM_ARRAY_NAME[api_index];
            break;
        }
    }

    if (!api)
    {
        KAN_LOG (context, KAN_LOG_ERROR, "Unable to find API for system \"%s\"", system_name)
        return KAN_FALSE;
    }

    struct system_instance_node_t *node = kan_allocate_batched (context->group, sizeof (struct system_instance_node_t));
    node->node.hash = (uint64_t) interned_system_name;
    node->name = interned_system_name;
    node->instance = KAN_INVALID_CONTEXT_SYSTEM_HANDLE;
    node->api = api;
    node->user_config = user_config;
    node->initialized = KAN_FALSE;
    node->connection_references_to_others = 0u;
    node->initialization_references_to_me = 0u;

    kan_dynamic_array_init (&node->initialization_references_to_others, KAN_CONTEXT_SYSTEM_CONNECTIONS_INITIAL_COUNT,
                            sizeof (void *), _Alignof (void *), context->group);

    kan_dynamic_array_init (&node->connection_references_to_me, KAN_CONTEXT_SYSTEM_CONNECTIONS_INITIAL_COUNT,
                            sizeof (void *), _Alignof (void *), context->group);

    if (context->systems.bucket_count * KAN_CONTEXT_SYSTEM_LOAD_FACTOR <= context->systems.items.size)
    {
        kan_hash_storage_set_bucket_count (&context->systems, context->systems.bucket_count * 2u);
    }

    kan_hash_storage_add (&context->systems, &node->node);
    return KAN_TRUE;
}

void kan_context_assembly (kan_context_handle_t handle)
{
    struct context_t *context = (struct context_t *) handle;
    context->state = CONTEXT_STATE_CREATION;

    struct system_instance_node_t *node = (struct system_instance_node_t *) context->systems.items.first;
    while (node)
    {
        node->instance =
            node->api->create (kan_allocation_group_get_child (context->group, node->name), node->user_config);

        if (node->instance == KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
        {
            KAN_LOG (context, KAN_LOG_ERROR, "Failed to create instance of system \"%s\"", node->name)
        }

        node = (struct system_instance_node_t *) node->node.list_node.next;
    }

    context->state = CONTEXT_STATE_CONNECTION;
    node = (struct system_instance_node_t *) context->systems.items.first;

    while (node)
    {
        if (node->instance != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
        {
            context_push_operation (context, node);
            node->api->connect (node->instance, handle);
            context_pop_operation (context);
        }

        node = (struct system_instance_node_t *) node->node.list_node.next;
    }

    KAN_ASSERT (!context->operation_stack_top)
    context->state = CONTEXT_STATE_CONNECTED_INITIALIZATION;
    node = (struct system_instance_node_t *) context->systems.items.first;

    while (node)
    {
        context_initialize_system (context, node);
        node = (struct system_instance_node_t *) node->node.list_node.next;
    }

    KAN_ASSERT (!context->operation_stack_top)
    context->state = CONTEXT_STATE_READY;
}

kan_context_system_handle_t kan_context_query (kan_context_handle_t handle, const char *system_name)
{
    struct context_t *context = (struct context_t *) handle;
    const kan_interned_string_t interned_system_name = kan_string_intern (system_name);
    struct system_instance_node_t *node = context_query_system (context, interned_system_name);

    if (!node)
    {
        return KAN_INVALID_CONTEXT_SYSTEM_HANDLE;
    }

    switch (context->state)
    {
    case CONTEXT_STATE_CONNECTION:
        if (context->operation_stack_top)
        {
            struct system_instance_node_t *top = context->operation_stack_top->system;
            if (!node_array_contains (&node->connection_references_to_me, top))
            {
                node_array_add (&node->connection_references_to_me, top);
                ++top->connection_references_to_others;
            }
        }

        KAN_ASSERT (!node->initialized)
        return node->instance;

    case CONTEXT_STATE_CONNECTED_INITIALIZATION:
        if (context->operation_stack_top)
        {
            struct system_instance_node_t *top = context->operation_stack_top->system;
            if (!node_array_contains (&top->initialization_references_to_others, node))
            {
                node_array_add (&top->initialization_references_to_others, node);
                ++node->initialization_references_to_me;
            }
        }

        context_initialize_system (context, node);
        return node->instance;

    case CONTEXT_STATE_READY:
        KAN_ASSERT (node->initialized)
        return node->instance;

    case CONTEXT_STATE_CONNECTED_SHUTDOWN:
#if defined(KAN_WITH_ASSERT)
        if (context->operation_stack_top)
        {
            struct system_instance_node_t *top = context->operation_stack_top->system;
            KAN_ASSERT (node_array_contains (&top->initialization_references_to_others, node))
        }
#endif

        KAN_ASSERT (node->initialized)
        return node->instance;

    case CONTEXT_STATE_DISCONNECTION:
        KAN_ASSERT (!node->initialized)
        return node->instance;

    case CONTEXT_STATE_COLLECTING_REQUESTS:
    case CONTEXT_STATE_CREATION:
    case CONTEXT_STATE_DESTRUCTION:
        // States that do not support querying.
        KAN_ASSERT (KAN_FALSE)
        return KAN_INVALID_CONTEXT_SYSTEM_HANDLE;
    }

    KAN_ASSERT (KAN_FALSE)
    return KAN_INVALID_CONTEXT_SYSTEM_HANDLE;
}

void kan_context_destroy (kan_context_handle_t handle)
{
    struct context_t *context = (struct context_t *) handle;
    KAN_ASSERT (context->state == CONTEXT_STATE_READY)

    context->state = CONTEXT_STATE_CONNECTED_SHUTDOWN;
    struct system_instance_node_t *node = (struct system_instance_node_t *) context->systems.items.first;

    while (node)
    {
        context_shutdown_system (context, node);
        node = (struct system_instance_node_t *) node->node.list_node.next;
    }

    context->state = CONTEXT_STATE_DISCONNECTION;
    node = (struct system_instance_node_t *) context->systems.items.first;

    while (node)
    {
        if (node->instance != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
        {
            KAN_ASSERT (!node->initialized)
            node->api->disconnect (node->instance);
        }

        node = (struct system_instance_node_t *) node->node.list_node.next;
    }

    context->state = CONTEXT_STATE_DESTRUCTION;
    node = (struct system_instance_node_t *) context->systems.items.first;

    while (node)
    {
        struct system_instance_node_t *next = (struct system_instance_node_t *) node->node.list_node.next;
        if (node->instance != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
        {
            node->api->destroy (node->instance);
        }

        kan_dynamic_array_shutdown (&node->connection_references_to_me);
        kan_dynamic_array_shutdown (&node->initialization_references_to_others);
        kan_free_batched (context->group, node);
        node = next;
    }

    KAN_ASSERT (!context->operation_stack_top)
    kan_hash_storage_shutdown (&context->systems);
    kan_free_general (context->group, context, sizeof (struct context_t));
}
