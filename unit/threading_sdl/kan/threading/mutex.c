#include <SDL_mutex.h>

#include <kan/threading/mutex.h>
#include <kan/log/logging.h>

kan_mutex_handle_t kan_mutex_create ()
{
    void *sdl_handle = SDL_CreateMutex ();
    if (!sdl_handle)
    {
        KAN_LOG (threading, KAN_LOG_ERROR, "Failed to create mutex: %s.", SDL_GetError())
        return KAN_INVALID_MUTEX_HANDLE;
    }

    return (kan_mutex_handle_t) sdl_handle;
}

kan_bool_t kan_mutex_lock (kan_mutex_handle_t handle)
{
    return SDL_LockMutex ((void *) handle) == 0 ? KAN_TRUE : KAN_FALSE;
}

kan_bool_t kan_mutex_try_lock (kan_mutex_handle_t handle)
{
    return SDL_TryLockMutex ((void *) handle) == 0 ? KAN_TRUE : KAN_FALSE;
}

kan_bool_t kan_mutex_unlock (kan_mutex_handle_t handle)
{
    return SDL_UnlockMutex ((void *) handle) == 0 ? KAN_TRUE : KAN_FALSE;
}

void kan_mutex_destroy (kan_mutex_handle_t handle)
{
    SDL_DestroyMutex ((void *) handle);
}
