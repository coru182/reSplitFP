// reSplitFP: whether a disc's default.xbe is the release this build was recompiled from, when its bytes aren't the
// same as the build's: a media-patched dump (or a clean one, when the build was made from a patched one) differs in
// a few bytes of code, which the recompiled code doesn't run (it is the build's own) and the hooks cover either way.
//
// The build records its default.xbe's layout (the image header's boot fields and the section table, as
// xbe::SectionsHash takes them) and each section's bytes in 256-byte blocks, hashed (tools/xbe_blocks.cpp writes
// them). A disc's default.xbe is the same release when its layout is the same and at most kMaxCodeBlocks blocks
// differ, all in executable sections (nearly every section of an xbe is). A media patch changes one or two blocks;
// another release, printing or region moves or changes far more, or its layout.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace ps5::xbe_match {

constexpr uint32_t kBlock = 256;
constexpr uint32_t kMaxCodeBlocks = 8;
constexpr uint32_t kExecutable = 0x4;  // section flag

inline uint64_t Fnv(const uint8_t* p, size_t n, uint64_t h = 0xCBF29CE484222325ull) {
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 0x100000001B3ull;
    return h;
}

// the layout's hash, and each block's hash and whether it is code; false when it isn't a readable xbe
inline bool Blocks(const std::vector<uint8_t>& f, uint64_t& layout, std::vector<uint64_t>& hashes,
                   std::vector<uint8_t>& code) {
    auto u32 = [&](size_t at) -> uint32_t {
        uint32_t v = 0;
        if (at + 4 <= f.size()) std::memcpy(&v, f.data() + at, 4);
        return v;
    };
    if (f.size() < 0x180 || std::memcmp(f.data(), "XBEH", 4) != 0) return false;
    uint64_t h = 0xCBF29CE484222325ull;
    auto mix32 = [&](uint32_t v) { h = Fnv(reinterpret_cast<const uint8_t*>(&v), 4, h); };
    uint32_t base = u32(0x104);
    for (uint32_t at : {0x104u, 0x10Cu, 0x128u, 0x158u, 0x108u, 0x124u, 0x12Cu, 0x130u, 0x134u, 0x138u}) mix32(u32(at));
    uint32_t n = u32(0x11C), table = u32(0x120);
    if (table < base || n > 256) return false;
    hashes.clear();
    code.clear();
    for (uint32_t i = 0; i < n; i++) {
        size_t s = size_t(table - base) + size_t(i) * 0x38;
        uint32_t flags = u32(s), va = u32(s + 4), vsize = u32(s + 8), raw = u32(s + 12), rsize = u32(s + 16);
        for (uint32_t v : {flags, va, vsize, rsize}) mix32(v);
        if (size_t(raw) + rsize > f.size()) return false;
        for (uint32_t at = 0; at < rsize; at += kBlock) {
            hashes.push_back(Fnv(f.data() + raw + at, std::min(kBlock, rsize - at)));
            code.push_back((flags & kExecutable) != 0);
        }
    }
    layout = h;
    return true;
}

// the disc's default.xbe against the build's (tools/xbe_blocks.cpp): true for the same release; differ is how many
// blocks of code differ
inline bool SameRelease(const std::vector<uint8_t>& f, uint64_t layout, const uint64_t* hashes, uint32_t count,
                        uint32_t& differ) {
    uint64_t l = 0;
    std::vector<uint64_t> h;
    std::vector<uint8_t> code;
    differ = 0;
    if (!Blocks(f, l, h, code) || l != layout || h.size() != count) return false;
    for (uint32_t i = 0; i < count; i++) {
        if (h[i] == hashes[i]) continue;
        if (!code[i] || ++differ > kMaxCodeBlocks) return false;
    }
    return true;
}

}  // namespace ps5::xbe_match
