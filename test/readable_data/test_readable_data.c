#define _CRT_SECURE_NO_WARNINGS __CUSHION_PRESERVE__

#include <math.h>
#include <stddef.h>
#include <string.h>

#include <kan/file_system/stream.h>
#include <kan/readable_data/readable_data.h>
#include <kan/stream/random_access_stream_buffer.h>
#include <kan/testing/testing.h>

static void emit_to_file (struct kan_readable_data_event_t *events, kan_instance_size_t events_count)
{
    struct kan_stream_t *direct_file_stream = kan_direct_file_stream_open_for_write ("test.rd", true);
    struct kan_stream_t *buffered_file_stream =
        kan_random_access_stream_buffer_open_for_write (direct_file_stream, 1024u);

    kan_readable_data_emitter_t emitter = kan_readable_data_emitter_create (buffered_file_stream);
    for (kan_loop_size_t index = 0u; index < events_count; ++index)
    {
        KAN_TEST_CHECK (kan_readable_data_emitter_step (emitter, &events[index]))
    }

    kan_readable_data_emitter_destroy (emitter);
    buffered_file_stream->operations->close (buffered_file_stream);
}

static void save_text_to_file (const char *text)
{
    struct kan_stream_t *direct_file_stream = kan_direct_file_stream_open_for_write ("test.rd", true);
    struct kan_stream_t *buffered_file_stream =
        kan_random_access_stream_buffer_open_for_write (direct_file_stream, 1024u);

    const kan_instance_size_t length = (kan_instance_size_t) strlen (text);
    KAN_TEST_CHECK (buffered_file_stream->operations->write (buffered_file_stream, length, text) == length)
    buffered_file_stream->operations->close (buffered_file_stream);
}

