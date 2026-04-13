/*
** Command & Conquer Generals(tm)
** Copyright 2025 Electronic Arts Inc.
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
*/

/*
** webgl2wrapper.h
**
** Drop-in WebGL2/Emscripten replacement for DX8Wrapper.
**
** When __EMSCRIPTEN__ is defined, dx8wrapper.h includes this file and
** exposes  typedef WebGL2Wrapper DX8Wrapper;  so every existing call site
** (DX8Wrapper::Set_Transform, etc.) compiles unchanged.
**
** Build flags required in your em++ invocation:
**   -s USE_WEBGL2=1  -s FULL_ES3=1
**
** See docs/emscripten_porting_guide.md for the full migration guide.
*/

#pragma once

#ifdef __EMSCRIPTEN__
  #include <emscripten.h>
  #include <emscripten/html5.h>
  #include <GLES3/gl3.h>
#else
  // Allow the header to be parsed on non-Emscripten hosts (static analysis)
  typedef unsigned int   GLuint;
  typedef int            GLint;
  typedef unsigned int   GLenum;
  typedef int            EMSCRIPTEN_WEBGL_CONTEXT_HANDLE;
  #define EM_BOOL        int
#endif

#include "always.h"
#include "matrix4.h"
#include "vector3.h"
#include "vector4.h"
#include "wwstring.h"
#include "shader.h"
#include "lightenvironment.h"
#include "texture.h"
#include "ww3dformat.h"
#include "texturefilter.h"
#include "dx8vertexbuffer.h"
#include "dx8indexbuffer.h"
#include "vertmaterial.h"

// Forward declarations kept identical to dx8wrapper.h so callers compile
class VertexMaterialClass;
class CameraClass;
class LightEnvironmentClass;
class RenderDeviceDescClass;
class VertexBufferClass;
class DynamicVBAccessClass;
class IndexBufferClass;
class DynamicIBAccessClass;
class TextureClass;
class ZTextureClass;
class LightClass;
class SurfaceClass;

// These constants are re-declared here with the same values so translation
// units that only include webgl2wrapper.h still compile.
#ifndef MAX_TEXTURE_STAGES
const unsigned MAX_TEXTURE_STAGES          = 8;
const unsigned MAX_VERTEX_STREAMS          = 2;
const unsigned MAX_VERTEX_SHADER_CONSTANTS = 96;
const unsigned MAX_PIXEL_SHADER_CONSTANTS  = 8;
const unsigned MAX_SHADOW_MAPS             = 1;
#endif
const unsigned MAX_LIGHTS                  = 4;

// Buffer type enum kept from original
enum {
    BUFFER_TYPE_DX8,
    BUFFER_TYPE_SORTING,
    BUFFER_TYPE_DYNAMIC_DX8,
    BUFFER_TYPE_DYNAMIC_SORTING,
    BUFFER_TYPE_INVALID
};

// ---------------------------------------------------------------------------
// WGL_Light  –  mirrors the D3DLIGHT8 fields actually used by the engine.
// Call sites that passed a D3DLIGHT8* need to be adapted to WGL_Light*.
// A thin adapter in dx8wrapper.h (below the typedef) handles this.
// ---------------------------------------------------------------------------
struct WGL_Light
{
    int     Type          = 3;  // 1=point, 2=spot, 3=directional
    Vector3 Diffuse       = {1,1,1};
    Vector3 Specular      = {0,0,0};
    Vector3 Ambient       = {0,0,0};
    Vector3 Position      = {0,0,0};
    Vector3 Direction     = {0,0,-1};
    float   Range         = 0.0f;
    float   Attenuation0  = 1.0f;
    float   Attenuation1  = 0.0f;
    float   Attenuation2  = 0.0f;
    bool    Enabled       = false;
};

