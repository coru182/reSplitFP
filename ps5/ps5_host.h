// reSplitFP: what the PS5 host's parts share
#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace ps5 {

// /data/tsfp: settings, saves, logs, caches, the game's files, and where the player's disc image goes
const std::filesystem::path& DataDir();

// a system notification (the bubble in the corner), for errors there is no screen for
void Notify(const char* text);

// the player's copy of the game: its files (a folder holding default.xbe), or a disc image (.iso) played directly
struct Game {
    std::filesystem::path path;  // the folder holding default.xbe, or the .iso
    bool image = false;
};
// /data/tsfp/game: where the first start unpacks the disc image
std::filesystem::path GameDir();
// the game's files: /data/tsfp/game, then /mnt/usb0..7/tsfp/game; the first one found
bool FindGame(Game& game);
// the disc images to unpack: /data/tsfp/*.iso, then /mnt/usbN/tsfp/*.iso and /mnt/usbN/*.iso
std::vector<std::filesystem::path> FindImages();
// name in dir, in whatever case the disc has it; empty when there is none
std::filesystem::path Child(const std::filesystem::path& dir, const char* name);
// whether dir holds the game's files: default.xbe and its Data folder
bool Unpacked(const std::filesystem::path& dir);

}  // namespace ps5
