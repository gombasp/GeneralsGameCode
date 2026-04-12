/*
** Command & Conquer Generals(tm)
** Copyright 2025 Electronic Arts Inc.
**
** webgl2wrapper.cpp  –  WebGL2/Emscripten implementation of DX8Wrapper API.
**
** Compiled only when __EMSCRIPTEN__ is defined.
** On other platforms dx8wrapper.cpp is used instead.
*/

#ifdef __EMSCRIPTEN__

#include "webgl2wrapper.h"
#include "lightenvironment.h"
#include "light.h"
#include "texture.h"
#include "vertmaterial.h"
#include "dx8vertexbuffer.h"
#include "dx8indexbuffer.h"
#include "shader.h"
#include <cstring>
#include <cstdio>

// ===========================================================================
// Compat shader source  (fixed-function pipeline emulation)
// ===========================================================================

static const char* s_compatVertSrc = R"GLSL(
#version 300 es
precision highp float;

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aColor;
layout(location = 3) in vec2 aTexCoord0;
layout(location = 4) in vec2 aTexCoord1;

uniform mat4 uWorld;
uniform mat4 uView;
uniform mat4 uProjection;

struct Light {
    int   type;
    vec3  position;
    vec3  direction;
    vec3  diffuse;
    vec3  ambient;
    float atten0, atten1, atten2;
    bool  enabled;
};
uniform Light uLights[4];
uniform vec3  uAmbient;
uniform bool  uLightingEnabled;

uniform vec4  uMatDiffuse;
uniform vec4  uMatSpecular;
uniform float uMatShininess;

uniform bool  uFogEnable;
uniform float uFogStart;
uniform float uFogEnd;

out vec4  vDiffuse;
out vec2  vTexCoord0;
out vec2  vTexCoord1;
out float vFogFactor;

vec3 ComputeLighting(vec3 posWorld, vec3 normWorld)
{
    vec3 result = uAmbient * uMatDiffuse.rgb;
    for (int i = 0; i < 4; ++i) {
        if (!uLights[i].enabled) continue;
        vec3  L;
        float atten = 1.0;
        if (uLights[i].type == 3) {
            L = normalize(-uLights[i].direction);
        } else {
            vec3 toLight = uLights[i].position - posWorld;
            float dist   = length(toLight);
            L = toLight / dist;
            atten = 1.0 / (uLights[i].atten0
                         + uLights[i].atten1 * dist
                         + uLights[i].atten2 * dist * dist);
        }
        float NdotL = max(dot(normWorld, L), 0.0);
        result += atten * (uLights[i].ambient * uMatDiffuse.rgb
                         + uLights[i].diffuse * uMatDiffuse.rgb * NdotL);
    }
    return result;
}

void main()
{
    vec4 posWorld = uWorld * vec4(aPosition, 1.0);
    vec3 normWorld = normalize(mat3(uWorld) * aNormal);
    vec4 posEye   = uView * posWorld;
    gl_Position   = uProjection * posEye;

    if (uLightingEnabled)
        vDiffuse = vec4(clamp(ComputeLighting(posWorld.xyz, normWorld), 0.0, 1.0), uMatDiffuse.a) * aColor;
    else
        vDiffuse = aColor;

    vTexCoord0 = aTexCoord0;
    vTexCoord1 = aTexCoord1;

    if (uFogEnable) {
        float d = -posEye.z;
        vFogFactor = clamp((uFogEnd - d) / (uFogEnd - uFogStart), 0.0, 1.0);
    } else {
        vFogFactor = 1.0;
    }
}
)GLSL";

static const char* s_compatFragSrc = R"GLSL(
#version 300 es
precision mediump float;

in vec4  vDiffuse;
in vec2  vTexCoord0;
in vec2  vTexCoord1;
in float vFogFactor;

const int TOP_DISABLE           = 1;
const int TOP_SELECTARG1        = 2;
const int TOP_SELECTARG2        = 3;
const int TOP_MODULATE          = 4;
const int TOP_MODULATE2X        = 5;
const int TOP_MODULATE4X        = 6;
const int TOP_ADD               = 7;
const int TOP_ADDSIGNED         = 8;
const int TOP_BLENDDIFFUSEALPHA = 12;
const int TOP_BLENDTEXTUREALPHA = 13;
const int TARG_DIFFUSE = 0;
const int TARG_CURRENT = 1;
const int TARG_TEXTURE = 2;

uniform sampler2D uSampler[8];
uniform bool uTexEnable[8];
uniform int  uTexColorOp[8];
uniform int  uTexAlphaOp[8];
uniform int  uTexColorArg1[8];
uniform int  uTexColorArg2[8];
uniform int  uTexAlphaArg1[8];
uniform int  uTexAlphaArg2[8];

uniform bool  uFogEnable;
uniform vec4  uFogColor;
uniform bool  uAlphaTestEnable;
uniform float uAlphaRef;

out vec4 fragColor;

vec2 TexCoordForStage(int s) { return (s == 0) ? vTexCoord0 : vTexCoord1; }

vec4 GetArg(int arg, vec4 cur, vec4 tex)
{
    if (arg == TARG_DIFFUSE) return vDiffuse;
    if (arg == TARG_TEXTURE) return tex;
    return cur;
}

vec4 ApplyOp(int op, vec4 a1, vec4 a2)
{
    if (op == TOP_DISABLE)            return a1;
    if (op == TOP_SELECTARG1)         return a1;
    if (op == TOP_SELECTARG2)         return a2;
    if (op == TOP_MODULATE)           return a1 * a2;
    if (op == TOP_MODULATE2X)         return clamp(a1 * a2 * 2.0, 0.0, 1.0);
    if (op == TOP_MODULATE4X)         return clamp(a1 * a2 * 4.0, 0.0, 1.0);
    if (op == TOP_ADD)                return clamp(a1 + a2, 0.0, 1.0);
    if (op == TOP_ADDSIGNED)          return clamp(a1 + a2 - 0.5, 0.0, 1.0);
    if (op == TOP_BLENDDIFFUSEALPHA)  return mix(a2, a1, vDiffuse.a);
    if (op == TOP_BLENDTEXTUREALPHA)  return mix(a2, a1, a1.a);
    return a1;
}