// ---------------------------------------------------------------------------
// RenderStateStruct – same public layout as the DX8 version.
// D3DMATRIX world/view replaced by Matrix4x4 (row-major, same convention).
// ---------------------------------------------------------------------------
struct RenderStateStruct
{
    ShaderClass          shader;
    VertexMaterialClass* material                          = nullptr;
    TextureBaseClass*    Textures[MAX_TEXTURE_STAGES]      = {};
    WGL_Light            Lights[MAX_LIGHTS];
    bool                 LightEnable[MAX_LIGHTS]           = {};
    Matrix4x4            world;
    Matrix4x4            view;
    unsigned             vertex_buffer_types[MAX_VERTEX_STREAMS] = {};
    unsigned             index_buffer_type                 = 0;
    unsigned short       vba_offset                        = 0;
    unsigned short       vba_count                         = 0;
    unsigned short       iba_offset                        = 0;
    VertexBufferClass*   vertex_buffers[MAX_VERTEX_STREAMS] = {};
    IndexBufferClass*    index_buffer                      = nullptr;
    unsigned short       index_base_offset                 = 0;

    RenderStateStruct();
    ~RenderStateStruct();
    RenderStateStruct& operator=(const RenderStateStruct& src);
};

// ---------------------------------------------------------------------------
// DX8_CleanupHook – unchanged interface
// ---------------------------------------------------------------------------
class DX8_CleanupHook
{
public:
    virtual void ReleaseResources()   = 0;
    virtual void ReAcquireResources() = 0;
};

// ===========================================================================
//  D3D type stubs — not present on web, referenced by some headers
// ===========================================================================
struct IDirect3DTexture8    {};
struct IDirect3DBaseTexture8{};
struct IDirect3DSurface8    {};

// ===========================================================================
//  WebGL2Wrapper
// ===========================================================================
class WebGL2Wrapper
{
public:
    // -----------------------------------------------------------------------
    // Init / Shutdown
    //   canvas_id : CSS selector of the <canvas> element, e.g. "#canvas"
    //   The hwnd / lite parameters from DX8Wrapper::Init are ignored;
    //   pass nullptr / false from the Emscripten entry point.
    // -----------------------------------------------------------------------
    static bool Init(const char* canvas_id, int width = 1280, int height = 720);
    static void Shutdown();

    static bool Is_Initted()     { return s_initted; }
    static bool Is_Device_Lost() { return s_contextLost; }
    static void SetCleanupHook(DX8_CleanupHook* h) { s_cleanupHook = h; }

    // -----------------------------------------------------------------------
    // Frame control – Begin/End_Scene map to glViewport + clear setup.
    // There is no explicit Present(); the browser compositor swaps after
    // the requestAnimationFrame callback returns.
    // -----------------------------------------------------------------------
    static void Begin_Scene();
    static void End_Scene(bool flip_frame = true);   // flip_frame ignored
    static void Flip_To_Primary() {}                 // no-op in WebGL

    static void Clear(bool clear_color, bool clear_z_stencil,
                      const Vector3& color, float dest_alpha = 0.0f,
                      float z = 1.0f, unsigned stencil = 0);

    // -----------------------------------------------------------------------
    // Viewport  (D3DVIEWPORT8 → glViewport)
    // We define our own struct with the same field names.
    // -----------------------------------------------------------------------
    struct WGLViewport { int X, Y, Width, Height; float MinZ, MaxZ; };
    static void Set_Viewport(const WGLViewport* vp);

    // -----------------------------------------------------------------------
    // Transforms
    // D3DTRANSFORMSTATETYPE numeric values are preserved so switch statements
    // in callers compile unchanged.
    // -----------------------------------------------------------------------
    enum TransformType {
        TRANSFORM_PROJECTION = 3,
        TRANSFORM_VIEW       = 2,
        TRANSFORM_WORLD      = 256
    };

    static void Set_Transform(TransformType t, const Matrix4x4& m);
    static void Set_Transform(TransformType t, const Matrix3D& m);
    static void Get_Transform(TransformType t, Matrix4x4& m);
    static void Set_World_Identity();
    static void Set_View_Identity();
    static bool Is_World_Identity();
    static bool Is_View_Identity();

    // Kept for callers that use D3DTRANSFORMSTATETYPE directly
    static void Set_Transform(int t, const Matrix4x4& m) { Set_Transform((TransformType)t, m); }
    static void Set_Transform(int t, const Matrix3D& m)  { Set_Transform((TransformType)t, m); }
    static void Get_Transform(int t, Matrix4x4& m)        { Get_Transform((TransformType)t, m); }

