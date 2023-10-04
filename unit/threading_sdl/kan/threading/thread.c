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