void main()
{
    vec4 current = vDiffuse;
    for (int s = 0; s < 8; ++s) {
        if (!uTexEnable[s]) break;
        vec4 tex  = texture(uSampler[s], TexCoordForStage(s));
        vec4 a1c  = GetArg(uTexColorArg1[s], current, tex);
        vec4 a2c  = GetArg(uTexColorArg2[s], current, tex);
        vec4 a1a  = GetArg(uTexAlphaArg1[s], current, tex);
        vec4 a2a  = GetArg(uTexAlphaArg2[s], current, tex);
        current   = vec4(ApplyOp(uTexColorOp[s], a1c, a2c).rgb,
                         ApplyOp(uTexAlphaOp[s], a1a, a2a).a);
    }
    if (uAlphaTestEnable && current.a < uAlphaRef) discard;
    if (uFogEnable) current.rgb = mix(uFogColor.rgb, current.rgb, vFogFactor);
    fragColor = current;
}
)GLSL";

// ===========================================================================
// Static member definitions
// ===========================================================================
EMSCRIPTEN_WEBGL_CONTEXT_HANDLE WebGL2Wrapper::s_glContext        = 0;
bool     WebGL2Wrapper::s_initted             = false;
bool     WebGL2Wrapper::s_contextLost         = false;
bool     WebGL2Wrapper::s_enableTriangleDraw  = true;
int      WebGL2Wrapper::s_width               = 1280;
int      WebGL2Wrapper::s_height              = 720;

RenderStateStruct WebGL2Wrapper::s_renderState;
unsigned          WebGL2Wrapper::s_renderStateChanged = 0xFFFFFFFFu;

unsigned WebGL2Wrapper::s_renderStates[256]                         = {};
unsigned WebGL2Wrapper::s_texStageStates[MAX_TEXTURE_STAGES][64]    = {};
GLuint   WebGL2Wrapper::s_boundTextures[MAX_TEXTURE_STAGES]         = {};
GLuint   WebGL2Wrapper::s_currentVBO[MAX_VERTEX_STREAMS]            = {};
GLuint   WebGL2Wrapper::s_currentIBO                                = 0;
GLuint   WebGL2Wrapper::s_currentFBO                                = 0;
GLuint   WebGL2Wrapper::s_defaultFBO                                = 0;
GLuint   WebGL2Wrapper::s_activeVertexProgram                       = 0;
GLuint   WebGL2Wrapper::s_compatProgram                             = 0;

Vector4  WebGL2Wrapper::s_vertexShaderConstants[MAX_VERTEX_SHADER_CONSTANTS] = {};
Vector4  WebGL2Wrapper::s_pixelShaderConstants[MAX_PIXEL_SHADER_CONSTANTS]   = {};

LightEnvironmentClass* WebGL2Wrapper::s_lightEnv           = nullptr;
ZTextureClass*         WebGL2Wrapper::s_shadowMap[MAX_SHADOW_MAPS] = {};

Vector3  WebGL2Wrapper::s_ambientColor  = {0,0,0};
bool     WebGL2Wrapper::s_fogEnable     = false;
unsigned WebGL2Wrapper::s_fogColor      = 0;
float    WebGL2Wrapper::s_fogStart      = 0.0f;
float    WebGL2Wrapper::s_fogEnd        = 1.0f;

Matrix4x4 WebGL2Wrapper::s_worldMatrix;
Matrix4x4 WebGL2Wrapper::s_viewMatrix;
Matrix4x4 WebGL2Wrapper::s_projMatrix;
bool      WebGL2Wrapper::s_worldIdentity = true;
bool      WebGL2Wrapper::s_viewIdentity  = true;

int   WebGL2Wrapper::s_zBias  = 0;
float WebGL2Wrapper::s_zNear  = 1.0f;
float WebGL2Wrapper::s_zFar   = 1000.0f;
bool  WebGL2Wrapper::s_renderToTexture = false;

DX8_CleanupHook* WebGL2Wrapper::s_cleanupHook = nullptr;

unsigned WebGL2Wrapper::s_matrix_changes   = 0; unsigned WebGL2Wrapper::s_last_matrix_changes   = 0;
unsigned WebGL2Wrapper::s_material_changes = 0; unsigned WebGL2Wrapper::s_last_material_changes = 0;
unsigned WebGL2Wrapper::s_vb_changes       = 0; unsigned WebGL2Wrapper::s_last_vb_changes       = 0;
unsigned WebGL2Wrapper::s_ib_changes       = 0; unsigned WebGL2Wrapper::s_last_ib_changes       = 0;
unsigned WebGL2Wrapper::s_light_changes    = 0; unsigned WebGL2Wrapper::s_last_light_changes    = 0;
unsigned WebGL2Wrapper::s_tex_changes      = 0; unsigned WebGL2Wrapper::s_last_tex_changes      = 0;
unsigned WebGL2Wrapper::s_rs_changes       = 0; unsigned WebGL2Wrapper::s_last_rs_changes       = 0;
unsigned WebGL2Wrapper::s_tss_changes      = 0; unsigned WebGL2Wrapper::s_last_tss_changes      = 0;
unsigned WebGL2Wrapper::s_draw_calls       = 0; unsigned WebGL2Wrapper::s_last_draw_calls       = 0;
unsigned long WebGL2Wrapper::s_frameCount  = 0;

WebGL2Wrapper::CompatUniforms WebGL2Wrapper::s_uniforms = {};

