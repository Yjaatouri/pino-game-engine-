#include "cooker.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace pino;

struct Options {
    fs::path input_dir;
    fs::path output_dir;
    bool     verbose = false;
};

static void print_usage() {
    printf("Usage: asset_cooker --input <dir> --output <dir> [--verbose]\n");
    printf("  Cooks raw assets from --input into .pino_cooked files in --output.\n");
    printf("  Supported: .obj .png .jpg .jpeg .ppm .vert (.vert+.frag pairs)\n");
}

static bool parse_args(int argc, char** argv, Options& opts) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--input") == 0 && i + 1 < argc) {
            opts.input_dir = argv[++i];
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            opts.output_dir = argv[++i];
        } else if (strcmp(argv[i], "--verbose") == 0) {
            opts.verbose = true;
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
    std::string rel_path;     // relative to input_dir, e.g. "models/cube.obj"
    std::string asset_name;   // stem, e.g. "cube"
    std::string identifier;   // without extension, e.g. "models/cube"
};

static std::vector<SourceAsset> scan_directory(const fs::path& root, CookerRegistry&) {
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

        // identifier = rel_path without extension
        auto id = rel_str;
        auto dot = id.rfind('.');
        if (dot != std::string::npos) id.resize(dot);
        asset.identifier = id;

        assets.push_back(asset);
    }
    return assets;
}

int main(int argc, char** argv) {
    Options opts;
    if (!parse_args(argc, argv, opts)) {
        return 1;
    }

    CookerRegistry reg;
    register_all_cookers(reg);

    auto assets = scan_directory(opts.input_dir, reg);
    if (assets.empty()) {
        printf("No supported assets found in '%s'\n", opts.input_dir.string().c_str());
        return 1;
    }

    // Create output directory
    fs::create_directories(opts.output_dir);

    int cooked_count = 0;
    int error_count = 0;
    int skipped_count = 0;

    printf("Cooking assets from '%s' -> '%s'\n\n",
           opts.input_dir.string().c_str(),
           opts.output_dir.string().c_str());

    for (const auto& asset : assets) {
        ICooker* cooker = reg.find(asset.ext);
        if (!cooker) {
            if (opts.verbose) {
                printf("  SKIP  %s  (no cooker for .%s)\n", asset.rel_path.c_str(), asset.ext.c_str());
            }
            ++skipped_count;
            continue;
        }

        // For shader cookers, skip .frag files (handled by .vert)
        if (cooker->extension() == "vert" && asset.ext == "frag") {
            continue;
        }

        if (opts.verbose) {
            printf("  COOK  %s\n", asset.rel_path.c_str());
        }

        CookInput input;
        input.source_path = asset.full_path.string();
        input.asset_name  = asset.asset_name;
        input.identifier  = asset.identifier;

        BinaryChunkWriter writer;
        auto result = cooker->cook(input, writer);

        if (!result.ok) {
            printf("  ERROR %s: %s\n", asset.rel_path.c_str(), result.error.c_str());
            ++error_count;
            continue;
        }

        // Write output: asset_identifier.pino_cooked
        // Replace directory separators with underscores to avoid creating subdirectories
        std::string out_name = asset.identifier;
        for (auto& c : out_name) {
            if (c == '/' || c == '\\') c = '_';
        }
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

        if (opts.verbose) {
            printf("  -> %s (%zu bytes)\n", out_path.filename().string().c_str(), buf.size());
        }
        ++cooked_count;
    }

    printf("\nResults: %d cooked, %d errors, %d skipped\n",
           cooked_count, error_count, skipped_count);

    // Write a simple manifest
    fs::path manifest_path = opts.output_dir / "asset_manifest.txt";
    std::ofstream mf(manifest_path);
    if (mf) {
        mf << "# Pino Game Engine - Cooked Asset Manifest\n";
        mf << "# Generated by asset_cooker\n\n";
        for (const auto& asset : assets) {
            ICooker* cooker = reg.find(asset.ext);
            if (!cooker) continue;
            if (cooker->extension() == "vert" && asset.ext == "frag") continue;
            std::string out_name = asset.identifier;
            for (auto& c : out_name) if (c == '/' || c == '\\') c = '_';
            mf << asset.identifier << " = " << out_name << ".pino_cooked\n";
        }
        mf.close();
    }

    return error_count > 0 ? 1 : 0;
}
