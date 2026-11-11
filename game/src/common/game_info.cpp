#include "app/app.h"

namespace input {
struct Pad;
}
namespace tsfp {
void Loaded();                         // game/tsfp_ps5.cpp
extern bool g_left_trigger_aims;       // game/tsfp_controls.cpp
void Pad(int port, input::Pad& p);
}  // namespace tsfp

// what the runtime asks of the game; strings must be set, hooks may stay null
const app::GameInfo app::kGame = {
    .title = L"TimeSplitters: Future Perfect",  // window and splash
    .short_title = L"TimeSplitters: Future Perfect",  // message boxes
    .name = L"TSFP",                       // window classes, crash dumps, handover event
    .log = "tsfp",                         // log file prefix
    .settings = "settings.toml",           // settings file beside the exe
    .splash = L"splash",                   // optional splash.png or splash.bmp beside the exe
    .missing = L"Put the game in the assets folder: its disc image (.iso), or default.xbe with its folders beside it.",
    .images = {{"default.xbe", "TSFP"}},   // each recompiled xbe and its exe, for title launches
    // the game's own settings, in the settings file
    .options = {{"input", "left_trigger_aims", cfg::Type::Bool, "Left trigger aims",
                 "L2 aims, as clicking the right stick does, and R2 fires. Clicking the right stick then throws a "
                 "grenade, as L2 did. Set the game's Controls > Aim Mode to Hold to aim only while L2 is held.",
                 &tsfp::g_left_trigger_aims}},
    .load = nullptr,
    .save = nullptr,
    .init = nullptr,
    .loaded = tsfp::Loaded,                // image in memory, before its entry point: the PS5 in its text and buttons
    .window = nullptr,
    .message = nullptr,
    .ui = nullptr,
    .pad = tsfp::Pad,                      // changes a polled pad: the left trigger aims
    .rumble = nullptr,
};
