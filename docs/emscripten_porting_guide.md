# Porting C&C Generals ZH to the Web
## DX8Wrapper → WebGL2Wrapper via Emscripten

---

## 1. Prerequisites

```bash
# Install Emscripten SDK (emsdk)
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh          # add em++, emcc to PATH

# Verify
em++ --version                 # should print 3.x.x
```

---

## 2. Minimal compile command

```bash
em++ \
  -std=c++17 \
  -O2 \
  -s USE_WEBGL2=1 \           # enables WebGL2 / GLES3 headers + JS glue
  -s FULL_ES3=1 \             # expose full GLES3 surface (required for UBOs etc.)
  -s ALLOW_MEMORY_GROWTH=1 \  # heap can expand; needed for asset loading
  -s INITIAL_MEMORY=134217728 \ # 128 MB initial heap
  -s WASM=1 \
  -s EXPORTED_RUNTIME_METHODS='["cwrap","ccall","UTF8ToString"]' \
  -s EXPORTED_FUNCTIONS='["_main","_malloc","_free"]' \
  --preload-file assets/ \    # embed asset directory into .data file
  -o build/generals.html \    # emits .html + .js + .wasm + .data
  src/webgl2wrapper.cpp \
  src/game_main.cpp \
  # ... rest of your source files
```

> **Tip:** During development replace `-O2` with `-O0 -g` and add
> `-s ASSERTIONS=2 -s SAFE_HEAP=1` to catch memory errors early.

---

## 3. HTML canvas setup

Emscripten targets a `<canvas>` element.  The default generated `.html`
works fine for testing.  For a custom page:

```html
<!DOCTYPE html>
<html>
<head>
  <style>
    body { margin: 0; background: #000; }
    canvas { display: block; width: 100vw; height: 100vh; }
  </style>
</head>
<body>
  <!-- The id must match the canvas_id passed to WebGL2Wrapper::Init() -->
  <canvas id="canvas" oncontextmenu="event.preventDefault()"></canvas>

  <script>
    var Module = {
      canvas: document.getElementById('canvas'),
      // Resize the canvas to actual pixel size
      onRuntimeInitialized: function() {
        const dpr = window.devicePixelRatio || 1;
        Module.canvas.width  = window.innerWidth  * dpr;
        Module.canvas.height = window.innerHeight * dpr;
      }
    };
  </script>
  <script src="generals.js"></script>
</body>
</html>
```

---

## 4. Wrapper init in your game main

```cpp
// game_main.cpp  (replaces the Win32 WinMain entry point)

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>

// Forward-declare the per-frame function
void WebMain_Loop();

int main()
{
    // Init subsystems
    WebGL2Wrapper::Init("#canvas", 1280, 720);

    // Hand control to the browser event loop.
    // emscripten_set_main_loop calls WebMain_Loop ~60 times/sec.
    // fps=0 means "use requestAnimationFrame" (preferred).
    // simulate_infinite_loop=1 means main() never returns.
    emscripten_set_main_loop(WebMain_Loop, 0, 1);

    return 0;   // never reached
}

void WebMain_Loop()
{
    WebGL2Wrapper::Begin_Scene();

    // --- your normal per-frame game update + render here ---
    // WW3D::Render(scene, camera, ...);

    WebGL2Wrapper::End_Scene();
}

#else
// Original Win32 entry point unchanged
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) { ... }
#endif
```

---

## 5. File I/O: replacing Win32 file access

Emscripten provides a virtual filesystem (MEMFS / NODEFS / WASMFS).
Assets packed with `--preload-file assets/` are available at the same path
via standard `fopen` / `std::ifstream`.

```cpp
// This works unchanged in Emscripten:
FILE* f = fopen("assets/maps/USA01.map", "rb");

// If you use Westwood's FileFactoryClass or similar, redirect it to
// use the virtual FS paths.  No Win32 HANDLE / CreateFile needed.
```

For **save games** / **config files** use `IDBFS` (IndexedDB-backed persistence):

```cpp
// Mount IndexedDB filesystem for writable data
EM_ASM(
    FS.mkdir('/saves');
    FS.mount(IDBFS, {}, '/saves');
    FS.syncfs(true, function(err) {});   // true = populate from IDB on load
);

// After writing, flush back:
EM_ASM( FS.syncfs(false, function(err) {}); );
```

---

## 6. Threading

Emscripten supports pthreads via WebWorkers (`-s USE_PTHREADS=1`), but
WebGL calls **must** happen on the main thread (same restriction as DX8's
`_DX8SingleThreaded` flag).

```bash
# If you enable pthreads:
em++ ... -s USE_PTHREADS=1 -s PTHREAD_POOL_SIZE=4 \
         -s PROXY_TO_PTHREAD=0   # keep GL on main thread
```

The wrapper's context-lost / restored callbacks already run on the main
thread via Emscripten's HTML5 API.

---

## 7. Key DX8 → WebGL2 concept mapping (quick reference)

