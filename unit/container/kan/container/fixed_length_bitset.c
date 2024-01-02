#include <kan/api_common/min_max.h>
#include <kan/container/fixed_length_bitset.h>
#include <kan/error/critical.h>

void kan_fixed_length_bitset_init (struct kan_fixed_length_bitset_t *bitset, uint64_t length)
{
    uint64_t items = length / 64u;
    if (length % 64u)
    {
        ++items;
    }

    bitset->items = items;
    for (uint64_t index = 0u; index < items; ++index)
    {
        bitset->data[index] = 0u;
    }
}

void kan_fixed_length_bitset_set (struct kan_fixed_length_bitset_t *bitset, uint64_t index, kan_bool_t value)
{
    const uint64_t item_index = index / 64u;
    KAN_ASSERT (item_index < bitset->items)
    const uint64_t bit_index = index % 64u;
    const uint64_t mask = ((uint64_t) 1u) << bit_index;

    if (value)
    {
        bitset->data[item_index] |= mask;
    }
    else
    {
        bitset->data[item_index] &= ~mask;
    }
}

kan_bool_t kan_fixed_length_bitset_get (const struct kan_fixed_length_bitset_t *bitset, uint64_t index)
{
    const uint64_t item_index = index / 64u;
    KAN_ASSERT (item_index < bitset->items)
    const uint64_t bit_index = index % 64u;
    const uint64_t mask = ((uint64_t) 1u) << bit_index;
    return (bitset->data[item_index] & mask) > 0u ? KAN_TRUE : KAN_FALSE;
}

void kan_fixed_length_bitset_or_assign (struct kan_fixed_length_bitset_t *bitset,
                                        const struct kan_fixed_length_bitset_t *source_bitset)
{
    const uint64_t common_items = KAN_MIN (bitset->items, source_bitset->items);
    for (uint64_t index = 0u; index < common_items; ++index)
    {
        bitset->data[index] |= source_bitset->data[index];
    }
}

kan_bool_t kan_fixed_length_bitset_check_intersection (const struct kan_fixed_length_bitset_t *bitset,
                                                       const struct kan_fixed_length_bitset_t *other_bitset)
{
    const uint64_t common_items = KAN_MIN (bitset->items, other_bitset->items);
    for (uint64_t index = 0u; index < common_items; ++index)
    {
        if ((bitset->data[index] & other_bitset->data[index]) != 0u)
        {
            return KAN_TRUE;
        }
    }

    return KAN_FALSE;
}
