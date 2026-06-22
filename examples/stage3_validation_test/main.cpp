#include "engine/core/binary_chunk.h"
#include "engine/core/serializer.h"
#include "engine/core/type_registry.h"
#include "engine/core/string_table.h"
#include "engine/core/version_registry.h"
#include "engine/serialization/prefab_serializer.h"
#include "engine/serialization/asset_serializer.h"
#include "engine/serialization/save_game_serializer.h"
#include "engine/ecs/prefab.h"
#include "engine/ecs/components.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

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

#define CHECK(expr) do { \
    if (!(expr)) { printf("  ASSERT FAIL at line %d\n", __LINE__); return 1; } \
} while(0)

// ─── Test-only version loaders (no captures) ─────────────────────

static int s_v1_called = 0;
static void loader_v1(Deserializer& d) {
    s_v1_called++;
    uint32_t val = d.readUInt32();
    printf("  Version 1 loader: val=%u\n", val);
}

static int s_v2_called = 0;
static void loader_v2(Deserializer& d) {
    s_v2_called++;
    uint32_t a = d.readUInt32();
    float b = d.readFloat();
    printf("  Version 2 loader: a=%u b=%f\n", a, b);
}

static int s_tform_v1 = 0;
static void transform_v1_loader(Deserializer& d) {
    s_tform_v1++;
    glm::vec3 pos = d.readVec3();
    printf("  Transform v1: pos=(%.1f,%.1f,%.1f)\n", pos.x, pos.y, pos.z);
}

static int s_tform_v2 = 0;
static void transform_v2_loader(Deserializer& d) {
    s_tform_v2++;
    glm::vec3 pos = d.readVec3();
    glm::vec3 rot = d.readVec3();
    printf("  Transform v2: pos=(%.1f,%.1f,%.1f) rot=(%.1f,%.1f,%.1f)\n",
           pos.x, pos.y, pos.z, rot.x, rot.y, rot.z);
}

// ─── Debug dump helpers ──────────────────────────────────────────

static void dump_type_registry(const TypeRegistry& types) {
    printf("\n[TypeRegistry Dump]\n");
    const char* names[] = {
        "PrefabTransform", "PrefabComponents", "PrefabAssets",
        "AssetMeta", "SaveScene", "SaveEntity",
        "TestTypeV1", "TestTypeV2"
    };
    for (const char* name : names) {
        uint32_t id = types.getTypeID(name);
        if (id != 0)
            printf("  %4u -> %s\n", id, name);
    }
}

static void dump_string_table(const StringTable& table) {
    printf("\n[StringTable Dump] (%zu entries)\n", table.size());
    for (size_t i = 0; i < table.size(); ++i)
        printf("  %4zu -> \"%s\"\n", i, table.getString((uint32_t)i).c_str());
}

// ─── Scenario 1: Full prefab round-trip ──────────────────────────

