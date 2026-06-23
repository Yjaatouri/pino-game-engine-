#include "cooker.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>

namespace pino {
namespace {

static std::string replace_extension(const std::string& path, const std::string& new_ext) {
    auto pos = path.rfind('.');
    if (pos == std::string::npos) return path + "." + new_ext;
    return path.substr(0, pos + 1) + new_ext;
}

static bool read_file(const std::string& path, std::vector<u8>& out) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;
    usize size = static_cast<usize>(file.tellg());
    file.seekg(0);
    out.resize(size);
    file.read(reinterpret_cast<char*>(out.data()), size);
    return true;
}

class ShaderCooker : public ICooker {
public:
    std::string extension() const override { return "vert"; }
    u32 asset_type() const override { return CookedType::Shader; }

    CookResult cook(const CookInput& input, BinaryChunkWriter& writer) override {
        // Find matching .frag file
        std::string frag_path = replace_extension(input.source_path, "frag");

        std::vector<u8> vert_data;
        std::vector<u8> frag_data;

        if (!read_file(input.source_path, vert_data)) {
            return {false, "Cannot read " + input.source_path};
        }
        if (!read_file(frag_path, frag_data)) {
            return {false, "Cannot find matching frag: " + frag_path};
        }

        CookedShaderData shader;
        shader.identifier = input.identifier;
        shader.vert_stage = std::move(vert_data);
        shader.frag_stage = std::move(frag_data);

        write_cooked_shader(writer, CookedPlatform::Desktop, shader);
        return {true, "", CookedType::Shader, {}};
    }
};

} // namespace

void register_shader_cooker(CookerRegistry& reg) {
    static ShaderCooker cooker;
    reg.register_cooker(&cooker);
}

} // namespace pino
