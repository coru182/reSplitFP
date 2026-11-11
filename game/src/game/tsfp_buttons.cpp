// TimeSplitters: Future Perfect on a PS5: the game's Xbox buttons drawn as PlayStation ones. Its menus, prompts and
// tips draw them from small textures: the A, B, X and Y buttons, the White and Black buttons and the two trigger signs
// (32x32 each), and a sheet with B A X Y, Start, Back and two arrows (128x64). Each is replaced, by the hash the
// renderer gives its contents, with reSplitFP's own drawing four times larger (tools/mkbuttons.py, game/art): A Cross,
// B Circle, X Square, Y Triangle, White L1 and Black R1, the triggers L2 and R2, as the DualSense's buttons are mapped;
// Start is Options and Back the touch pad. The Controls menu's picture of the Xbox controller (512x256) and the glow
// behind it (128x128) are replaced the same way, with reSplitFP's own controller.
#include <cstddef>
#include <cstdint>

#include "core/log.h"
#include "gpu/texture_pack.h"

namespace tsfp {

namespace art {  // tsfp_button_art.cpp
extern const unsigned char kCross[], kCircle[], kSquare[], kTriangle[], kL1[], kR1[], kL2[], kR2[], kSheet[], kPad[],
    kGlow[];
extern const size_t kCrossSize, kCircleSize, kSquareSize, kTriangleSize, kL1Size, kR1Size, kL2Size, kR2Size,
    kSheetSize, kPadSize, kGlowSize;
}  // namespace art

namespace {

struct Replacement {
    uint64_t hash;  // the renderer's texture pack hash of the game's picture
    uint32_t width, height;
    const unsigned char* png;
    const size_t* size;
    const char* what;
};

const Replacement kButtons[] = {
    {0x69fe95eb07becb13ull, 32, 32, art::kCross, &art::kCrossSize, "A button (as Cross)"},
    {0x70684cb98cd9458full, 32, 32, art::kCircle, &art::kCircleSize, "B button (as Circle)"},
    {0x584432de70b31dafull, 32, 32, art::kSquare, &art::kSquareSize, "X button (as Square)"},
    {0x6fdd27120e16aeaeull, 32, 32, art::kTriangle, &art::kTriangleSize, "Y button (as Triangle)"},
    {0xb7761a438c0f5dc4ull, 32, 32, art::kL1, &art::kL1Size, "White button (as L1)"},
    {0x8153bfe551cc80bfull, 32, 32, art::kR1, &art::kR1Size, "Black button (as R1)"},
    {0x415af65ddec342a2ull, 32, 32, art::kL2, &art::kL2Size, "left trigger (as L2)"},
    {0x065b6c5d280830ccull, 32, 32, art::kR2, &art::kR2Size, "right trigger (as R2)"},
    {0x174367013703b38bull, 128, 64, art::kSheet, &art::kSheetSize, "button sheet"},
    {0xa7f43da330a7ae6aull, 512, 256, art::kPad, &art::kPadSize, "controller picture"},
    {0x889644c267120065ull, 128, 128, art::kGlow, &art::kGlowSize, "controller picture's glow"},
};

bool Buttons(uint64_t hash, uint32_t width, uint32_t height, gpu::pack::Image& out) {
    for (const Replacement& r : kButtons) {
        if (r.hash != hash || r.width != width || r.height != height) continue;
        bool ok = gpu::pack::DecodePng(r.png, *r.size, out);
        LOGI("tsfp: the Xbox %s %s", r.what, ok ? "drawn as reSplitFP's PlayStation one" : "could not be replaced (PNG)");
        return ok;
    }
    return false;
}

}  // namespace

void PlayStationButtons() { gpu::pack::g_builtin = Buttons; }

}  // namespace tsfp
