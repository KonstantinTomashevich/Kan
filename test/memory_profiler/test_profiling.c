#include <string.h>

#include <kan/memory_profiler/capture.h>
#include <kan/testing/testing.h>

KAN_TEST_CASE (creation)
{
    kan_allocation_group_t a1 = kan_allocation_group_get_child (kan_allocation_group_root (), "A1");
    kan_allocation_group_t a2 = kan_allocation_group_get_child (kan_allocation_group_root (), "A2");
    KAN_TEST_CHECK (!KAN_HANDLE_IS_EQUAL (a1, a2))
    KAN_TEST_CHECK (KAN_HANDLE_IS_EQUAL (a1, kan_allocation_group_get_child (kan_allocation_group_root (), "A1")))
    KAN_TEST_CHECK (KAN_HANDLE_IS_EQUAL (a2, kan_allocation_group_get_child (kan_allocation_group_root (), "A2")))

    kan_allocation_group_t b1 = kan_allocation_group_get_child (a1, "B1");
    kan_allocation_group_t b2 = kan_allocation_group_get_child (a2, "B2");
    KAN_TEST_CHECK (!KAN_HANDLE_IS_EQUAL (b1, b2))
    KAN_TEST_CHECK (KAN_HANDLE_IS_EQUAL (b1, kan_allocation_group_get_child (a1, "B1")))
    KAN_TEST_CHECK (KAN_HANDLE_IS_EQUAL (b2, kan_allocation_group_get_child (a2, "B2")))
}

KAN_TEST_CASE (stack)
{
    KAN_TEST_CHECK (KAN_HANDLE_IS_EQUAL (kan_allocation_group_stack_get (), kan_allocation_group_root ()))
    kan_allocation_group_t a1 = kan_allocation_group_get_child (kan_allocation_group_root (), "A1");
    kan_allocation_group_t a2 = kan_allocation_group_get_child (kan_allocation_group_root (), "A2");

    kan_allocation_group_stack_push (a1);
    KAN_TEST_CHECK (KAN_HANDLE_IS_EQUAL (kan_allocation_group_stack_get (), a1))

    kan_allocation_group_stack_push (a2);
    KAN_TEST_CHECK (KAN_HANDLE_IS_EQUAL (kan_allocation_group_stack_get (), a2))

    kan_allocation_group_stack_pop ();
    KAN_TEST_CHECK (KAN_HANDLE_IS_EQUAL (kan_allocation_group_stack_get (), a1))

    kan_allocation_group_stack_pop ();
    KAN_TEST_CHECK (KAN_HANDLE_IS_EQUAL (kan_allocation_group_stack_get (), kan_allocation_group_root ()))
}

KAN_TEST_CASE (group_capture)
{
    kan_allocation_group_t a1 = kan_allocation_group_get_child (kan_allocation_group_root (), "A1");
    kan_allocation_group_t a2 = kan_allocation_group_get_child (kan_allocation_group_root (), "A2");
    kan_allocation_group_t b1 = kan_allocation_group_get_child (a1, "B1");
    kan_allocation_group_t b2 = kan_allocation_group_get_child (a2, "B2");

    kan_allocation_group_allocate (a1, 100u);
    kan_allocation_group_allocate (a2, 200u);
    kan_allocation_group_allocate (b1, 1000u);
    kan_allocation_group_allocate (b2, 300u);
    kan_allocation_group_free (b1, 500u);
    kan_allocation_group_free (a2, 50u);

    struct kan_allocation_group_capture_t capture = kan_allocation_group_begin_capture ();

    KAN_TEST_CHECK (KAN_HANDLE_IS_EQUAL (kan_captured_allocation_group_get_source (capture.captured_root),
                                         kan_allocation_group_root ()))
    KAN_TEST_CHECK (kan_captured_allocation_group_get_directly_allocated (capture.captured_root) == 0u)
    KAN_TEST_CHECK (kan_captured_allocation_group_get_total_allocated (capture.captured_root) == 1050u)

    kan_bool_t found_a1 = KAN_FALSE;
    kan_bool_t found_a2 = KAN_FALSE;

    for (kan_captured_allocation_group_iterator_t child_iterator =
             kan_captured_allocation_group_children_begin (capture.captured_root);
         KAN_HANDLE_IS_VALID (kan_captured_allocation_group_children_get (child_iterator));
         child_iterator = kan_captured_allocation_group_children_next (child_iterator))
    {
        kan_captured_allocation_group_t group = kan_captured_allocation_group_children_get (child_iterator);

        if (strcmp (kan_captured_allocation_group_get_name (group), "A1") == 0)
        {
            found_a1 = KAN_TRUE;
            KAN_TEST_CHECK (KAN_HANDLE_IS_EQUAL (kan_captured_allocation_group_get_source (group), a1))
            KAN_TEST_CHECK (kan_captured_allocation_group_get_directly_allocated (group) == 100u)
            KAN_TEST_CHECK (kan_captured_allocation_group_get_total_allocated (group) == 600u)
        }

        if (strcmp (kan_captured_allocation_group_get_name (group), "A2") == 0)
        {
            found_a2 = KAN_TRUE;
            KAN_TEST_CHECK (KAN_HANDLE_IS_EQUAL (kan_captured_allocation_group_get_source (group), a2))
            KAN_TEST_CHECK (kan_captured_allocation_group_get_directly_allocated (group) == 150u)
            KAN_TEST_CHECK (kan_captured_allocation_group_get_total_allocated (group) == 450u)
        }
    }

    KAN_TEST_CHECK (found_a1)
    KAN_TEST_CHECK (found_a2)

    kan_captured_allocation_group_destroy (capture.captured_root);
    kan_allocation_group_event_iterator_destroy (capture.event_iterator);
}

