#include "cooker.h"
#include "engine/assets/asset_manifest.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;
using namespace pino;

struct Options {
    fs::path input_dir;
    fs::path output_dir;
    bool     verbose = false;
    CookedPlatform target = CookedPlatform::Desktop;
};

struct CookedEntry {
    std::string key;
    u32         type_id;
    u64         asset_hash;
    u32         file_size;
    std::vector<std::string> dependencies;
};

static void print_usage() {
    printf("Usage: asset_cooker --input <dir> --output <dir> [--verbose] [--target <platform>]\n");
    printf("  Cooks raw assets from --input into .pino_cooked files in --output.\n");
    printf("  Produces asset_manifest.bin for runtime loading.\n");
    printf("  --target: desktop (default), android, ios, any\n");
    printf("  Supported: .obj .png .jpg .jpeg .bmp .ppm .tga .vert (.vert+.frag pairs)\n");
}

static bool parse_args(int argc, char** argv, Options& opts) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--input") == 0 && i + 1 < argc) {
            opts.input_dir = argv[++i];
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            opts.output_dir = argv[++i];
        } else if (strcmp(argv[i], "--verbose") == 0) {
            opts.verbose = true;
        } else if (strcmp(argv[i], "--target") == 0 && i + 1 < argc) {
            std::string t = argv[++i];
            if (t == "desktop")       opts.target = CookedPlatform::Desktop;
            else if (t == "android")  opts.target = CookedPlatform::Android;
            else if (t == "ios")      opts.target = CookedPlatform::iOS;
            else if (t == "any")      opts.target = CookedPlatform::Any;
            else { printf("Unknown target: %s (desktop|android|ios|any)\n", t.c_str()); return false; }
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage();
            return false;
        } else {
            printf("Unknown option: %s\n\n", argv[i]);
            print_usage();
            return false;
        }
    }
    if (opts.input_dir.empty()) {
        printf("Error: --input is required\n\n");
        print_usage();
        return false;
    }
    if (opts.output_dir.empty()) {
        opts.output_dir = opts.input_dir;
    }
    return true;
}

static std::string ext_lower(const std::string& path) {
    auto pos = path.rfind('.');
    if (pos == std::string::npos) return "";
    std::string ext = path.substr(pos + 1);
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

struct SourceAsset {
    fs::path full_path;
    std::string ext;
    std::string rel_path;
    std::string asset_name;
    std::string identifier;
};

static std::vector<SourceAsset> scan_directory(const fs::path& root) {
    std::vector<SourceAsset> assets;
    if (!fs::exists(root)) {
        printf("Error: input directory '%s' does not exist\n", root.string().c_str());
        return assets;
    }

    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) continue;
        auto path = entry.path();
        std::string ext = ext_lower(path.string());
        if (ext.empty()) continue;

        auto rel = fs::relative(path, root);
        std::string rel_str = rel.string();
        for (auto& c : rel_str) if (c == '\\') c = '/';

        SourceAsset asset;
        asset.full_path   = path;
        asset.ext         = ext;
        asset.rel_path    = rel_str;
        asset.asset_name  = path.stem().string();

        auto id = rel_str;
        auto dot = id.rfind('.');
        if (dot != std::string::npos) id.resize(dot);
        asset.identifier = id;

        assets.push_back(asset);
    }
    return assets;
}

static std::string asset_path_to_key(const std::string& identifier, u32) {
    return identifier;
}

