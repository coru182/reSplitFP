// reSplitFP: the in-game settings menu. L3 + R3 (both stick clicks) opens it over the game, which stands still behind
// it; Circle, Options or L3 + R3 again goes back. L1 / R1 step through its pages.
//
// The PC port's menu (sdk/runtime/app/overlay.cpp, XboxXBE-Recomp, MIT, TGP482, and Athelas, MIT, Shagg) on a PS5:
// the same standstill (the title's thread waits in Hold at its next Swap, the guest's clocks frozen, the sound
// output taking no samples), the same controls and look, without the keyboard, mouse and window. It shows the
// settings that make sense on a PS5 and that the Vulkan renderer honours; the frame rate changes at once here.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include <imgui.h>

#include "app/app.h"
#include "app/overlay_font.h"
#include "core/input.h"
#include "core/log.h"
#include "kernel/kernel.h"

namespace app {

namespace {

std::atomic<bool> g_open{false}, g_held{false}, g_dirty{false};
std::atomic<uint16_t> g_mask_buttons[4];  // buttons held as the menu opened or closed, kept from the game until let go
std::atomic<uint8_t> g_mask_analog[4];
std::atomic<bool> g_combo[4];
input::Pad g_last{};  // FeedPads: the controllers last time, so a button counts once as it goes down
ImFont* g_font = nullptr;
constexpr float kFontPixels = 48.0f;  // the atlas size; drawn smaller with UiScale

// what the menu shows, page by page: 0 picture, 1 controls. A setting marked restart takes effect at the
// next start. choices/labels, when given, replace the setting's own (the frame rates and render sizes that fit a PS5).
struct Item {
    const char* key;
    int page;
    bool restart = false;
    std::vector<const char*> choices = {}, labels = {};
    const char* label = nullptr;  // in place of the setting's own caption
    const char* help = nullptr;
};
const std::vector<Item>& Items() {
    static const std::vector<Item> items = {
        {"display.render_percent", 0, false, {"50", "75", "100"}, {"1080p (50%)", "1620p (75%)", "4K (100%)"}},
        {"display.vsync", 0},
        {"display.show_fps", 0},
        {"graphics.anisotropic_filtering", 0},
        {"graphics.lod_bias", 0},
        {"graphics.smooth_2d", 0},
        {"input.vibration", 1},
        {"input.stick_deadzone", 1},
        {"input.trigger_threshold", 1},
    };
    return items;
}

void MaskHeld() {
    for (int port = 0; port < 4; port++) {
        input::Pad p = input::Raw(port);
        uint8_t a = 0;
        for (int i = 0; i < 8; i++)
            if (p.analog[i] > 30) a |= uint8_t(1u << i);
        g_mask_buttons[port] = p.buttons;
        g_mask_analog[port] = a;
    }
}

void SaveIfChanged() {
    if (!g_dirty.exchange(false)) return;
    if (kGame.save_settings) kGame.save_settings();
    else Save();
    LOGR("menu: settings saved");
}

input::Pad AllPads();

void Open() {
    if (g_open.exchange(true)) return;
    MaskHeld();
    // what is held now (the L3 + R3 that opened it) is not a press in the menu: 0.1.3 took the opening clicks as
    // the closing ones and shut the menu at once
    g_last = AllPads();
    LOGR("menu: open");
}

void Close() {
    if (!g_open.load()) return;
    MaskHeld();
    SaveIfChanged();
    g_open = false;
    LOGR("menu: closed");
}

bool NumberChoices(const cfg::Desc& d) {
    for (const char* c : d.choices) {
        char* end = nullptr;
        std::strtol(c, &end, 10);
        if (!*c || *end) return false;
    }
    return !d.choices.empty();
}

// one setting's control, as overlay.cpp's; true when it changed
bool Control(const cfg::Desc& d, bool restart) {
    ImGui::PushID(d.key);
    ImGui::PushID(d.section);
    bool changed = false;
    float w = ImGui::GetContentRegionAvail().x;
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(d.label);
    if (restart) {
        ImGui::SameLine();
        ImGui::TextDisabled("(next start)");
    }
    ImGui::SameLine(w * 0.45f);
    ImGui::SetNextItemWidth(w * 0.55f);
    switch (d.type) {
        case cfg::Type::Bool:
            if (d.flag) changed = ImGui::Checkbox("##v", d.flag);
            break;
        case cfg::Type::Int:
            if (d.value) {
                int v = int(*d.value);
                if (ImGui::SliderInt("##v", &v, int(d.min), int(d.max))) {
                    *d.value = uint32_t(std::clamp<int64_t>(v, d.min, d.max));
                    changed = true;
                }
            }
            break;
        case cfg::Type::Float:
            if (d.real) {
                float v = *d.real;
                if (ImGui::SliderFloat("##v", &v, float(d.min) / 100.0f, float(d.max) / 100.0f, "%.2f")) {
                    *d.real = std::round(v * 100.0f) / 100.0f;
                    changed = true;
                }
            }
            break;
        case cfg::Type::Choice: {
            int cur = -1;
            bool numbers = NumberChoices(d);
            for (size_t i = 0; i < d.choices.size(); i++) {
                if (d.flag) {
                    if (int(*d.flag) == int(i)) cur = int(i);
                } else if (d.value) {
                    uint32_t want = numbers ? uint32_t(std::atoi(d.choices[i])) : uint32_t(i);
                    if (*d.value == want) cur = int(i);
                }
            }
            auto label = [&](int i) { return size_t(i) < d.labels.size() ? d.labels[size_t(i)] : d.choices[size_t(i)]; };
            if (ImGui::BeginCombo("##v", cur >= 0 ? label(cur) : "")) {
                for (int i = 0; i < int(d.choices.size()); i++) {
                    if (ImGui::Selectable(label(i), i == cur) && i != cur) {
                        if (d.flag) *d.flag = i != 0;
                        else if (d.value) *d.value = numbers ? uint32_t(std::atoi(d.choices[size_t(i)])) : uint32_t(i);
                        changed = true;
                    }
                    if (i == cur) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            break;
        }
        case cfg::Type::String: break;
    }
    if (d.help && ImGui::IsItemFocused()) ImGui::SetItemTooltip("%s", d.help);
    ImGui::PopID();
    ImGui::PopID();
    if (changed) {
        std::string k = std::string(d.section) + "." + d.key;
        if (kGame.setting_changed) kGame.setting_changed(d);
        LOGR("menu: %s = %s", k.c_str(), cfg::ValueText(d).c_str());
    }
    return changed;
}

// every controller's buttons together (the menu answers any player)
input::Pad AllPads() {
    input::Pad p{};
    for (int port = 0; port < 4; port++) {
        input::Pad q = input::Raw(port);
        if (!q.connected) continue;
        p.buttons |= q.buttons;
        for (int i = 0; i < 8; i++) p.analog[i] = std::max(p.analog[i], q.analog[i]);
        if (std::abs(q.lx) > std::abs(p.lx)) p.lx = q.lx;
        if (std::abs(q.ly) > std::abs(p.ly)) p.ly = q.ly;
    }
    return p;
}

// the controllers while the menu is open, into ImGui's gamepad keys (the game reads none of it)
void FeedPads() {
    input::Pad p = AllPads();
    const input::Pad& last = g_last;
    auto pressed = [&](uint16_t bit) { return (p.buttons & bit) && !(last.buttons & bit); };
    bool circle = p.analog[1] > 30, circle_was = last.analog[1] > 30;
    bool both = (p.buttons & 0xC0) == 0xC0, both_was = (last.buttons & 0xC0) == 0xC0;
    {
        std::lock_guard lock(UiLock());
        ImGuiIO& io = ImGui::GetIO();
        io.AddKeyEvent(ImGuiKey_GamepadDpadUp, p.buttons & 0x01);
        io.AddKeyEvent(ImGuiKey_GamepadDpadDown, p.buttons & 0x02);
        io.AddKeyEvent(ImGuiKey_GamepadDpadLeft, p.buttons & 0x04);
        io.AddKeyEvent(ImGuiKey_GamepadDpadRight, p.buttons & 0x08);
        io.AddKeyEvent(ImGuiKey_GamepadFaceDown, p.analog[0] > 30);  // Cross: choose
        io.AddKeyEvent(ImGuiKey_GamepadFaceRight, circle);           // Circle: back out of a list
        io.AddKeyEvent(ImGuiKey_GamepadFaceLeft, p.analog[2] > 30);
        io.AddKeyEvent(ImGuiKey_GamepadFaceUp, p.analog[3] > 30);
        auto stick = [&](ImGuiKey neg, ImGuiKey pos, int v) {
            float f = std::clamp(v / 32767.0f, -1.0f, 1.0f);
            f = std::fabs(f) < 0.3f ? 0.0f : f;
            io.AddKeyAnalogEvent(neg, f < 0, f < 0 ? -f : 0.0f);
            io.AddKeyAnalogEvent(pos, f > 0, f > 0 ? f : 0.0f);
        };
        stick(ImGuiKey_GamepadLStickLeft, ImGuiKey_GamepadLStickRight, p.lx);
        stick(ImGuiKey_GamepadLStickDown, ImGuiKey_GamepadLStickUp, p.ly);
        // Circle with no list open, Options, or both stick clicks again: back to the game
        if (pressed(0x10) || (both && !both_was) ||
            (circle && !circle_was && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)))
            Close();
    }
    g_last = p;
}

}  // namespace

bool OverlayOpen() { return g_open.load(); }
bool Paused() { return g_open.load(); }
bool Held() { return g_held.load(); }
bool Muted() { return g_held.load(); }
bool Focused() { return true; }
bool FastForward() { return false; }
void ToggleFastForward() {}

void Hold(const std::function<void()>& present) {
    if (!Paused()) return;
    kernel::SetSpeed(1);
    kernel::Freeze(true);
    g_held = true;
    auto t0 = std::chrono::steady_clock::now();
    while (Paused()) {
        FeedPads();
        present();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));  // the screen's 60 a second
    }
    g_held = false;
    kernel::Freeze(false);
    LOG_FIRST(4, "pause: %.1f s", std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
}

// input::Poll, before the game sees the pad: both stick clicks open the menu; nothing reaches the game while it is
// open, and what was held as it opened or closed stays away from the game until let go
void OverlayPad(int port, input::Pad& p) {
    if (port < 0 || port > 3) return;
    bool both = (p.buttons & 0xC0) == 0xC0;
    if (both && !g_combo[port].exchange(true)) {
        Open();
        g_mask_buttons[port] = uint16_t(g_mask_buttons[port] | 0xC0);
    }
    if (!both) g_combo[port] = false;
    if (g_open) {
        bool c = p.connected;
        p = input::Pad{};
        p.connected = c;
        return;
    }
    uint16_t mb = g_mask_buttons[port];
    if (mb) {
        g_mask_buttons[port] = uint16_t(mb & p.buttons);
        p.buttons = uint16_t(p.buttons & ~mb);
    }
    uint8_t ma = g_mask_analog[port];
    if (ma) {
        uint8_t still = 0;
        for (int i = 0; i < 8; i++)
            if (ma & (1u << i)) {
                if (p.analog[i] > 30) still |= uint8_t(1u << i);
                p.analog[i] = 0;
            }
        g_mask_analog[port] = still;
    }
}

void OverlayInit() {
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig fc;
    fc.OversampleH = 2;
    static const ImWchar kRanges[] = {0x0020, 0x024F, 0x0400, 0x052F, 0x2000, 0x206F, 0x20AC, 0x20AC, 0x2122, 0x2122, 0};
    g_font = io.Fonts->AddFontFromMemoryCompressedTTF(overlay_font_compressed_data, int(overlay_font_compressed_size),
                                                      kFontPixels, &fc, kRanges);
    io.FontDefault = g_font;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    ImGuiStyle& st = ImGui::GetStyle();
    ImGui::StyleColorsDark(&st);
    st.WindowRounding = 6;
    st.FrameRounding = 4;
    st.GrabRounding = 4;
    st.WindowBorderSize = 1;
    ImVec4* c = st.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.06f, 0.05f, 0.94f);
    c[ImGuiCol_Border] = ImVec4(0.55f, 0.45f, 0.25f, 0.6f);
    c[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.17f, 0.12f, 1.0f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.32f, 0.26f, 0.16f, 1.0f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.40f, 0.32f, 0.18f, 1.0f);
    c[ImGuiCol_SliderGrab] = c[ImGuiCol_CheckMark] = ImVec4(0.85f, 0.70f, 0.40f, 1.0f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.95f, 0.80f, 0.50f, 1.0f);
    c[ImGuiCol_Button] = c[ImGuiCol_Header] = c[ImGuiCol_Tab] = ImVec4(0.26f, 0.21f, 0.13f, 1.0f);
    c[ImGuiCol_ButtonHovered] = c[ImGuiCol_HeaderHovered] = c[ImGuiCol_TabHovered] = ImVec4(0.42f, 0.33f, 0.18f, 1.0f);
    c[ImGuiCol_ButtonActive] = c[ImGuiCol_HeaderActive] = c[ImGuiCol_TabSelected] = ImVec4(0.52f, 0.40f, 0.20f, 1.0f);
    c[ImGuiCol_NavHighlight] = ImVec4(0.95f, 0.80f, 0.45f, 1.0f);
    c[ImGuiCol_Separator] = ImVec4(0.55f, 0.45f, 0.25f, 0.5f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.26f, 0.21f, 0.13f, 1.0f);
}

