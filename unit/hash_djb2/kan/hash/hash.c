#include <kan/hash/hash.h>

kan_hash_t kan_string_hash (const char *string) { return kan_string_hash_append (5381u, string); }

kan_hash_t kan_string_hash_append (kan_hash_t hash_value, const char *string)
{
    char symbol;
    while ((symbol = *string))
    {
        hash_value = (hash_value << 5u) + hash_value + (unsigned char) symbol;
        ++string;
    }

    return hash_value;
}

kan_hash_t kan_char_sequence_hash (const char *begin, const char *end)
{
    return kan_char_sequence_hash_append (5381u, begin, end);
}

kan_hash_t kan_char_sequence_hash_append (kan_hash_t hash_value, const char *begin, const char *end)
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

kan_hash_t kan_hash_combine (kan_hash_t first, kan_hash_t second)
{
    const kan_hash_t bits_in_hash = sizeof (kan_hash_t) * 8u;
    // Hash combination from Mike Ash.
    // https://www.mikeash.com/pyblog/friday-qa-2010-06-18-implementing-equality-and-hashing.html
    return ((first << (bits_in_hash / 2u)) | (first >> (bits_in_hash / 2u))) ^ second;
}