// ===========================================================================
// Helpers
// ===========================================================================
static GLuint CompileShader(GLenum type, const char* src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, 512, nullptr, log);
        printf("[WebGL2] Shader compile error: %s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

// ===========================================================================
// Init / Shutdown
// ===========================================================================
bool WebGL2Wrapper::Init(const char* canvas_id, int width, int height)
{
    s_width  = width;
    s_height = height;

#ifdef __EMSCRIPTEN__
    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    attrs.majorVersion   = 2;
    attrs.minorVersion   = 0;
    attrs.depth          = 1;
    attrs.stencil        = 1;
    attrs.antialias      = 0;
    attrs.alpha          = 0;
    attrs.powerPreference = EM_WEBGL_POWER_PREFERENCE_HIGH_PERFORMANCE;

    s_glContext = emscripten_webgl_create_context(canvas_id, &attrs);
    if (s_glContext <= 0) {
        printf("[WebGL2] Failed to create WebGL2 context on %s\n", canvas_id);
        return false;
    }
    emscripten_webgl_make_context_current(s_glContext);

    emscripten_set_webglcontextlost_callback(canvas_id, nullptr, false, On_Context_Lost);
    emscripten_set_webglcontextrestored_callback(canvas_id, nullptr, false, On_Context_Restored);
#endif

    if (!Compile_Compat_Shader()) return false;

    // Default GL state
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glViewport(0, 0, s_width, s_height);

    // Check for S3TC (DXT) compressed texture support.
    // WEBGL_compressed_texture_s3tc is present on virtually all desktop browsers.
    // Without it DDS textures won't render, but the game will still start.
    bool has_s3tc = false;
#ifdef __EMSCRIPTEN__
    has_s3tc = (bool)EM_ASM_INT({
        var gl = Module.ctx || (typeof GL !== 'undefined' ? GL.currentContext && GL.currentContext.GLctx : null);
        if (!gl) return 0;
        return (gl.getExtension('WEBGL_compressed_texture_s3tc') ||
                gl.getExtension('MOZ_WEBGL_compressed_texture_s3tc') ||
                gl.getExtension('WEBKIT_WEBGL_compressed_texture_s3tc')) ? 1 : 0;
    });
    if (!has_s3tc) {
        printf("[WebGL2] WARNING: WEBGL_compressed_texture_s3tc not available — DXT textures will be blank.\n");
        printf("[WebGL2] Use a Chromium-based browser or Firefox for best compatibility.\n");
    } else {
        printf("[WebGL2] S3TC compressed textures supported.\n");
    }
#endif

    s_renderStateChanged = 0xFFFFFFFFu;
    s_initted = true;
    return true;
}

void WebGL2Wrapper::Shutdown()
{
    if (!s_initted) return;
    if (s_compatProgram) { glDeleteProgram(s_compatProgram); s_compatProgram = 0; }
#ifdef __EMSCRIPTEN__
    if (s_glContext) { emscripten_webgl_destroy_context(s_glContext); s_glContext = 0; }
#endif
    s_initted = false;
}

bool WebGL2Wrapper::Compile_Compat_Shader()
{
    GLuint vs = CompileShader(GL_VERTEX_SHADER,   s_compatVertSrc);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, s_compatFragSrc);
    if (!vs || !fs) return false;

    s_compatProgram = glCreateProgram();
    glAttachShader(s_compatProgram, vs);
    glAttachShader(s_compatProgram, fs);
    glLinkProgram(s_compatProgram);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(s_compatProgram, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(s_compatProgram, 512, nullptr, log);
        printf("[WebGL2] Program link error: %s\n", log);
        glDeleteProgram(s_compatProgram);
        s_compatProgram = 0;
        return false;
    }

    // Cache all uniform locations
    auto& u = s_uniforms;
    u.uWorld      = glGetUniformLocation(s_compatProgram, "uWorld");
    u.uView       = glGetUniformLocation(s_compatProgram, "uView");
    u.uProjection = glGetUniformLocation(s_compatProgram, "uProjection");
    u.uAmbient    = glGetUniformLocation(s_compatProgram, "uAmbient");
    u.uLightingEnabled = glGetUniformLocation(s_compatProgram, "uLightingEnabled");
    u.uFogEnable  = glGetUniformLocation(s_compatProgram, "uFogEnable");
    u.uFogColor   = glGetUniformLocation(s_compatProgram, "uFogColor");
    u.uFogStart   = glGetUniformLocation(s_compatProgram, "uFogStart");
    u.uFogEnd     = glGetUniformLocation(s_compatProgram, "uFogEnd");
    u.uMatDiffuse = glGetUniformLocation(s_compatProgram, "uMatDiffuse");
    u.uMatSpecular= glGetUniformLocation(s_compatProgram, "uMatSpecular");
    u.uMatShininess=glGetUniformLocation(s_compatProgram,"uMatShininess");
    u.uAlphaTestEnable = glGetUniformLocation(s_compatProgram, "uAlphaTestEnable");
    u.uAlphaRef   = glGetUniformLocation(s_compatProgram, "uAlphaRef");

    char name[64];
    for (unsigned i = 0; i < MAX_LIGHTS; ++i) {
        snprintf(name, sizeof(name), "uLights[%u].position",  i); u.uLightPos[i]     = glGetUniformLocation(s_compatProgram, name);
        snprintf(name, sizeof(name), "uLights[%u].direction", i); u.uLightDir[i]     = glGetUniformLocation(s_compatProgram, name);
        snprintf(name, sizeof(name), "uLights[%u].diffuse",   i); u.uLightDiffuse[i] = glGetUniformLocation(s_compatProgram, name);
        snprintf(name, sizeof(name), "uLights[%u].ambient",   i); u.uLightAmbient[i] = glGetUniformLocation(s_compatProgram, name);
        snprintf(name, sizeof(name), "uLights[%u].enabled",   i); u.uLightEnable[i]  = glGetUniformLocation(s_compatProgram, name);
        snprintf(name, sizeof(name), "uLights[%u].type",      i); u.uLightType[i]    = glGetUniformLocation(s_compatProgram, name);
        snprintf(name, sizeof(name), "uLights[%u].atten0",    i); u.uLightAtten0[i]  = glGetUniformLocation(s_compatProgram, name);
        snprintf(name, sizeof(name), "uLights[%u].atten1",    i); u.uLightAtten1[i]  = glGetUniformLocation(s_compatProgram, name);
        snprintf(name, sizeof(name), "uLights[%u].atten2",    i); u.uLightAtten2[i]  = glGetUniformLocation(s_compatProgram, name);
    }
    for (unsigned s = 0; s < MAX_TEXTURE_STAGES; ++s) {
        snprintf(name, sizeof(name), "uTexEnable[%u]",    s); u.uTexEnable[s]    = glGetUniformLocation(s_compatProgram, name);
        snprintf(name, sizeof(name), "uTexColorOp[%u]",   s); u.uTexColorOp[s]   = glGetUniformLocation(s_compatProgram, name);
        snprintf(name, sizeof(name), "uTexAlphaOp[%u]",   s); u.uTexAlphaOp[s]   = glGetUniformLocation(s_compatProgram, name);
        snprintf(name, sizeof(name), "uTexColorArg1[%u]", s); u.uTexColorArg1[s] = glGetUniformLocation(s_compatProgram, name);
        snprintf(name, sizeof(name), "uTexColorArg2[%u]", s); u.uTexColorArg2[s] = glGetUniformLocation(s_compatProgram, name);
        snprintf(name, sizeof(name), "uTexAlphaArg1[%u]", s); u.uTexAlphaArg1[s] = glGetUniformLocation(s_compatProgram, name);
        snprintf(name, sizeof(name), "uTexAlphaArg2[%u]", s); u.uTexAlphaArg2[s] = glGetUniformLocation(s_compatProgram, name);
        snprintf(name, sizeof(name), "uSampler[%u]",      s); u.uSampler[s]      = glGetUniformLocation(s_compatProgram, name);
    }
    return true;
}

