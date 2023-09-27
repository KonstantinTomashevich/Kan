#include <stdio.h>

#include <kan/testing/testing.h>
#include <kan/threading/atomic.h>

static struct kan_atomic_int_t failed_checks_count = {.value = 0};

void kan_test_check_failed (const char *message, const char *file, int line)
{
    // TODO: Replace with the normal log.
    fprintf (stderr, "Check failed: %s. File: %s. Line: %d.\n", message, file, line);
    kan_atomic_int_add (&failed_checks_count, 1);
}

kan_bool_t kan_test_are_checks_passed ()
{
    return kan_atomic_int_get (&failed_checks_count) == 0;
}
