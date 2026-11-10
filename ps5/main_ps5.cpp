// reSplitFP: the app's entry on a PS5. Finds the player's copy of TimeSplitters: Future Perfect (US Xbox), checks it is the release
// this build was recompiled from, and boots it from its files, as the PC port does.
// The first start unpacks the player's disc image (.iso) into /data/tsfp/game, with a screen of its own, and the game
// starts from that folder from then on (the .iso can then be deleted), as reUndr does. When there is no room for it,
// the game reads the disc image directly instead (the image mounted as the D: drive).
// The boot order follows the PC port's (runtime/app/main.cpp) and the harness's main_test.cpp; the console parts
// (affinity, splash, notifications, folders, the first start and its screens) follow reUndr's ps5/src/main_ps5.c
// (MIT, bystritskiy).
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <imgui.h>

#include "app/app.h"
#include "core/input.h"
#include "core/log.h"
#include "image_symbols.h"
#include "kernel/disc.h"
#include "kernel/kernel.h"
#include "ps5/disc_image.h"
#include "ps5/ps5_host.h"
#include "ps5/screen_ps5.h"
#include "ps5/xbe_match.h"
#include "xbe/image_hash.h"

extern const uint64_t kImageSectionsHash;  // written by xbe_recomp from the default.xbe this build was made from
namespace ps5 {  // written by tools/xbe_blocks.cpp from the same default.xbe (ps5/xbe_match.h)
extern const uint64_t kXbeLayout;
extern const uint32_t kXbeBlockCount;
extern const uint64_t kXbeBlocks[];
}  // namespace ps5
extern "C" {
int sceSystemServiceHideSplashScreen(void);
int sceKernelSendNotificationRequest(int, void*, size_t, int);
int scePthreadSetaffinity(unsigned long, unsigned long);
int sceKernelDebugOutText(int, const char*);
int sceKernelUsleep(unsigned);
}

namespace ps5 {

const std::filesystem::path& DataDir() {
    static const std::filesystem::path d = "/data/tsfp";
    return d;
}

void Notify(const char* text) {
    struct {
        char reserved[45];
        char message[3075];
    } note{};
    std::snprintf(note.message, sizeof note.message, "%s", text);
    sceKernelSendNotificationRequest(0, &note, sizeof note, 0);
}

// name in dir, in whatever case the disc has it
std::filesystem::path Child(const std::filesystem::path& dir, const char* name) {
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(dir, ec))
        if (strcasecmp(e.path().filename().c_str(), name) == 0) return e.path();
    return {};
}

// the game's files: default.xbe and its Data folder
bool Unpacked(const std::filesystem::path& dir) {
    std::error_code ec;
    auto xbe = Child(dir, IMAGE_XBE), d = Child(dir, "Data");
    return !xbe.empty() && std::filesystem::is_regular_file(xbe, ec) && !d.empty() &&
           std::filesystem::is_directory(d, ec);
}

std::filesystem::path GameDir() { return DataDir() / "game"; }

bool FindGame(Game& game) {
    std::vector<std::filesystem::path> dirs{GameDir()};
    for (int i = 0; i < 8; i++) dirs.push_back("/mnt/usb" + std::to_string(i) + "/tsfp/game");
    for (const auto& d : dirs)
        if (Unpacked(d)) {
            game = {d, false};
            return true;
        }
    return false;
}

std::vector<std::filesystem::path> FindImages() {
    std::vector<std::filesystem::path> dirs{DataDir()}, images;
    for (int i = 0; i < 8; i++) {
        dirs.push_back("/mnt/usb" + std::to_string(i) + "/tsfp");
        dirs.push_back("/mnt/usb" + std::to_string(i));
    }
    for (const auto& dir : dirs) {
        std::error_code ec;
        std::vector<std::filesystem::path> here;
        for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
            std::string n = e.path().filename().string();
            if (n.size() < 5 || n[0] == '.' || strcasecmp(n.c_str() + n.size() - 4, ".iso") != 0) continue;
            if (e.is_regular_file(ec)) here.push_back(e.path());
        }
        std::sort(here.begin(), here.end());
        images.insert(images.end(), here.begin(), here.end());
    }
    return images;
}

}  // namespace ps5

