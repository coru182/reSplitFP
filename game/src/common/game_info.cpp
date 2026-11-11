#include "app/app.h"

namespace tsfp {
void Loaded();  // game/tsfp_ps5.cpp
}

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
    .options = {},
    .load = nullptr,
    .save = nullptr,
    .init = nullptr,
    .loaded = tsfp::Loaded,                // image in memory, before its entry point: the PS5 in its text and buttons
    .window = nullptr,
    .message = nullptr,
    .ui = nullptr,
    .pad = nullptr,
    .rumble = nullptr,
};
