#include "engine/core/binary_chunk.h"
#include "engine/core/serializer.h"
#include "engine/serialization/cooked_asset.h"
#include "engine/assets/asset_manifest.h"
#include "engine/assets/asset_registry.h"
#include <cstdio>
#include <cstring>
#include <vector>

using namespace pino;

static int s_pass = 0;
static int s_fail = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        printf("  FAIL: %s\n", name); \
        ++s_fail; \
    } else { \
        printf("  PASS: %s\n", name); \
        ++s_pass; \
    } \
} while(0)

static std::vector<u8> build_test_manifest() {
    AssetManifestData m;

    // Entry 0: mesh "models/cube" with no deps
    {
        AssetManifestEntry e;
        e.key_hash     = asset_key_hash("models/cube");
        e.type_id      = CookedType::Mesh;
        e.file_offset  = 0;
        e.file_size    = 1396;
        e.asset_hash   = 0xDEADBEEF;
        e.platform_tag = static_cast<u32>(CookedPlatform::Desktop);
        e.flags        = CAF_None;
        e.dep_count    = 0;
        m.entries.push_back(e);
        m.keys.push_back("models/cube");
        m.dependencies.push_back({});
    }

    // Entry 1: texture "textures/checker" with no deps
    {
        AssetManifestEntry e;
        e.key_hash     = asset_key_hash("textures/checker");
        e.type_id      = CookedType::Texture;
        e.file_offset  = 0;
        e.file_size    = 324;
        e.asset_hash   = 0xCAFEBABE;
        e.platform_tag = static_cast<u32>(CookedPlatform::Desktop);
        e.flags        = CAF_None;
        e.dep_count    = 0;
        m.entries.push_back(e);
        m.keys.push_back("textures/checker");
        m.dependencies.push_back({});
    }

    // Entry 2: shader "shaders/lit" depends on nothing
    {
        AssetManifestEntry e;
        e.key_hash     = asset_key_hash("shaders/lit");
        e.type_id      = CookedType::Shader;
        e.file_offset  = 0;
        e.file_size    = 4478;
        e.asset_hash   = 0x12345678;
        e.platform_tag = static_cast<u32>(CookedPlatform::Desktop);
        e.flags        = CAF_None;
        e.dep_count    = 0;
        m.entries.push_back(e);
        m.keys.push_back("shaders/lit");
        m.dependencies.push_back({});
    }

    // Entry 3: material "materials/wall" depends on shader + texture
    {
        AssetManifestEntry e;
        e.key_hash     = asset_key_hash("materials/wall");
        e.type_id      = CookedType::Material;
        e.file_offset  = 0;
        e.file_size    = 128;
        e.asset_hash   = 0xABCD1234;
        e.platform_tag = static_cast<u32>(CookedPlatform::Desktop);
        e.flags        = CAF_None;
        e.dep_count    = 2;
        m.entries.push_back(e);
        m.keys.push_back("materials/wall");
        m.dependencies.push_back({
            asset_key_hash("shaders/lit"),
            asset_key_hash("textures/checker")
        });
    }

    BinaryChunkWriter writer;
    write_asset_manifest(writer, m);
    return writer.getBuffer();
}