// ===========================================================================
// Frame
// ===========================================================================
void WebGL2Wrapper::Begin_Scene()
{
    ++s_frameCount;
    glBindFramebuffer(GL_FRAMEBUFFER, s_currentFBO);
    glViewport(0, 0, s_width, s_height);
}

void WebGL2Wrapper::End_Scene(bool)
{
    // Browser compositor handles swap
}

void WebGL2Wrapper::Clear(bool clear_color, bool clear_z_stencil,
                          const Vector3& color, float dest_alpha,
                          float z, unsigned stencil)
{
    GLbitfield mask = 0;
    if (clear_color) {
        glClearColor(color.X, color.Y, color.Z, dest_alpha);
        mask |= GL_COLOR_BUFFER_BIT;
    }
    if (clear_z_stencil) {
        glClearDepthf(z);
        glClearStencil(stencil);
        mask |= GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
    }
    if (mask) glClear(mask);
}

// ===========================================================================
// Viewport
// ===========================================================================
void WebGL2Wrapper::Set_Viewport(const WGLViewport* vp)
{
    if (!vp) return;
    glViewport(vp->X, vp->Y, vp->Width, vp->Height);
    glDepthRangef(vp->MinZ, vp->MaxZ);
}

// ===========================================================================
// Transforms
// ===========================================================================
void WebGL2Wrapper::Set_Transform(TransformType t, const Matrix4x4& m)
{
    switch (t) {
    case TRANSFORM_WORLD:
        s_worldMatrix = m;
        s_renderStateChanged |= WORLD_CHANGED;
        s_renderStateChanged &= ~WORLD_IDENTITY;
        s_worldIdentity = false;
        ++s_matrix_changes;
        break;
    case TRANSFORM_VIEW:
        s_viewMatrix = m;
        s_renderStateChanged |= VIEW_CHANGED;
        s_renderStateChanged &= ~VIEW_IDENTITY;
        s_viewIdentity = false;
        ++s_matrix_changes;
        break;
    case TRANSFORM_PROJECTION:
        s_projMatrix = m;
        ++s_matrix_changes;
        break;
    }
}

void WebGL2Wrapper::Set_Transform(TransformType t, const Matrix3D& m)
{
    Set_Transform(t, Matrix4x4(m));
}

void WebGL2Wrapper::Get_Transform(TransformType t, Matrix4x4& m)
{
    switch (t) {
    case TRANSFORM_WORLD:      m = s_worldIdentity ? Matrix4x4(true) : s_worldMatrix; break;
    case TRANSFORM_VIEW:       m = s_viewIdentity  ? Matrix4x4(true) : s_viewMatrix;  break;
    case TRANSFORM_PROJECTION: m = s_projMatrix;  break;
    }
}

void WebGL2Wrapper::Set_World_Identity()
{
    s_renderStateChanged |=  WORLD_IDENTITY;
    s_renderStateChanged &= ~WORLD_CHANGED;
    s_worldIdentity = true;
    ++s_matrix_changes;
}

void WebGL2Wrapper::Set_View_Identity()
{
    s_renderStateChanged |=  VIEW_IDENTITY;
    s_renderStateChanged &= ~VIEW_CHANGED;
    s_viewIdentity = true;
    ++s_matrix_changes;
}

void WebGL2Wrapper::_Set_DX8_Transform(int t, const Matrix4x4& m)
{
    Set_Transform((TransformType)t, m);
}

void WebGL2Wrapper::_Get_DX8_Transform(int t, Matrix4x4& m)
{
    Get_Transform((TransformType)t, m);
}

// ===========================================================================
// Render states
// ===========================================================================
void WebGL2Wrapper::Set_DX8_Render_State(int state, unsigned value)
{
    unsigned idx = (unsigned)state & 0xFF;
    if (s_renderStates[idx] == value) return;
    s_renderStates[idx] = value;
    Apply_Single_Render_State((unsigned)state, value);
    ++s_rs_changes;
}

