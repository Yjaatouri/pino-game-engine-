#pragma once

#include "engine/core/types.h"
#include "engine/serialization/cooked_asset.h"
#include "engine/renderer/mesh_upload_layout.h"
#include <memory>

namespace pino {

class Mesh;

// Dedicated upload module for cooked mesh data.
//
// Responsibilities:
//   - Interpret CookedMeshData (vertex stride, attribute presence)
//   - Create GPU buffers (VBO/IBO)
//   - Bind vertex attributes at their assigned locations (0–n)
//   - Upload vertex data and index data to GPU
//   - Handle optional attributes (tangent, bitangent) safely
//
// AssetManager must NEVER directly:
//   - Create VBOs or IBOs
//   - Bind vertex attributes
//   - Interpret vertex layout or stride
//
class MeshUploader {
public:
    // Upload a cooked mesh to the GPU.
    // Infers the attribute layout from CookedMeshData automatically.
    // Returns a fully initialized Mesh, or nullptr on failure.
    static std::shared_ptr<Mesh> upload(const CookedMeshData& data, const char* debug_name);

private:
    static MeshUploadLayout infer_layout(const CookedMeshData& data);
    static bool upload_vertex_data(Mesh& mesh, const CookedMeshData& data,
                                   const MeshUploadLayout& layout);
    static void upload_optional_attribs(Mesh& mesh, const CookedMeshData& data,
                                        const MeshUploadLayout& layout);
    static void compute_bounds(Mesh& mesh, const CookedMeshData& data);
};

} // namespace pino