static void parse_file_and_check (const struct kan_readable_data_event_t *events, kan_instance_size_t events_count)
{
    struct kan_stream_t *direct_file_stream = kan_direct_file_stream_open_for_read ("test.rd", true);
    struct kan_stream_t *buffered_file_stream =
        kan_random_access_stream_buffer_open_for_read (direct_file_stream, 1024u);

    kan_instance_size_t event_index = 0u;
    kan_readable_data_parser_t parser = kan_readable_data_parser_create (buffered_file_stream);

    while (true)
    {
        enum kan_readable_data_parser_response_t response = kan_readable_data_parser_step (parser);
        KAN_TEST_CHECK (response != KAN_READABLE_DATA_PARSER_RESPONSE_FAILED)

        if (response == KAN_READABLE_DATA_PARSER_RESPONSE_COMPLETED ||
            response == KAN_READABLE_DATA_PARSER_RESPONSE_FAILED)
        {
            break;
        }

        const struct kan_readable_data_event_t *expected_event = &events[event_index];
        const struct kan_readable_data_event_t *parsed_event = kan_readable_data_parser_get_last_event (parser);
        KAN_TEST_CHECK (expected_event->type == parsed_event->type)

        if (expected_event->type == parsed_event->type)
        {
            switch (expected_event->type)
            {
            case KAN_READABLE_DATA_EVENT_ELEMENTAL_IDENTIFIER_SETTER:
            {
                KAN_TEST_CHECK (
                    strcmp (expected_event->output_target.identifier, parsed_event->output_target.identifier) == 0)
                KAN_TEST_CHECK (expected_event->output_target.array_index == parsed_event->output_target.array_index)

                struct kan_readable_data_value_node_t *expected_node = expected_event->setter_value_first;
                struct kan_readable_data_value_node_t *parsed_node = parsed_event->setter_value_first;

                while (expected_node && parsed_node)
                {
                    KAN_TEST_CHECK (strcmp (expected_node->identifier, parsed_node->identifier) == 0)
                    expected_node = expected_node->next;
                    parsed_node = parsed_node->next;
                }

                KAN_TEST_CHECK (!expected_node)
                KAN_TEST_CHECK (!parsed_node)
                break;
            }

            case KAN_READABLE_DATA_EVENT_ELEMENTAL_STRING_SETTER:
            {
                KAN_TEST_CHECK (
                    strcmp (expected_event->output_target.identifier, parsed_event->output_target.identifier) == 0)
                KAN_TEST_CHECK (expected_event->output_target.array_index == parsed_event->output_target.array_index)

                struct kan_readable_data_value_node_t *expected_node = expected_event->setter_value_first;
                struct kan_readable_data_value_node_t *parsed_node = parsed_event->setter_value_first;

                while (expected_node && parsed_node)
                {
                    KAN_TEST_CHECK (strcmp (expected_node->string, parsed_node->string) == 0)
                    expected_node = expected_node->next;
                    parsed_node = parsed_node->next;
                }

                KAN_TEST_CHECK (!expected_node)
                KAN_TEST_CHECK (!parsed_node)
                break;
            }

            case KAN_READABLE_DATA_EVENT_ELEMENTAL_INTEGER_SETTER:
            {
                KAN_TEST_CHECK (
                    strcmp (expected_event->output_target.identifier, parsed_event->output_target.identifier) == 0)
                KAN_TEST_CHECK (expected_event->output_target.array_index == parsed_event->output_target.array_index)

                struct kan_readable_data_value_node_t *expected_node = expected_event->setter_value_first;
                struct kan_readable_data_value_node_t *parsed_node = parsed_event->setter_value_first;

                while (expected_node && parsed_node)
                {
                    KAN_TEST_CHECK (expected_node->integer == parsed_node->integer)
                    expected_node = expected_node->next;
                    parsed_node = parsed_node->next;
                }

                KAN_TEST_CHECK (!expected_node)
                KAN_TEST_CHECK (!parsed_node)
                break;
            }

            case KAN_READABLE_DATA_EVENT_ELEMENTAL_FLOATING_SETTER:
            {
                KAN_TEST_CHECK (
                    strcmp (expected_event->output_target.identifier, parsed_event->output_target.identifier) == 0)
                KAN_TEST_CHECK (expected_event->output_target.array_index == parsed_event->output_target.array_index)

                struct kan_readable_data_value_node_t *expected_node = expected_event->setter_value_first;
                struct kan_readable_data_value_node_t *parsed_node = parsed_event->setter_value_first;

                while (expected_node && parsed_node)
                {
                    KAN_TEST_CHECK (fabs (expected_node->floating - parsed_node->floating) < 0.00001)
                    expected_node = expected_node->next;
                    parsed_node = parsed_node->next;
                }

                KAN_TEST_CHECK (!expected_node)
                KAN_TEST_CHECK (!parsed_node)
                break;
            }

            case KAN_READABLE_DATA_EVENT_STRUCTURAL_SETTER_BEGIN:
                KAN_TEST_CHECK (
                    strcmp (expected_event->output_target.identifier, parsed_event->output_target.identifier) == 0)
                KAN_TEST_CHECK (expected_event->output_target.array_index == parsed_event->output_target.array_index)
                break;

            case KAN_READABLE_DATA_EVENT_ARRAY_APPENDER_BEGIN:
                KAN_TEST_CHECK (
                    strcmp (expected_event->output_target.identifier, parsed_event->output_target.identifier) == 0)
                KAN_TEST_CHECK (expected_event->output_target.array_index == parsed_event->output_target.array_index)
                break;

            case KAN_READABLE_DATA_EVENT_BLOCK_END:
                break;
            }
        }

        ++event_index;
    }

    KAN_TEST_CHECK (event_index == events_count)
    kan_readable_data_parser_destroy (parser);
    buffered_file_stream->operations->close (buffered_file_stream);
}

