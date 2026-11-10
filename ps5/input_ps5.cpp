// reSplitFP: DualSense controllers through libScePad, as the console's Xbox controllers. Replaces core/input.cpp
// (SDL on Windows) and keeps what it does: [input] stick_deadzone and trigger_threshold, the [controls] layout,
// vibration scaled by [input] vibration, and GameInfo::pad / rumble for the game's own changes.
// Player 1 is the user who started the app; players 2 to 4 are the other signed-in users whose controllers are on
// (guest accounts will do), for the game's split screen. The pad data prefix, button bits and user handling follow reUndr's ps5/src/input_ps5.c and
// pad_map.c (MIT, bystritskiy).
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <mutex>

#include "app/app.h"
#include "core/input.h"
#include "core/log.h"

extern "C" {
int scePadInit(void);
int scePadOpen(int user, int type, int index, const void* param);
int scePadGetHandle(int user, int type, int index);
int scePadClose(int handle);
int scePadReadState(int handle, void* data);
int scePadSetVibration(int handle, const void* motors);
int scePadSetVibrationMode(int handle, int mode);
int sceUserServiceInitialize(const void* param);
int sceUserServiceGetInitialUser(int* user);
int sceUserServiceGetLoginUserIdList(void* list);  // four signed user ids, -1 where none
}

