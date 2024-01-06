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
    kan_bool_t initialized;
    uint64_t connected_references_to_me;

    // Array of interned strings.
    struct kan_dynamic_array_t connected_references_to_others;
};

struct connected_operation_stack_item_t
{
    struct connected_operation_stack_item_t *next;
    struct system_instance_node_t *system;
};

struct context_t
{
    enum context_state_t state;
    struct kan_hash_storage_t systems;
    struct connected_operation_stack_item_t *connected_operation_stack_top;
    kan_allocation_group_t group;
};

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

static void context_push_connected_operation (struct context_t *context, struct system_instance_node_t *system)
{
    struct connected_operation_stack_item_t *item =
        kan_allocate_batched (context->group, sizeof (struct connected_operation_stack_item_t));
    item->system = system;
    item->next = context->connected_operation_stack_top;
    context->connected_operation_stack_top = item;
}

static void context_pop_connected_operation (struct context_t *context)
{
    KAN_ASSERT (context->connected_operation_stack_top)
    struct connected_operation_stack_item_t *next = context->connected_operation_stack_top->next;
    kan_free_batched (context->group, context->connected_operation_stack_top);
    context->connected_operation_stack_top = next;
}

static inline void context_initialize_system (struct context_t *context, struct system_instance_node_t *node)
{
    KAN_LOG (context, KAN_LOG_ERROR, "Begin system \"%s\" initialization.", node->name)
    context_push_connected_operation (context, node);
    node->api->connected_init (node->instance);
    context_pop_connected_operation (context);
    node->initialized = KAN_TRUE;
    KAN_LOG (context, KAN_LOG_ERROR, "End system \"%s\" initialization.", node->name)
}

#if defined(_WIN32)
__declspec (dllimport) extern uint64_t KAN_CONTEXT_SYSTEM_COUNT_NAME;
__declspec (dllimport) extern struct kan_context_system_api_t *KAN_CONTEXT_SYSTEM_ARRAY_NAME[];
#else
extern uint64_t KAN_CONTEXT_SYSTEM_COUNT_NAME;
extern struct kan_context_system_api_t *KAN_CONTEXT_SYSTEM_ARRAY_NAME[];
#endif

kan_context_handle_t kan_context_create (kan_allocation_group_t group)
{
    struct context_t *context = kan_allocate_general (group, sizeof (struct context_t), _Alignof (struct context_t));
    context->state = CONTEXT_STATE_COLLECTING_REQUESTS;
    kan_hash_storage_init (&context->systems, group, KAN_CONTEXT_SYSTEM_INITIAL_BUCKETS);
    context->connected_operation_stack_top = NULL;
    context->group = group;
    return (kan_context_handle_t) context;
}

kan_bool_t kan_context_request_system (kan_context_handle_t handle, const char *system_name)
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
    node->initialized = KAN_FALSE;
    node->connected_references_to_me = 0u;
    kan_dynamic_array_init (&node->connected_references_to_others, KAN_CONTEXT_SYSTEM_CONNECTIONS_INITIAL_COUNT,
                            sizeof (kan_interned_string_t), _Alignof (kan_interned_string_t), context->group);

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
        node->instance = node->api->create (kan_allocation_group_get_child (context->group, node->name));
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
            node->api->connect (node->instance, handle);
        }

        node = (struct system_instance_node_t *) node->node.list_node.next;
    }

    context->state = CONTEXT_STATE_CONNECTED_INITIALIZATION;
    node = (struct system_instance_node_t *) context->systems.items.first;

    while (node)
    {
        if (node->instance != KAN_INVALID_CONTEXT_SYSTEM_HANDLE && !node->initialized)
        {
            context_initialize_system (context, node);
        }

        node = (struct system_instance_node_t *) node->node.list_node.next;
    }

    KAN_ASSERT (!context->connected_operation_stack_top)
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
        KAN_ASSERT (!node->initialized)
        return node->instance;

    case CONTEXT_STATE_CONNECTED_INITIALIZATION:
        if (context->connected_operation_stack_top)
        {
            kan_bool_t found_in_references = KAN_FALSE;
            struct system_instance_node_t *top = context->connected_operation_stack_top->system;

            for (uint64_t index = 0u; index < top->connected_references_to_others.size; ++index)
            {
                if (((kan_interned_string_t *) top->connected_references_to_others.data)[index] == interned_system_name)
                {
                    found_in_references = KAN_TRUE;
                    break;
                }
            }

            if (!found_in_references)
            {
                void *spot = kan_dynamic_array_add_last (&top->connected_references_to_others);
                if (!spot)
                {
                    kan_dynamic_array_set_capacity (&top->connected_references_to_others,
                                                    top->connected_references_to_others.capacity * 2u);
                    spot = kan_dynamic_array_add_last (&top->connected_references_to_others);
                }

                *((kan_interned_string_t *) spot) = interned_system_name;
                ++node->connected_references_to_me;
            }
        }

        if (node->instance != KAN_INVALID_CONTEXT_SYSTEM_HANDLE && !node->initialized)
        {
            context_initialize_system (context, node);
        }

        return node->instance;

    case CONTEXT_STATE_READY:
        KAN_ASSERT (node->initialized)
        return node->instance;

    case CONTEXT_STATE_CONNECTED_SHUTDOWN:
