#include "metal_renderer.h"
#include "engine/core/log.h"

namespace pino {

// ── Metal API mapping notes ─────────────────────────────────────
// Each stub documents the Metal API correspondence so that when
// Metal rendering is implemented, the mapping is pre-defined:
//
//   GL concept      → Metal concept
//   ─────────         ────────────
//   VAO / VBO        → MTLVertexDescriptor + MTLBuffer
//   EBO/IBO          → MTLBuffer (indexBuffer)
//   glDrawArrays     → [renderEncoder drawPrimitives:...]
//   glDrawElements   → [renderEncoder drawIndexedPrimitives:...]
//   glUniform*       → [renderEncoder setVertexBytes:] / fragmentBytes
//   glBindTexture    → [renderEncoder setFragmentTexture:]
//   GLSL uniform     → Metal argument buffer (id<MTLBuffer>)
//   glClear          → [renderEncoder setRenderPassDescriptor:...]
//   glViewport       → [renderEncoder setViewport:]
//   glEnable(DEPTH)  → MTLDepthStencilDescriptor + depthStencilState
//   GL_TEXTURE_2D    → MTLTexture (MTLTextureType2D)
//   GL_FRAMEBUFFER   → MTLRenderPassDescriptor + MTLDrawable texture
//   glCullFace       → MTLCullMode (set in MTLRenderPipelineDescriptor)
//
// Uniforms in Metal are typically passed via argument buffer
// (MTLBuffer) or directly via [renderEncoder setVertexBytes:].
// The stub handle types (ShaderHandle / BufferHandle / TextureHandle)
// map to indexed Metal resource arrays.

MetalRenderer::MetalRenderer() {
    m_caps.api_name          = "Metal (stub)";
    m_caps.supports_shadows  = true;
    m_caps.supports_instancing = true;
    m_caps.max_texture_size  = 16384;
    m_caps.max_texture_units = 32;
    m_caps.max_lights        = 8;
}

MetalRenderer::~MetalRenderer() {
    shutdown();
}

bool MetalRenderer::init(void* native_window) {
    (void)native_window;
    // Mapping: native_window → CAMetalLayer
    //   CAMetalLayer* layer = (__bridge CAMetalLayer*)native_window;
    //   layer.device = MTLCreateSystemDefaultDevice();
    //   layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    //   m_device = [layer.device retain];
    //   m_command_queue = [m_device newCommandQueue];
    m_initialized = true;
    PINO_INFO("MetalRenderer initialized (stub)");
    return true;
}

void MetalRenderer::shutdown() {
    if (!m_initialized) return;
    // Mapping: release MTLDevice, MTLCommandQueue, MTLibrary
    m_initialized = false;
    PINO_INFO("MetalRenderer shutdown (stub)");
}

void MetalRenderer::begin_frame() {
    // Mapping:
    //   id<MTLCommandBuffer> cmd = [m_command_queue commandBuffer];
    //   MTLRenderPassDescriptor* desc = ...;
    //   id<MTLRenderCommandEncoder> enc = [cmd renderCommandEncoderWithDescriptor:desc];
    //   store encoder
}

void MetalRenderer::end_frame() {
    // Mapping:
    //   [m_encoder endEncoding];
    //   [cmd presentDrawable:layer.nextDrawable];
    //   [cmd commit];
}

const RendererCapabilities& MetalRenderer::capabilities() const {
    return m_caps;
}

// ── Shaders ─────────────────────────────────────────────────────

ShaderHandle MetalRenderer::create_shader(const char* vert_src,
                                           const char* frag_src) {
    (void)vert_src;
    (void)frag_src;
    // Mapping:
    //   NSString* src = [NSString stringWithUTF8String:vert_src];
    //   id<MTLLibrary> lib = [m_device newLibraryWithSource:src ...];
    //   id<MTLFunction> vert = [lib newFunctionWithName:@"vertexMain"];
    //   id<MTLFunction> frag = [lib newFunctionWithName:@"fragmentMain"];
    //   MTLRenderPipelineDescriptor* pd = [MTLRenderPipelineDescriptor new];
    //   pd.vertexFunction = vert;
    //   pd.fragmentFunction = frag;
    //   id<MTLRenderPipelineState> pso = [m_device newRenderPipelineStateWithDescriptor:pd ...];
    return ShaderHandle{m_next_shader_handle++};
}

void MetalRenderer::destroy_shader(ShaderHandle sh) {
    (void)sh;
}

void MetalRenderer::bind_shader(ShaderHandle sh) {
    (void)sh;
    // Mapping:
    //   [m_encoder setRenderPipelineState:pso];
}

void MetalRenderer::set_uniform_int(ShaderHandle sh, const char* name, i32 v) {
    (void)sh; (void)name; (void)v;
    // Mapping: set bytes via argument buffer
    //   [m_encoder setFragmentBytes:&v length:sizeof(v) atIndex:slot];
}

void MetalRenderer::set_uniform_float(ShaderHandle sh, const char* name, f32 v) {
    (void)sh; (void)name; (void)v;
}

void MetalRenderer::set_uniform_vec3(ShaderHandle sh, const char* name, const f32* v) {
    (void)sh; (void)name; (void)v;
}

void MetalRenderer::set_uniform_vec4(ShaderHandle sh, const char* name, const f32* v) {
    (void)sh; (void)name; (void)v;
}

void MetalRenderer::set_uniform_mat4(ShaderHandle sh, const char* name, const f32* m) {
    (void)sh; (void)name; (void)m;
}

// ── Buffers ─────────────────────────────────────────────────────

BufferHandle MetalRenderer::create_vertex_buffer(const void* data, u32 size,
                                                   BufferUsage usage) {
    (void)data; (void)size; (void)usage;
    // Mapping:
    //   MTLResourceOptions opt = (usage == Dynamic) ? MTLResourceStorageModeShared : ...;
    //   id<MTLBuffer> buf = [m_device newBufferWithBytes:data length:size options:opt];
    return BufferHandle{m_next_buffer_handle++};
}

BufferHandle MetalRenderer::create_index_buffer(const void* data, u32 size,
                                                  BufferUsage usage) {
    (void)data; (void)size; (void)usage;
    return BufferHandle{m_next_buffer_handle++};
}

void MetalRenderer::destroy_buffer(BufferHandle buf) {
    (void)buf;
}

void MetalRenderer::update_buffer(BufferHandle buf, const void* data,
                                   u32 offset, u32 size) {
    (void)buf; (void)data; (void)offset; (void)size;
    // Mapping:
    //   memcpy((char*)[buf contents] + offset, data, size);
    //   [buf didModifyRange:NSMakeRange(offset, size)];
}

void MetalRenderer::bind_vertex_buffer(BufferHandle buf, u32 binding, u32 stride) {
    (void)buf; (void)binding; (void)stride;
    // Mapping:
    //   [m_encoder setVertexBuffer:mtl_buf offset:0 atIndex:binding];
}

void MetalRenderer::bind_index_buffer(BufferHandle buf) {
    (void)buf;
    // Mapping:
    //   [m_encoder setIndexBuffer:mtl_buf offset:0];
}

// ── Textures ────────────────────────────────────────────────────

TextureHandle MetalRenderer::create_texture(u32 width, u32 height,
                                             TextureFormat fmt,
                                             const void* data) {
    (void)width; (void)height; (void)fmt; (void)data;
    // Mapping:
    //   MTLTextureDescriptor* td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
    //       (fmt == RGBA8 ? MTLPixelFormatRGBA8Unorm : MTLPixelFormatR8Unorm)
    //       width:width height:height mipmapped:NO];
    //   id<MTLTexture> tex = [m_device newTextureWithDescriptor:td];
    //   [tex replaceRegion:MTLRegionMake2D(0,0,width,height)
    //          mipmapLevel:0 withBytes:data bytesPerRow:width*4];
    return TextureHandle{m_next_texture_handle++};
}

void MetalRenderer::destroy_texture(TextureHandle tex) {
    (void)tex;
}

void MetalRenderer::bind_texture(TextureHandle tex, u32 slot) {
    (void)tex; (void)slot;
    // Mapping:
    //   [m_encoder setFragmentTexture:mtl_tex atIndex:slot];
}

// ── Draw calls ──────────────────────────────────────────────────

void MetalRenderer::draw(u32 vertex_count, u32 instance_count) {
    (void)vertex_count; (void)instance_count;
    // Mapping:
    //   [m_encoder drawPrimitives:MTLPrimitiveTypeTriangle
    //                  vertexStart:0 vertexCount:vertex_count
    //                instanceCount:instance_count];
}

void MetalRenderer::draw_indexed(u32 index_count, u32 instance_count) {
    (void)index_count; (void)instance_count;
    // Mapping:
    //   [m_encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
    //                          indexCount:index_count indexType:MTLIndexTypeUInt16
    //                        indexBuffer:ib offset:0
    //                    instanceCount:instance_count];
}

// ── State ───────────────────────────────────────────────────────

void MetalRenderer::set_viewport(i32 x, i32 y, u32 w, u32 h) {
    (void)x; (void)y; (void)w; (void)h;
    // Mapping:
    //   MTLViewport vp = { (double)x, (double)y, (double)w, (double)h, 0.0, 1.0 };
    //   [m_encoder setViewport:vp];
}

void MetalRenderer::set_depth_test(bool enable) {
    (void)enable;
    // Mapping: set MTLDepthStencilState
}

void MetalRenderer::set_cull_mode(CullMode mode) {
    (void)mode;
    // Mapping: set MTLCullMode in pipeline descriptor
}

void MetalRenderer::clear(const ClearFlags& flags, const f32* color) {
    (void)flags; (void)color;
    // Mapping: set clearColor in MTLRenderPassDescriptor
}

} // namespace pino
