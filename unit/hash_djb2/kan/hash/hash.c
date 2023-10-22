#include <kan/hash/hash.h>

uint64_t kan_string_hash (const char *string)
{
    return kan_string_hash_append (5381u, string);
}

uint64_t kan_string_hash_append (uint64_t hash_value, const char *string)
{
    char symbol;
    while ((symbol = *string))
    {
        hash_value = (hash_value << 5u) + hash_value + (unsigned char) symbol;
        ++string;
    }

    return hash_value;
}

uint64_t kan_char_sequence_hash (const char *begin, const char *end)
{
    return kan_char_sequence_hash_append (5381u, begin, end);
}

uint64_t kan_char_sequence_hash_append (uint64_t hash_value, const char *begin, const char *end)
{
    char symbol;

    while (begin != end)
    {
        symbol = *begin;
        hash_value = (hash_value << 5u) + hash_value + (unsigned char) symbol;
        ++begin;
    }

    return hash_value;
}

uint64_t kan_hash_combine (uint64_t first, uint64_t second)
{
    // Hash combination from Mike Ash.
    // https://www.mikeash.com/pyblog/friday-qa-2010-06-18-implementing-equality-and-hashing.html
    return ((first << 32u) | (first >> 32u)) ^ second;
}