#if defined(KAN_WITH_ASSERT)
        if (context->connected_operation_stack_top)
        {
            kan_bool_t found_in_references = KAN_FALSE;
            struct system_instance_node_t *top = context->connected_operation_stack_top->system;

            for (uint64_t index = 0u; index < top->connected_references_to_others.size; ++index)
            {
                if (((kan_interned_string_t *) top->connected_references_to_others.data)[index] == interned_system_name)
                {
                    found_in_references = KAN_TRUE;
                    break;
                }
            }

            KAN_ASSERT (found_in_references)
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
    uint64_t initialized_count = 0u;

    do
    {
        initialized_count = 0u;
#if defined(KAN_WITH_ASSERT)
        uint64_t freed_count = 0u;
#endif

        struct system_instance_node_t *node = (struct system_instance_node_t *) context->systems.items.first;
        while (node)
        {
            if (node->instance != KAN_INVALID_CONTEXT_SYSTEM_HANDLE && node->initialized)
            {
                if (node->connected_references_to_me == 0u)
                {
                    KAN_LOG (context, KAN_LOG_ERROR, "Begin system \"%s\" shutdown.", node->name)
                    context_push_connected_operation (context, node);
                    node->api->connected_shutdown (node->instance);
                    context_pop_connected_operation (context);
                    node->initialized = KAN_FALSE;
                    KAN_LOG (context, KAN_LOG_ERROR, "End system \"%s\" shutdown.", node->name)

                    for (uint64_t index = 0u; index < node->connected_references_to_others.size; ++index)
                    {
                        struct system_instance_node_t *other_node = context_query_system (
                            context, ((kan_interned_string_t *) node->connected_references_to_others.data)[index]);
                        --other_node->connected_references_to_me;

#if defined(KAN_WITH_ASSERT)
                        if (other_node->connected_references_to_me == 0u)
                        {
                            ++freed_count;
                        }
#endif
                    }
                }
                else
                {
                    ++initialized_count;
                }
            }

            node = (struct system_instance_node_t *) node->node.list_node.next;
        }

        KAN_ASSERT (freed_count > 0u || initialized_count == 0u)
    } while (initialized_count > 0u);

    context->state = CONTEXT_STATE_DISCONNECTION;
    struct system_instance_node_t *node = (struct system_instance_node_t *) context->systems.items.first;

    while (node)
    {
        if (node->instance != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
        {
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

        kan_dynamic_array_shutdown (&node->connected_references_to_others);
        kan_free_batched (context->group, node);
        node = next;
    }

    KAN_ASSERT (!context->connected_operation_stack_top)
    kan_hash_storage_shutdown (&context->systems);
    kan_free_general (context->group, context, sizeof (struct context_t));
}
