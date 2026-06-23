#pragma once

#include "engine/core/types.h"

namespace pino {

// Describes the set of vertex attributes present in a cooked mesh
// and their GPU attribute location mapping.
// Used by MeshUploader to decide which buffers and attribute pointers to create.
struct MeshUploadLayout {
    bool has_positions    = true;
    bool has_normals      = true;
    bool has_uvs          = true;
    bool has_tangents     = false;
    bool has_bitangents   = false;
    u32  vertex_stride    = 0;  // bytes per interleaved vertex

    // Attribute locations (GL layout qualifier indices)
    u32 position_location  = 0;
    u32 normal_location    = 1;
    u32 uv_location        = 2;
    u32 tangent_location   = 3;
    u32 bitangent_location = 4;
};

} // namespace pino
