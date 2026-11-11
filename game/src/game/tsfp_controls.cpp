// TimeSplitters: Future Perfect on a PS5: the left trigger aims, as shooters on the PS5 do.
//
// The Xbox game's default layout throws a grenade with the left trigger, fires with the right one and aims with a
// click of the right stick (Controls > Aim Mode: a toggle, or held). With [input] left_trigger_aims on, the left
// trigger and the right stick's click trade places: L2 aims and the click throws a grenade, so no action is lost. The
// game's other layouts and its own customised ones trade the same two buttons. Turned off, the Xbox layout as it was.
#include <cstdint>

#include "core/input.h"

namespace tsfp {

// [input] left_trigger_aims (game_info.cpp)
bool g_left_trigger_aims = true;

void Pad(int port, input::Pad& p) {
    (void)port;
    if (!g_left_trigger_aims || !p.connected) return;
    constexpr uint16_t kRightThumb = 0x80;  // the right stick's click
    constexpr uint8_t kPressed = 40;        // of 255: pressed this far, the trigger counts as the click (as reSplit2)
    const bool trigger = p.analog[6] > kPressed, click = (p.buttons & kRightThumb) != 0;
    p.buttons = trigger ? uint16_t(p.buttons | kRightThumb) : uint16_t(p.buttons & ~kRightThumb);
    p.analog[6] = click ? 255 : 0;
}

}  // namespace tsfp