int main(int argc, char** argv) {
    Options opts;
    if (!parse_args(argc, argv, opts)) {
        return 1;
    }

    CookerRegistry reg;
    register_all_cookers(reg);

    auto assets = scan_directory(opts.input_dir);
    if (assets.empty()) {
        printf("No supported assets found in '%s'\n", opts.input_dir.string().c_str());
        return 1;
    }

    fs::create_directories(opts.output_dir);

    std::vector<CookedEntry> cooked_entries;
    int error_count = 0;
    int skipped_count = 0;

    printf("Cooking assets from '%s' -> '%s'\n\n",
           opts.input_dir.string().c_str(),
           opts.output_dir.string().c_str());

    // Sort assets by identifier for deterministic ordering
    std::sort(assets.begin(), assets.end(),
              [](const SourceAsset& a, const SourceAsset& b) {
                  return a.identifier < b.identifier;
              });

    for (const auto& asset : assets) {
        ICooker* cooker = reg.find(asset.ext);
        if (!cooker) {
            if (opts.verbose)
                printf("  SKIP  %s  (no cooker for .%s)\n", asset.rel_path.c_str(), asset.ext.c_str());
            ++skipped_count;
            continue;
        }

        // For shader cookers, skip .frag files (handled by .vert)
        if (cooker->extension() == "vert" && asset.ext == "frag") {
            continue;
        }

        if (opts.verbose)
            printf("  COOK  %s\n", asset.rel_path.c_str());

        CookInput input;
        input.source_path    = asset.full_path.string();
        input.asset_name     = asset.asset_name;
        input.identifier     = asset.identifier;
        input.target_platform = opts.target;

        BinaryChunkWriter writer;
        auto result = cooker->cook(input, writer);

        if (!result.ok) {
            printf("  ERROR %s: %s\n", asset.rel_path.c_str(), result.error.c_str());
            ++error_count;
            continue;
        }

        std::string asset_key = asset_path_to_key(asset.identifier, result.asset_type);
        std::string out_name = asset.identifier;
        for (auto& c : out_name) if (c == '/' || c == '\\') c = '_';
        fs::path out_path = opts.output_dir / (out_name + ".pino_cooked");

        const auto& buf = writer.getBuffer();
        std::ofstream out_file(out_path, std::ios::binary);
        if (!out_file) {
            printf("  ERROR %s: cannot write output\n", asset.rel_path.c_str());
            ++error_count;
            continue;
        }
        out_file.write(reinterpret_cast<const char*>(buf.data()), buf.size());
        out_file.close();

        u32 file_size = static_cast<u32>(buf.size());
        u64 asset_hash = cooked_hash_fnv1a(buf.data(), file_size);

        cooked_entries.push_back({asset_key, result.asset_type, asset_hash, file_size, result.dependencies});

        if (opts.verbose)
            printf("  -> %s (%u bytes)\n", out_path.filename().string().c_str(), file_size);
    }

    printf("\nResults: %zu cooked, %d errors, %d skipped\n",
           cooked_entries.size(), error_count, skipped_count);

    // ── Write binary manifest ─────────────────────────────────────
    AssetManifestData manifest;
    manifest.entries.reserve(cooked_entries.size());
    manifest.keys.reserve(cooked_entries.size());
    manifest.dependencies.reserve(cooked_entries.size());

    for (const auto& entry : cooked_entries) {
        AssetManifestEntry me;
        me.key_hash     = asset_key_hash(entry.key);
        me.type_id      = entry.type_id;
        me.file_offset  = 0; // loose file
        me.file_size    = entry.file_size;
        me.asset_hash   = entry.asset_hash;
        me.platform_tag = static_cast<u32>(opts.target);
        me.flags        = CAF_None;

        std::vector<u64> dep_hashes;
        dep_hashes.reserve(entry.dependencies.size());
        for (const auto& dep : entry.dependencies)
            dep_hashes.push_back(asset_key_hash(dep));

        me.dep_count = static_cast<u32>(dep_hashes.size());

        manifest.entries.push_back(me);
        manifest.keys.push_back(entry.key);
        manifest.dependencies.push_back(dep_hashes);
    }

    BinaryChunkWriter manifest_writer;
    write_asset_manifest(manifest_writer, manifest);

    fs::path manifest_path = opts.output_dir / "asset_manifest.bin";
    const auto& mbuf = manifest_writer.getBuffer();
    std::ofstream mf(manifest_path, std::ios::binary);
    if (mf) {
        mf.write(reinterpret_cast<const char*>(mbuf.data()), mbuf.size());
        mf.close();
        printf("Manifest: %s (%zu entries, %zu bytes)\n",
               manifest_path.filename().string().c_str(),
               manifest.entries.size(), mbuf.size());
    } else {
        printf("  ERROR: cannot write manifest\n");
        ++error_count;
    }

    return error_count > 0 ? 1 : 0;
}
