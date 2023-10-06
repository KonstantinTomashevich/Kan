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