KAN_TEST_CASE (events)
{
    kan_allocation_group_t a1 = kan_allocation_group_get_child (kan_allocation_group_root (), "A1");
    kan_allocation_group_t a2 = kan_allocation_group_get_child (kan_allocation_group_root (), "A2");
    kan_allocation_group_t b1 = kan_allocation_group_get_child (a1, "B1");
    kan_allocation_group_t b2 = kan_allocation_group_get_child (a2, "B2");

    struct kan_allocation_group_capture_t capture = kan_allocation_group_begin_capture ();
    const struct kan_allocation_group_event_t *event = kan_allocation_group_event_iterator_get (capture.event_iterator);
    KAN_TEST_CHECK (!event)

    kan_allocation_group_allocate (b1, 1000u);
    kan_allocation_group_allocate (b2, 300u);
    kan_allocation_group_free (b1, 500u);
    kan_allocation_group_free (b2, 50u);

    event = kan_allocation_group_event_iterator_get (capture.event_iterator);
    KAN_TEST_ASSERT (event)
    KAN_TEST_CHECK (event->type == KAN_ALLOCATION_GROUP_EVENT_ALLOCATE)
    KAN_TEST_CHECK (KAN_HANDLE_IS_EQUAL (event->group, b1))
    KAN_TEST_CHECK (event->amount == 1000u)

    capture.event_iterator = kan_allocation_group_event_iterator_advance (capture.event_iterator);
    event = kan_allocation_group_event_iterator_get (capture.event_iterator);
    KAN_TEST_ASSERT (event)
    KAN_TEST_CHECK (event->type == KAN_ALLOCATION_GROUP_EVENT_ALLOCATE)
    KAN_TEST_CHECK (KAN_HANDLE_IS_EQUAL (event->group, b2))
    KAN_TEST_CHECK (event->amount == 300u)

    capture.event_iterator = kan_allocation_group_event_iterator_advance (capture.event_iterator);
    event = kan_allocation_group_event_iterator_get (capture.event_iterator);
    KAN_TEST_ASSERT (event)
    KAN_TEST_CHECK (event->type == KAN_ALLOCATION_GROUP_EVENT_FREE)
    KAN_TEST_CHECK (KAN_HANDLE_IS_EQUAL (event->group, b1))
    KAN_TEST_CHECK (event->amount == 500u)

    capture.event_iterator = kan_allocation_group_event_iterator_advance (capture.event_iterator);
    event = kan_allocation_group_event_iterator_get (capture.event_iterator);
    KAN_TEST_ASSERT (event)
    KAN_TEST_CHECK (event->type == KAN_ALLOCATION_GROUP_EVENT_FREE)
    KAN_TEST_CHECK (KAN_HANDLE_IS_EQUAL (event->group, b2))
    KAN_TEST_CHECK (event->amount == 50u)

    capture.event_iterator = kan_allocation_group_event_iterator_advance (capture.event_iterator);
    event = kan_allocation_group_event_iterator_get (capture.event_iterator);
    KAN_TEST_CHECK (!event)

    kan_allocation_group_marker (a2, "Hello, world!");
    kan_allocation_group_t b3 = kan_allocation_group_get_child (a2, "B3");
    kan_allocation_group_allocate (b3, 200u);

    event = kan_allocation_group_event_iterator_get (capture.event_iterator);
    KAN_TEST_ASSERT (event)
    KAN_TEST_CHECK (event->type == KAN_ALLOCATION_GROUP_EVENT_MARKER)
    KAN_TEST_CHECK (KAN_HANDLE_IS_EQUAL (event->group, a2))
    KAN_TEST_CHECK (strcmp (event->name, "Hello, world!") == 0)

    capture.event_iterator = kan_allocation_group_event_iterator_advance (capture.event_iterator);
    event = kan_allocation_group_event_iterator_get (capture.event_iterator);
    KAN_TEST_ASSERT (event)
    KAN_TEST_CHECK (event->type == KAN_ALLOCATION_GROUP_EVENT_NEW_GROUP)
    KAN_TEST_CHECK (KAN_HANDLE_IS_EQUAL (event->group, b3))
    KAN_TEST_CHECK (strcmp (event->name, "B3") == 0)

    capture.event_iterator = kan_allocation_group_event_iterator_advance (capture.event_iterator);
    event = kan_allocation_group_event_iterator_get (capture.event_iterator);
    KAN_TEST_ASSERT (event)
    KAN_TEST_CHECK (event->type == KAN_ALLOCATION_GROUP_EVENT_ALLOCATE)
    KAN_TEST_CHECK (KAN_HANDLE_IS_EQUAL (event->group, b3))
    KAN_TEST_CHECK (event->amount == 200u)

    capture.event_iterator = kan_allocation_group_event_iterator_advance (capture.event_iterator);
    event = kan_allocation_group_event_iterator_get (capture.event_iterator);
    KAN_TEST_CHECK (!event)

    kan_captured_allocation_group_destroy (capture.captured_root);
    kan_allocation_group_event_iterator_destroy (capture.event_iterator);
}