KAN_TEST_CASE (emit_and_parse_all_elemental_setters)
{
#define TEST_EVENTS_COUNT 6u
    struct kan_readable_data_event_t events_to_emit[TEST_EVENTS_COUNT];

    struct kan_readable_data_value_node_t movement_config_identifier_node = {
        .next = NULL,
        .identifier = "mc_warrior",
    };

    events_to_emit[0u] = (struct kan_readable_data_event_t) {
        .type = KAN_READABLE_DATA_EVENT_ELEMENTAL_IDENTIFIER_SETTER,
        .output_target =
            {
                .identifier = "movement_config",
                .array_index = KAN_READABLE_DATA_ARRAY_INDEX_NONE,
            },
        .setter_value_first = &movement_config_identifier_node,
    };

    struct kan_readable_data_value_node_t some_enum_flags_third_node = {
        .next = NULL,
        .identifier = "BITFLAG_THIRD",
    };

    struct kan_readable_data_value_node_t some_enum_flags_second_node = {
        .next = &some_enum_flags_third_node,
        .identifier = "BITFLAG_SECOND",
    };

    struct kan_readable_data_value_node_t some_enum_flags_first_node = {
        .next = &some_enum_flags_second_node,
        .identifier = "BITFLAG_FIRST",
    };

    events_to_emit[1u] = (struct kan_readable_data_event_t) {
        .type = KAN_READABLE_DATA_EVENT_ELEMENTAL_IDENTIFIER_SETTER,
        .output_target =
            {
                .identifier = "some_enum_flags",
                .array_index = KAN_READABLE_DATA_ARRAY_INDEX_NONE,
            },
        .setter_value_first = &some_enum_flags_first_node,
    };

    struct kan_readable_data_value_node_t phrases_0_node = {
        .next = NULL,
        .string = "Hello world!",
    };

    events_to_emit[2u] = (struct kan_readable_data_event_t) {
        .type = KAN_READABLE_DATA_EVENT_ELEMENTAL_STRING_SETTER,
        .output_target =
            {
                .identifier = "phrases",
                .array_index = 0u,
            },
        .setter_value_first = &phrases_0_node,
    };

    struct kan_readable_data_value_node_t phrases_1_node = {
        .next = NULL,
        .string = "Hello readable data!",
    };

    events_to_emit[3u] = (struct kan_readable_data_event_t) {
        .type = KAN_READABLE_DATA_EVENT_ELEMENTAL_STRING_SETTER,
        .output_target =
            {
                .identifier = "phrases",
                .array_index = 1u,
            },
        .setter_value_first = &phrases_1_node,
    };

    struct kan_readable_data_value_node_t health_max_node = {
        .next = NULL,
        .integer = 100,
    };

    events_to_emit[4u] = (struct kan_readable_data_event_t) {
        .type = KAN_READABLE_DATA_EVENT_ELEMENTAL_INTEGER_SETTER,
        .output_target =
            {
                .identifier = "health_max",
                .array_index = KAN_READABLE_DATA_ARRAY_INDEX_NONE,
            },
        .setter_value_first = &health_max_node,
    };

    struct kan_readable_data_value_node_t position_y_node = {
        .next = NULL,
        .floating = (kan_readable_data_floating_t) 5.5,
    };

    struct kan_readable_data_value_node_t position_x_node = {
        .next = &position_y_node,
        .floating = (kan_readable_data_floating_t) -1.34,
    };

    events_to_emit[5u] = (struct kan_readable_data_event_t) {
        .type = KAN_READABLE_DATA_EVENT_ELEMENTAL_FLOATING_SETTER,
        .output_target =
            {
                .identifier = "transform.position",
                .array_index = KAN_READABLE_DATA_ARRAY_INDEX_NONE,
            },
        .setter_value_first = &position_x_node,
    };

    emit_to_file (events_to_emit, TEST_EVENTS_COUNT);
    parse_file_and_check (events_to_emit, TEST_EVENTS_COUNT);
#undef TEST_EVENTS_COUNT
}

