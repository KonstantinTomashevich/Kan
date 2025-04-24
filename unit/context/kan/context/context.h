#pragma once

#include <context_api.h>

#include <kan/api_common/c_header.h>
#include <kan/api_common/core_types.h>
#include <kan/memory_profiler/allocation_group.h>

/// \file
/// \brief Provides API for context unit -- global execution context management library.
///
/// \par Definition
/// \parblock
/// Context standardizes lookup, initialization, query and shutdown routines for systems -- global runtime modules that
/// may depend on each other. For example, virtual file system, reflection provision system, update cycle system, etc.
/// \endparblock
///
/// \par System definition
/// \parblock
/// Basically, any singleton that supports context lifecycle (described below) can be a system. It needs to provide
/// appropriate functions through global `struct kan_context_system_api_t` instance and everything else will be handled
/// by context. The only requirement is that system name must be unique (prefer structure names) and API structure
/// must be named using `KAN_CONTEXT_SYSTEM_API_NAME`, for example:
///
/// ```c
/// TEST_CONTEXT_API struct kan_context_system_api_t KAN_CONTEXT_SYSTEM_API_NAME (example_system_t) = {
///     .name = "example_system_t",
///     .create = example_system_create,
///     .connect = example_system_connect,
///     .connected_init = example_system_init,
///     .connected_shutdown = example_system_shutdown,
///     .disconnect = example_system_disconnect,
///     .destroy = example_system_destroy,
/// };
/// ```
/// \endparblock
///
/// \par Lookup
/// \parblock
/// Context relies on build system to provide list of available systems through build-system generated configuration
/// files that define `KAN_CONTEXT_SYSTEM_COUNT_NAME` and `KAN_CONTEXT_SYSTEM_ARRAY_NAME` global variables.
/// See `context.cmake` for more information.
/// \endparblock
///
/// \par Lifecycle
/// \parblock
/// After context is created through `kan_context_create`, it is in request processing state -- it waits for requests
/// to bootstrap systems. System can be requested by name through `kan_context_request_system`. After all requests
/// were passed, context can be assembled using `kan_context_assembly`.
///
/// Assembly routine consists of several steps:
/// - Creation stage. All requested systems are created through their create functor.
/// - Connection stage. Systems are allowed to connect to each other, mostly to setup delegates and other logical
///   connections. In this stage systems are still considered uninitialized and should not call any heavy logic.
///   It is allowed to get other systems through `kan_context_query`. Accessing other systems from functor through
///   `kan_context_query` will create connection dependency: this system will be initialized (next stage) earlier than
///   every system to which it is connected. Also, this system won't be shut down until these systems will be shut down.
/// - Initialization stage. Systems run their initialization functors and do all heavy initialization logic. Accessing
///   other systems from functor through `kan_context_query` will create logical dependency: accessed system will be
///   initialized unless it is already initialized and it won't be uninitialized until this system is uninitialized.
///   Keep in mind that circular logical dependencies result in a deadlock.
///
/// After the assembly, context is in ready stage and can serve any system through `kan_context_query`.
///
/// When you no longer need context, it should be freed using `kan_context_destroy` which triggers shutdown routine:
///
/// - Shutdown stage. Systems run their shutdown functors and do all heavy shutdown logic. In this stage it is
///   guaranteed that systems that accessed other systems during initialization will shutdown before accessed systems.
/// - Disconnection stage. Systems disconnect their delegates from each other if needed.
/// - Destruction stage. All systems are destroyed.
/// \endparblock
///
/// \par Thread safety
/// \parblock
/// Generally, context is aimed to be single-threaded top level logical abstraction, therefore it is not thread-safe.
/// Parallelization is expected to be done inside systems. But there is one exception: `kan_context_query` is thread
/// safe when context is guaranteed to be in ready state as context structure is not modified by it in this case.
/// \endparblock

KAN_C_HEADER_BEGIN

#define KAN_CONTEXT_SYSTEM_COUNT_NAME kan_context_available_systems_count
#define KAN_CONTEXT_SYSTEM_ARRAY_NAME kan_context_available_systems
#define KAN_CONTEXT_SYSTEM_ARRAY_INITIALIZER_NAME kan_context_fill_available_systems
#define KAN_CONTEXT_SYSTEM_API_NAME(SYSTEM_NAME) kan_context_system_api_##SYSTEM_NAME

KAN_HANDLE_DEFINE (kan_context_t);
KAN_HANDLE_DEFINE (kan_context_system_t);

#define KAN_INVALID_CONTEXT_SYSTEM_HANDLE 0u

typedef kan_context_system_t (*kan_context_system_create_functor_t) (kan_allocation_group_t group, void *user_config);

typedef void (*kan_context_system_connect_functor_t) (kan_context_system_t handle, kan_context_t context);

typedef void (*kan_context_system_connected_init_functor_t) (kan_context_system_t handle);

typedef void (*kan_context_system_connected_shutdown_functor_t) (kan_context_system_t handle);

typedef void (*kan_context_system_disconnect_functor_t) (kan_context_system_t handle);

typedef void (*kan_context_system_destroy_functor_t) (kan_context_system_t handle);

/// \brief Structure that contains system API for integration into context.
struct kan_context_system_api_t
{
    const char *name;
    kan_context_system_create_functor_t create;
    kan_context_system_connect_functor_t connect;
    kan_context_system_connected_init_functor_t connected_init;
    kan_context_system_connected_shutdown_functor_t connected_shutdown;
    kan_context_system_disconnect_functor_t disconnect;
    kan_context_system_destroy_functor_t destroy;
};

/// \brief Creates new instance of context.
CONTEXT_API kan_context_t kan_context_create (kan_allocation_group_t group);

/// \brief Requests system with given name to be added to context.
/// \invariant Should be called before `kan_context_assembly`.
CONTEXT_API kan_bool_t kan_context_request_system (kan_context_t handle, const char *system_name, void *user_config);

/// \brief Check if system with given name is already requested in this context.
/// \invariant Should be called before `kan_context_assembly`.
CONTEXT_API kan_bool_t kan_context_is_requested (kan_context_t handle, const char *system_name);

/// \brief Assembles and initializes all the requested systems.
CONTEXT_API void kan_context_assembly (kan_context_t handle);

/// \brief Queries for system with given name. Should not be called before `kan_context_assembly`.
CONTEXT_API kan_context_system_t kan_context_query (kan_context_t handle, const char *system_name);

/// \brief Special version of `kan_context_query` for connection phase where we don't want to connect, but only to
///        check system existence or configuration or refer to it as dependency for some connection.
/// \details It is mostly used to avoid circular dependencies and stack overflow later during init, as requesting
///          system to which we can be connected in init causes stack overflow as both systems would want each other
///          to be initialized due to potential connection. If connection can never happen, we should use
///          this function and there will be no stack overflow due to absence of potential connection.
/// \invariant If system was received through this function, its connect functions should not be called!
CONTEXT_API kan_context_system_t kan_context_query_no_connect (kan_context_t handle, const char *system_name);

/// \brief Destroys given context along with all its systems.
CONTEXT_API void kan_context_destroy (kan_context_t handle);

KAN_C_HEADER_END
