#include <kan/api_common/mute_third_party_warnings.h>

KAN_MUTE_THIRD_PARTY_WARNINGS_BEGIN
#include <SDL_mouse.h>
KAN_MUTE_THIRD_PARTY_WARNINGS_END

#include <kan/platform/mouse.h>

static struct kan_mouse_button_table_t mouse_button_table = {
    .left = SDL_BUTTON_LEFT,
    .middle = SDL_BUTTON_MIDDLE,
    .right = SDL_BUTTON_RIGHT,
    .x1 = SDL_BUTTON_X1,
    .x2 = SDL_BUTTON_X2,
};

const struct kan_mouse_button_table_t *kan_platform_get_mouse_button_table (void)
{
    return &mouse_button_table;
}

uint8_t kan_platform_get_mouse_button_mask (kan_mouse_button_t button)
{
    return SDL_BUTTON (button);
}
