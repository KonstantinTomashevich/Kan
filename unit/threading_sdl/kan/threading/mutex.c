#include <SDL3/SDL_mutex.h>

#include <kan/log/logging.h>
#include <kan/threading/mutex.h>

kan_mutex_t kan_mutex_create (void)
{
    void *sdl_handle = SDL_CreateMutex ();
    if (!sdl_handle)
    {
        KAN_LOG (threading, KAN_LOG_ERROR, "Failed to create mutex: %s.", SDL_GetError ())
        return KAN_HANDLE_SET_INVALID (kan_mutex_t);
    }

    return KAN_HANDLE_SET (kan_mutex_t, sdl_handle);
}

kan_bool_t kan_mutex_lock (kan_mutex_t handle)
{
    SDL_LockMutex (KAN_HANDLE_GET (handle));
    return KAN_TRUE;
}

kan_bool_t kan_mutex_try_lock (kan_mutex_t handle)
{
    return SDL_TryLockMutex (KAN_HANDLE_GET (handle)) == 0;
}

kan_bool_t kan_mutex_unlock (kan_mutex_t handle)
{
    SDL_UnlockMutex (KAN_HANDLE_GET (handle));
    return KAN_TRUE;
}

void kan_mutex_destroy (kan_mutex_t handle)
{
    SDL_DestroyMutex (KAN_HANDLE_GET (handle));
}
