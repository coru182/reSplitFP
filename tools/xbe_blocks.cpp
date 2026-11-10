// Writes the release check's data for the app (ps5/xbe_match.h): the build's default.xbe's layout and its sections'
// 256-byte blocks, hashed, as a C++ source the app links. Built and run by build-ps5.sh on this machine.
// usage: xbe_blocks default.xbe out.cpp
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

#include "ps5/xbe_match.h"

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: xbe_blocks default.xbe out.cpp\n");
        return 2;
    }
    std::ifstream in(argv[1], std::ios::binary);
    std::vector<uint8_t> f((std::istreambuf_iterator<char>(in)), {});
    uint64_t layout = 0;
    std::vector<uint64_t> hashes;
    std::vector<uint8_t> code;
    if (!ps5::xbe_match::Blocks(f, layout, hashes, code)) {
        std::fprintf(stderr, "xbe_blocks: %s is not an xbe\n", argv[1]);
        return 1;
    }
    FILE* o = std::fopen(argv[2], "w");
    if (!o) return 1;
    std::fprintf(o, "// written by tools/xbe_blocks.cpp from the default.xbe this build was made from\n#include <cstdint>\n");
    std::fprintf(o, "namespace ps5 {\nextern const uint64_t kXbeLayout = 0x%016llXull;\n", (unsigned long long)layout);
    std::fprintf(o, "extern const uint32_t kXbeBlockCount = %zu;\nextern const uint64_t kXbeBlocks[] = {", hashes.size());
    for (size_t i = 0; i < hashes.size(); i++)
        std::fprintf(o, "%s0x%016llXull,", i % 4 ? " " : "\n    ", (unsigned long long)hashes[i]);
    std::fprintf(o, "\n};\n}  // namespace ps5\n");
    std::fclose(o);
    std::printf("xbe_blocks: %zu blocks, %zu of them code\n", hashes.size(), size_t(std::count(code.begin(), code.end(), 1)));
    return 0;
}