void WebGL2Wrapper::Apply_Single_Render_State(unsigned state, unsigned value)
{
    // D3DRS_* values used in the engine mapped to GL calls
    switch (state) {
    case 7:  // D3DRS_ZENABLE
        value ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST); break;
    case 14: // D3DRS_ZWRITEENABLE
        glDepthMask(value ? GL_TRUE : GL_FALSE); break;
    case 23: // D3DRS_ZFUNC
        { GLenum fn[] = {GL_NEVER,GL_LESS,GL_EQUAL,GL_LEQUAL,GL_GREATER,GL_NOTEQUAL,GL_GEQUAL,GL_ALWAYS};
          if (value >= 1 && value <= 8) glDepthFunc(fn[value-1]); } break;
    case 22: // D3DRS_CULLMODE  1=none 2=CW 3=CCW
        if (value == 1) { glDisable(GL_CULL_FACE); }
        else { glEnable(GL_CULL_FACE); glCullFace(value == 2 ? GL_FRONT : GL_BACK); } break;
    case 27: // D3DRS_ALPHABLENDENABLE
        value ? glEnable(GL_BLEND) : glDisable(GL_BLEND); break;
    case 19: // D3DRS_SRCBLEND
    case 20: // D3DRS_DESTBLEND
        Apply_Blend_State(); break;
    case 52: // D3DRS_STENCILENABLE
        value ? glEnable(GL_STENCIL_TEST) : glDisable(GL_STENCIL_TEST); break;
    case 53: case 54: case 55: case 56: case 57: case 58: case 59:
        Apply_Stencil_State(); break;
    case 47: // D3DRS_ZBIAS  – glPolygonOffset
        if (value == 0) {
            glDisable(GL_POLYGON_OFFSET_FILL);
        } else {
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(-(float)value, -(float)value);
        }
        break;
    case 8:  // D3DRS_FILLMODE 2=solid 3=wireframe
        // WebGL2 doesn't support wireframe natively; silently ignored
        break;
    // Fog, alpha test, ambient, lighting handled as uniforms in Upload_Compat_Uniforms
    }
}

static GLenum DX8BlendToGL(unsigned v)
{
    switch (v) {
    case 1: return GL_ZERO;
    case 2: return GL_ONE;
    case 3: return GL_SRC_COLOR;
    case 4: return GL_ONE_MINUS_SRC_COLOR;
    case 5: return GL_SRC_ALPHA;
    case 6: return GL_ONE_MINUS_SRC_ALPHA;
    case 7: return GL_DST_ALPHA;
    case 8: return GL_ONE_MINUS_DST_ALPHA;
    case 9: return GL_DST_COLOR;
    case 10:return GL_ONE_MINUS_DST_COLOR;
    default:return GL_ONE;
    }
}

void WebGL2Wrapper::Apply_Blend_State()
{
    glBlendFunc(DX8BlendToGL(s_renderStates[19]), DX8BlendToGL(s_renderStates[20]));
}

static GLenum DX8CmpToGL(unsigned v)
{
    GLenum fn[] = {GL_NEVER,GL_LESS,GL_EQUAL,GL_LEQUAL,GL_GREATER,GL_NOTEQUAL,GL_GEQUAL,GL_ALWAYS};
    return (v >= 1 && v <= 8) ? fn[v-1] : GL_ALWAYS;
}

static GLenum DX8StencilOpToGL(unsigned v)
{
    switch(v) {
    case 1: return GL_KEEP;
    case 2: return GL_ZERO;
    case 3: return GL_REPLACE;
    case 4: return GL_INCR;
    case 5: return GL_DECR;
    case 6: return GL_INVERT;
    case 7: return GL_INCR_WRAP;
    case 8: return GL_DECR_WRAP;
    default:return GL_KEEP;
    }
}

void WebGL2Wrapper::Apply_Stencil_State()
{
    glStencilFunc(DX8CmpToGL(s_renderStates[56]),
                  s_renderStates[57],
                  s_renderStates[58]);
    glStencilOp(DX8StencilOpToGL(s_renderStates[53]),
                DX8StencilOpToGL(s_renderStates[54]),
                DX8StencilOpToGL(s_renderStates[55]));
    glStencilMask(s_renderStates[59]);
}

void WebGL2Wrapper::Invalidate_Cached_Render_States()
{
    memset(s_renderStates,     0xFF, sizeof(s_renderStates));
    memset(s_texStageStates,   0xFF, sizeof(s_texStageStates));
    memset(s_boundTextures,    0,    sizeof(s_boundTextures));
}

// ===========================================================================
// Texture stage states
// ===========================================================================
void WebGL2Wrapper::Set_DX8_Texture_Stage_State(unsigned stage, int state, unsigned value)
{
    if (stage >= MAX_TEXTURE_STAGES) return;
    unsigned idx = (unsigned)state & 0x3F;
    if (s_texStageStates[stage][idx] == value) return;
    s_texStageStates[stage][idx] = value;
    ++s_tss_changes;
    // Actual upload happens in Upload_Compat_Uniforms at draw time
}

// ===========================================================================
// Textures
// ===========================================================================
void WebGL2Wrapper::Set_DX8_Texture(unsigned stage, GLuint texture)
{
    if (stage >= MAX_TEXTURE_STAGES) return;
    if (s_boundTextures[stage] == texture) return;
    s_boundTextures[stage] = texture;
    glActiveTexture(GL_TEXTURE0 + stage);
    glBindTexture(GL_TEXTURE_2D, texture);
    ++s_tex_changes;
}

void WebGL2Wrapper::Set_Texture(unsigned stage, TextureBaseClass* texture)
{
    if (stage >= MAX_TEXTURE_STAGES) return;
    if (texture == s_renderState.Textures[stage]) return;
    REF_PTR_SET(s_renderState.Textures[stage], texture);
    s_renderStateChanged |= (TEXTURE0_CHANGED << stage);
}

// ===========================================================================
// Lights
// ===========================================================================
void WebGL2Wrapper::Set_DX8_Light(int index, const WGL_Light* light)
{
    if (index < 0 || index >= (int)MAX_LIGHTS) return;
    if (light) {
        s_renderState.Lights[index]     = *light;
        s_renderState.LightEnable[index] = true;
    } else {
        s_renderState.LightEnable[index] = false;
    }
    s_renderStateChanged |= LIGHTS_CHANGED;
    ++s_light_changes;
}

void WebGL2Wrapper::Set_Light_Environment(LightEnvironmentClass* env)
{
    s_lightEnv = env;
}

