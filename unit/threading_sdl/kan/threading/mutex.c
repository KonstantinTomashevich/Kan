#include <SDL_mutex.h>

#include <kan/log/logging.h>
#include <kan/threading/mutex.h>

kan_mutex_handle_t kan_mutex_create (void)
{
    void *sdl_handle = SDL_CreateMutex ();
    if (!sdl_handle)
    {
        KAN_LOG (threading, KAN_LOG_ERROR, "Failed to create mutex: %s.", SDL_GetError ())
        return KAN_INVALID_MUTEX_HANDLE;
    }

    return (kan_mutex_handle_t) sdl_handle;
}

kan_bool_t kan_mutex_lock (kan_mutex_handle_t handle)
{
    SDL_LockMutex ((void *) handle);
    return KAN_TRUE;
}

kan_bool_t kan_mutex_try_lock (kan_mutex_handle_t handle)
{
    return SDL_TryLockMutex ((void *) handle) == 0 ? KAN_TRUE : KAN_FALSE;
}

kan_bool_t kan_mutex_unlock (kan_mutex_handle_t handle)
{
    SDL_UnlockMutex ((void *) handle);
    return KAN_TRUE;
}

void kan_mutex_destroy (kan_mutex_handle_t handle)
{
    SDL_DestroyMutex ((void *) handle);
}