    // Raw DX8-style accessors (used internally and by a few call sites)
    static void _Set_DX8_Transform(int transform, const Matrix4x4& m);
    static void _Get_DX8_Transform(int transform, Matrix4x4& m);

    // -----------------------------------------------------------------------
    // Render states
    // Values use the same D3DRS_* numeric constants (0-255) so existing
    // Set_DX8_Render_State call sites compile unchanged.
    // -----------------------------------------------------------------------
    static void     Set_DX8_Render_State(int state, unsigned value);
    static unsigned Get_DX8_Render_State(int state) { return s_renderStates[state & 0xFF]; }
    static void     Invalidate_Cached_Render_States();

    // -----------------------------------------------------------------------
    // Texture stage states – stored and forwarded to the compat shader
    // -----------------------------------------------------------------------
    static void Set_DX8_Texture_Stage_State(unsigned stage, int state, unsigned value);

    // -----------------------------------------------------------------------
    // Textures
    // -----------------------------------------------------------------------
    static void Set_DX8_Texture(unsigned stage, GLuint texture);
    static void Set_Texture(unsigned stage, TextureBaseClass* texture);

    // -----------------------------------------------------------------------
    // Lights (4 lights, uniform-based – no GL fixed-function light calls)
    // -----------------------------------------------------------------------
    static void Set_DX8_Light(int index, const WGL_Light* light);
    static void Set_Light(unsigned index, const LightClass& light);
    static void Set_Light_Environment(LightEnvironmentClass* env);
    static LightEnvironmentClass* Get_Light_Environment()  { return s_lightEnv; }
    static const WGL_Light& Peek_Light(unsigned i)          { return s_renderState.Lights[i]; }
    static bool Is_Light_Enabled(unsigned i)                { return s_renderState.LightEnable[i]; }

    // -----------------------------------------------------------------------
    // Fog (uniforms, not GL render states)
    // -----------------------------------------------------------------------
    static void     Set_Fog(bool enable, const Vector3& color, float start, float end);
    static bool     Get_Fog_Enable()  { return s_fogEnable; }
    static unsigned Get_Fog_Color()   { return s_fogColor; }

    // -----------------------------------------------------------------------
    // Ambient
    // -----------------------------------------------------------------------
    static void           Set_Ambient(const Vector3& color);
    static const Vector3& Get_Ambient() { return s_ambientColor; }

    // -----------------------------------------------------------------------
    // Material
    // -----------------------------------------------------------------------
    static void Set_DX8_Material(const void* mat); // accepts legacy D3DMATERIAL8*
    static void Set_Material(const VertexMaterialClass* mat);

    // -----------------------------------------------------------------------
    // Shaders (GLSL programs replace D3D vertex/pixel shaders)
    // -----------------------------------------------------------------------
    static void Set_Shader(const ShaderClass& shader);
    static void Get_Shader(ShaderClass& shader);

    // GLuint replaces DWORD shader handles
    static void Set_Vertex_Shader(GLuint program);
    static void Set_Pixel_Shader(GLuint program);
    static void Set_Vertex_Shader_Constant(int reg, const void* data, int count);
    static void Set_Pixel_Shader_Constant(int reg, const void* data, int count);

    static GLuint Get_Vertex_Processing_Behavior() { return 0; } // stub

    // -----------------------------------------------------------------------
    // Buffers
    // -----------------------------------------------------------------------
    static void Set_Vertex_Buffer(const VertexBufferClass* vb, unsigned stream = 0);
    static void Set_Vertex_Buffer(const DynamicVBAccessClass& vba);
    static void Set_Index_Buffer(const IndexBufferClass* ib, unsigned short base_offset);
    static void Set_Index_Buffer(const DynamicIBAccessClass& iba, unsigned short base_offset);
    static void Set_Index_Buffer_Index_Offset(unsigned offset);

    static void Get_Render_State(RenderStateStruct& state);
    static void Set_Render_State(const RenderStateStruct& state);
    static void Release_Render_State();

    // -----------------------------------------------------------------------
    // Drawing
    // -----------------------------------------------------------------------
    static void Apply_Render_State_Changes();