namespace input {

namespace {

// the public prefix of libScePad's state, padded to well past its real size
struct PadData {
    uint32_t buttons;
    uint8_t lx, ly, rx, ry, l2, r2;
    uint8_t reserved[66];
    uint8_t connected;
    uint8_t tail[179];
};
static_assert(offsetof(PadData, connected) == 76, "libScePad connected ABI");

// libScePad button bits
constexpr uint32_t kL3 = 0x2, kR3 = 0x4, kOptions = 0x8, kUp = 0x10, kRight = 0x20, kDown = 0x40, kLeft = 0x80,
                   kL1 = 0x400, kR1 = 0x800, kTriangle = 0x1000, kCircle = 0x2000, kCross = 0x4000,
                   kSquare = 0x8000, kTouchpad = 0x100000;

constexpr int kPorts = 4;
constexpr int kNoUser = -1;

struct Slot {
    int user = kNoUser, handle = -1;
    bool owned = false, connected = false;
};

std::mutex g_lock;
Slot g_slots[kPorts];
bool g_init = false;
std::chrono::steady_clock::time_point g_next_scan{};

bool ValidUser(int u) { return u >= 0 && u != 255; }

void Connection(int port, bool on) {
    Slot& s = g_slots[port];
    if (s.connected == on) return;
    s.connected = on;
    LOGR("input: player %d controller %s", port + 1, on ? "connected" : "disconnected");
}

void Release(int port) {
    Slot& s = g_slots[port];
    if (s.handle >= 0) {
        const uint8_t stop[2] = {0, 0};
        scePadSetVibration(s.handle, stop);
        if (s.owned) scePadClose(s.handle);
    }
    Connection(port, false);
    s.handle = -1;
    s.owned = false;
}

void Open(Slot& s) {
    if (s.handle >= 0 || !ValidUser(s.user)) return;
    s.handle = scePadOpen(s.user, 0, 0, nullptr);
    s.owned = s.handle >= 0;
    if (s.handle < 0) s.handle = scePadGetHandle(s.user, 0, 0);  // already open (the system's own handle)
    if (s.handle >= 0) scePadSetVibrationMode(s.handle, 2);
}

bool ReadSlot(int port, PadData& d) {
    std::memset(&d, 0, sizeof d);
    Slot& s = g_slots[port];
    if (s.handle < 0) return false;
    if (scePadReadState(s.handle, &d) < 0) {
        Release(port);
        return false;
    }
    Connection(port, d.connected != 0);
    return s.connected;
}

// once a second: player 1 on the starting user, players 2 to 4 on other signed-in users with a controller on
void Scan() {
    auto now = std::chrono::steady_clock::now();
    if (now < g_next_scan) return;
    g_next_scan = now + std::chrono::seconds(1);
    struct { int user[4]; } users = {{-1, -1, -1, -1}};
    bool listed = sceUserServiceGetLoginUserIdList(&users) >= 0;
    auto signed_in = [&](int u) { return std::find(std::begin(users.user), std::end(users.user), u) != std::end(users.user); };
    auto taken = [&](int u) {
        for (const Slot& s : g_slots)
            if (s.user == u) return true;
        return false;
    };
    for (int i = 0; i < kPorts; i++)
        if (listed && ValidUser(g_slots[i].user) && !signed_in(g_slots[i].user)) {
            Release(i);
            g_slots[i].user = kNoUser;
        }
    int initial = kNoUser;
    sceUserServiceGetInitialUser(&initial);
    if (!ValidUser(g_slots[0].user) && ValidUser(initial) && (!listed || signed_in(initial))) {
        for (int i = 1; i < kPorts; i++)
            if (g_slots[i].user == initial) {
                Release(i);
                g_slots[i].user = kNoUser;
            }
        g_slots[0].user = initial;
    }
    for (Slot& s : g_slots) Open(s);
    if (!listed) return;
    // a port without a controller on goes to the next signed-in user who has one
    for (int i = 1; i < kPorts; i++) {
        PadData probe;
        if (ReadSlot(i, probe)) continue;
        for (int u : users.user) {
            if (!ValidUser(u) || taken(u)) continue;
            Slot c;
            c.user = u;
            Open(c);
            std::memset(&probe, 0, sizeof probe);
            if (c.handle >= 0 && scePadReadState(c.handle, &probe) >= 0 && probe.connected) {
                Release(i);
                g_slots[i] = c;
                g_slots[i].connected = false;
                Connection(i, true);
                break;
            }
            if (c.handle >= 0 && c.owned) scePadClose(c.handle);
        }
    }
}

// a libScePad stick (0-255, 128 centred, y down) as SDL reads one: -32768..32767, y down
int Axis(uint8_t v) {
    int d = int(v) - 128;
    return d >= 0 ? d * 32767 / 127 : d * 256;
}

// [input] stick_deadzone, as core/input.cpp
void Deadzone(int& x, int& y, uint32_t pct) {
    if (!pct || pct >= 100) return;
    float dz = pct / 100.0f;
    float fx = x / 32767.0f, fy = y / 32767.0f;
    float mag = std::sqrt(fx * fx + fy * fy);
    if (mag <= dz) {
        x = y = 0;
        return;
    }
    float k = (mag - dz) / (1 - dz) / mag;
    x = std::clamp(int(std::lround(fx * k * 32767)), -32768, 32767);
    y = std::clamp(int(std::lround(fy * k * 32767)), -32768, 32767);
}

// [input] trigger_threshold, as core/input.cpp (there from SDL's 0..32767, here 0..255)
uint8_t Trigger(int v, uint32_t pct) {
    if (!pct) return uint8_t(v);
    int t = int(std::min(pct, 90u) * 255 / 100);
    return v <= t ? 0 : uint8_t(std::min(255, (v - t) * 255 / (255 - t)));
}

// the controller on that port; remap: the Xbox buttons as [controls] sets them, else as on the Xbox
Pad Read(int port, bool remap) {
    Pad p;
    if (port < 0 || port >= kPorts) return p;
    std::lock_guard lock(g_lock);
    if (!g_init) return p;
    Scan();
    PadData d;
    if (!ReadSlot(port, d)) return p;
    p.connected = true;
    constexpr uint32_t kDigital[8] = {kUp, kDown, kLeft, kRight, kOptions, kTouchpad, kL3, kR3};
    for (int i = 0; i < 8; i++)
        if (d.buttons & kDigital[i]) p.buttons |= uint16_t(1u << i);
    const app::Settings& st = app::Get();
    // the controller's buttons as input::kSources (A B X Y LB RB LT RT LS RS), then each Xbox button from its source
    uint8_t src[kSourceCount] = {};
    constexpr uint32_t kFace[6] = {kCross, kCircle, kSquare, kTriangle, kL1, kR1};
    for (int i = 0; i < 6; i++) src[i] = d.buttons & kFace[i] ? 0xFF : 0;
    src[6] = Trigger(d.l2, st.trigger_threshold);
    src[7] = Trigger(d.r2, st.trigger_threshold);
    src[8] = d.buttons & kL3 ? 0xFF : 0;
    src[9] = d.buttons & kR3 ? 0xFF : 0;
    bool moved[2] = {};
    for (int i = 0; i < 8; i++) {
        static constexpr uint32_t kXbox[8] = {0, 1, 2, 3, 5, 4, 6, 7};  // Black on R1, White on L1
        uint32_t from = remap ? std::min(st.buttons[i], kSourceCount - 1) : kXbox[i];
        p.analog[i] = src[from];
        if (from == 8 || from == 9) moved[from - 8] = true;
    }
    if (moved[0]) p.buttons &= uint16_t(~(1u << 6));
    if (moved[1]) p.buttons &= uint16_t(~(1u << 7));
    int lx = Axis(d.lx), ly = Axis(d.ly), rx = Axis(d.rx), ry = Axis(d.ry);
    Deadzone(lx, ly, st.stick_deadzone);
    Deadzone(rx, ry, st.stick_deadzone);
    p.lx = int16_t(lx);
    p.ly = int16_t(-1 - ly);  // the Xbox's y is up
    p.rx = int16_t(rx);
    p.ry = int16_t(-1 - ry);
    return p;
}

}  // namespace

void Init() {
    std::lock_guard lock(g_lock);
    if (g_init) return;
    sceUserServiceInitialize(nullptr);
    int rc = scePadInit();
    g_init = true;
    LOGI("input: libScePad %s", rc >= 0 ? "ready" : "init failed");
}

Pad Raw(int port) { return Read(port, false); }

bool Connected(int port) { return Read(port, false).connected; }

Pad Poll(int port) {
    Pad p = Read(port, true);
    app::OverlayPad(port, p);  // both stick clicks open the in-game menu; nothing reaches the game while it is open
    if (app::kGame.pad) app::kGame.pad(port, p);
    return p;
}

void Rumble(int port, uint16_t left, uint16_t right) {
    if (app::kGame.rumble) app::kGame.rumble(port, left, right);
    uint32_t pct = std::min(app::Get().vibration, 100u);
    const uint8_t motors[2] = {uint8_t((left * pct / 100) >> 8), uint8_t((right * pct / 100) >> 8)};
    std::lock_guard lock(g_lock);
    if (port >= 0 && port < kPorts && g_slots[port].handle >= 0 && g_slots[port].connected)
        scePadSetVibration(g_slots[port].handle, motors);
}

// every controller here is a DualSense: the game's hints show PlayStation buttons
int Style(int) { return 1; }

}  // namespace input
