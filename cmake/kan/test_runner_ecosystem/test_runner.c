#include <SDL3/SDL_main.h>

#include <kan/error/context.h>
#include <kan/testing/testing.h>

#if defined(KAN_TEST_ECOSYSTEM_NO_ASAN_LEAK_TEST)
const char *__asan_default_options (void)
{
    return "detect_leaks=0";
}
#endif

extern void execute_test_case_${TEST_NAME} (void);

int main (int argc, char *argv[])
{
    kan_error_initialize ();
    execute_test_case_${TEST_NAME} ();
    return kan_test_are_checks_passed () ? 0 : -1;
}
