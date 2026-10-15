#pragma once
#include "gltf_writer.h"
#include <cstdint>
#include <vector>

// A still of a model for the MO2 export tool's preview (--export-preview):
// textured, lit from the upper left, from a fixed three-quarter view in front
// of +x (where characters face) that fits the model into the frame. CPU only;
// skinned meshes are drawn in their bind pose.
namespace modding::model_preview
{
struct Texture
{
    uint32_t width = 0, height = 0, channels = 0; // 1 grey, 3 RGB or 4 RGBA; 0 = untextured
    const uint8_t *pixels = nullptr;
    bool mask = false; // cut-out alpha: texels below 128 are holes
};
struct Image
{
    uint32_t width = 0, height = 0;
    std::vector<uint8_t> rgb;
};
// textures: one per mesh.materials entry (missing entries draw untextured).
Image Render(const gltf::Mesh &mesh, const std::vector<Texture> &textures, uint32_t size = 512);
}