KAN_TEST_CASE (emit_and_parse_complex_setters)
{
#define TEST_EVENTS_COUNT 13u
    struct kan_readable_data_event_t events_to_emit[TEST_EVENTS_COUNT];

    events_to_emit[0u] = (struct kan_readable_data_event_t) {
        .type = KAN_READABLE_DATA_EVENT_STRUCTURAL_SETTER_BEGIN,
        .output_target =
            {
                .identifier = "movement",
                .array_index = KAN_READABLE_DATA_ARRAY_INDEX_NONE,
            },
    };

    events_to_emit[1u] = (struct kan_readable_data_event_t) {
        .type = KAN_READABLE_DATA_EVENT_STRUCTURAL_SETTER_BEGIN,
        .output_target =
            {
                .identifier = "modes.default",
                .array_index = KAN_READABLE_DATA_ARRAY_INDEX_NONE,
            },
    };

    struct kan_readable_data_value_node_t max_velocity_node = {
        .next = NULL,
        .floating = (kan_readable_data_floating_t) 10.0,
    };

    events_to_emit[2u] = (struct kan_readable_data_event_t) {
        .type = KAN_READABLE_DATA_EVENT_ELEMENTAL_FLOATING_SETTER,
        .output_target =
            {
                .identifier = "max_velocity",
                .array_index = KAN_READABLE_DATA_ARRAY_INDEX_NONE,
            },
        .setter_value_first = &max_velocity_node,
    };

    struct kan_readable_data_value_node_t max_acceleration_node = {
        .next = NULL,
        .floating = (kan_readable_data_floating_t) 20.0,
    };

    events_to_emit[3u] = (struct kan_readable_data_event_t) {
        .type = KAN_READABLE_DATA_EVENT_ELEMENTAL_FLOATING_SETTER,
        .output_target =
            {
                .identifier = "max_acceleration",
                .array_index = KAN_READABLE_DATA_ARRAY_INDEX_NONE,
            },
        .setter_value_first = &max_acceleration_node,
    };

    events_to_emit[4u] = (struct kan_readable_data_event_t) {
        .type = KAN_READABLE_DATA_EVENT_BLOCK_END,
    };

    struct kan_readable_data_value_node_t animation_graph_identifier_node = {
        .next = NULL,
        .identifier = "mc_warrior",
    };

    events_to_emit[5u] = (struct kan_readable_data_event_t) {
        .type = KAN_READABLE_DATA_EVENT_ELEMENTAL_IDENTIFIER_SETTER,
        .output_target =
            {
                .identifier = "animation_graph",
                .array_index = KAN_READABLE_DATA_ARRAY_INDEX_NONE,
            },
        .setter_value_first = &animation_graph_identifier_node,
    };

    events_to_emit[6u] = (struct kan_readable_data_event_t) {
        .type = KAN_READABLE_DATA_EVENT_BLOCK_END,
    };

    events_to_emit[7u] = (struct kan_readable_data_event_t) {
        .type = KAN_READABLE_DATA_EVENT_ARRAY_APPENDER_BEGIN,
        .output_target =
            {
                .identifier = "inventory.weapons",
                .array_index = KAN_READABLE_DATA_ARRAY_INDEX_NONE,
            },
    };

    struct kan_readable_data_value_node_t weapon_0_node = {
        .next = NULL,
        .identifier = "w_sword",
    };

    events_to_emit[8u] = (struct kan_readable_data_event_t) {
        .type = KAN_READABLE_DATA_EVENT_ELEMENTAL_IDENTIFIER_SETTER,
        .output_target =
            {
                .identifier = "id",
                .array_index = KAN_READABLE_DATA_ARRAY_INDEX_NONE,
            },
        .setter_value_first = &weapon_0_node,
    };

    events_to_emit[9u] = (struct kan_readable_data_event_t) {
        .type = KAN_READABLE_DATA_EVENT_BLOCK_END,
    };

    events_to_emit[10u] = (struct kan_readable_data_event_t) {
        .type = KAN_READABLE_DATA_EVENT_ARRAY_APPENDER_BEGIN,
        .output_target =
            {
                .identifier = "inventory.weapons",
                .array_index = KAN_READABLE_DATA_ARRAY_INDEX_NONE,
            },
    };

    struct kan_readable_data_value_node_t weapon_1_node = {
        .next = NULL,
        .identifier = "w_bow",
    };

    events_to_emit[11u] = (struct kan_readable_data_event_t) {
        .type = KAN_READABLE_DATA_EVENT_ELEMENTAL_IDENTIFIER_SETTER,
        .output_target =
            {
                .identifier = "id",
                .array_index = KAN_READABLE_DATA_ARRAY_INDEX_NONE,
            },
        .setter_value_first = &weapon_1_node,
    };

    events_to_emit[12u] = (struct kan_readable_data_event_t) {
        .type = KAN_READABLE_DATA_EVENT_BLOCK_END,
    };

    emit_to_file (events_to_emit, TEST_EVENTS_COUNT);
    parse_file_and_check (events_to_emit, TEST_EVENTS_COUNT);
#undef TEST_EVENTS_COUNT
}