namespace {

// the image's default.xbe, read from the disc image or the folder
bool ReadXbe(const ps5::Game& g, std::unique_ptr<kernel::Disc>& disc, std::vector<uint8_t>& xbe, std::string& error) {
    if (g.image) {
        disc = std::make_unique<kernel::Disc>();
        if (!disc->Open(g.path, error)) return false;
        if (!disc->Load(IMAGE_XBE, xbe)) {
            error = "no " IMAGE_XBE " on the disc image";
            return false;
        }
        return true;
    }
    std::ifstream f(ps5::Child(g.path, IMAGE_XBE), std::ios::binary);
    xbe.assign(std::istreambuf_iterator<char>(f), {});
    if (xbe.empty()) error = "could not read " IMAGE_XBE;
    return !xbe.empty();
}

constexpr uint32_t kTitleId = 0x45410066;  // TimeSplitters: Future Perfect

uint32_t TitleId(const std::vector<uint8_t>& x) {
    auto u32 = [&](size_t at) -> uint32_t {
        uint32_t v = 0;
        if (at + 4 <= x.size()) std::memcpy(&v, x.data() + at, 4);
        return v;
    };
    if (x.size() < 0x180 || std::memcmp(x.data(), "XBEH", 4)) return 0;
    uint32_t base = u32(0x104), cert = u32(0x118);
    return cert >= base ? u32(cert - base + 8) : 0;
}

// the same bytes as the build's default.xbe, or the same release with a few bytes of code changed: a media-patched
// dump, or a clean one (ps5/xbe_match.h)
bool SameRelease(const std::vector<uint8_t>& x, uint32_t& differ) {
    differ = 0;
    return xbe::SectionsHash(x) == kImageSectionsHash ||
           ps5::xbe_match::SameRelease(x, ps5::kXbeLayout, ps5::kXbeBlocks, ps5::kXbeBlockCount, differ);
}

// The screens before the game (ps5/screen_ps5.c): unpacking the disc image, and what is wrong. Its font has letters,
// digits and . , : - _ / ( ) ' % only.
constexpr uint32_t kAccent = 0xff62d6f2;
using Lines = std::vector<TsfpScreenLine>;

void WaitForCross() {
    input::Init();
    bool was = true;  // a Cross still held from starting the app doesn't count
    for (;;) {
        bool down = input::Raw(0).analog[0] != 0;
        if (down && !was) return;
        was = down;
        sceKernelUsleep(50000);
    }
}

// says what is wrong, then waits for Cross and closes the app
[[noreturn]] void Fail(const Lines& lines, const char* note) {
    LOGE("%s", note);
    logx::Flush();
    ps5::Notify(note);
    if (tsfp_screen_show(lines.data(), unsigned(lines.size()), -1) == 0) WaitForCross();
    else sceKernelUsleep(200000);
    tsfp_screen_close();
    sceKernelDebugOutText(0, "[tsfp] stopped\n");
    _exit(0);
}

[[noreturn]] void NotFound() {
    Fail({{260, 7, kAccent, "Game not found"},
          {380, 4, 0, "reSplitFP runs your own copy of the US Xbox game."},
          {450, 4, 0, "Copy your disc image (.iso) to one of these places:"},
          {520, 4, kAccent, "/data/tsfp/"},
          {570, 4, kAccent, "/mnt/usb0/tsfp/  or  /mnt/usb0/   (usb0 to usb7)"},
          {650, 4, 0, "Or copy the unpacked disc to this folder:"},
          {700, 4, kAccent, "/data/tsfp/game/"},
          {860, 4, 0, "Then start reSplitFP again. Press Cross to close."}},
         "reSplitFP: game not found. Copy your disc image (.iso) to /data/tsfp/. See README.txt.");
}

[[noreturn]] void Damaged() {
    Fail({{260, 7, kAccent, "The disc image is damaged"},
          {380, 4, 0, "It isn't an Xbox disc image, or it is incomplete."},
          {440, 4, 0, "Make it again from your disc, or copy it to the console again."},
          {860, 4, 0, "Press Cross to close."}},
         "reSplitFP: the disc image is damaged. See README.txt.");
}

// what to remove: the disc image or the folder that was refused
std::string g_refused_path;

[[noreturn]] void OtherGame() {
    static std::string remove;
    remove = "Remove " + g_refused_path + " and copy yours instead.";
    Fail({{260, 7, kAccent, "This is another game"},
          {380, 4, 0, "reSplitFP runs TimeSplitters: Future Perfect for the Xbox, US release."},
          {440, 4, 0, remove.c_str()},
          {860, 4, 0, "Press Cross to close."}},
         "reSplitFP: that disc is another game. It needs TimeSplitters: Future Perfect (US Xbox).");
}

[[noreturn]] void OtherRelease() {
    static std::string remove;
    remove = "Remove " + g_refused_path + " and copy yours instead.";
    Fail({{260, 7, kAccent, "This is another release of the game"},
          {380, 4, 0, "This build was made from the US Xbox release"},
          {440, 4, 0, "and only runs that one."},
          {500, 4, 0, remove.c_str()},
          {860, 4, 0, "Press Cross to close."}},
         "reSplitFP: this is another release of the game (not the US one this build was made from).");
}

[[noreturn]] void Unreadable(const std::string& error) {
    static std::string line;
    line = error;
    Fail({{260, 7, kAccent, "The game could not be read"},
          {380, 4, 0, line.c_str()},
          {860, 4, 0, "Press Cross to close."}},
         ("reSplitFP: the game could not be read: " + error).c_str());
}

// The first start: the disc image unpacked into /data/tsfp/game, with the unpacking screen, and its percentage in a
// notification now and then when there is no screen.
int g_refused;  // what accepting the image's default.xbe found: 1 another game, 2 another release
char g_size_line[96];
bool g_shown = true;

int AcceptXbe(const char* path) {
    std::ifstream f(path, std::ios::binary);
    std::vector<uint8_t> x((std::istreambuf_iterator<char>(f)), {});
    uint32_t differ = 0;
    g_refused = TitleId(x) != kTitleId ? 1 : !SameRelease(x, differ) ? 2 : 0;
    LOGI("unpack: %s %s", IMAGE_XBE, g_refused == 1 ? "is another game" : g_refused ? "is another release" : "is the game");
    return g_refused == 0;
}

void Progress(unsigned percent, uint64_t total) {
    if (percent == 0)
        std::snprintf(g_size_line, sizeof g_size_line, "This happens once. The game needs about %.1f GB on the console.",
                      double(total) / 1e9);
    const TsfpScreenLine lines[] = {{260, 7, kAccent, "Unpacking your disc image"},
                                    {380, 4, 0, g_size_line},
                                    {440, 4, 0, "When it is done, the game starts and you can delete the .iso."}};
    if (g_shown) g_shown = tsfp_screen_show(lines, 3, int(percent)) == 0;
    if (!g_shown && percent % 20 == 0) {
        char note[96];
        std::snprintf(note, sizeof note, "reSplitFP: unpacking your disc image (once), %u%%", percent);
        ps5::Notify(note);
    }
}

enum class Unpack { kDone, kNoRoom, kNone };

// The images, until one of the game is unpacked into the game folder (through game.part, renamed when complete).
// kNoRoom: an image of the game that the folder had no room for, in image, to play from directly.
Unpack UnpackImage(const std::vector<std::filesystem::path>& images, std::filesystem::path& image) {
    const std::filesystem::path target = ps5::GameDir(), part = target.string() + ".part";
    int worst = TSFP_IMAGE_NOT_A_DISC;
    bool no_room = false;
    for (const auto& i : images) {
        std::error_code ec;
        std::filesystem::remove_all(part, ec);
        g_refused = 0;
        LOGI("unpack: %s into %s", i.c_str(), target.c_str());
        int r = tsfp_image_extract(i.c_str(), part.c_str(), AcceptXbe, Progress);
        if (r == TSFP_IMAGE_OK) {
            // a game folder there already isn't complete (FindGame passed it over): kept beside, not deleted
            if (std::filesystem::exists(target, ec)) {
                std::string aside = target.string() + ".incomplete";
                for (int n = 2; std::filesystem::exists(aside, ec); n++)
                    aside = target.string() + ".incomplete-" + std::to_string(n);
                std::filesystem::rename(target, aside, ec);
            }
            std::filesystem::rename(part, target, ec);
            if (!ec && ps5::Unpacked(target)) {
                LOGI("unpack: done");
                return Unpack::kDone;
            }
            r = TSFP_IMAGE_WRITE;
        }
        LOGW("unpack: %s: %s", i.c_str(),
             r == TSFP_IMAGE_WRITE ? "the folder could not be written (no room?)" : r == TSFP_IMAGE_DAMAGED ? "damaged"
             : r == TSFP_IMAGE_WRONG_DISC ? "not the game" : "not an Xbox disc image");
        std::filesystem::remove_all(part, ec);
        if (r == TSFP_IMAGE_WRITE && !no_room) {
            no_room = true;
            image = i;
        }
        if (r == TSFP_IMAGE_WRONG_DISC && 10 + g_refused > worst) {  // above damaged: the clearer news
            worst = 10 + g_refused;
            g_refused_path = i.string();
        }
        else if (r == TSFP_IMAGE_DAMAGED) worst = std::max(worst, int(TSFP_IMAGE_DAMAGED));
    }
    if (no_room) return Unpack::kNoRoom;
    if (worst == 11) OtherGame();
    if (worst == 12) OtherRelease();
    if (worst == TSFP_IMAGE_DAMAGED || !images.empty()) Damaged();
    return Unpack::kNone;
}

// a pipe's read end drained into a file by a thread of its own (reUndr's open_log, main_ps5.c)
struct Sink {
    int from, to;
};
void* Drain(void* p) {
    auto* s = static_cast<Sink*>(p);
    char buf[16384];
    for (;;) {
        ssize_t n = read(s->from, buf, sizeof buf);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        if (s->to < 0) continue;  // no log: the lines are dropped
        for (ssize_t done = 0; done < n;) {
            ssize_t w = write(s->to, buf + done, size_t(n - done));
            if (w < 0 && errno == EINTR) continue;
            if (w <= 0) break;
            done += w;
        }
    }
    close(s->from);
    if (s->to >= 0) close(s->to);
    return nullptr;
}
// path null: the lines are read and dropped, so nothing that writes to stderr ever waits
FILE* OpenLog(const char* path) {
    int fd = -1;
    if (path && (fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0) return nullptr;
    int ends[2];
    if (pipe(ends) == 0) {
        static Sink sink;
        sink = {ends[0], fd};
        pthread_t t;
        if (FILE* stream = fdopen(ends[1], "w"); stream && pthread_create(&t, nullptr, Drain, &sink) == 0) {
            pthread_detach(t);
            std::setvbuf(stream, nullptr, _IOLBF, 0);  // a line at a time into the pipe: memory, not the disk
            return stream;
        }
        close(ends[0]);
        close(ends[1]);
    }
    return fd >= 0 ? fdopen(fd, "w") : nullptr;
}

// after the screens: a notification only (the game's renderer may hold VideoOut)
[[noreturn]] void Stop(const char* why) {
    LOGE("%s", why);
    logx::Flush();
    ps5::Notify(why);
    sceKernelDebugOutText(0, "[tsfp] stopped\n");
    _exit(0);
}

}  // namespace

int main() {
    scePthreadSetaffinity((unsigned long)pthread_self(), 0x1fff);  // the cores a title may use
    const auto& data = ps5::DataDir();
    for (const char* sub : {"", "/logs", "/hdd", "/shadercache", "/textures"}) {
        std::string d = data.string() + sub;
        mkdir(d.c_str(), 0777);
        chmod(d.c_str(), 0777);
    }
    // stderr (Mesa, the C runtime, and a copy of every log line) into a file beside the game's own log. The lines go
    // into a pipe a thread of its own drains to the file: the console ends a crashing app at once, and the file still
    // holds nearly everything up to that moment, while the game never waits on the disk (0.1.1 wrote each line
    // straight to the file, 40-65 ms a line on the game's thread: its hiccups). dup2 onto fd 2 isn't seen by the
    // PS5 libc's stderr, so the stream itself is replaced, as reUndr's main_ps5.c does.
    // Logging is for finding a problem: on only while the file /data/tsfp/logging-on exists. Off, no line is
    // formatted or written, and stderr's (Mesa's own) are dropped in the pipe.
    std::error_code ec;
    const bool logging = std::filesystem::exists(data / "logging-on", ec);
    if (!logging) logx::Off();
    if (FILE* err = OpenLog(logging ? (data / "logs/stderr.log").c_str() : nullptr)) stderr = err;
    // Mesa's shader cache: compiled GPU code kept between runs, in a folder the app can write
    setenv("MESA_SHADER_CACHE_DIR", (data / "shadercache/mesa").c_str(), 0);
    setenv("XDG_CACHE_HOME", (data / "shadercache").c_str(), 0);
    logx::Init(app::LogFolder().string(), app::kGame.log);
    LOGI("reSplitFP: %s", TSFP_PS5_VERSION);
    app::Load();
    app::LogSystem();

    // the game's files (/data/tsfp/game, or tsfp/game on a USB drive), else the first start: the disc image unpacked
    // into /data/tsfp/game, or, when there is no room for it, the image played from directly
    ps5::Game game;
    if (!ps5::FindGame(game)) {
        std::filesystem::path image;
        switch (UnpackImage(ps5::FindImages(), image)) {
        case Unpack::kDone:
            game = {ps5::GameDir(), false};
            break;
        case Unpack::kNoRoom:
            game = {image, true};
            ps5::Notify("reSplitFP: not enough room to unpack the disc image, so the game reads it directly. "
                        "See README.txt.");
            break;
        case Unpack::kNone:
            NotFound();
        }
    }
    LOGI("game: %s (%s)", game.path.c_str(), game.image ? "disc image" : "files");
    g_refused_path = game.path.string() + (game.image ? "" : "/");
    std::unique_ptr<kernel::Disc> disc;
    std::vector<uint8_t> xbe;
    std::string error;
    if (!ReadXbe(game, disc, xbe, error)) Unreadable(error);
    if (TitleId(xbe) != kTitleId) OtherGame();
    uint32_t differ = 0;
    if (!SameRelease(xbe, differ)) OtherRelease();
    LOGI("game: %s is the release this build was made from%s", IMAGE_XBE,
         differ ? " (a few bytes of code differ: a media-patched or clean dump)" : "");
    tsfp_screen_close();  // the game's renderer opens VideoOut itself

    sceSystemServiceHideSplashScreen();
    ImGui::CreateContext();  // the renderer's overlay: the in-game menu and the fps counter, as the PC port's main.cpp
    ImGui::GetIO().IniFilename = nullptr;
    app::OverlayInit();
    input::Init();
    if (app::kGame.init) app::kGame.init();
    std::string root = game.image ? game.path.parent_path().string() : game.path.string();
    if (disc) kernel::MountDisc(std::move(disc));
    if (!kernel::Boot(std::move(xbe), IMAGE_XBE, root, app::SavesDir().string())) Stop("reSplitFP: the game could not start.");
    for (;;) {
        sleep(1);
        logx::Flush();
    }
}
