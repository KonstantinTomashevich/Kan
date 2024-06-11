#include <SDL3/SDL_mutex.h>

#include <kan/log/logging.h>
#include <kan/threading/conditional_variable.h>

kan_conditional_variable_handle_t kan_conditional_variable_create (void)
{
    void *sdl_handle = SDL_CreateCondition ();
    if (!sdl_handle)
    {
        KAN_LOG (threading, KAN_LOG_ERROR, "Failed to create conditional variable: %s.", SDL_GetError ())
        return KAN_INVALID_MUTEX_HANDLE;
    }

    return (kan_conditional_variable_handle_t) sdl_handle;
}

kan_bool_t kan_conditional_variable_wait (kan_conditional_variable_handle_t handle, kan_mutex_handle_t associated_mutex)
{
    return SDL_WaitCondition ((void *) handle, (void *) associated_mutex) == 0;
}

kan_bool_t kan_conditional_variable_signal_one (kan_conditional_variable_handle_t handle)
{
    return SDL_SignalCondition ((void *) handle) == 0;
}

kan_bool_t kan_conditional_variable_signal_all (kan_conditional_variable_handle_t handle)
{
    return SDL_BroadcastCondition ((void *) handle) == 0;
}

void kan_conditional_variable_destroy (kan_conditional_variable_handle_t handle)
{
    SDL_DestroyCondition ((void *) handle);
}
