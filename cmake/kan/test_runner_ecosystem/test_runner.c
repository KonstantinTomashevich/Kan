#include <SDL3/SDL_main.h>

#include <kan/error/context.h>
#include <kan/testing/testing.h>

extern void execute_test_case_${TEST_NAME} (void);

int main (int argc, char *argv[])
{
    kan_error_initialize ();
    execute_test_case_${TEST_NAME} ();
    return kan_test_are_checks_passed () ? 0 : -1;
}