float UiScale() {
    float h = ImGui::GetMainViewport()->Size.y;
    return std::max(16.0f, h / 720.0f * 20.0f) / kFontPixels;
}

void DrawOverlay(double) {
    static bool shown = false;  // drawn last frame: a new opening takes the controller focus
    if (!g_open) {
        shown = false;
        return;
    }
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    float scale = UiScale();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x / 2, vp->Pos.y + vp->Size.y / 2), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(vp->Size.x * 0.9f, kFontPixels * scale * 34), vp->Size.y * 0.8f), ImGuiCond_Always);
    if (!shown) ImGui::SetNextWindowFocus();
    ImGui::Begin("Settings  -  Circle, Options or L3 + R3 to go back to the game", nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    shown = true;
    ImGui::SetWindowFontScale(scale);
    static const char* const kPages[2] = {"Picture", "Controls"};
    // the settings, with the PS5's choices where they differ
    std::vector<cfg::Desc> all = SettingsList();
    std::vector<std::pair<cfg::Desc, const Item*>> shown_items;
    for (const Item& it : Items())
        for (const cfg::Desc& d : all)
            if (std::string(d.section) + "." + d.key == it.key) {
                cfg::Desc c = d;
                if (!it.choices.empty()) {
                    c.choices = it.choices;
                    c.labels = it.labels;
                }
                if (it.label) c.label = it.label;
                if (it.help) c.help = it.help;
                shown_items.push_back({c, &it});
                break;
            }
    static int page = 0, want = -1;
    static bool l1 = false, r1 = false;
    {
        input::Pad p = input::Raw(0);
        bool l = p.analog[5] > 30, r = p.analog[4] > 30;  // White on L1, Black on R1
        int step = (l && !l1) || (r && !r1) ? 1 : 0;
        if (step) want = (page + step) % 2;
        l1 = l;
        r1 = r;
    }
    if (ImGui::BeginTabBar("##pages")) {
        for (int i = 0; i < 2; i++) {
            ImGuiTabItemFlags f = want == i ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem(kPages[i], nullptr, f)) {
                page = i;
                ImGui::BeginChild("##list", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 1.4f), ImGuiChildFlags_NavFlattened);
                if (i == 0) ImGui::SeparatorText("Picture");
                for (auto& [d, it] : shown_items) {
                    if (it->page != i) continue;
                    if (Control(d, it->restart)) g_dirty = true;
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
        }
        want = -1;
        ImGui::EndTabBar();
    }
    ImGui::Separator();
    if (ImGui::Button("Back to the game")) Close();
    ImGui::SameLine();
    ImGui::TextDisabled("L1 / R1: pages");
    ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize("Quit the game").x - ImGui::GetStyle().FramePadding.x * 2);
    if (ImGui::Button("Quit the game")) ImGui::OpenPopup("##quit");
    if (ImGui::BeginPopupModal("##quit", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) {
        ImGui::SetWindowFontScale(scale);
        ImGui::TextUnformatted("Quit the game? Progress since the last save is lost.");
        if (ImGui::Button("Quit")) {
            SaveIfChanged();
            kernel::Shutdown(0);  // the saves tidied and the log closed; back to the home screen
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::End();
}

}  // namespace app