    static void Draw_Triangles(unsigned buffer_type,
                               unsigned short start_index,
                               unsigned short polygon_count,
                               unsigned short min_vertex_index,
                               unsigned short vertex_count);
    static void Draw_Triangles(unsigned short start_index,
                               unsigned short polygon_count,
                               unsigned short min_vertex_index,
                               unsigned short vertex_count);
    static void Draw_Strip(unsigned short start_index,
                           unsigned short index_count,
                           unsigned short min_vertex_index,
                           unsigned short vertex_count);

    // -----------------------------------------------------------------------
    // Render targets (FBOs replace DX8 surfaces)
    // -----------------------------------------------------------------------
    static TextureClass* Create_Render_Target(int width, int height,
                                              int format = 0 /*WW3D_FORMAT_UNKNOWN*/);
    static void Create_Render_Target(int width, int height, int format, int zformat,
                                     TextureClass** target, ZTextureClass** depth_buffer);
    static void Set_Render_Target(GLuint fbo, bool use_default_depth = false);
    static void Set_Render_Target(void* surface, bool use_default_depth = false); // compat overload
    static void Set_Render_Target_With_Z(TextureClass* color, ZTextureClass* depth = nullptr);
    static bool Is_Render_To_Texture() { return s_renderToTexture; }

    static void         Set_Shadow_Map(int i, ZTextureClass* z) { s_shadowMap[i] = z; }
    static ZTextureClass* Get_Shadow_Map(int i)                 { return s_shadowMap[i]; }

    // -----------------------------------------------------------------------
    // Texture / surface creation (return GLuint, not IDirect3DTexture8*)
    // -----------------------------------------------------------------------
    static GLuint Create_Texture(unsigned w, unsigned h,
                                 unsigned gl_internal_format,
                                 unsigned gl_format,
                                 unsigned gl_type,
                                 bool mipmaps = true);
    static GLuint Create_Render_Target_Texture(unsigned w, unsigned h,
                                               unsigned gl_internal_format);
    static GLuint Create_Depth_Texture(unsigned w, unsigned h);

    // Stubs for surface calls used in a few places
    static void* _Get_DX8_Back_Buffer(unsigned num = 0) { return nullptr; }
    static void  _Copy_DX8_Rects(void*, const void*, unsigned, void*, const void*) {}
    static void  Flush_DX8_Resource_Manager(unsigned = 0) {}
    static unsigned Get_Free_Texture_RAM() { return 256 * 1024 * 1024; }

    // -----------------------------------------------------------------------
    // Z-bias (glPolygonOffset emulation of D3DRS_ZBIAS)
    // -----------------------------------------------------------------------
    static void Set_DX8_ZBias(int zbias);
    static void Set_Projection_Transform_With_Z_Bias(const Matrix4x4& m,
                                                     float znear, float zfar);

    // -----------------------------------------------------------------------
    // Gamma / display – stubs (browser controls gamma)
    // -----------------------------------------------------------------------
    static void Set_Gamma(float, float, float, bool = true, bool = true) {}

    // -----------------------------------------------------------------------
    // Color utilities – identical implementation to DX8Wrapper
    // -----------------------------------------------------------------------
    static Vector4      Convert_Color(unsigned color);
    static unsigned int Convert_Color(const Vector4& color);
    static unsigned int Convert_Color(const Vector3& color, float alpha);
    static void         Clamp_Color(Vector4& color);
    static unsigned int Convert_Color_Clamp(const Vector4& color);
    static void         Set_Alpha(float alpha, unsigned int& color);

    // -----------------------------------------------------------------------
    // Resolution / device info
    // -----------------------------------------------------------------------
    static int  Get_Device_Resolution_Width()  { return s_width; }
    static int  Get_Device_Resolution_Height() { return s_height; }
    static void Get_Device_Resolution(int& w, int& h, int& bits, bool& windowed)
                { w = s_width; h = s_height; bits = 32; windowed = true; }
    static void Get_Render_Target_Resolution(int& w, int& h, int& bits, bool& windowed)
                { Get_Device_Resolution(w, h, bits, windowed); }
    static bool Is_Windowed() { return true; }  // always true in browser

