#include "engine/ecs/prefab.h"
#include "engine/ecs/ecs_world.h"
#include <cstdio>
#include <fstream>

namespace pino {

// ── Instantiation ─────────────────────────────────────────────────

// Shared implementation: if assets is non-null, resolve mesh handle.
// If assets is null, mesh handle stays empty.
static EntityId instantiate_impl(const Prefab& self, EcsScene& scene, AssetManager* assets, EntityId parent) {
    EntityId e = scene.create_entity();

    if (self.has_transform()) {
        scene.scene_graph().attach(e, parent);
        auto* t = scene.scene_graph().get(e);
        if (t) {
            t->position = self.transform().position;
            t->rotation = self.transform().rotation;
            t->scale    = self.transform().scale;
        }
    }

    for (auto& entry : self.components()) {
        if (entry.type_hash == kRenderComponentHash && entry.data.size() == sizeof(RenderComponent)) {
            RenderComponent rc;
            memcpy(&rc, entry.data.data(), sizeof(RenderComponent));
            if (assets && !self.mesh_path().empty()) {
                rc.mesh = assets->get_mesh(self.mesh_path().c_str());
            }
            scene.add_component<RenderComponent>(e) = rc;
        } else if (entry.type_hash == kPhysicsComponentHash && entry.data.size() == sizeof(PhysicsComponent)) {
            PhysicsComponent pc;
            memcpy(&pc, entry.data.data(), sizeof(PhysicsComponent));
            scene.add_component<PhysicsComponent>(e) = pc;
        } else if (entry.type_hash == kAudioComponentHash && entry.data.size() == sizeof(AudioComponent)) {
            AudioComponent ac;
            memcpy(&ac, entry.data.data(), sizeof(AudioComponent));
            if (!self.sound_path().empty()) {
                ac.sound_path = self.sound_path();
            }
            scene.add_component<AudioComponent>(e) = ac;
        }
    }

    return e;
}

EntityId Prefab::instantiate(EcsScene& scene, EntityId parent) const {
    return instantiate_impl(*this, scene, nullptr, parent);
}

EntityId Prefab::instantiate(EcsScene& scene, AssetManager& assets, EntityId parent) const {
    return instantiate_impl(*this, scene, &assets, parent);
}

EntityId Prefab::instantiate(EcsWorld& world, AssetManager& assets, EntityId parent) const {
    return instantiate(world.scene(), assets, parent);
}

// ── Serialization helpers ─────────────────────────────────────────

static void write_u8(std::vector<u8>& out, u8 v) { out.push_back(v); }
static void write_u16(std::vector<u8>& out, u16 v) {
    out.push_back(static_cast<u8>(v & 0xFF));
    out.push_back(static_cast<u8>((v >> 8) & 0xFF));
}
static void write_u32(std::vector<u8>& out, u32 v) {
    for (int i = 0; i < 4; ++i) { out.push_back(static_cast<u8>((v >> (i * 8)) & 0xFF)); }
}
static void write_bytes(std::vector<u8>& out, const u8* data, u32 size) {
    for (u32 i = 0; i < size; ++i) out.push_back(data[i]);
}
static void write_string(std::vector<u8>& out, const std::string& s) {
    u16 len = static_cast<u16>(s.size());
    write_u16(out, len);
    write_bytes(out, reinterpret_cast<const u8*>(s.data()), len);
}

static u8  read_u8(const u8*& p)  { u8 v = *p; ++p; return v; }
static u16 read_u16(const u8*& p) {
    u16 v = p[0] | (static_cast<u16>(p[1]) << 8);
    p += 2; return v;
}
static u32 read_u32(const u8*& p) {
    u32 v = 0;
    for (int i = 0; i < 4; ++i) { v |= static_cast<u32>(p[i]) << (i * 8); }
    p += 4; return v;
}
static void read_bytes(const u8*& p, u8* out, u32 size) {
    memcpy(out, p, size); p += size;
}
static std::string read_string(const u8*& p) {
    u16 len = read_u16(p);
    std::string s(reinterpret_cast<const char*>(p), len);
    p += len; return s;
}

std::vector<u8> Prefab::serialize() const {
    std::vector<u8> out;
    // Magic + version + flags
    write_bytes(out, reinterpret_cast<const u8*>("PREF"), 4);
    write_u32(out, 1);   // version
    write_u32(out, 0);   // flags (reserved)

    // Transform
    write_u8(out, m_has_transform ? 1 : 0);
    if (m_has_transform) {
        write_bytes(out, reinterpret_cast<const u8*>(&m_transform.position), sizeof(glm::vec3));
        write_bytes(out, reinterpret_cast<const u8*>(&m_transform.rotation), sizeof(glm::quat));
        write_bytes(out, reinterpret_cast<const u8*>(&m_transform.scale), sizeof(glm::vec3));
    }

    // Components
    write_u32(out, static_cast<u32>(m_components.size()));
    for (auto& entry : m_components) {
        write_u32(out, entry.type_hash);
        write_u32(out, static_cast<u32>(entry.data.size()));
        write_bytes(out, entry.data.data(), static_cast<u32>(entry.data.size()));
    }

    // Asset paths
    write_string(out, m_mesh_path);
    write_string(out, m_sound_path);

    return out;
}

bool Prefab::deserialize(const std::vector<u8>& data) {
    const u8* p = data.data();
    const u8* end = p + data.size();

    // Magic
    if (end - p < 4 || memcmp(p, "PREF", 4) != 0) return false;
    p += 4;

    // Version
    u32 version = read_u32(p);
    (void)version;

    // Flags
    read_u32(p); // skip

    // Transform
    m_has_transform = read_u8(p) != 0;
    if (m_has_transform) {
        if (end - p < static_cast<ptrdiff_t>(sizeof(glm::vec3) + sizeof(glm::quat) + sizeof(glm::vec3)))
            return false;
        read_bytes(p, reinterpret_cast<u8*>(&m_transform.position), sizeof(glm::vec3));
        read_bytes(p, reinterpret_cast<u8*>(&m_transform.rotation), sizeof(glm::quat));
        read_bytes(p, reinterpret_cast<u8*>(&m_transform.scale), sizeof(glm::vec3));
    } else {
        m_transform = {};
    }

    // Components
    m_components.clear();
    u32 comp_count = read_u32(p);
    for (u32 i = 0; i < comp_count; ++i) {
        if (end - p < 8) return false; // type_hash + data_size
        ComponentEntry entry;
        entry.type_hash = read_u32(p);
        u32 data_size = read_u32(p);
        if (end - p < static_cast<ptrdiff_t>(data_size)) return false;
        entry.data.resize(data_size);
        read_bytes(p, entry.data.data(), data_size);
        m_components.push_back(std::move(entry));
    }

    // Asset paths
    m_mesh_path  = read_string(p);
    m_sound_path = read_string(p);

    return true;
}

bool Prefab::save(const std::string& filepath) const {
    auto blob = serialize();
    std::ofstream f(filepath, std::ios::binary);
    if (!f) { std::fprintf(stderr, "Prefab::save: failed to open %s\n", filepath.c_str()); return false; }
    f.write(reinterpret_cast<const char*>(blob.data()), blob.size());
    return f.good();
}

bool Prefab::load(const std::string& filepath) {
    std::ifstream f(filepath, std::ios::binary | std::ios::ate);
    if (!f) { std::fprintf(stderr, "Prefab::load: failed to open %s\n", filepath.c_str()); return false; }
    auto size = static_cast<u32>(f.tellg());
    f.seekg(0);
    std::vector<u8> blob(size);
    f.read(reinterpret_cast<char*>(blob.data()), size);
    if (!f.good()) return false;
    return deserialize(blob);
}

} // namespace pino
