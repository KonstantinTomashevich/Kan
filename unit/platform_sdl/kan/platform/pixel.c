#include <kan/api_common/mute_third_party_warnings.h>

KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN
#include <SDL_pixels.h>
KAN_MUTE_THIRD_PARTY_WARNINGS_END

#include <kan/platform/pixel.h>

static struct kan_platform_pixel_format_table_t pixel_format_table = {
    .rgba_32 = SDL_PIXELFORMAT_RGBA32,
    .abgr_32 = SDL_PIXELFORMAT_ABGR32,
    .argb_32 = SDL_PIXELFORMAT_ARGB32,
};

const struct kan_platform_pixel_format_table_t *kan_platform_get_pixel_format_table (void)
{
    return &pixel_format_table;
}