    // Stubs for registry/device-selection API (not needed on web)
    static bool Registry_Save_Render_Device(const char*) { return false; }
    static bool Registry_Load_Render_Device(const char*, bool) { return false; }
    static bool Validate_Device() { return !s_contextLost; }
    static bool Has_Stencil()     { return true; }

    // -----------------------------------------------------------------------
    // Statistics – same counters as DX8Wrapper
    // -----------------------------------------------------------------------
    static void     Begin_Statistics();
    static void     End_Statistics();
    static unsigned Get_Last_Frame_Matrix_Changes()             { return s_last_matrix_changes; }
    static unsigned Get_Last_Frame_Material_Changes()           { return s_last_material_changes; }
    static unsigned Get_Last_Frame_Vertex_Buffer_Changes()      { return s_last_vb_changes; }
    static unsigned Get_Last_Frame_Index_Buffer_Changes()       { return s_last_ib_changes; }
    static unsigned Get_Last_Frame_Light_Changes()              { return s_last_light_changes; }
    static unsigned Get_Last_Frame_Texture_Changes()            { return s_last_tex_changes; }
    static unsigned Get_Last_Frame_Render_State_Changes()       { return s_last_rs_changes; }
    static unsigned Get_Last_Frame_Texture_Stage_State_Changes(){ return s_last_tss_changes; }
    static unsigned Get_Last_Frame_Draw_Calls()                 { return s_last_draw_calls; }
    static unsigned long Get_FrameCount()                       { return s_frameCount; }

    // -----------------------------------------------------------------------
    // Raw context access (equivalent of _Get_D3D_Device8)
    // -----------------------------------------------------------------------
    static EMSCRIPTEN_WEBGL_CONTEXT_HANDLE _Get_GL_Context() { return s_glContext; }

    // -----------------------------------------------------------------------
    // Enable / disable triangle drawing (debug feature, kept for compat)
    // -----------------------------------------------------------------------
    static void _Enable_Triangle_Draw(bool e)    { s_enableTriangleDraw = e; }
    static bool _Is_Triangle_Draw_Enabled()      { return s_enableTriangleDraw; }

    static unsigned _Get_Main_Thread_ID() { return 0; }

private:
    // --- Compat shader (fixed-function emulation) --------------------------
    static bool   Compile_Compat_Shader();
    static void   Upload_Compat_Uniforms();

    static GLuint s_compatProgram;

    struct CompatUniforms {
        GLint uWorld, uView, uProjection;
        GLint uLightPos[MAX_LIGHTS];
        GLint uLightDir[MAX_LIGHTS];
        GLint uLightDiffuse[MAX_LIGHTS];
        GLint uLightAmbient[MAX_LIGHTS];
        GLint uLightEnable[MAX_LIGHTS];
        GLint uLightType[MAX_LIGHTS];
        GLint uLightAtten0[MAX_LIGHTS];
        GLint uLightAtten1[MAX_LIGHTS];
        GLint uLightAtten2[MAX_LIGHTS];
        GLint uAmbient;
        GLint uLightingEnabled;
        GLint uFogEnable, uFogColor, uFogStart, uFogEnd;
        GLint uMatDiffuse, uMatSpecular, uMatShininess;
        GLint uAlphaTestEnable, uAlphaRef;
        GLint uTexEnable[MAX_TEXTURE_STAGES];
        GLint uTexColorOp[MAX_TEXTURE_STAGES];
        GLint uTexAlphaOp[MAX_TEXTURE_STAGES];
        GLint uTexColorArg1[MAX_TEXTURE_STAGES];
        GLint uTexColorArg2[MAX_TEXTURE_STAGES];
        GLint uTexAlphaArg1[MAX_TEXTURE_STAGES];
        GLint uTexAlphaArg2[MAX_TEXTURE_STAGES];
        GLint uSampler[MAX_TEXTURE_STAGES];
    };
    static CompatUniforms s_uniforms;

    // --- GL state ----------------------------------------------------------
    static EMSCRIPTEN_WEBGL_CONTEXT_HANDLE s_glContext;
    static bool      s_initted;
    static bool      s_contextLost;
    static bool      s_enableTriangleDraw;
    static int       s_width;
    static int       s_height;