void WebGL2Wrapper::Set_Fog(bool enable, const Vector3& color, float start, float end)
{
    s_fogEnable = enable;
    s_fogColor  = Convert_Color(color, 0.0f);
    s_fogStart  = start;
    s_fogEnd    = end;
    ShaderClass::Invalidate();
}

void WebGL2Wrapper::Set_Ambient(const Vector3& color)
{
    s_ambientColor = color;
}

// ===========================================================================
// Material
// ===========================================================================
void WebGL2Wrapper::Set_DX8_Material(const void* /*mat*/)
{
    // D3DMATERIAL8 fields read at draw time via Set_Material path
    ++s_material_changes;
}

void WebGL2Wrapper::Set_Material(const VertexMaterialClass* mat)
{
    REF_PTR_SET(s_renderState.material, const_cast<VertexMaterialClass*>(mat));
    s_renderStateChanged |= MATERIAL_CHANGED;
    ++s_material_changes;
}

// ===========================================================================
// Shaders
// ===========================================================================
void WebGL2Wrapper::Set_Shader(const ShaderClass& shader)
{
    if (!ShaderClass::ShaderDirty && ((unsigned&)shader == (unsigned&)s_renderState.shader))
        return;
    s_renderState.shader = shader;
    s_renderStateChanged |= SHADER_CHANGED;
}

void WebGL2Wrapper::Set_Vertex_Shader(GLuint program)
{
    if (s_activeVertexProgram == program) return;
    s_activeVertexProgram = program;
    glUseProgram(program ? program : s_compatProgram);
}

void WebGL2Wrapper::Set_Pixel_Shader(GLuint /*program*/)
{
    // In GL, pixel and vertex shader are part of the same linked program.
    // This is a no-op; programs are set via Set_Vertex_Shader.
}

void WebGL2Wrapper::Set_Vertex_Shader_Constant(int reg, const void* data, int count)
{
    int sz = sizeof(Vector4) * count;
    if (memcmp(data, &s_vertexShaderConstants[reg], sz) == 0) return;
    memcpy(&s_vertexShaderConstants[reg], data, sz);
    // Upload to current program if a custom one is active
    if (s_activeVertexProgram) {
        GLint loc = glGetUniformLocation(s_activeVertexProgram, "uVSConstants");
        if (loc >= 0) glUniform4fv(loc, count, (const float*)data);
    }
}

void WebGL2Wrapper::Set_Pixel_Shader_Constant(int reg, const void* data, int count)
{
    int sz = sizeof(Vector4) * count;
    if (memcmp(data, &s_pixelShaderConstants[reg], sz) == 0) return;
    memcpy(&s_pixelShaderConstants[reg], data, sz);
}

// ===========================================================================
// Buffers
// ===========================================================================
void WebGL2Wrapper::Set_Vertex_Buffer(const VertexBufferClass* vb, unsigned stream)
{
    if (stream >= MAX_VERTEX_STREAMS) return;
    REF_PTR_SET(s_renderState.vertex_buffers[stream], const_cast<VertexBufferClass*>(vb));
    s_renderStateChanged |= VERTEX_BUFFER_CHANGED;
    ++s_vb_changes;
}

void WebGL2Wrapper::Set_Vertex_Buffer(const DynamicVBAccessClass& vba)
{
    // DynamicVBAccessClass carries its offset/count inline
    s_renderState.vba_offset = vba.Get_Start_Index();
    s_renderState.vba_count  = vba.Get_Vertex_Count();
    s_renderStateChanged |= VERTEX_BUFFER_CHANGED;
    ++s_vb_changes;
}

void WebGL2Wrapper::Set_Index_Buffer(const IndexBufferClass* ib, unsigned short base_offset)
{
    REF_PTR_SET(s_renderState.index_buffer, const_cast<IndexBufferClass*>(ib));
    s_renderState.index_base_offset = base_offset;
    s_renderStateChanged |= INDEX_BUFFER_CHANGED;
    ++s_ib_changes;
}

void WebGL2Wrapper::Set_Index_Buffer(const DynamicIBAccessClass& iba, unsigned short base_offset)
{
    s_renderState.iba_offset        = iba.Get_Start_Index();
    s_renderState.index_base_offset = base_offset;
    s_renderStateChanged |= INDEX_BUFFER_CHANGED;
    ++s_ib_changes;
}

void WebGL2Wrapper::Set_Render_State(const RenderStateStruct& state)
{
    if (s_renderState.index_buffer)
        s_renderState.index_buffer->Release_Engine_Ref();
    for (int i = 0; i < MAX_VERTEX_STREAMS; ++i)
        if (s_renderState.vertex_buffers[i])
            s_renderState.vertex_buffers[i]->Release_Engine_Ref();

    s_renderState = state;
    s_renderStateChanged = 0xFFFFFFFFu;

    if (s_renderState.index_buffer)
        s_renderState.index_buffer->Add_Engine_Ref();
    for (int i = 0; i < MAX_VERTEX_STREAMS; ++i)
        if (s_renderState.vertex_buffers[i])
            s_renderState.vertex_buffers[i]->Add_Engine_Ref();
}

void WebGL2Wrapper::Release_Render_State()
{
    if (s_renderState.index_buffer)
        s_renderState.index_buffer->Release_Engine_Ref();
    for (int i = 0; i < MAX_VERTEX_STREAMS; ++i)
        if (s_renderState.vertex_buffers[i])
            s_renderState.vertex_buffers[i]->Release_Engine_Ref();

    for (int i = 0; i < MAX_VERTEX_STREAMS; ++i)
        REF_PTR_RELEASE(s_renderState.vertex_buffers[i]);
    REF_PTR_RELEASE(s_renderState.index_buffer);
    REF_PTR_RELEASE(s_renderState.material);
    for (int i = 0; i < MAX_TEXTURE_STAGES; ++i)
        REF_PTR_RELEASE(s_renderState.Textures[i]);
}

