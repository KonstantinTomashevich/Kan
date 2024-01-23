#include <kan/context/reflection_system.h>
#include <kan/context/universe_system.h>

struct universe_system_t
{
    kan_context_handle_t context;
    kan_allocation_group_t group;
    kan_universe_t universe;
};

kan_context_system_handle_t universe_system_create (kan_allocation_group_t group, void *user_config)
{
    struct universe_system_t *system =
        kan_allocate_general (group, sizeof (struct universe_system_t), _Alignof (struct universe_system_t));
    system->group = group;
    system->universe = KAN_INVALID_UNIVERSE;
    return (kan_context_system_handle_t) system;
}

static void on_reflection_generated (kan_context_system_handle_t other_system,
                                     kan_reflection_registry_t registry,
                                     kan_reflection_migration_seed_t migration_seed,
                                     kan_reflection_struct_migrator_t migrator)
{
    struct universe_system_t *system = (struct universe_system_t *) other_system;
    if (system->universe == KAN_INVALID_UNIVERSE)
    {
        system->universe = kan_universe_create (system->group, registry, system->context);
    }
    else
    {
        kan_universe_migrate (system->universe, registry, migration_seed, migrator);
    }
}

void universe_system_connect (kan_context_system_handle_t handle, kan_context_handle_t context)
{
    struct universe_system_t *system = (struct universe_system_t *) handle;
    system->context = context;

    kan_context_system_handle_t reflection_system = kan_context_query (context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);
    if (reflection_system != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
    {
        kan_reflection_system_connect_on_generated (reflection_system, handle, on_reflection_generated);
    }
}

void universe_system_init (kan_context_system_handle_t handle)
{
}

void universe_system_shutdown (kan_context_system_handle_t handle)
{
    struct universe_system_t *system = (struct universe_system_t *) handle;
    if (system->universe != KAN_INVALID_UNIVERSE)
    {
        kan_universe_destroy (system->universe);
        system->universe = KAN_INVALID_UNIVERSE;
    }
}

void universe_system_disconnect (kan_context_system_handle_t handle)
{
    struct universe_system_t *system = (struct universe_system_t *) handle;
    kan_context_system_handle_t reflection_system =
        kan_context_query (system->context, KAN_CONTEXT_REFLECTION_SYSTEM_NAME);

    if (reflection_system != KAN_INVALID_CONTEXT_SYSTEM_HANDLE)
    {
        kan_reflection_system_disconnect_on_generated (reflection_system, handle);
    }
}

void universe_system_destroy (kan_context_system_handle_t handle)
{
    struct universe_system_t *system = (struct universe_system_t *) handle;
    kan_free_general (system->group, system, sizeof (struct universe_system_t));
}

struct kan_context_system_api_t KAN_CONTEXT_SYSTEM_API_NAME (universe_system_t) = {
    .name = KAN_CONTEXT_UNIVERSE_SYSTEM_NAME,
    .create = universe_system_create,
    .connect = universe_system_connect,
    .connected_init = universe_system_init,
    .connected_shutdown = universe_system_shutdown,
    .disconnect = universe_system_disconnect,
    .destroy = universe_system_destroy,
};

kan_universe_t kan_universe_system_get_universe (kan_context_system_handle_t universe_system)
{
    struct universe_system_t *system = (struct universe_system_t *) universe_system;
    return system->universe;
}
