#include <SDL3/SDL_thread.h>

#include <kan/error/critical.h>
#include <kan/log/logging.h>
#include <kan/threading/atomic.h>
#include <kan/threading/thread.h>

kan_thread_t kan_thread_create (const char *name, kan_thread_function_t function, kan_thread_user_data_t data)
{
    void *handle = SDL_CreateThread (function, name, data);
    if (!handle)
    {
        KAN_LOG (threading, KAN_LOG_ERROR, "Failed to create thread: %s.", SDL_GetError ())
        return KAN_HANDLE_SET_INVALID (kan_thread_t);
    }

    return KAN_HANDLE_SET (kan_thread_t, handle);
}

kan_thread_result_t kan_thread_wait (kan_thread_t handle)
{
    void *sdl_handle = KAN_HANDLE_GET (handle);
    int sdl_result;
    SDL_WaitThread (sdl_handle, &sdl_result);
    return (kan_thread_result_t) sdl_result;
}

void kan_thread_detach (kan_thread_t handle)
{
    void *sdl_handle = KAN_HANDLE_GET (handle);
    SDL_DetachThread (sdl_handle);
}

const char *kan_thread_get_name (kan_thread_t handle)
{
    void *sdl_handle = KAN_HANDLE_GET (handle);
    return SDL_GetThreadName (sdl_handle);
}

const char *kan_current_thread_set_priority (enum kan_thread_priority_t priority)
{
    switch (priority)
    {
    case KAN_THREAD_PRIORITY_LOW:
        SDL_SetCurrentThreadPriority (SDL_THREAD_PRIORITY_LOW);
        break;

    case KAN_THREAD_PRIORITY_NORMAL:
        SDL_SetCurrentThreadPriority (SDL_THREAD_PRIORITY_NORMAL);
        break;

    case KAN_THREAD_PRIORITY_HIGH:
        SDL_SetCurrentThreadPriority (SDL_THREAD_PRIORITY_HIGH);
        break;

    case KAN_THREAD_PRIORITY_TIME_CRITICAL:
        SDL_SetCurrentThreadPriority (SDL_THREAD_PRIORITY_TIME_CRITICAL);
        break;
    }

    KAN_ASSERT (false)
    return NULL;
}

_Static_assert (sizeof (kan_thread_local_storage_t) >= sizeof (SDL_TLSID),
                "kan_thread_local_storage_t is able to hold SDL TLS id.");

THREADING_API void kan_thread_local_storage_set (kan_thread_local_storage_t *storage,
                                                 void *value,
                                                 kan_thread_local_storage_destructor_t destructor)
{
    if (!SDL_SetTLS ((SDL_TLSID *) storage, value, destructor))
    {
        KAN_LOG (threading, KAN_LOG_ERROR, "Failed to set TLS: %s.", SDL_GetError ())
    }
}

THREADING_API void *kan_thread_local_storage_get (kan_thread_local_storage_t *storage)
{
    return SDL_GetTLS ((SDL_TLSID *) storage);
}
