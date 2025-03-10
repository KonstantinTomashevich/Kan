#include <SDL3/SDL_loadso.h>

#include <kan/log/logging.h>
#include <kan/platform/dynamic_library.h>

KAN_LOG_DEFINE_CATEGORY (platform_dynamic_library);

kan_platform_dynamic_library_t kan_platform_dynamic_library_load (const char *path)
{
    void *object = SDL_LoadObject (path);
    if (!object)
    {
        KAN_LOG (platform_dynamic_library, KAN_LOG_ERROR,
                 "Failed to load dynamic library at path \"%s\", backend error: %s", path, SDL_GetError ());
        return KAN_HANDLE_SET_INVALID (kan_platform_dynamic_library_t);
    }

    return KAN_HANDLE_SET (kan_platform_dynamic_library_t, object);
}

void *kan_platform_dynamic_library_find_function (kan_platform_dynamic_library_t library, const char *name)
{
    void *function = (void *) SDL_LoadFunction (KAN_HANDLE_GET (library), name);
    if (!function)
    {
        KAN_LOG (platform_dynamic_library, KAN_LOG_ERROR, "Failed to find function \"%s\", backend error: %s", name,
                 SDL_GetError ());
        return NULL;
    }

    return function;
}

void kan_platform_dynamic_library_unload (kan_platform_dynamic_library_t library)
{
    SDL_UnloadObject (KAN_HANDLE_GET (library));
}