static int test_prefab_roundtrip() {
    printf("\n=== Scenario 1: Prefab Full Round-Trip ===\n");

    TypeRegistry types;
    StringTable strings;
    VersionRegistry versions;

    PrefabSerializer::registerTypes(types);
    PrefabSerializer::registerVersions(versions);

    Prefab original;
    original.set_transform({1.0f, 2.0f, 3.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, {2.0f, 2.0f, 2.0f});

    RenderComponent rc;
    rc.transparent = true;
    original.set_component(rc);

    PhysicsComponent pc;
    pc.is_static = true;
    pc.velocity = {1.0f, 0.0f, 0.0f};
    pc.collision_layer = 2;
    pc.collision_mask = 4;
    original.set_component(pc);

    AudioComponent ac;
    ac.sound_path = "sounds/test.wav";
    ac.volume = 0.75f;
    ac.looping = true;
    ac.spatial = true;
    original.set_component(ac);

    original.set_mesh("models/player.obj");
    original.set_sound("sounds/step.wav");

    printf("  Original: has_transform=%d mesh='%s' sound='%s' components=%zu\n",
           original.has_transform(), original.mesh_path().c_str(),
           original.sound_path().c_str(), original.components().size());

    BinaryChunkWriter writer;
    Serializer ser(writer);
    PrefabSerializer prefab_ser(types, versions, strings);
    prefab_ser.serialize(ser, original);

    BinaryChunkWriter st_writer;
    strings.write(st_writer);

    std::vector<uint8_t> final_data = writer.getBuffer();
    printf("  Serialized: %zu bytes | String table: %zu bytes\n",
           final_data.size(), st_writer.getBuffer().size());

    StringTable loaded_strings;
    BinaryChunkReader st_reader(st_writer.getBuffer().data(), st_writer.getBuffer().size());
    loaded_strings.read(st_reader);

    TypeRegistry types2;
    VersionRegistry versions2;
    PrefabSerializer::registerTypes(types2);
    PrefabSerializer::registerVersions(versions2);

    PrefabSerializer prefab_deser(types2, versions2, loaded_strings);
    BinaryChunkReader reader(final_data.data(), final_data.size());
    Deserializer deser(reader);
    Prefab reconstructed;
    prefab_deser.deserialize(deser, reconstructed);

    TEST("has_transform matches", original.has_transform() == reconstructed.has_transform());
    if (original.has_transform()) {
        const auto& t1 = original.transform();
        const auto& t2 = reconstructed.transform();
        TEST("position.x", t1.position.x == t2.position.x);
        TEST("position.y", t1.position.y == t2.position.y);
        TEST("position.z", t1.position.z == t2.position.z);
        TEST("scale.x", t1.scale.x == t2.scale.x);
        TEST("scale.y", t1.scale.y == t2.scale.y);
        TEST("scale.z", t1.scale.z == t2.scale.z);
    }
    TEST("mesh_path", original.mesh_path() == reconstructed.mesh_path());
    TEST("sound_path", original.sound_path() == reconstructed.sound_path());
    TEST("component count", original.components().size() == reconstructed.components().size());

    if (original.components().size() == reconstructed.components().size()) {
        for (size_t i = 0; i < original.components().size(); ++i) {
            const auto& c1 = original.components()[i];
            const auto& c2 = reconstructed.components()[i];
            TEST("type_hash", c1.type_hash == c2.type_hash);
            TEST("data size", c1.data.size() == c2.data.size());
            if (c1.data.size() > 0 && c1.data.size() == c2.data.size())
                TEST("data", memcmp(c1.data.data(), c2.data.data(), c1.data.size()) == 0);
        }
    }

    return 0;
}

// ─── Scenario 2: Multi-chunk with different versions ─────────────

static int test_multi_chunk_versions() {
    printf("\n=== Scenario 2: Multi-Chunk with Different Versions ===\n");

    TypeRegistry types;
    types.registerType("TestTypeV1");
    types.registerType("TestTypeV2");

    VersionRegistry versions;
    uint32_t kTestType = 42;
    uint32_t kTestType2 = 43;

    s_v1_called = s_v2_called = 0;
    versions.registerVersion(kTestType, 1, loader_v1);
    versions.registerVersion(kTestType, 2, loader_v2);

    BinaryChunkWriter writer;
    Serializer ser(writer);

    ser.beginChunk(kTestType, 1);
    ser.writeUInt32(123);
    ser.endChunk();

    ser.beginChunk(kTestType, 2);
    ser.writeUInt32(456);
    ser.writeFloat(3.14f);
    ser.endChunk();

    ser.beginChunk(kTestType2, 1);
    ser.writeUInt32(999);
    ser.endChunk();

    BinaryChunkReader reader(writer.getBuffer().data(), writer.getBuffer().size());
    Deserializer deser(reader);

    int chunk_count = 0;
    while (deser.nextChunk()) {
        chunk_count++;
        uint32_t tid = deser.getHeader().type_id;
        uint32_t ver = deser.getHeader().version;
        printf("  Chunk %d: type_id=%u version=%u size=%u\n",
               chunk_count, tid, ver, deser.getHeader().size);

        if (versions.supports(tid, ver))
            versions.dispatch(tid, ver, deser);
        else
            deser.skipChunk();
    }

    TEST("Version 1 loader called", s_v1_called == 1);
    TEST("Version 2 loader called", s_v2_called == 1);
    TEST("3 chunks read", chunk_count == 3);

    return 0;
}

// ─── Scenario 3: Unknown chunk skipping ──────────────────────────

static int test_unknown_chunk_skip() {
    printf("\n=== Scenario 3: Unknown Chunk Skipping ===\n");

    BinaryChunkWriter writer;
    Serializer ser(writer);

    ser.beginChunk(100, 1);
    ser.writeUInt32(1);
    ser.endChunk();

    ser.beginChunk(999, 1);
    ser.writeUInt32(2);
    ser.endChunk();

    ser.beginChunk(101, 1);
    ser.writeUInt32(3);
    ser.endChunk();

    BinaryChunkReader reader(writer.getBuffer().data(), writer.getBuffer().size());
    Deserializer deser(reader);

    int known_count = 0;
    while (deser.nextChunk()) {
        if (deser.getHeader().type_id == 999) {
            printf("  Skipping unknown chunk type_id=999\n");
            deser.skipChunk();
            TEST("valid immediately after skip", deser.isValid());
            continue;
        }
        known_count++;
        uint32_t val = deser.readUInt32();
        printf("  Known chunk type_id=%u val=%u\n", deser.getHeader().type_id, val);
    }

    TEST("2 known chunks read", known_count == 2);

    return 0;
}

// ─── Scenario 4: String deduplication ────────────────────────────

static int test_string_dedup() {
    printf("\n=== Scenario 4: String Table Deduplication ===\n");

    StringTable table;

    uint32_t a1 = table.addString("hello");
    uint32_t b  = table.addString("world");
    uint32_t a2 = table.addString("hello");
    uint32_t c  = table.addString("test/path/file.txt");
    uint32_t a3 = table.addString("hello");

    TEST("identical strings same index", a1 == a2 && a2 == a3);
    TEST("different strings different index", a1 != b);
    TEST("count is 3", table.size() == 3);
    TEST("exists('hello')", table.exists("hello"));
    TEST("exists('world')", table.exists("world"));
    TEST("!exists('missing')", !table.exists("missing"));
    TEST("findString('hello') == 0", table.findString("hello") == 0);
    TEST("findString('missing') == UINT32_MAX", table.findString("missing") == UINT32_MAX);

    // Round-trip through binary
    BinaryChunkWriter writer;
    table.write(writer);

    StringTable loaded;
    BinaryChunkReader reader(writer.getBuffer().data(), writer.getBuffer().size());
    loaded.read(reader);

    dump_string_table(loaded);

    TEST("loaded count", loaded.size() == table.size());
    TEST("index 0 == 'hello'", loaded.getString(0) == "hello");
    TEST("index 1 == 'world'", loaded.getString(1) == "world");
    TEST("index 2 == 'test/path/file.txt'", loaded.getString(2) == "test/path/file.txt");

    return 0;
}

// ─── Scenario 5: Type Registry consistency ───────────────────────

static int test_type_registry_consistency() {
    printf("\n=== Scenario 5: Type Registry Consistency ===\n");

    TypeRegistry types;
    uint32_t id1 = types.registerType("TransformComponent");
    uint32_t id2 = types.registerType("RenderComponent");
    uint32_t id3 = types.registerType("PhysicsComponent");

    TEST("IDs sequential (1,2,3)", id1 == 1 && id2 == 2 && id3 == 3);
    TEST("getTypeID", types.getTypeID("TransformComponent") == id1);
    TEST("getTypeID", types.getTypeID("RenderComponent") == id2);
    TEST("getTypeName(id1)", types.getTypeName(id1) == "TransformComponent");
    TEST("getTypeName(999) == UNKNOWN", types.getTypeName(999) == "UNKNOWN");
    TEST("isValid(id1)", types.isValid(id1));
    TEST("!isValid(999)", !types.isValid(999));

    TypeRegistry types2;
    uint32_t id1b = types2.registerType("TransformComponent");
    uint32_t id2b = types2.registerType("RenderComponent");
    uint32_t id3b = types2.registerType("PhysicsComponent");
    TEST("deterministic IDs", id1 == id1b && id2 == id2b && id3 == id3b);

    dump_type_registry(types);

    return 0;
}

// ─── Scenario 6: Version dispatch with multiple versions ─────────

static int test_version_dispatch() {
    printf("\n=== Scenario 6: Version Dispatch ===\n");

    uint32_t kTransformType = 10;

    s_tform_v1 = s_tform_v2 = 0;

    VersionRegistry versions;
    versions.registerVersion(kTransformType, 1, transform_v1_loader);
    versions.registerVersion(kTransformType, 2, transform_v2_loader);

    BinaryChunkWriter writer;
    Serializer ser(writer);

    ser.beginChunk(kTransformType, 1);
    ser.writeVec3({1.0f, 2.0f, 3.0f});
    ser.endChunk();

    ser.beginChunk(kTransformType, 2);
    ser.writeVec3({4.0f, 5.0f, 6.0f});
    ser.writeVec3({0.0f, 0.0f, 1.0f});
    ser.endChunk();

    BinaryChunkReader reader(writer.getBuffer().data(), writer.getBuffer().size());
    Deserializer deser(reader);

    while (deser.nextChunk()) {
        uint32_t tid = deser.getHeader().type_id;
        uint32_t ver = deser.getHeader().version;
        printf("  Chunk: type=%u version=%u\n", tid, ver);

        if (versions.supports(tid, ver))
            versions.dispatch(tid, ver, deser);
        else
            deser.skipChunk();
    }

    TEST("v1 loader called", s_tform_v1 == 1);
    TEST("v2 loader called", s_tform_v2 == 1);

    return 0;
}

// ─── Scenario 7: Corrupted input handling ────────────────────────

static int test_corrupted_input() {
    printf("\n=== Scenario 7: Corrupted Input Handling ===\n");

    BinaryChunkWriter writer;
    Serializer ser(writer);
    ser.beginChunk(100, 1);
    ser.writeUInt32(42);
    ser.endChunk();
    std::vector<uint8_t> data = writer.getBuffer();

    // Bad magic
    {
        printf("  Test: corrupted magic\n");
        std::vector<uint8_t> bad = data;
        bad[0] = 0xFF;
        BinaryChunkReader r(bad.data(), (uint32_t)bad.size());
        Deserializer d(r);
        TEST("failed on bad magic", !d.nextChunk());
    }

    // Size exceeds buffer
    {
        printf("  Test: oversized chunk size\n");
        std::vector<uint8_t> big(20, 0);
        big[0]='P'; big[1]='I'; big[2]='N'; big[3]='O';
        big[12]=0xFF; big[13]=0xFF; big[14]=0xFF; big[15]=0xFF;
        BinaryChunkReader r(big.data(), (uint32_t)big.size());
        Deserializer d(r);
        TEST("failed on oversized", !d.nextChunk());
        TEST("safe after fail", !d.nextChunk());
    }

    // Truncated
    {
        printf("  Test: truncated data\n");
        uint8_t tiny[4] = {'P','I','N','O'};
        BinaryChunkReader r(tiny, 4);
        Deserializer d(r);
        TEST("failed on truncated", !d.nextChunk());
    }

    // Unknown type_id still reads valid data
    {
        printf("  Test: unknown type_id safe\n");
        BinaryChunkReader r(data.data(), (uint32_t)data.size());
        Deserializer d(r);
        TEST("valid chunk ok", d.nextChunk());
        TEST("data intact", d.readUInt32() == 42);
    }

    return 0;
}

// ─── Scenario 8: AssetSerializer round-trip ──────────────────────

static int test_asset_roundtrip() {
    printf("\n=== Scenario 8: Asset Serializer Round-Trip ===\n");

    TypeRegistry types;
    StringTable strings;
    VersionRegistry versions;
    AssetSerializer::registerTypes(types);
    AssetSerializer::registerVersions(versions);

    AssetMeta original;
    original.path = "models/character.obj";
    original.type = AssetType::Mesh;
    original.version = 3;

    BinaryChunkWriter writer;
    Serializer ser(writer);
    AssetSerializer asset_ser(types, versions, strings);
    asset_ser.serialize(ser, original);

    BinaryChunkWriter st_writer;
    strings.write(st_writer);

    StringTable loaded_strings;
    BinaryChunkReader st_reader(st_writer.getBuffer().data(), st_writer.getBuffer().size());
    loaded_strings.read(st_reader);

    TypeRegistry types2;
    VersionRegistry versions2;
    AssetSerializer::registerTypes(types2);
    AssetSerializer::registerVersions(versions2);

    AssetSerializer asset_deser(types2, versions2, loaded_strings);
    BinaryChunkReader reader(writer.getBuffer().data(), writer.getBuffer().size());
    Deserializer deser(reader);

    AssetMeta reconstructed;
    asset_deser.deserialize(deser, reconstructed);

    TEST("path", original.path == reconstructed.path);
    TEST("type", original.type == reconstructed.type);
    TEST("version", original.version == reconstructed.version);
    printf("  Asset: path='%s' type=%u version=%u\n",
           reconstructed.path.c_str(), (uint32_t)reconstructed.type, reconstructed.version);

    return 0;
}

// ─── Scenario 9: Determinism check ───────────────────────────────

static int test_determinism() {
    printf("\n=== Scenario 9: Determinism Check ===\n");

    auto run = [](std::vector<uint8_t>& out) {
        TypeRegistry types;
        StringTable strings;
        VersionRegistry versions;
        PrefabSerializer::registerTypes(types);
        PrefabSerializer::registerVersions(versions);

        Prefab prefab;
        prefab.set_transform({1.0f, 2.0f, 3.0f}, glm::identity<glm::quat>(), {1.0f, 1.0f, 1.0f});
        prefab.set_mesh("models/test.obj");

        BinaryChunkWriter w;
        Serializer s(w);
        PrefabSerializer ps(types, versions, strings);
        ps.serialize(s, prefab);
        out = w.getBuffer();
    };

    std::vector<uint8_t> r1, r2;
    run(r1);
    run(r2);

    TEST("deterministic size", r1.size() == r2.size());
    if (r1.size() == r2.size())
        TEST("deterministic binary", memcmp(r1.data(), r2.data(), r1.size()) == 0);

    return 0;
}

// ─── Scenario 10: Mixed chunk stream ─────────────────────────────

static int test_mixed_stream() {
    printf("\n=== Scenario 10: Mixed Chunk Stream ===\n");

    TypeRegistry types;
    StringTable strings;
    VersionRegistry versions;
    types.registerType("TestTransform");
    types.registerType("TestMesh");
    types.registerType("TestMaterial");

    BinaryChunkWriter writer;
    Serializer ser(writer);

    ser.beginChunk(1, 1);
    ser.writeVec3({1.0f, 2.0f, 3.0f});
    ser.endChunk();

    uint32_t mesh_idx = strings.addString("models/player.obj");
    ser.beginChunk(2, 2);
    ser.writeUInt32(mesh_idx);
    ser.writeUInt32(1024);
    ser.endChunk();

    ser.beginChunk(9999, 1);
    ser.writeBytes("junk", 4);
    ser.endChunk();

    uint32_t mat_idx = strings.addString("materials/player.mat");
    ser.beginChunk(3, 1);
    ser.writeUInt32(mat_idx);
    ser.writeVec3({0.8f, 0.2f, 0.1f});
    ser.endChunk();

    std::vector<uint8_t> data = writer.getBuffer();
    printf("  Total stream: %zu bytes\n", data.size());

    BinaryChunkWriter st_writer;
    strings.write(st_writer);
    StringTable loaded_strings;
    BinaryChunkReader st_reader(st_writer.getBuffer().data(), st_writer.getBuffer().size());
    loaded_strings.read(st_reader);

    BinaryChunkReader reader(data.data(), (uint32_t)data.size());
    Deserializer deser(reader);

    int chunk_idx = 0;
    TEST("initially valid", deser.isValid());

    while (deser.nextChunk()) {
        uint32_t tid = deser.getHeader().type_id;
        uint32_t ver = deser.getHeader().version;
        printf("  Chunk %d: type_id=%u version=%u size=%u\n",
               ++chunk_idx, tid, ver, deser.getHeader().size);

        if (tid == 1 && ver == 1) {
            auto pos = deser.readVec3();
            TEST("v1 pos.x", pos.x == 1.0f);
            TEST("v1 pos.y", pos.y == 2.0f);
            TEST("v1 pos.z", pos.z == 3.0f);
        } else if (tid == 2 && ver == 2) {
            uint32_t idx = deser.readUInt32();
            uint32_t vc = deser.readUInt32();
            TEST("mesh name", loaded_strings.getString(idx) == "models/player.obj");
            TEST("vertex count", vc == 1024);
        } else if (tid == 3 && ver == 1) {
            uint32_t idx = deser.readUInt32();
            auto color = deser.readVec3();
            TEST("mat name", loaded_strings.getString(idx) == "materials/player.mat");
            TEST("color.r", color.x == 0.8f);
        } else {
            deser.skipChunk();
            TEST("valid after skip", deser.isValid());
        }
    }
    TEST("4 chunks total", chunk_idx == 4);

    return 0;
}

// ─── Scenario 11: SaveGameSerializer round-trip ──────────────────

static int test_savegame_roundtrip() {
    printf("\n=== Scenario 11: SaveGameSerializer Round-Trip ===\n");

    TypeRegistry types;
    StringTable strings;
    VersionRegistry versions;
    SaveGameSerializer::registerTypes(types);
    SaveGameSerializer::registerVersions(versions);

    EcsScene scene;
    EntityId e1 = scene.create_entity();
    scene.scene_graph().attach(e1);
    scene.scene_graph().set_position(e1, {10.0f, 20.0f, 30.0f});
    scene.scene_graph().set_scale(e1, {2.0f, 2.0f, 2.0f});
    scene.add_component<RenderComponent>(e1);
    scene.add_component<PhysicsComponent>(e1);
    {
        PhysicsComponent* pc = scene.get_component<PhysicsComponent>(e1);
        pc->is_static = true;
        pc->velocity = {5.0f, 0.0f, 0.0f};
    }

    EntityId e2 = scene.create_entity();
    scene.scene_graph().attach(e2);
    scene.scene_graph().set_position(e2, {100.0f, 0.0f, 0.0f});
    scene.add_component<AudioComponent>(e2);
    {
        AudioComponent* ac = scene.get_component<AudioComponent>(e2);
        ac->sound_path = "sounds/ambient.wav";
        ac->volume = 0.5f;
        ac->looping = true;
    }

    printf("  Original: %u entities\n", scene.entity_count());

    BinaryChunkWriter writer;
    Serializer ser(writer);
    SaveGameSerializer save_ser(types, versions, strings);
    save_ser.serialize(ser, scene);

    BinaryChunkWriter st_writer;
    strings.write(st_writer);
    StringTable loaded_strings;
    BinaryChunkReader st_reader(st_writer.getBuffer().data(), st_writer.getBuffer().size());
    loaded_strings.read(st_reader);

    printf("  Serialized: %zu bytes | String table: %zu bytes\n",
           writer.getBuffer().size(), st_writer.getBuffer().size());

    TypeRegistry types2;
    VersionRegistry versions2;
    SaveGameSerializer::registerTypes(types2);
    SaveGameSerializer::registerVersions(versions2);

    EcsScene scene2;
    BinaryChunkReader reader(writer.getBuffer().data(), writer.getBuffer().size());
    Deserializer deser(reader);
    SaveGameSerializer save_deser(types2, versions2, loaded_strings);
    save_deser.deserialize(deser, scene2);

    TEST("entity count matches", scene.entity_count() == scene2.entity_count());
    TEST("reconstruction valid", scene2.entity_count() > 0);

    int count = 0;
    scene2.registry().each([&](EntityId) { count++; });
    TEST("iterable entities", count == (int)scene.entity_count());

    return 0;
}

// ─── Scenario 12: Empty StringTable round-trip ────────────────────

static int test_empty_stringtable() {
    printf("\n=== Scenario 12: Empty StringTable Round-Trip ===\n");

    StringTable table;
    TEST("empty initially", table.size() == 0);

    BinaryChunkWriter writer;
    table.write(writer);
    printf("  Serialized empty table: %zu bytes\n", writer.getBuffer().size());

    StringTable loaded;
    BinaryChunkReader reader(writer.getBuffer().data(), writer.getBuffer().size());
    loaded.read(reader);

    TEST("still empty after round-trip", loaded.size() == 0);
    TEST("!exists after round-trip", !loaded.exists("anything"));

    return 0;
}

// ─── Scenario 13: Single string StringTable ──────────────────────

static int test_single_string_stringtable() {
    printf("\n=== Scenario 13: Single String StringTable ===\n");

    StringTable table;
    uint32_t idx = table.addString("only_one");
    TEST("index 0", idx == 0);

    BinaryChunkWriter writer;
    table.write(writer);

    StringTable loaded;
    BinaryChunkReader reader(writer.getBuffer().data(), writer.getBuffer().size());
    loaded.read(reader);

    TEST("one entry", loaded.size() == 1);
    TEST("string preserved", loaded.getString(0) == "only_one");

    return 0;
}

// ─── Scenario 14: StringTable chunk header validation ───────────

static int test_stringtable_chunk_header() {
    printf("\n=== Scenario 14: StringTable Chunk Header Validation ===\n");

    StringTable st;
    st.addString("hello");
    st.addString("world");

    BinaryChunkWriter st_writer;
    st.write(st_writer);

    BinaryChunkReader st_reader(st_writer.getBuffer().data(), st_writer.getBuffer().size());
    TEST("string table chunk available", st_reader.nextChunk());
    TEST("correct chunk type", st_reader.getHeader().type_id == StringTable::kChunkType);
    TEST("correct version", st_reader.getHeader().version == StringTable::kChunkVersion);
    printf("  StringTable chunk: type=%u version=%u size=%u\n",
           st_reader.getHeader().type_id, st_reader.getHeader().version, st_reader.getHeader().size);

    uint32_t count = st_reader.readUInt32();
    TEST("count is 2", count == 2);
    uint32_t len0 = st_reader.readUInt32();
    TEST("first string len", len0 == 5);

    return 0;
}

// ─── Scenario 15: Invalid chunk type ─────────────────────────────

static int test_stringtable_invalid_chunk_type() {
    printf("\n=== Scenario 15: StringTable Invalid Chunk Type ===\n");

    // Write a non-StringTable chunk to a buffer, then try to read it as StringTable
    BinaryChunkWriter writer;
    writer.beginChunk(999, 1);
    writer.writeUInt32(0);
    writer.endChunk();

    StringTable loaded;
    BinaryChunkReader reader(writer.getBuffer().data(), writer.getBuffer().size());
    loaded.read(reader);

    TEST("table empty after wrong type", loaded.size() == 0);

    return 0;
}

// ─── Scenario 16: Missing chunk (empty buffer) ───────────────────

static int test_stringtable_missing_chunk() {
    printf("\n=== Scenario 16: StringTable Missing Chunk ===\n");

    // Empty buffer
    {
        StringTable loaded;
        uint8_t empty[4] = {0, 0, 0, 0};
        BinaryChunkReader reader(empty, 4);
        loaded.read(reader);
        TEST("empty buffer -> empty table", loaded.size() == 0);
    }

    // Buffer too small for header
    {
        StringTable loaded;
        uint8_t tiny[4] = {'P','I','N','O'};
        BinaryChunkReader reader(tiny, 4);
        loaded.read(reader);
        TEST("truncated header -> empty table", loaded.size() == 0);
    }

    return 0;
}

// ─── Scenario 17: Truncated StringTable payload ──────────────────

static int test_stringtable_truncated_payload() {
    printf("\n=== Scenario 17: StringTable Truncated Payload ===\n");

    StringTable table;
    table.addString("this_is_a_long_test_string_for_truncation_check");
    BinaryChunkWriter writer;
    table.write(writer);

    std::vector<uint8_t> full = writer.getBuffer();

    // Truncate in the middle of the payload (after reading some bytes)
    std::vector<uint8_t> truncated(full.begin(), full.begin() + 24);
    StringTable loaded;
    BinaryChunkReader reader(truncated.data(), (uint32_t)truncated.size());
    loaded.read(reader);

    // Should not crash; table should be empty due to corrupt data
    printf("  Truncated to %zu bytes, loaded size=%zu\n", truncated.size(), loaded.size());

    return 0;
}

// ─── Main ────────────────────────────────────────────────────────

int main() {
    printf("============================================\n");
    printf("  Stage 3 Serialization Foundation - E2E Test\n");
    printf("============================================\n\n");

    CHECK(test_prefab_roundtrip() == 0);
    CHECK(test_multi_chunk_versions() == 0);
    CHECK(test_unknown_chunk_skip() == 0);
    CHECK(test_string_dedup() == 0);
    CHECK(test_type_registry_consistency() == 0);
    CHECK(test_version_dispatch() == 0);
    CHECK(test_corrupted_input() == 0);
    CHECK(test_asset_roundtrip() == 0);
    CHECK(test_determinism() == 0);
    CHECK(test_mixed_stream() == 0);
    CHECK(test_savegame_roundtrip() == 0);
    CHECK(test_empty_stringtable() == 0);
    CHECK(test_single_string_stringtable() == 0);
    CHECK(test_stringtable_chunk_header() == 0);
    CHECK(test_stringtable_invalid_chunk_type() == 0);
    CHECK(test_stringtable_missing_chunk() == 0);
    CHECK(test_stringtable_truncated_payload() == 0);

    printf("\n============================================\n");
    printf("  Results: %d passed, %d failed out of %d\n",
           s_pass, s_fail, s_pass + s_fail);
    printf("============================================\n");

    return s_fail > 0 ? 1 : 0;
}