// ===========================================================================
// Apply deferred state + uniforms before each draw
// ===========================================================================
void WebGL2Wrapper::Upload_Compat_Uniforms()
{
    GLuint prog = s_activeVertexProgram ? s_activeVertexProgram : s_compatProgram;
    glUseProgram(prog);
    if (prog != s_compatProgram) return; // custom shaders manage their own uniforms

    auto& u = s_uniforms;

    // Matrices – transpose from row-major to column-major
    Matrix4x4 world = s_worldIdentity ? Matrix4x4(true) : s_worldMatrix;
    Matrix4x4 view  = s_viewIdentity  ? Matrix4x4(true) : s_viewMatrix;
    glUniformMatrix4fv(u.uWorld,      1, GL_TRUE, (const float*)&world);
    glUniformMatrix4fv(u.uView,       1, GL_TRUE, (const float*)&view);
    glUniformMatrix4fv(u.uProjection, 1, GL_TRUE, (const float*)&s_projMatrix);

    // Lights
    for (unsigned i = 0; i < MAX_LIGHTS; ++i) {
        const WGL_Light& L = s_renderState.Lights[i];
        glUniform1i (u.uLightEnable[i],  s_renderState.LightEnable[i] ? 1 : 0);
        glUniform1i (u.uLightType[i],    L.Type);
        glUniform3fv(u.uLightPos[i],  1, (const float*)&L.Position);
        glUniform3fv(u.uLightDir[i],  1, (const float*)&L.Direction);
        glUniform3fv(u.uLightDiffuse[i],1,(const float*)&L.Diffuse);
        glUniform3fv(u.uLightAmbient[i],1,(const float*)&L.Ambient);
        glUniform1f (u.uLightAtten0[i],  L.Attenuation0);
        glUniform1f (u.uLightAtten1[i],  L.Attenuation1);
        glUniform1f (u.uLightAtten2[i],  L.Attenuation2);
    }
    glUniform3fv(u.uAmbient, 1, (const float*)&s_ambientColor);
    glUniform1i(u.uLightingEnabled, s_renderStates[137] ? 1 : 0); // D3DRS_LIGHTING

    // Fog
    glUniform1i(u.uFogEnable, s_fogEnable ? 1 : 0);
    if (s_fogEnable) {
        Vector4 fc = Convert_Color(s_fogColor);
        glUniform4fv(u.uFogColor, 1, (const float*)&fc);
        glUniform1f(u.uFogStart, s_fogStart);
        glUniform1f(u.uFogEnd,   s_fogEnd);
    }

    // Alpha test
    bool alphaTest = !!s_renderStates[15]; // D3DRS_ALPHATESTENABLE
    glUniform1i(u.uAlphaTestEnable, alphaTest ? 1 : 0);
    glUniform1f(u.uAlphaRef, s_renderStates[24] / 255.0f); // D3DRS_ALPHAREF

    // Texture stages – D3DTSS indices used in engine:
    // ColorOp=1 AlphaOp=2 ColorArg1=3 ColorArg2=4 AlphaArg1=5 AlphaArg2=6
    for (unsigned s = 0; s < MAX_TEXTURE_STAGES; ++s) {
        bool texOn = (s_renderState.Textures[s] != nullptr);
        glUniform1i(u.uTexEnable[s],    texOn ? 1 : 0);
        glUniform1i(u.uTexColorOp[s],   s_texStageStates[s][1]);
        glUniform1i(u.uTexAlphaOp[s],   s_texStageStates[s][2]);
        glUniform1i(u.uTexColorArg1[s], s_texStageStates[s][3]);
        glUniform1i(u.uTexColorArg2[s], s_texStageStates[s][4]);
        glUniform1i(u.uTexAlphaArg1[s], s_texStageStates[s][5]);
        glUniform1i(u.uTexAlphaArg2[s], s_texStageStates[s][6]);
        glUniform1i(u.uSampler[s],      (int)s);
    }
}

void WebGL2Wrapper::Apply_Render_State_Changes()
{
    if (!s_renderStateChanged) return;
    Upload_Compat_Uniforms();
    Bind_Textures();
    s_renderStateChanged = 0;
}

void WebGL2Wrapper::Bind_Textures()
{
    for (unsigned s = 0; s < MAX_TEXTURE_STAGES; ++s) {
        if (!(s_renderStateChanged & (TEXTURE0_CHANGED << s))) continue;
        // TextureClass exposes a Get_Texture_Handle() or similar – adapt as needed
        // For now we bind 0 (white texture) as placeholder
        glActiveTexture(GL_TEXTURE0 + s);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

// ===========================================================================
// Draw calls
// ===========================================================================
void WebGL2Wrapper::Draw_Triangles(unsigned /*buffer_type*/,
    unsigned short start_index, unsigned short polygon_count,
    unsigned short min_vertex_index, unsigned short vertex_count)
{
    Draw_Triangles(start_index, polygon_count, min_vertex_index, vertex_count);
}

void WebGL2Wrapper::Draw_Triangles(unsigned short start_index,
    unsigned short polygon_count,
    unsigned short /*min_vertex_index*/, unsigned short /*vertex_count*/)
{
    if (!s_enableTriangleDraw || !polygon_count) return;
    Apply_Render_State_Changes();
    glDrawElements(GL_TRIANGLES, polygon_count * 3, GL_UNSIGNED_SHORT,
                   (void*)(uintptr_t)(start_index * sizeof(unsigned short)));
    ++s_draw_calls;
}

void WebGL2Wrapper::Draw_Strip(unsigned short start_index, unsigned short index_count,
    unsigned short /*min_vertex_index*/, unsigned short /*vertex_count*/)
{
    if (!s_enableTriangleDraw || !index_count) return;
    Apply_Render_State_Changes();
    glDrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_SHORT,
                   (void*)(uintptr_t)(start_index * sizeof(unsigned short)));
    ++s_draw_calls;
}

// ===========================================================================
// Z-bias
// ===========================================================================
void WebGL2Wrapper::Set_DX8_ZBias(int zbias)
{
    if (zbias == s_zBias) return;
    s_zBias = zbias < 0 ? 0 : (zbias > 15 ? 15 : zbias);
    if (s_zBias == 0) {
        glDisable(GL_POLYGON_OFFSET_FILL);
    } else {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-(float)s_zBias * 0.0625f, -(float)s_zBias);
    }
}

