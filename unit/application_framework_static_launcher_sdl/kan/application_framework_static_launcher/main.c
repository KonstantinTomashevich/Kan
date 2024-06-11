#include <SDL3/SDL_main.h>

#include <kan/application_framework/application_framework.h>

extern const char *core_configuration_path;
extern const char *program_configuration_path;

const char *__asan_default_options (void)
{
    return "detect_leaks=0";
}

int main (int argc, char *argv[])
{
    return kan_application_framework_run (core_configuration_path, program_configuration_path, (uint64_t) argc, argv);
}
