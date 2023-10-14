#include <kan/hash/hash.h>

uint64_t kan_string_hash (const char *string)
{
    uint64_t hash = 5381u;
    char symbol;

    while ((symbol = *string))
    {
        hash = (hash << 5u) + hash + (unsigned char) symbol;
        ++string;
    }

    return hash;
}

uint64_t kan_char_sequence_hash (const char *begin, const char *end)
{
    uint64_t hash = 5381u;
    char symbol;

    while (begin != end)
    {
        symbol = *begin;
        hash = (hash << 5u) + hash + (unsigned char) symbol;
        ++begin;
    }

    return hash;
}
