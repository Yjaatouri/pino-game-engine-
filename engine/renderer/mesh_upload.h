#pragma once

#include "engine/core/types.h"
#include "engine/serialization/cooked_asset.h"
#include <memory>

namespace pino {

class Mesh;

// Creates a fully uploaded Mesh from cooked mesh data.
// The upload layer owns all format interpretation:
//   - vertex buffer stride and attribute layout
//   - attribute locations (position, normal, uv)
//   - optional attribute creation (tangent, bitangent, future)
//   - GPU buffer allocation and data transfer
//
// AssetManager must NOT interpret cooked mesh structure directly;
// it only orchestrates loading, hashing, and deserialization.
//
// Returns nullptr on failure (empty data, invalid format).
std::shared_ptr<Mesh> upload_cooked_mesh(const CookedMeshData& data, const char* debug_name);

} // namespace pino