| DX8 concept | WebGL2 replacement | Notes |
|---|---|---|
| `IDirect3DDevice8` | `EMSCRIPTEN_WEBGL_CONTEXT_HANDLE` | Created via `emscripten_webgl_create_context` |
| `IDirect3DTexture8*` | `GLuint` | `glGenTextures / glTexImage2D` |
| `IDirect3DVertexBuffer8*` | `GLuint` VBO | `glGenBuffers / GL_ARRAY_BUFFER` |
| `IDirect3DIndexBuffer8*` | `GLuint` IBO | `glGenBuffers / GL_ELEMENT_ARRAY_BUFFER` |
| `D3DVERTEXSHADER` | `GLuint` GLSL program | `glCreateProgram / glLinkProgram` |
| `D3DPIXELSHADER` | (same linked program) | No separate pixel shader object |
| `SetRenderState` | individual GL calls | `glEnable/Disable`, `glDepthFunc`, `glBlendFunc` etc. |
| `SetTextureStageState` | uniforms in compat shader | `uTexColorOp[stage]` etc. |
| `D3DLIGHT8 x4` | uniform struct array | `uLights[4]` in compat shader |
| `D3DMATRIX` world/view/prj | `glUniformMatrix4fv` | Transpose on upload (row→col major) |
| `D3DPOOL_MANAGED` | `GL_STATIC_DRAW` | Driver manages no pool; use STATIC for geometry |
| `D3DPOOL_DEFAULT` dynamic | `GL_DYNAMIC_DRAW` / `GL_STREAM_DRAW` | |
| `Present` / flip | none (implicit) | Browser compositor swaps after rAF |
| Lost device | `webglcontextlost` event | Handled via `emscripten_set_webglcontextlost_callback` |
| `D3DRS_ZBIAS` | `glPolygonOffset` | `glEnable(GL_POLYGON_OFFSET_FILL)` |
| `CopyRects` (surface blit) | `glBlitFramebuffer` | FBO blit |
| `UpdateTexture` | `glTexSubImage2D` | |
| Stencil | `glStencilFunc / glStencilOp` | WebGL2 supports stencil fully |
| Multiple render targets | FBOs + `glFramebufferTexture2D` | Create one FBO per render target |
| Swap chain (multi-window) | N/A (single canvas) | Not needed in a browser |

---

## 8. Incremental migration strategy

The safest way to avoid breaking the entire engine at once:

```
Step 1 – Compile guard
    Add  #ifdef __EMSCRIPTEN__  around WebGL2Wrapper.
    All other platforms still compile DX8Wrapper unchanged.

Step 2 – Alias
    Uncomment the typedef at the bottom of webgl2wrapper.h:
        typedef WebGL2Wrapper DX8Wrapper;
    This lets ALL existing call sites (DX8Wrapper::Set_Transform, etc.)
    compile against the new implementation without a search-and-replace.

Step 3 – Replace Win32 types at call sites
    The only places that break are those passing raw DX8 struct pointers:
        D3DLIGHT8*        →  WGL_Light*
        D3DVIEWPORT8*     →  WebGL2Wrapper::Viewport*
        D3DMATERIAL8*     →  pass as void*, wrapper reads only diffuse/spec
        IDirect3DTexture8* →  GLuint   (wrap in TextureClass accessor)
    Use a thin adapter struct for each to minimize search-and-replace scope.

Step 4 – Shaders
    Code that creates D3D vertex/pixel shaders needs to be replaced with
    GLSL.  Use the compat shader as a fallback for everything else.
    Port shaders file-by-file; wrap with:
        #ifdef __EMSCRIPTEN__
            // GLSL version
        #else
            // Original HLSL / D3D assembly
        #endif

Step 5 – Asset pipeline
    Run your texture assets through a converter that produces WebP or PNG
    (Emscripten's SDL_image or libpng handle these fine).
    DDS with DXT1/3/5 needs either:
        a) Run-time decompression (crunch / squish library, slow)
        b) Convert to ASTC/ETC2 for WebGL2 hardware compression support
           (recommended for production)

Step 6 – Audio
    Replace Miles Sound System / DirectSound with:
        - Emscripten's OpenAL emulation  (easiest: link -lopenal)
        - OR Web Audio API via EM_ASM{}  (more control)

Step 7 – Input
    Replace Win32 WM_KEYDOWN / DirectInput with:
        emscripten_set_keydown_callback
        emscripten_set_mousemove_callback
        emscripten_set_mousedown_callback
        (These fire on the main thread; pass events to your existing
         input manager class.)
```

---

## 9. Useful Emscripten flags for debugging

```bash
# Verbose GL error checking (significant perf hit, dev only)
-s GL_ASSERTIONS=1

# Catch out-of-bounds memory access
-s SAFE_HEAP=1

# Readable WASM names in browser devtools
-g2 --source-map-base /

# Profile with Chrome's GPU timeline
-s OFFSCREEN_FRAMEBUFFER=1    # needed for some profiling setups

# Inspect the virtual FS in the browser console:
# > FS.readdir('/')
# > FS.stat('assets/maps/USA01.map')
```

---

## 10. Expected pain points (in priority order)

1. **Vertex declaration format** – DX8 `D3DVERTEXELEMENT8` arrays must be
   translated to `glVertexAttribPointer` calls.  The attribute locations in
   the compat shader (`aPosition=0, aNormal=1, aColor=2, aTexCoord0=3…`)
   must match exactly.

2. **Texture formats** – DX8 surfaces expose many formats WebGL2 doesn't.
   Most common: `D3DFMT_A8R8G8B8` → `GL_RGBA8` (with BGRA→RGBA swizzle at
   upload time via `GL_BGRA_EXT` if the extension is available, or a
   CPU-side swap).

3. **Fixed-function alpha blending modes** – All standard D3DRS_SRCBLEND /
   DESTBLEND values map 1:1 to `GL_SRC_ALPHA` etc.  The wrapper handles
   this in `Apply_Blend_State()`.

4. **`D3DLOCK_DISCARD` / `D3DLOCK_NOOVERWRITE`** – Map these to
   `glMapBufferRange(GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT)`
   and `GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT` respectively.

5. **The `flimby()` function** – That function in the original header uses
   undefined-behaviour macros (`prevVer`, `nextVer`, `lnt`, `D3D2_BASE_VEC`
   that expand to nothing / `unsigned`).  It's dead code (the call site is
   commented out).  Delete it.
