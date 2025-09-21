#include <SDL3/SDL_mutex.h>

#include <kan/threading/log_category.h>
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

void kan_mutex_lock (kan_mutex_t handle) { SDL_LockMutex (KAN_HANDLE_GET (handle)); }

bool kan_mutex_try_lock (kan_mutex_t handle) { return SDL_TryLockMutex (KAN_HANDLE_GET (handle)) == 0; }

bool kan_mutex_unlock (kan_mutex_t handle)
{
    SDL_UnlockMutex (KAN_HANDLE_GET (handle));
    return true;
}

void kan_mutex_destroy (kan_mutex_t handle) { SDL_DestroyMutex (KAN_HANDLE_GET (handle)); }
