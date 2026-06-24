#include "cooker.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cmath>

#include <stb_image.h>

namespace pino {
namespace {

// ── Mipmap generation (box-filter downscale) ─────────────────────
static std::vector<u8> half_rgba(const u8* src, u32 sw, u32 sh) {
    u32 dw = std::max(sw / 2, 1u);
    u32 dh = std::max(sh / 2, 1u);
    std::vector<u8> dst(dw * dh * 4);

    for (u32 dy = 0; dy < dh; ++dy) {
        for (u32 dx = 0; dx < dw; ++dx) {
            u32 sy = dy * 2;
            u32 sx = dx * 2;
            u32 r = 0, g = 0, b = 0, a = 0, n = 0;

            for (u32 oy = 0; oy < 2 && sy + oy < sh; ++oy) {
                for (u32 ox = 0; ox < 2 && sx + ox < sw; ++ox) {
                    const u8* p = src + ((sy + oy) * sw + (sx + ox)) * 4;
                    r += p[0]; g += p[1]; b += p[2]; a += p[3];
                    ++n;
                }
            }

            u8* d = dst.data() + (dy * dw + dx) * 4;
            d[0] = static_cast<u8>((r + n / 2) / n);
            d[1] = static_cast<u8>((g + n / 2) / n);
            d[2] = static_cast<u8>((b + n / 2) / n);
            d[3] = static_cast<u8>((a + n / 2) / n);
        }
    }
    return dst;
}

struct MipChain {
    u32 base_width;
    u32 base_height;
    std::vector<std::vector<u8>> levels;
};

static MipChain generate_mips(const u8* rgba, u32 w, u32 h) {
    MipChain chain;
    chain.base_width = w;
    chain.base_height = h;
    chain.levels.push_back(std::vector<u8>(rgba, rgba + w * h * 4));

    u32 cw = w, ch = h;
    while (cw > 1 || ch > 1) {
        auto next = half_rgba(chain.levels.back().data(), cw, ch);
        cw = std::max(cw / 2, 1u);
        ch = std::max(ch / 2, 1u);
        chain.levels.push_back(std::move(next));
    }
    return chain;
}

// ── BC1 (DXT1) Block Compression ─────────────────────────────────

static u16 rgb_to_565(u8 r, u8 g, u8 b) {
    return static_cast<u16>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static void lerp_rgb(u8 r0, u8 g0, u8 b0, u8 r1, u8 g1, u8 b1,
                     u8& r, u8& g, u8& b, u32 num, u32 den) {
    r = static_cast<u8>((r0 * (den - num) + r1 * num) / den);
    g = static_cast<u8>((g0 * (den - num) + g1 * num) / den);
    b = static_cast<u8>((b0 * (den - num) + b1 * num) / den);
}

static void compress_bc1_block(const u8 block[4][4][4], u8 output[8]) {
    u32 min_r = 255, min_g = 255, min_b = 255;
    u32 max_r = 0,   max_g = 0,   max_b = 0;
    bool has_alpha = false;

    for (u32 y = 0; y < 4; ++y) {
        for (u32 x = 0; x < 4; ++x) {
            const u8* c = block[y][x];
            if (c[3] < 128) has_alpha = true;
            min_r = std::min(min_r, static_cast<u32>(c[0]));
            min_g = std::min(min_g, static_cast<u32>(c[1]));
            min_b = std::min(min_b, static_cast<u32>(c[2]));
            max_r = std::max(max_r, static_cast<u32>(c[0]));
            max_g = std::max(max_g, static_cast<u32>(c[1]));
            max_b = std::max(max_b, static_cast<u32>(c[2]));
        }
    }

    u16 c0 = rgb_to_565(static_cast<u8>(max_r), static_cast<u8>(max_g), static_cast<u8>(max_b));
    u16 c1 = rgb_to_565(static_cast<u8>(min_r), static_cast<u8>(min_g), static_cast<u8>(min_b));

    if (has_alpha && c0 <= c1) {
        std::swap(c0, c1);
        u8 col[3][3];
        col[0][0] = static_cast<u8>((c0 >> 11) << 3);
        col[0][1] = static_cast<u8>(((c0 >> 5) & 0x3F) << 2);
        col[0][2] = static_cast<u8>((c0 & 0x1F) << 3);
        col[1][0] = static_cast<u8>((c1 >> 11) << 3);
        col[1][1] = static_cast<u8>(((c1 >> 5) & 0x3F) << 2);
        col[1][2] = static_cast<u8>((c1 & 0x1F) << 3);
        col[2][0] = (col[0][0] + col[1][0]) / 2;
        col[2][1] = (col[0][1] + col[1][1]) / 2;
        col[2][2] = (col[0][2] + col[1][2]) / 2;

        u32 indices = 0;
        for (i32 y = 3; y >= 0; --y)
            for (i32 x = 3; x >= 0; --x) {
                const u8* c = block[y][x];
                if (c[3] < 128) { indices = (indices << 2) | 3; continue; }
                u32 best = 0;
                u32 best_dist = UINT32_MAX;
                for (u32 k = 0; k < 3; ++k) {
                    i32 dr = static_cast<i32>(c[0]) - static_cast<i32>(col[k][0]);
                    i32 dg = static_cast<i32>(c[1]) - static_cast<i32>(col[k][1]);
                    i32 db = static_cast<i32>(c[2]) - static_cast<i32>(col[k][2]);
                    u32 dist = static_cast<u32>(dr * dr + dg * dg + db * db);
                    if (dist < best_dist) { best_dist = dist; best = k; }
                }
                indices = (indices << 2) | best;
            }

        std::memcpy(output, &c0, 2);
        std::memcpy(output + 2, &c1, 2);
        output[4] = static_cast<u8>((indices >> 24) & 0xFF);
        output[5] = static_cast<u8>((indices >> 16) & 0xFF);
        output[6] = static_cast<u8>((indices >> 8) & 0xFF);
        output[7] = static_cast<u8>(indices & 0xFF);
        return;
    }

    if (c0 < c1) std::swap(c0, c1);

    u8 col[4][3];
    col[0][0] = static_cast<u8>((c0 >> 11) << 3);
    col[0][1] = static_cast<u8>(((c0 >> 5) & 0x3F) << 2);
    col[0][2] = static_cast<u8>((c0 & 0x1F) << 3);
    col[1][0] = static_cast<u8>((c1 >> 11) << 3);
    col[1][1] = static_cast<u8>(((c1 >> 5) & 0x3F) << 2);
    col[1][2] = static_cast<u8>((c1 & 0x1F) << 3);
    lerp_rgb(col[0][0], col[0][1], col[0][2], col[1][0], col[1][1], col[1][2],
             col[2][0], col[2][1], col[2][2], 1, 3);
    lerp_rgb(col[0][0], col[0][1], col[0][2], col[1][0], col[1][1], col[1][2],
             col[3][0], col[3][1], col[3][2], 2, 3);

    u32 indices = 0;
    for (i32 y = 3; y >= 0; --y)
        for (i32 x = 3; x >= 0; --x) {
            const u8* c = block[y][x];
            u32 best = 0;
            u32 best_dist = UINT32_MAX;
            for (u32 k = 0; k < 4; ++k) {
                i32 dr = static_cast<i32>(c[0]) - static_cast<i32>(col[k][0]);
                i32 dg = static_cast<i32>(c[1]) - static_cast<i32>(col[k][1]);
                i32 db = static_cast<i32>(c[2]) - static_cast<i32>(col[k][2]);
                u32 dist = static_cast<u32>(dr * dr + dg * dg + db * db);
                if (dist < best_dist) { best_dist = dist; best = k; }
            }
            indices = (indices << 2) | best;
        }

    std::memcpy(output, &c0, 2);
    std::memcpy(output + 2, &c1, 2);
    output[4] = static_cast<u8>((indices >> 24) & 0xFF);
    output[5] = static_cast<u8>((indices >> 16) & 0xFF);
    output[6] = static_cast<u8>((indices >> 8) & 0xFF);
    output[7] = static_cast<u8>(indices & 0xFF);
}

static std::vector<u8> compress_bc1(const u8* rgba, u32 w, u32 h) {
    u32 bw = (w + 3) / 4;
    u32 bh = (h + 3) / 4;
    std::vector<u8> result(bw * bh * 8, 0);

    for (u32 by = 0; by < bh; ++by)
        for (u32 bx = 0; bx < bw; ++bx) {
            u8 block[4][4][4] = {};
            for (u32 y = 0; y < 4; ++y)
                for (u32 x = 0; x < 4; ++x) {
                    u32 py = by * 4 + y;
                    u32 px = bx * 4 + x;
                    const u8* src = rgba + (py * w + px) * 4;
                    block[y][x][0] = (py < h && px < w) ? src[0] : 0;
                    block[y][x][1] = (py < h && px < w) ? src[1] : 0;
                    block[y][x][2] = (py < h && px < w) ? src[2] : 0;
                    block[y][x][3] = (py < h && px < w) ? src[3] : 0;
                }
            compress_bc1_block(block, result.data() + (by * bw + bx) * 8);
        }
    return result;
}

// ── BC3 (DXT5) Block Compression ─────────────────────────────────

static void compress_alpha_block(const u8 block[16], u8 output[8]) {
    u8 min_a = 255, max_a = 0;
    for (u32 i = 0; i < 16; ++i) {
        min_a = std::min(min_a, block[i]);
        max_a = std::max(max_a, block[i]);
    }

    output[0] = max_a;
    output[1] = min_a;

    u8 alphas[8];
    alphas[0] = max_a;
    alphas[1] = min_a;
    if (max_a > min_a) {
        for (u32 i = 0; i < 6; ++i)
            alphas[2 + i] = static_cast<u8>(((6 - i) * max_a + (i + 1) * min_a) / 7);
    } else {
        for (u32 i = 0; i < 6; ++i)
            alphas[2 + i] = static_cast<u8>(((5 - i) * max_a + (i + 1) * min_a) / 6);
    }

    u64 bits = 0;
    for (i32 i = 15; i >= 0; --i) {
        u32 best = 0;
        u32 best_dist = UINT32_MAX;
        for (u32 k = 0; k < 8; ++k) {
            u32 dist = static_cast<u32>(std::abs(static_cast<i32>(block[i]) - static_cast<i32>(alphas[k])));
            if (dist < best_dist) { best_dist = dist; best = k; }
        }
        bits = (bits << 3) | best;
    }

    for (u32 i = 0; i < 6; ++i)
        output[2 + i] = static_cast<u8>((bits >> (40 - i * 8)) & 0xFF);
}

static std::vector<u8> compress_bc3(const u8* rgba, u32 w, u32 h) {
    u32 bw = (w + 3) / 4;
    u32 bh = (h + 3) / 4;
    std::vector<u8> result(bw * bh * 16, 0);

    for (u32 by = 0; by < bh; ++by)
        for (u32 bx = 0; bx < bw; ++bx) {
            u8 alpha_block[16];
            u8 color_block[4][4][4];

            for (u32 y = 0; y < 4; ++y)
                for (u32 x = 0; x < 4; ++x) {
                    u32 py = by * 4 + y;
                    u32 px = bx * 4 + x;
                    if (py < h && px < w) {
                        const u8* src = rgba + (py * w + px) * 4;
                        alpha_block[y * 4 + x] = src[3];
                        color_block[y][x][0] = src[0];
                        color_block[y][x][1] = src[1];
                        color_block[y][x][2] = src[2];
                        color_block[y][x][3] = 255;
                    } else {
                        alpha_block[y * 4 + x] = 0;
                        color_block[y][x][0] = 0;
                        color_block[y][x][1] = 0;
                        color_block[y][x][2] = 0;
                        color_block[y][x][3] = 255;
                    }
                }

            u8* out = result.data() + (by * bw + bx) * 16;
            compress_alpha_block(alpha_block, out);
            compress_bc1_block(color_block, out + 8);
        }
    return result;
}

// ── ETC2 RGB Block Compression ───────────────────────────────────

static void compress_etc2_block(const u8 block[4][4][4], u8 output[8]) {
    u32 ar = 0, ag = 0, ab = 0;
    for (u32 y = 0; y < 4; ++y)
        for (u32 x = 0; x < 4; ++x) {
            ar += block[y][x][0];
            ag += block[y][x][1];
            ab += block[y][x][2];
        }
    ar /= 16; ag /= 16; ab /= 16;

    u32 avg_r[2] = {0, 0}, avg_g[2] = {0, 0}, avg_b[2] = {0, 0};
    for (u32 h = 0; h < 2; ++h)
        for (u32 y = 0; y < 4; ++y)
            for (u32 x = h * 2; x < h * 2 + 2; ++x) {
                avg_r[h] += block[y][x][0];
                avg_g[h] += block[y][x][1];
                avg_b[h] += block[y][x][2];
            }
    avg_r[0] /= 8; avg_g[0] /= 8; avg_b[0] /= 8;
    avg_r[1] /= 8; avg_g[1] /= 8; avg_b[1] /= 8;

    static const i32 mod_table[8][2] = {
        {2, 5}, {3, 7}, {4, 9}, {5, 11},
        {6, 13}, {8, 17}, {10, 21}, {14, 29}
    };

    u32 base0_r = avg_r[0] / 17; if (base0_r > 15) base0_r = 15;
    u32 base0_g = avg_g[0] / 17; if (base0_g > 15) base0_g = 15;
    u32 base0_b = avg_b[0] / 17; if (base0_b > 15) base0_b = 15;
    u32 base1_r = avg_r[1] / 17; if (base1_r > 15) base1_r = 15;
    u32 base1_g = avg_g[1] / 17; if (base1_g > 15) base1_g = 15;
    u32 base1_b = avg_b[1] / 17; if (base1_b > 15) base1_b = 15;

    u8 table_idx = 0;
    i32 mod_val[2][2] = {{0, 0}, {0, 0}};

    for (u32 h = 0; h < 2; ++h) {
        i32 diff_r = static_cast<i32>(avg_r[h]) - static_cast<i32>(ar);
        i32 diff_g = static_cast<i32>(avg_g[h]) - static_cast<i32>(ag);
        i32 diff_b = static_cast<i32>(avg_b[h]) - static_cast<i32>(ab);
        i32 best_err = INT32_MAX;

        for (i32 m = 0; m < 2; ++m)
            for (i32 t = 0; t < 8; ++t) {
                i32 val = mod_table[t][m];
                i32 er = diff_r - (m == 0 ? val : -val);
                i32 eg = diff_g - (m == 0 ? val : -val);
                i32 eb = diff_b - (m == 0 ? val : -val);
                i32 err = er * er + eg * eg + eb * eb;
                if (err < best_err) {
                    best_err = err;
                    mod_val[h][0] = mod_table[t][m];
                    mod_val[h][1] = -mod_table[t][m];
                    table_idx = static_cast<u8>(t);
                }
            }
    }

    u64 bits = 0;
    bits |= static_cast<u64>(base0_r);
    bits |= static_cast<u64>(base0_g) << 4;
    bits |= static_cast<u64>(base0_b) << 8;
    bits |= static_cast<u64>(base1_r) << 12;
    bits |= static_cast<u64>(base1_g) << 16;
    bits |= static_cast<u64>(base1_b) << 20;
    bits |= static_cast<u64>(table_idx) << 24;

    u32 pixel_bits = 0;
    for (i32 y = 3; y >= 0; --y)
        for (i32 x = 3; x >= 0; --x) {
            u32 h = (x < 2) ? 0 : 1;
            const u8* c = block[y][x];
            i32 lum = static_cast<i32>(c[0]) + static_cast<i32>(c[1]) + static_cast<i32>(c[2]);
            i32 base_lum = static_cast<i32>(avg_r[h]) + static_cast<i32>(avg_g[h]) + static_cast<i32>(avg_b[h]);
            i32 d0 = std::abs(lum - (base_lum + mod_val[h][0]));
            i32 d1 = std::abs(lum - (base_lum + mod_val[h][1]));
            pixel_bits = (pixel_bits << 1) | static_cast<u32>(d1 < d0 ? 1 : 0);
        }
    bits |= (static_cast<u64>(pixel_bits) << 32);

    for (u32 i = 0; i < 8; ++i)
        output[i] = static_cast<u8>((bits >> (i * 8)) & 0xFF);
}

static std::vector<u8> compress_etc2_rgb(const u8* rgba, u32 w, u32 h) {
    u32 bw = (w + 3) / 4;
    u32 bh = (h + 3) / 4;
    std::vector<u8> result(bw * bh * 8, 0);

    for (u32 by = 0; by < bh; ++by)
        for (u32 bx = 0; bx < bw; ++bx) {
            u8 block[4][4][4] = {};
            for (u32 y = 0; y < 4; ++y)
                for (u32 x = 0; x < 4; ++x) {
                    u32 py = by * 4 + y;
                    u32 px = bx * 4 + x;
                    if (py < h && px < w) {
                        const u8* src = rgba + (py * w + px) * 4;
                        std::memcpy(block[y][x], src, 4);
                    }
                }
            compress_etc2_block(block, result.data() + (by * bw + bx) * 8);
        }
    return result;
}

// ── Format dispatch ──────────────────────────────────────────────
static std::vector<u8> compress_to_format(const u8* rgba, u32 w, u32 h,
                                           CookedTextureFormat fmt) {
    switch (fmt) {
        case CookedTextureFormat::BC1:    return compress_bc1(rgba, w, h);
        case CookedTextureFormat::BC3:    return compress_bc3(rgba, w, h);
        case CookedTextureFormat::ETC2_RGB: return compress_etc2_rgb(rgba, w, h);
        default: return {};
    }
}

// ── Platform-format mapping ──────────────────────────────────────
static CookedTextureFormat format_for_platform(CookedPlatform platform, bool has_alpha) {
    switch (platform) {
        case CookedPlatform::Android:
            return has_alpha ? CookedTextureFormat::ETC2_RGBA
                             : CookedTextureFormat::ETC2_RGB;
        case CookedPlatform::Desktop:
            return has_alpha ? CookedTextureFormat::BC3
                             : CookedTextureFormat::BC1;
        default:
            return CookedTextureFormat::RGBA8;
    }
}

// ── TextureCooker implementation ─────────────────────────────────
class TextureCooker : public ICooker {
public:
    std::string extension() const override { return "png"; }
    u32 asset_type() const override { return CookedType::Texture; }

    bool accepts(const std::string& ext) const override {
        return ext == "png" || ext == "jpg" || ext == "jpeg"
            || ext == "bmp" || ext == "ppm" || ext == "tga";
    }

    CookResult cook(const CookInput& input, BinaryChunkWriter& writer) override {
        int w = 0, h = 0, channels = 0;
        unsigned char* pixels = stbi_load(input.source_path.c_str(), &w, &h, &channels, 4);
        if (!pixels)
            return {false, std::string("stb_image: ") + stbi_failure_reason()};

        u32 width  = static_cast<u32>(w);
        u32 height = static_cast<u32>(h);

        // Detect alpha
        bool has_alpha = false;
        for (u32 i = 0; i < width * height; ++i) {
            if (pixels[i * 4 + 3] < 255) { has_alpha = true; break; }
        }

        // Generate CPU-side mip chain (box-filter)
        auto mips = generate_mips(pixels, width, height);
        stbi_image_free(pixels);

        // Select format for target platform
        auto fmt = format_for_platform(input.target_platform, has_alpha);

        // Build CookedTextureData
        CookedTextureData tex;
        tex.width     = width;
        tex.height    = height;
        tex.format    = static_cast<u32>(fmt);
        tex.mip_count = static_cast<u32>(mips.levels.size());

        tex.mip_sizes.reserve(tex.mip_count);
        u32 total_data = 0;

        for (u32 i = 0; i < tex.mip_count; ++i) {
            u32 mw = std::max(width >> i, 1u);
            u32 mh = std::max(height >> i, 1u);

            if (fmt == CookedTextureFormat::RGBA8) {
                u32 sz = mw * mh * 4;
                tex.mip_sizes.push_back(sz);
                total_data += sz;
                tex.mip_data.insert(tex.mip_data.end(),
                                    mips.levels[i].begin(), mips.levels[i].end());
            } else {
                auto compressed = compress_to_format(mips.levels[i].data(), mw, mh, fmt);
                u32 sz = static_cast<u32>(compressed.size());
                tex.mip_sizes.push_back(sz);
                total_data += sz;
                tex.mip_data.insert(tex.mip_data.end(),
                                    compressed.begin(), compressed.end());
            }
        }

        tex.mip_data.shrink_to_fit();

        write_cooked_texture(writer, input.target_platform, tex);
        return {true, "", CookedType::Texture, {}};
    }
};

} // namespace

void register_texture_cooker(CookerRegistry& reg) {
    static TextureCooker cooker;
    reg.register_cooker(&cooker);
}

} // namespace pino
