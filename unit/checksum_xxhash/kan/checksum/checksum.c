#include <kan/api_common/mute_warnings.h>

KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN
#define XXH_IMPLEMENTATION __CUSHION_PRESERVE__
#define XXH_STATIC_LINKING_ONLY __CUSHION_PRESERVE__
#include <xxhash.h>
KAN_MUTE_THIRD_PARTY_WARNINGS_END

#include <kan/checksum/checksum.h>
#include <kan/memory/allocation.h>

static bool statics_initialized = false;
static kan_allocation_group_t allocation_group;

static inline void ensure_statics_initialized (void)
{
    if (!statics_initialized)
    {
        allocation_group = kan_allocation_group_get_child (kan_allocation_group_root (), "checksum");
        statics_initialized = true;
    }
}

kan_checksum_state_t kan_checksum_create (void)
{
    ensure_statics_initialized ();
    XXH3_state_t *state = kan_allocate_general (allocation_group, sizeof (XXH3_state_t), alignof (struct XXH3_state_s));
    XXH3_INITSTATE (state);
    XXH3_64bits_reset (state);
    return KAN_HANDLE_SET (kan_checksum_state_t, state);
}

void kan_checksum_append (kan_checksum_state_t state, kan_file_size_t size, void *data)
{
    XXH3_state_t *xxhash_state = KAN_HANDLE_GET (state);
    XXH3_64bits_update (xxhash_state, data, (size_t) size);
}

kan_file_size_t kan_checksum_finalize (kan_checksum_state_t state)
{
    XXH3_state_t *xxhash_state = KAN_HANDLE_GET (state);
    kan_file_size_t checksum = (kan_file_size_t) XXH3_64bits_digest (xxhash_state);
    kan_free_general (allocation_group, xxhash_state, sizeof (XXH3_state_t));
    return checksum;
}
