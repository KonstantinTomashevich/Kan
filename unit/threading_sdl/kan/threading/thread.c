#include <SDL_thread.h>

#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/threading/thread.h>

kan_thread_handle_t kan_thread_create (const char *name, kan_thread_function_t function, void *data)
{
    void *handle = SDL_CreateThread (function, name, data);
    if (!handle)
    {
        KAN_LOG (threading, KAN_LOG_ERROR, "Failed to create thread: %s.", SDL_GetError ())
        return KAN_INVALID_THREAD_HANDLE;
    }

    return (kan_thread_handle_t) handle;
}

kan_thread_result_t kan_thread_wait (kan_thread_handle_t handle)
{
    void *sdl_handle = (void *) handle;
    int sdl_result;
    SDL_WaitThread (sdl_handle, &sdl_result);
    return (kan_thread_result_t) sdl_result;
}

const char *kan_thread_get_name (kan_thread_handle_t handle)
{
    void *sdl_handle = (void *) handle;
    return SDL_GetThreadName (sdl_handle);
}

kan_thread_handle_t kan_current_thread ()
{
    return (kan_thread_handle_t) SDL_ThreadID ();
}

const char *kan_current_thread_set_priority (enum kan_thread_priority_t priority)
{
    switch (priority)
    {
    case KAN_THREAD_PRIORITY_LOW:
        SDL_SetThreadPriority (SDL_THREAD_PRIORITY_LOW);
        break;

    case KAN_THREAD_PRIORITY_NORMAL:
        SDL_SetThreadPriority (SDL_THREAD_PRIORITY_NORMAL);
        break;

    case KAN_THREAD_PRIORITY_HIGH:
        SDL_SetThreadPriority (SDL_THREAD_PRIORITY_HIGH);
        break;

    case KAN_THREAD_PRIORITY_TIME_CRITICAL:
        SDL_SetThreadPriority (SDL_THREAD_PRIORITY_TIME_CRITICAL);
        break;
    }

    KAN_ASSERT (KAN_FALSE)
    return NULL;
}

_Static_assert (sizeof (kan_thread_local_storage_t) >= sizeof (SDL_TLSID),
                "kan_thread_local_storage_t is able to hold SDL TLS id.");

THREADING_API kan_thread_local_storage_t kan_thread_local_storage_create ()
{
    SDL_TLSID tls = SDL_CreateTLS ();
    if (tls == 0u)
    {
        KAN_LOG (threading, KAN_LOG_ERROR, "Failed to create TLS: %s.", SDL_GetError ())
        return KAN_INVALID_THREAD_HANDLE;
    }

    KAN_ASSERT (tls != KAN_THREAD_LOCAL_STORAGE_INVALID)
    return (kan_thread_local_storage_t) tls;
}

THREADING_API void kan_thread_local_storage_set (kan_thread_local_storage_t storage,
                                                 void *value,
                                                 kan_thread_local_storage_destructor_t destructor)
{
    if (SDL_SetTLS ((SDL_TLSID) storage, value, destructor) != 0)
    {
        KAN_LOG (threading, KAN_LOG_ERROR, "Failed to set TLS: %s.", SDL_GetError ())
    }
}

THREADING_API void *kan_thread_local_storage_get (kan_thread_local_storage_t storage)
{
    return SDL_GetTLS ((SDL_TLSID) storage);
}
