#include "cooker.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

#include <stb_image.h>

namespace pino {
namespace {

class TextureCooker : public ICooker {
public:
    std::string extension() const override { return "png"; }

    bool accepts(const std::string& ext) const override {
        return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "ppm";
    }

    CookResult cook(const CookInput& input, BinaryChunkWriter& writer) override {
        int w = 0, h = 0, channels = 0;
        unsigned char* pixels = stbi_load(input.source_path.c_str(), &w, &h, &channels, 4);
        if (!pixels) {
            return {false, std::string("stb_image: ") + stbi_failure_reason()};
        }

        u32 width = static_cast<u32>(w);
        u32 height = static_cast<u32>(h);
        u32 total_bytes = width * height * 4;

        CookedTextureData tex;
        tex.width  = width;
        tex.height = height;
        tex.format = static_cast<u32>(CookedTextureFormat::RGBA8);
        tex.mip_count = 1;
        tex.mip_sizes.push_back(total_bytes);
        tex.mip_data.resize(total_bytes);
        std::memcpy(tex.mip_data.data(), pixels, total_bytes);

        stbi_image_free(pixels);

        write_cooked_texture(writer, CookedPlatform::Desktop, tex);
        return {true, ""};
    }
};

} // namespace

void register_texture_cooker(CookerRegistry& reg) {
    static TextureCooker cooker;
    reg.register_cooker(&cooker);
}

} // namespace pino
