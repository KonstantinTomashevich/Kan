#include <SDL_main.h>

#include <kan/testing/testing.h>

extern void execute_test_case_${TEST_NAME} (void);

int main (int argc, char *argv[])
{
    execute_test_case_${TEST_NAME} ();
    return kan_test_are_checks_passed () ? 0 : -1;
}
