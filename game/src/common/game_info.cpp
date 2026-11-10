#include "app/app.h"

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
    .loaded = nullptr,
    .window = nullptr,
    .message = nullptr,
    .ui = nullptr,
    .pad = nullptr,
    .rumble = nullptr,
};
