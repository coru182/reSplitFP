// TimeSplitters: Future Perfect on a PS5: the console named as a PS5 in the game's own text, the controller's parts as
// the DualSense's, and the PlayStation buttons (tsfp_buttons.cpp).
//
// The game's text, every language it carries, is in the front end's level file (pak/arcade/l_103.pak) as plain
// strings (Latin-1), read from the disc each time the menus load. That file is served from memory with its strings
// rewritten in place (kernel::SetFileProvider), each no longer than it was and the rest of it zeroed, so nothing
// else in the file moves; the disc image or the game folder are never changed.
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "core/log.h"
#include "kernel/kernel.h"

namespace tsfp {

void PlayStationButtons();  // tsfp_buttons.cpp

namespace {

constexpr char kFrontEnd[] = "d:\\pak\\arcade\\l_103.pak";

// text that stays as it is: the PS2 version's memory card messages, the memory unit and Dashboard ones (there are no
// memory units here, and no Dashboard to go to)
const char* const kLeave[] = {"memory card", "Memory Card", "Memory card", "memory unit", "Memory Unit", "Dashboard",
                              "Interfaz", "interfaccia", "Interface", "Steuerung", "memory\ncard"};

// whole strings said differently
struct Whole {
    const char* from;
    const char* to;
};
const Whole kWhole[] = {
    {"Please don't remove memory units, disconnect any controllers or turn off your Xbox console",
     "Please don't disconnect any controllers or turn off your PS5 console"},
    {"Play arcade matches on Xbox Live or System Link.", "Play arcade matches online or over System Link."},
    {"A, B, X and Y Buttons", "Face Buttons"},
    {"Trigger Buttons", "L2 / R2 Buttons"},
    {"Xbox Communicator", "Headset"},
    {"Communicateur Xbox", "Casque"},
    {"Xbox-Communicator", "Headset"},
};
// parts of strings, in order (each no longer than what it replaces)
const Whole kParts[] = {
    {"Xbox Hard Disk", "PS5 Storage"}, {"Xbox hard disk", "PS5 storage"}, {"Xbox Live", "Online"},
    {"Xbox\xae", "PS5"},  {"Xbox", "PS5"}, {"Left Thumbstick", "Left Stick"}, {"Right Thumbstick", "Right Stick"},
    {"thumbstick", "stick"}, {"Thumbstick", "Stick"},
};

std::string Rewrite(const std::string& s) {
    for (const Whole& w : kWhole)
        if (s == w.from) return w.to;
    for (const char* leave : kLeave)
        if (s.find(leave) != std::string::npos) return s;
    std::string out = s;
    for (const Whole& p : kParts) {
        const size_t n = std::strlen(p.from);
        for (size_t at = 0; (at = out.find(p.from, at)) != std::string::npos; at += std::strlen(p.to))
            out.replace(at, n, p.to);
    }
    return out;
}

// every string in the file that names the console or a part of its controller, rewritten where it lies
int RewriteText(std::vector<uint8_t>& d) {
    static const char* const kFind[] = {"Xbox", "humbstick", "Trigger Buttons", "A, B, X and Y Buttons"};
    int changed = 0;
    for (const char* find : kFind) {
        const size_t fn = std::strlen(find);
        for (size_t i = 0; i + fn <= d.size(); i++) {
            if (std::memcmp(d.data() + i, find, fn) != 0) continue;
            size_t s = i, e = i;
            while (s > 0 && d[s - 1]) s--;
            while (e < d.size() && d[e]) e++;
            std::string old(reinterpret_cast<const char*>(d.data() + s), e - s);
            std::string now = Rewrite(old);
            if (now != old && now.size() <= old.size()) {
                std::memcpy(d.data() + s, now.data(), now.size());
                std::memset(d.data() + s + now.size(), 0, old.size() - now.size());
                changed++;
            }
            i = e;
        }
    }
    return changed;
}

kernel::FileData Provide(const std::string& path) {
    const size_t n = sizeof(kFrontEnd) - 1;  // the name as the game gives it, "d:\pak\arcade\l_103.pak"
    if (path.size() < n - 2 || path.compare(path.size() - (n - 2), n - 2, kFrontEnd + 2) != 0) return nullptr;
    static std::mutex lock;
    static kernel::FileData text;
    std::lock_guard<std::mutex> hold(lock);
    if (!text) {
        auto d = std::make_shared<std::vector<uint8_t>>();
        if (!kernel::ReadGameFile(kFrontEnd, *d)) {
            LOGE("tsfp: %s could not be read to rewrite its text", kFrontEnd);
            return nullptr;
        }
        int changed = RewriteText(*d);
        LOGI("tsfp: %d of the game's strings name the PS5 and the DualSense's parts", changed);
        text = d;
    }
    return text;
}

}  // namespace

// the image is in memory: the buttons and the text
void Loaded() {
    PlayStationButtons();
    kernel::SetFileProvider(Provide);
}

}  // namespace tsfp