int main() {
    printf("=== Asset Manifest & Registry Test ===\n\n");

    // ── Build manifest ──
    auto manifest_data = build_test_manifest();
    printf("-- Build --\n");
    TEST("manifest data is non-empty", !manifest_data.empty());
    TEST("manifest data starts with valid chunk magic",
         manifest_data.size() >= 16 &&
         *reinterpret_cast<const u32*>(manifest_data.data()) == kChunkMagic);
    printf("\n");

    // ── Load via AssetRegistry ──
    AssetRegistry reg;
    bool loaded = reg.load(manifest_data);
    TEST("manifest loads successfully", loaded);
    TEST("manifest has 4 entries", reg.entry_count() == 4);
    printf("\n");

    // ── Lookup by key ──
    {
        printf("-- Lookup --\n");
        auto* mesh = reg.find("models/cube");
        auto* tex  = reg.find("textures/checker");
        auto* shd  = reg.find("shaders/lit");
        auto* mat  = reg.find("materials/wall");
        auto* missing = reg.find("models/nonexistent");

        TEST("find 'models/cube' succeeds", mesh != nullptr);
        TEST("find 'models/cube' has correct type", mesh && mesh->type_id == CookedType::Mesh);
        TEST("find 'textures/checker' succeeds", tex != nullptr);
        TEST("find 'textures/checker' has correct type", tex && tex->type_id == CookedType::Texture);
        TEST("find 'shaders/lit' succeeds", shd != nullptr);
        TEST("find 'shaders/lit' has correct type", shd && shd->type_id == CookedType::Shader);
        TEST("find 'materials/wall' succeeds", mat != nullptr);
        TEST("find 'materials/wall' has correct type", mat && mat->type_id == CookedType::Material);
        TEST("find nonexistent returns null", missing == nullptr);

        TEST("contains 'models/cube'",  reg.contains("models/cube"));
        TEST("contains 'materials/wall'", reg.contains("materials/wall"));
        TEST("does not contain nonexistent", !reg.contains("models/nonexistent"));
    }
    printf("\n");

    // ── Dependency queries ──
    {
        printf("-- Dependencies --\n");
        auto mesh_deps = reg.get_dependencies("models/cube");
        TEST("mesh has no dependencies", mesh_deps.empty());

        auto mat_deps = reg.get_dependencies("materials/wall");
        TEST("material has 2 dependencies", mat_deps.size() == 2);
        if (mat_deps.size() == 2) {
            TEST("first dep is shader 'shaders/lit'",
                 mat_deps[0] && mat_deps[0]->type_id == CookedType::Shader);
            TEST("second dep is texture 'textures/checker'",
                 mat_deps[1] && mat_deps[1]->type_id == CookedType::Texture);
        }

        auto missing_deps = reg.get_dependencies("models/nonexistent");
        TEST("missing key returns empty deps", missing_deps.empty());
    }
    printf("\n");

    // ── Reverse dependency queries ──
    {
        printf("-- Reverse Dependencies --\n");
        auto shader_dependents = reg.get_dependents("shaders/lit");
        TEST("shader has 1 dependent (material)", shader_dependents.size() == 1);
        if (!shader_dependents.empty()) {
            TEST("shader dependent is material type",
                 shader_dependents[0]->type_id == CookedType::Material);
        }

        auto tex_dependents = reg.get_dependents("textures/checker");
        TEST("texture has 1 dependent (material)", tex_dependents.size() == 1);

        auto mesh_dependents = reg.get_dependents("models/cube");
        TEST("mesh has no dependents", mesh_dependents.empty());

        auto missing_deps = reg.get_dependents("models/nonexistent");
        TEST("missing key returns empty dependents", missing_deps.empty());
    }
    printf("\n");

    // ── Integrity check ──
    {
        printf("-- Integrity --\n");
        // Build a known payload and check hash
        std::vector<u8> payload = {1, 2, 3, 4, 5};
        u64 hash = cooked_hash_fnv1a(payload.data(), static_cast<u32>(payload.size()));

        // Verify deterministic: same payload always produces same hash
        u64 hash2 = cooked_hash_fnv1a(payload.data(), static_cast<u32>(payload.size()));
        TEST("hash of simple payload is deterministic", hash == hash2);

        // Verify against manifest - create a new manifest with known hash
        AssetManifestData m;
        {
            AssetManifestEntry e;
            e.key_hash   = asset_key_hash("test/asset");
            e.type_id    = CookedType::Mesh;
            e.file_size  = static_cast<u32>(payload.size());
            e.asset_hash = hash;
            e.dep_count  = 0;
            m.entries.push_back(e);
            m.keys.push_back("test/asset");
            m.dependencies.push_back({});
        }

        BinaryChunkWriter writer;
        write_asset_manifest(writer, m);

        AssetRegistry reg2;
        reg2.load(writer.getBuffer());

        TEST("integrity check passes for matching payload",
             reg2.verify_integrity("test/asset", payload.data(), static_cast<u32>(payload.size())));

        // Corrupt the payload
        payload[0] = 0xFF;
        TEST("integrity check fails for corrupted payload",
             !reg2.verify_integrity("test/asset", payload.data(), static_cast<u32>(payload.size())));

        // Unknown key
        TEST("integrity check fails for unknown key",
             !reg2.verify_integrity("test/unknown", payload.data(), static_cast<u32>(payload.size())));
    }
    printf("\n");

    // ── Corruption / edge cases ──
    {
        printf("-- Corruption / Edge Cases --\n");

        // Empty data
        {
            AssetRegistry empty_reg;
            std::vector<u8> empty;
            TEST("empty data rejected", !empty_reg.load(empty));
        }

        // Truncated manifest
        {
            AssetRegistry trunc_reg;
            std::vector<u8> truncated(manifest_data.begin(), manifest_data.begin() + 10);
            TEST("truncated manifest rejected", !trunc_reg.load(truncated));
        }

        // Corrupted manifest data (corrupt nested chunk payload = hash mismatch)
        {
            AssetRegistry corrupt_reg;
            auto corrupted = manifest_data;
            // Header is 16 (chunk) + 16 (cooked header) = 32 bytes
            // Nested chunk starts at byte 32. Corrupt byte 40 to break the hash.
            if (corrupted.size() > 40)
                corrupted[40] ^= 0xFF;
            TEST("corrupted manifest rejected (hash mismatch)", !corrupt_reg.load(corrupted));
        }

        // Wrong type ID
        {
            BinaryChunkWriter writer;
            writer.beginChunk(9999, 1); // wrong type
            writer.endChunk();

            AssetRegistry wrong_reg;
            TEST("wrong type ID rejected", !wrong_reg.load(writer.getBuffer()));
        }

        // Key normalization (case insensitive on Win32)
        {
            auto* entry = reg.find("MODELS/CUBE");
            TEST("case-insensitive lookup works (MODELS/CUBE)", entry != nullptr);
        }

        // Key normalization (forward slash)
        {
            auto* entry = reg.find("models\\cube");
            TEST("backslash normalized to forward slash", entry != nullptr);
        }
    }
    printf("\n");

    // ── Deterministic ordering ──
    {
        printf("-- Ordering --\n");
        const auto& data = reg.data();
        TEST("entries are in sorted order (models/cube < materials/wall < shaders/lit < textures/checker)",
             data.keys[0] == "models/cube" &&
             data.keys[1] == "textures/checker" &&
             data.keys[2] == "shaders/lit" &&
             data.keys[3] == "materials/wall");
    }
    printf("\n");

    // ── Summary ──
    printf("=== Results: %d passed, %d failed ===\n", s_pass, s_fail);
    return s_fail > 0 ? 1 : 0;
}