KAN_TEST_CASE (human_input_corner_cases)
{
#define TEST_EVENTS_COUNT 6u
    struct kan_readable_data_event_t events_to_emit[TEST_EVENTS_COUNT];

    struct kan_readable_data_value_node_t movement_config_identifier_node = {
        .next = NULL,
        .identifier = "mc_warrior",
    };

    events_to_emit[0u] = (struct kan_readable_data_event_t) {
        .type = KAN_READABLE_DATA_EVENT_ELEMENTAL_IDENTIFIER_SETTER,
        .output_target =
            {
                .identifier = "movement_config",
                .array_index = KAN_READABLE_DATA_ARRAY_INDEX_NONE,
            },
        .setter_value_first = &movement_config_identifier_node,
    };

    struct kan_readable_data_value_node_t some_enum_flags_third_node = {
        .next = NULL,
        .identifier = "BITFLAG_THIRD",
    };

    struct kan_readable_data_value_node_t some_enum_flags_second_node = {
        .next = &some_enum_flags_third_node,
        .identifier = "BITFLAG_SECOND",
    };

    struct kan_readable_data_value_node_t some_enum_flags_first_node = {
        .next = &some_enum_flags_second_node,
        .identifier = "BITFLAG_FIRST",
    };

    events_to_emit[1u] = (struct kan_readable_data_event_t) {
        .type = KAN_READABLE_DATA_EVENT_ELEMENTAL_IDENTIFIER_SETTER,
        .output_target =
            {
                .identifier = "some_enum_flags",
                .array_index = KAN_READABLE_DATA_ARRAY_INDEX_NONE,
            },
        .setter_value_first = &some_enum_flags_first_node,
    };

    struct kan_readable_data_value_node_t phrases_0_node = {
        .next = NULL,
        .string = "Hello world!",
    };

    events_to_emit[2u] = (struct kan_readable_data_event_t) {
        .type = KAN_READABLE_DATA_EVENT_ELEMENTAL_STRING_SETTER,
        .output_target =
            {
                .identifier = "phrases",
                .array_index = 0u,
            },
        .setter_value_first = &phrases_0_node,
    };

    struct kan_readable_data_value_node_t phrases_1_node = {
        .next = NULL,
        .string = "Hello readable data!",
    };

    events_to_emit[3u] = (struct kan_readable_data_event_t) {
        .type = KAN_READABLE_DATA_EVENT_ELEMENTAL_STRING_SETTER,
        .output_target =
            {
                .identifier = "phrases",
                .array_index = 1u,
            },
        .setter_value_first = &phrases_1_node,
    };

    struct kan_readable_data_value_node_t health_max_node = {
        .next = NULL,
        .integer = 100,
    };

    events_to_emit[4u] = (struct kan_readable_data_event_t) {
        .type = KAN_READABLE_DATA_EVENT_ELEMENTAL_INTEGER_SETTER,
        .output_target =
            {
                .identifier = "health_max",
                .array_index = KAN_READABLE_DATA_ARRAY_INDEX_NONE,
            },
        .setter_value_first = &health_max_node,
    };

    struct kan_readable_data_value_node_t position_y_node = {
        .next = NULL,
        .floating = (kan_readable_data_floating_t) 5.5,
    };

    struct kan_readable_data_value_node_t position_x_node = {
        .next = &position_y_node,
        .floating = (kan_readable_data_floating_t) -1.34,
    };

    events_to_emit[5u] = (struct kan_readable_data_event_t) {
        .type = KAN_READABLE_DATA_EVENT_ELEMENTAL_FLOATING_SETTER,
        .output_target =
            {
                .identifier = "transform.position",
                .array_index = KAN_READABLE_DATA_ARRAY_INDEX_NONE,
            },
        .setter_value_first = &position_x_node,
    };

    save_text_to_file (
        "//! some_data_type_t\n"
        "\n"
        "// Movement config id for our character.\n"
        "movement_config  =  mc_warrior   \n"
        "\n"
        "some_enum_flags = BITFLAG_FIRST,   BITFLAG_SECOND,  BITFLAG_THIRD\n"
        "phrases[0] = \"Hello\"   \" world!\"\n"
        "phrases[1] = \"Hello\" \n"
        "// Split everything here just for fun.\n"
        "\" readable\" \" data!\"\n"
        "health_max =     100\n"
        "transform.position   = -1.34, 5.5\n");

    parse_file_and_check (events_to_emit, TEST_EVENTS_COUNT);
#undef TEST_EVENTS_COUNT
}