    static RenderStateStruct s_renderState;
    static unsigned          s_renderStateChanged;

    static unsigned s_renderStates[256];
    static unsigned s_texStageStates[MAX_TEXTURE_STAGES][64];
    static GLuint   s_boundTextures[MAX_TEXTURE_STAGES];

    static GLuint   s_currentVBO[MAX_VERTEX_STREAMS];
    static GLuint   s_currentIBO;
    static GLuint   s_currentFBO;
    static GLuint   s_defaultFBO;

    static GLuint   s_activeVertexProgram;

    static Vector4  s_vertexShaderConstants[MAX_VERTEX_SHADER_CONSTANTS];
    static Vector4  s_pixelShaderConstants[MAX_PIXEL_SHADER_CONSTANTS];

    static LightEnvironmentClass* s_lightEnv;
    static ZTextureClass*         s_shadowMap[MAX_SHADOW_MAPS];

    static Vector3  s_ambientColor;
    static bool     s_fogEnable;
    static unsigned s_fogColor;
    static float    s_fogStart;
    static float    s_fogEnd;

    static Matrix4x4 s_worldMatrix;
    static Matrix4x4 s_viewMatrix;
    static Matrix4x4 s_projMatrix;
    static bool      s_worldIdentity;
    static bool      s_viewIdentity;

    static int   s_zBias;
    static float s_zNear;
    static float s_zFar;

    static bool  s_renderToTexture;

    static DX8_CleanupHook* s_cleanupHook;

    // --- Statistics --------------------------------------------------------
    static unsigned s_matrix_changes,   s_last_matrix_changes;
    static unsigned s_material_changes, s_last_material_changes;
    static unsigned s_vb_changes,       s_last_vb_changes;
    static unsigned s_ib_changes,       s_last_ib_changes;
    static unsigned s_light_changes,    s_last_light_changes;
    static unsigned s_tex_changes,      s_last_tex_changes;
    static unsigned s_rs_changes,       s_last_rs_changes;
    static unsigned s_tss_changes,      s_last_tss_changes;
    static unsigned s_draw_calls,       s_last_draw_calls;
    static unsigned long s_frameCount;

    // --- Internal helpers --------------------------------------------------
    static void Apply_Single_Render_State(unsigned state, unsigned value);
    static void Apply_Blend_State();
    static void Apply_Depth_State();
    static void Apply_Stencil_State();
    static void Apply_Cull_State();
    static void Bind_Textures();

    static EM_BOOL On_Context_Lost(int type, const void*, void*);
    static EM_BOOL On_Context_Restored(int type, const void*, void*);

    enum ChangedBits : unsigned {
        WORLD_CHANGED         = 1u << 0,
        VIEW_CHANGED          = 1u << 1,
        LIGHTS_CHANGED        = 1u << 2,
        TEXTURE0_CHANGED      = 1u << 6,
        MATERIAL_CHANGED      = 1u << 14,
        SHADER_CHANGED        = 1u << 15,
        VERTEX_BUFFER_CHANGED = 1u << 16,
        INDEX_BUFFER_CHANGED  = 1u << 17,
        WORLD_IDENTITY        = 1u << 18,
        VIEW_IDENTITY         = 1u << 19,
        TEXTURES_CHANGED      = 0xFFu << 6,
    };

    // -----------------------------------------------------------------------
    // Texture creation stubs — no IDirect3DTexture8 on web
    // -----------------------------------------------------------------------
    static IDirect3DTexture8* _Create_DX8_Texture(
        unsigned, unsigned, WW3DFormat, MipCountType, int=0, bool=false)
        { return nullptr; }
    static IDirect3DTexture8* _Create_DX8_ZTexture(
        unsigned, unsigned, WW3DZFormat, MipCountType, int=0)
        { return nullptr; }
    static IDirect3DTexture8* _Create_DX8_Texture(const char*, MipCountType)
        { return nullptr; }
    static IDirect3DTexture8* _Create_DX8_Texture(IDirect3DSurface8*, MipCountType)
        { return nullptr; }
    static IDirect3DSurface8* _Create_DX8_Surface(unsigned, unsigned, WW3DFormat)
        { return nullptr; }
};

