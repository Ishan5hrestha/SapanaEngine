# SapanaEngine

Custom C++ game engine with an ECS-based scene, asset, and gameplay layer built on top of [Diligent Engine](https://github.com/DiligentGraphics/DiligentEngine) as the rendering abstraction.

## Why Diligent

| Approach | Trade-off |
| --- | --- |
| Full custom renderer (raw Vulkan) | Too slow to reach shipping games |
| Full existing engine (Godot / Bevy / Unreal) | Little real engine-building ownership |
| **Diligent Engine (middle tier)** | Real GPU/pipeline understanding without hand-rolling Vulkan sync primitives |

Diligent is a **dependency**, not the codebase itself. It exposes modern pipeline state and resource binding concepts, uses **HLSL** as the universal shading language, and targets **Vulkan / D3D12 / Metal / D3D11 / GL**. This is not a raw Vulkan implementation and not bgfx.

## Repo structure

```text
SapanaEngine/
├── engine/                  <- ECS, scene, assets, render, physics
│   └── include/sapana/
│       ├── camera/ input/ ecs/ assets/ scene/ render/
│       └── physics/         <- Sapana physics API (Jolt hidden behind pimpl)
├── game/sandbox_cube/       <- sandbox sample + assets (scenes, config, meshes)
├── third_party/
│   ├── DiligentEngine/      <- git submodule (pinned)
│   └── nlohmann/            <- json.hpp
├── CMakeLists.txt
├── rebuld.sh                <- incremental build (preferred day-to-day)
└── reconf_n_rebuild.sh      <- wipe build/Linux and full reconfigure (slow)
```

| Dependency | How it is obtained |
| --- | --- |
| Diligent Engine | Git submodule under `third_party/DiligentEngine` |
| EnTT | Fetched with DiligentFX |
| **Jolt Physics** | CMake `FetchContent`, pinned tag **`v5.3.0`** (not a submodule) |

## Dev environment

Reference machine: **Linux Mint 22.3 "Zena"** (Ubuntu 24.04 / Noble base).

| Component | Version / notes |
| --- | --- |
| Compiler | g++ 13.3.0 (via `build-essential`) |
| Build system | CMake 3.28.3 + Ninja |
| GPU | Intel HD Graphics 620 (Kaby Lake, integrated) — Vulkan 1.4 via Mesa |
| Vulkan SDK | LunarG APT repo (`vulkan-sdk`, noble) |
| Windowing libs | `libx11-dev`, `libxrandr-dev`, `libxinerama-dev`, `libxcursor-dev`, `libxi-dev`, `libwayland-dev` |

Default build config: **Debug** (`-DCMAKE_BUILD_TYPE=Debug`).

## Setup (Ubuntu / Mint / Debian-based)

```bash
sudo apt update
sudo apt install build-essential gdb git cmake ninja-build
sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libwayland-dev

wget -qO- https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo tee /etc/apt/trusted.gpg.d/lunarg.asc
sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-noble.list https://packages.lunarg.com/vulkan/lunarg-vulkan-noble.list
sudo apt update
sudo apt install vulkan-sdk

git clone --recursive <SapanaEngine repo URL> ~/SapanaEngine
```

First configure fetches Jolt once into the CMake build tree (`build/Linux/_deps/...`). Day-to-day:

```bash
bash rebuld.sh
cd build/Linux/game/sandbox_cube && ./Tutorial02_Cube
```

`rebuld.sh` builds the sandbox **and copies the entire** `game/sandbox_cube/assets/` tree next to the binary (configs, scenes, meshes, shaders). Edit files under `assets/`, run `rebuld.sh`, relaunch — no manual per-file copies. JSON is loaded at startup (restart the app to pick up changes).

Use `bash reconf_n_rebuild.sh` only when CMake targets are missing or the build tree is corrupted (it deletes `build/Linux`).

## Physics (Jolt)

Simulation is **opt-in** and separate from rendering:

- Scene entities may include a `"physics"` block (`body`: `static`|`dynamic`, `shape`: `box`|`plane`, mass/friction/restitution).
- No `physics` block → mesh-only (e.g. decorative props).
- `config/physics.json` sets gravity and fixed timestep (`1/60` by default).
- Each frame, `PhysicsSystem` steps Jolt and writes **dynamic** poses back into `ecs::Transform` before draw.
- Public headers never expose Jolt types — see [engine/include/sapana/physics/README.md](engine/include/sapana/physics/README.md).

Sandbox demo: green ground is **static**; cubes are **dynamic**; `Gltf_Cube` is visual-only. Entity **Drone** (`meshes/drone.glb`) is dynamic with gravity factor 0 for flying.

**Controls:** WASD/QE move, mouse look, **M** toggle cursor, **K** toggle freelook camera vs drone + third-person chase cam.

### Physics extension roadmap

- Contact callbacks for gameplay
- Heightfield terrain collider
- Character controller (`CharacterVirtual`)
- Extra collision layers / triggers
- Constraints and kinematic platforms

## Coordinates

- **Y-up**, right-handed style consistent with Diligent samples
- Scene `rotation_degrees` are Euler degrees; physics converts at the Jolt boundary
- Prefer meters as world units

**Other platforms:** swap `apt` / `noble` for your distro; adjust for Windows / macOS as needed.
