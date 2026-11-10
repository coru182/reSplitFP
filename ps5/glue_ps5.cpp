// reSplitFP: the pieces of the Windows runtime the PS5 build leaves out, answered here
#include <memory>

#include "gpu/backend.h"

namespace gpu {
// xdk/d3d8.cpp falls back to Direct3D 12 when Vulkan can't start: there is none on a PS5, so the fallback is Vulkan
// again (it fails the same way and the device reports it)
std::unique_ptr<Backend> CreateD3D12Backend() { return CreateVulkanBackend(); }
}  // namespace gpu
