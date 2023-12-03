#pragma once

#include <stddef.h>
#include <stdint.h>

struct manual_event_t
{
    int32_t x;
    int32_t y;
};

struct manual_event_second_t
{
    uint64_t id;
    float a;
    float b;
};
