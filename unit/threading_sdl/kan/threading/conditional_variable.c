#include <SDL3/SDL_mutex.h>

#include <kan/log/logging.h>
#include <kan/threading/conditional_variable.h>

kan_conditional_variable_t kan_conditional_variable_create (void)
{
    void *sdl_handle = SDL_CreateCondition ();
    if (!sdl_handle)
    {
        KAN_LOG (threading, KAN_LOG_ERROR, "Failed to create conditional variable: %s.", SDL_GetError ())
        return KAN_HANDLE_SET_INVALID (kan_conditional_variable_t);
    }

    return KAN_HANDLE_SET (kan_conditional_variable_t, sdl_handle);
}

bool kan_conditional_variable_wait (kan_conditional_variable_t handle, kan_mutex_t associated_mutex)
{
    SDL_WaitCondition (KAN_HANDLE_GET (handle), KAN_HANDLE_GET (associated_mutex));
    return true;
}

bool kan_conditional_variable_signal_one (kan_conditional_variable_t handle)
{
    SDL_SignalCondition (KAN_HANDLE_GET (handle));
    return true;
}

bool kan_conditional_variable_signal_all (kan_conditional_variable_t handle)
{
    SDL_BroadcastCondition (KAN_HANDLE_GET (handle));
    return true;
}

void kan_conditional_variable_destroy (kan_conditional_variable_t handle)
{
    SDL_DestroyCondition (KAN_HANDLE_GET (handle));
}
