# Generals/web/CMakeLists.txt
#
# Emscripten / WebGL2 build for C&C Generals Zero Hour
#
# Prerequisites:
#   - emsdk installed and activated (run emsdk_env.bat / emsdk_env.sh first)
#   - Ninja installed (Visual Studio ships one, or: winget install Ninja-build.Ninja)
#
# Usage (Windows, from repo root):
#
#   mkdir build_web && cd build_web
#   emcmake cmake -S ../Generals/web -B . -G Ninja -DCMAKE_BUILD_TYPE=Release -DRTS_ZEROHOUR=1
#   ninja
#
# Usage (Linux/macOS, from repo root):
#
#   mkdir build_web && cd build_web
#   emcmake cmake -S ../Generals/web -B . -G Ninja -DCMAKE_BUILD_TYPE=Release -DRTS_ZEROHOUR=1
#   ninja -j$(nproc)
#
# Outputs: generals.js  generals.wasm  generals.data  index.html

cmake_minimum_required(VERSION 3.25)
project(g_generals_web LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Require Emscripten toolchain
# ---------------------------------------------------------------------------
if (NOT EMSCRIPTEN)
    message(FATAL_ERROR
        "This CMakeLists is for Emscripten builds only.\n"
        "Configure with:  emcmake cmake ...")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Source roots (relative to this file which lives in Generals/)
# ---------------------------------------------------------------------------
# Root directories (web/ is one level inside Generals/)
set(GENERALS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(REPO_ROOT     "${GENERALS_ROOT}/..")
set(SRC_ROOT      "${GENERALS_ROOT}/Code")
set(CORE_ROOT     "${REPO_ROOT}/Core")
set(DEPS_ROOT     "${REPO_ROOT}/Dependencies")
set(WW3D2_DIR     "${SRC_ROOT}/Libraries/Source/WWVegas/WW3D2")
set(CORE_WW3D2    "${CORE_ROOT}/Libraries/Source/WWVegas/WW3D2")
set(MAIN_DIR      "${SRC_ROOT}/Main")

# ---------------------------------------------------------------------------
# Collect WW3D2 sources (Generals-local copy + Core copy)
# webgl2wrapper.cpp added, dx8wrapper.cpp excluded
# ---------------------------------------------------------------------------
file(GLOB_RECURSE WW3D2_SOURCES
    "${WW3D2_DIR}/*.cpp"
    "${CORE_WW3D2}/*.cpp"
)
list(FILTER WW3D2_SOURCES EXCLUDE REGEX "dx8wrapper\\.cpp$")

# ---------------------------------------------------------------------------
# Generals game engine sources
# ---------------------------------------------------------------------------
file(GLOB_RECURSE GENERALS_SOURCES
    "${SRC_ROOT}/GameEngine/Source/*.cpp"
    "${SRC_ROOT}/GameEngineDevice/Source/*.cpp"
    "${SRC_ROOT}/Libraries/Source/*.cpp"
)
list(FILTER GENERALS_SOURCES EXCLUDE REGEX "WW3D2/")
list(FILTER GENERALS_SOURCES EXCLUDE REGEX "WinMain\\.cpp$")

# ---------------------------------------------------------------------------
# Core engine sources
# ---------------------------------------------------------------------------
file(GLOB_RECURSE CORE_SOURCES
    "${CORE_ROOT}/GameEngine/Source/*.cpp"
    "${CORE_ROOT}/GameEngineDevice/Source/*.cpp"
    "${CORE_ROOT}/Libraries/Source/*.cpp"
)
list(FILTER CORE_SOURCES EXCLUDE REGEX "WW3D2/")

# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
set(MAIN_SOURCES
    "${MAIN_DIR}/EmscriptenMain.cpp"
)

# ---------------------------------------------------------------------------
# Executable target
# ---------------------------------------------------------------------------
add_executable(g_generals_web
    ${WW3D2_SOURCES}
    ${GENERALS_SOURCES}
    ${CORE_SOURCES}
    ${MAIN_SOURCES}
)

# ---------------------------------------------------------------------------
# Include paths
# ---------------------------------------------------------------------------
target_include_directories(g_generals_web PRIVATE
    # Generals
    "${SRC_ROOT}/GameEngine/Include"
    "${SRC_ROOT}/GameEngine/Include/Precompiled"
    "${SRC_ROOT}/GameEngineDevice/Include"
    "${SRC_ROOT}/Libraries/Source/WWVegas"
    "${SRC_ROOT}/Libraries/Source/WWVegas/WW3D2"
    "${SRC_ROOT}/Libraries/Source/WWVegas/WWLib"
    "${SRC_ROOT}/Libraries/Source/WWVegas/WWMath"
    "${SRC_ROOT}/Libraries/Source/WWVegas/WWDebug"
    "${SRC_ROOT}/Libraries/Source/WWVegas/WWSaveLoad"
    "${SRC_ROOT}/Main"
    # Core
    "${CORE_ROOT}/GameEngine/Include"
    "${CORE_ROOT}/GameEngineDevice/Include"
    "${CORE_ROOT}/Libraries/Source/WWVegas"
    "${CORE_ROOT}/Libraries/Source/WWVegas/WW3D2"
    "${CORE_ROOT}/Libraries/Source/WWVegas/WWLib"
    "${CORE_ROOT}/Libraries/Source/WWVegas/WWMath"
    "${CORE_ROOT}/Libraries/Source/WWVegas/WWDebug"
    "${CORE_ROOT}/Libraries/Source/WWVegas/WWAudio"
    "${CORE_ROOT}/Libraries/Source/WWVegas/WWSaveLoad"
    "${CORE_ROOT}/Libraries/Source/WWVegas/WWStub"
    "${CORE_ROOT}/Libraries/Include"
    # Dependencies
    "${DEPS_ROOT}/Utility/Utility"
    "${DEPS_ROOT}/Utility/Include"
)

# ---------------------------------------------------------------------------
# Compile definitions
# ---------------------------------------------------------------------------
option(RTS_ZEROHOUR "Build Zero Hour (vs base Generals)" ON)

target_compile_definitions(g_generals_web PRIVATE
    $<IF:$<BOOL:${RTS_ZEROHOUR}>,RTS_ZEROHOUR=1,RTS_GENERALS=1>
    __EMSCRIPTEN__=1         # should already be set by toolchain, but be explicit
    NDEBUG                   # swap for RTS_DEBUG if you want a debug web build
)

# ---------------------------------------------------------------------------
# Emscripten link flags
# ---------------------------------------------------------------------------
set_target_properties(g_generals_web PROPERTIES
    OUTPUT_NAME "generals"
    SUFFIX      ".html"      # emits generals.html + .js + .wasm + .data
)

target_link_options(g_generals_web PRIVATE
    # WebGL2
    -sUSE_WEBGL2=1
    -sFULL_ES3=1

    # Memory
    -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=134217728    # 128 MB

    # WASM output
    -sWASM=1

    # Exports needed by the JS glue
    -sEXPORTED_RUNTIME_METHODS=["cwrap","ccall","UTF8ToString","FS"]
    -sEXPORTED_FUNCTIONS=["_main","_malloc","_free"]

    # Pack assets directory into .data file
    # Adjust the path to wherever your game data lives relative to the build dir
    --preload-file ${CMAKE_CURRENT_SOURCE_DIR}/../Run/Data@/data

    # Use the custom HTML shell defined below
    --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/index_shell.html

    # Debug helpers (comment out for release)
    # -sASSERTIONS=2
    # -sSAFE_HEAP=1
    # -g2
)

# ---------------------------------------------------------------------------
# Optional: pthreads support
# If you enable this, GL calls MUST still happen on the main thread.
# ---------------------------------------------------------------------------
# target_compile_options(g_generals_web PRIVATE -pthread)
# target_link_options(g_generals_web PRIVATE -sUSE_PTHREADS=1 -sPTHREAD_POOL_SIZE=4)

message(STATUS "Web build configured. Run: emmake make -j$(nproc)")
message(STATUS "Serve build output with:  python3 -m http.server 8080")
message(STATUS "Open:                     http://localhost:8080/generals.html")