void WebGL2Wrapper::Set_Projection_Transform_With_Z_Bias(const Matrix4x4& m,
                                                         float znear, float zfar)
{
    s_zNear = znear;
    s_zFar  = zfar;
    s_projMatrix = m;
}

// ===========================================================================
// Render targets
// ===========================================================================
void WebGL2Wrapper::Set_Render_Target(GLuint fbo, bool /*use_default_depth*/)
{
    s_currentFBO    = fbo ? fbo : s_defaultFBO;
    s_renderToTexture = (fbo != 0);
    glBindFramebuffer(GL_FRAMEBUFFER, s_currentFBO);
}

void WebGL2Wrapper::Set_Render_Target(void* /*surface*/, bool use_default_depth)
{
    Set_Render_Target((GLuint)0, use_default_depth);
}

void WebGL2Wrapper::Set_Render_Target_With_Z(TextureClass* /*color*/, ZTextureClass* /*depth*/)
{
    // TODO: bind texture-backed FBO
}

TextureClass* WebGL2Wrapper::Create_Render_Target(int /*w*/, int /*h*/, int /*format*/)
{
    // TODO: create FBO-backed TextureClass
    return nullptr;
}

void WebGL2Wrapper::Create_Render_Target(int, int, int, int, TextureClass**, ZTextureClass**)
{
    // TODO
}

// ===========================================================================
// Texture creation
// ===========================================================================
GLuint WebGL2Wrapper::Create_Texture(unsigned w, unsigned h,
                                     unsigned gl_internal_format,
                                     unsigned gl_format,
                                     unsigned gl_type,
                                     bool mipmaps)
{
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)gl_internal_format,
                 (GLsizei)w, (GLsizei)h, 0,
                 (GLenum)gl_format, (GLenum)gl_type, nullptr);
    if (mipmaps) glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

GLuint WebGL2Wrapper::Create_Render_Target_Texture(unsigned w, unsigned h,
                                                   unsigned gl_internal_format)
{
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)gl_internal_format,
                 (GLsizei)w, (GLsizei)h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

GLuint WebGL2Wrapper::Create_Depth_Texture(unsigned w, unsigned h)
{
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8,
                 (GLsizei)w, (GLsizei)h, 0,
                 GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

// ===========================================================================
// Statistics
// ===========================================================================
void WebGL2Wrapper::Begin_Statistics()
{
    s_matrix_changes = s_material_changes = s_vb_changes = s_ib_changes =
    s_light_changes  = s_tex_changes      = s_rs_changes = s_tss_changes =
    s_draw_calls     = 0;
}

void WebGL2Wrapper::End_Statistics()
{
    s_last_matrix_changes   = s_matrix_changes;
    s_last_material_changes = s_material_changes;
    s_last_vb_changes       = s_vb_changes;
    s_last_ib_changes       = s_ib_changes;
    s_last_light_changes    = s_light_changes;
    s_last_tex_changes      = s_tex_changes;
    s_last_rs_changes       = s_rs_changes;
    s_last_tss_changes      = s_tss_changes;
    s_last_draw_calls       = s_draw_calls;
}

// ===========================================================================
// Context-loss callbacks
// ===========================================================================
EM_BOOL WebGL2Wrapper::On_Context_Lost(int, const void*, void*)
{
    s_contextLost = true;
    if (s_cleanupHook) s_cleanupHook->ReleaseResources();
    return EM_TRUE;
}

EM_BOOL WebGL2Wrapper::On_Context_Restored(int, const void*, void*)
{
    s_contextLost = false;
    Compile_Compat_Shader();
    if (s_cleanupHook) s_cleanupHook->ReAcquireResources();
    return EM_TRUE;
}

// ===========================================================================
// RenderStateStruct
// ===========================================================================
RenderStateStruct::RenderStateStruct() : material(nullptr), index_buffer(nullptr)
{
    for (unsigned i = 0; i < MAX_VERTEX_STREAMS; ++i) vertex_buffers[i] = nullptr;
    for (unsigned i = 0; i < MAX_TEXTURE_STAGES; ++i) Textures[i]       = nullptr;
}

RenderStateStruct::~RenderStateStruct()
{
    REF_PTR_RELEASE(material);
    for (unsigned i = 0; i < MAX_VERTEX_STREAMS; ++i) REF_PTR_RELEASE(vertex_buffers[i]);
    REF_PTR_RELEASE(index_buffer);
    for (unsigned i = 0; i < MAX_TEXTURE_STAGES; ++i) REF_PTR_RELEASE(Textures[i]);
}

RenderStateStruct& RenderStateStruct::operator=(const RenderStateStruct& src)
{
    REF_PTR_SET(material, src.material);
    for (unsigned i = 0; i < MAX_VERTEX_STREAMS; ++i)
        REF_PTR_SET(vertex_buffers[i], src.vertex_buffers[i]);
    REF_PTR_SET(index_buffer, src.index_buffer);
    for (unsigned i = 0; i < MAX_TEXTURE_STAGES; ++i)
        REF_PTR_SET(Textures[i], src.Textures[i]);
    for (unsigned i = 0; i < MAX_LIGHTS; ++i) {
        LightEnable[i] = src.LightEnable[i];
        if (LightEnable[i]) Lights[i] = src.Lights[i];
    }
    shader             = src.shader;
    world              = src.world;
    view               = src.view;
    for (unsigned i = 0; i < MAX_VERTEX_STREAMS; ++i)
        vertex_buffer_types[i] = src.vertex_buffer_types[i];
    index_buffer_type  = src.index_buffer_type;
    vba_offset         = src.vba_offset;
    vba_count          = src.vba_count;
    iba_offset         = src.iba_offset;
    index_base_offset  = src.index_base_offset;
    return *this;
}

#endif // __EMSCRIPTEN__