// ---------------------------------------------------------------------------
// D3DPOOL / D3DTS stubs
// ---------------------------------------------------------------------------
enum { D3DPOOL_DEFAULT = 0, D3DPOOL_MANAGED = 1, D3DPOOL_SYSTEMMEM = 2 };
enum { D3DTS_WORLD = 256, D3DTS_VIEW = 2, D3DTS_PROJECTION = 3 };

// ---------------------------------------------------------------------------
// FVFInfoClass stub for Emscripten
// Matches the XYZ+Normal+UV2+Diffuse vertex layout used by dynamic meshes:
//   pos(12) + normal(12) + uv0(8) + uv1(8) + diffuse(4) = 44 bytes
// ---------------------------------------------------------------------------
class W3DMPO;
class FVFInfoClass {
public:
    FVFInfoClass(unsigned /*fvf*/) {}
    unsigned Get_Location_Offset() const { return 0; }
    unsigned Get_Normal_Offset()   const { return 12; }
    unsigned Get_Tex_Offset(unsigned n) const { return 24 + n * 8; }
    unsigned Get_Diffuse_Offset()  const { return 40; }
    unsigned Get_Specular_Offset() const { return 44; }
    unsigned Get_FVF()             const { return 0; }
    unsigned Get_FVF_Size()        const { return 44; }
    void Set_FVF(unsigned) const {}
    void Set_FVF_Size(unsigned) const {}
};

// dynamic_fvf_type — used by DynamicVBAccessClass constructor in dynamesh/line3d
static const unsigned dynamic_fvf_type = 0;

// ---------------------------------------------------------------------------
// Inline color utilities (identical math to DX8Wrapper inlines)
// ---------------------------------------------------------------------------
inline Vector4 WebGL2Wrapper::Convert_Color(unsigned color)
{
    Vector4 c;
    c[3] = ((color & 0xff000000) >> 24) / 255.0f;
    c[0] = ((color & 0x00ff0000) >> 16) / 255.0f;
    c[1] = ((color & 0x0000ff00) >>  8) / 255.0f;
    c[2] = ((color & 0x000000ff)      ) / 255.0f;
    return c;
}

inline unsigned int WebGL2Wrapper::Convert_Color(const Vector3& color, float alpha)
{
    return color.Convert_To_ARGB(alpha);
}

inline unsigned int WebGL2Wrapper::Convert_Color(const Vector4& color)
{
    return Convert_Color(reinterpret_cast<const Vector3&>(color), color[3]);
}

inline void WebGL2Wrapper::Clamp_Color(Vector4& color)
{
    for (int i = 0; i < 4; ++i) {
        float f = (color[i] < 0.0f) ? 0.0f : color[i];
        color[i] = (f > 1.0f) ? 1.0f : f;
    }
}

inline unsigned int WebGL2Wrapper::Convert_Color_Clamp(const Vector4& color)
{
    Vector4 c = color;
    Clamp_Color(c);
    return Convert_Color(reinterpret_cast<const Vector3&>(c), c[3]);
}

inline void WebGL2Wrapper::Set_Alpha(float alpha, unsigned int& color)
{
    reinterpret_cast<unsigned char*>(&color)[3] = static_cast<unsigned char>(255.0f * alpha);
}

inline void WebGL2Wrapper::Get_Render_State(RenderStateStruct& state)
{
    state = s_renderState;
}

inline void WebGL2Wrapper::Get_Shader(ShaderClass& shader)
{
    shader = s_renderState.shader;
}

inline bool WebGL2Wrapper::Is_World_Identity()
{
    return !!(s_renderStateChanged & WORLD_IDENTITY);
}

inline bool WebGL2Wrapper::Is_View_Identity()
{
    return !!(s_renderStateChanged & VIEW_IDENTITY);
}

inline void WebGL2Wrapper::Set_Index_Buffer_Index_Offset(unsigned offset)
{
    if (s_renderState.index_base_offset == offset) return;
    s_renderState.index_base_offset = offset;
    s_renderStateChanged |= INDEX_BUFFER_CHANGED;
}
