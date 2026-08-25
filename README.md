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
SapanaEngine/                <- own git repo, own version control
├── engine/                  <- engine code (ECS, scene, asset systems, renderer wrapper)
├── game/                    <- actual game(s) built on the engine
├── third_party/
│   └── DiligentEngine/      <- git submodule, pinned to a specific commit/tag
├── assets/
├── CMakeLists.txt           <- top-level build, add_subdirectory(third_party/DiligentEngine)
└── .gitignore
```

Diligent is pulled in as a **git submodule** (not a manual clone) so the whole team builds against an identical, pinned commit. Upgrades are deliberate via `git submodule update --remote`, not accidental surprises.

## Dev environment

Reference machine: **Linux Mint 22.3 "Zena"** (Ubuntu 24.04 / Noble base).

| Component | Version / notes |
| --- | --- |
| Compiler | g++ 13.3.0 (via `build-essential`) |
| Build system | CMake 3.28.3 + Ninja |
| GPU | Intel HD Graphics 620 (Kaby Lake, integrated) — Vulkan 1.4 via Mesa. Fine for development; not representative of target performance for heavier scenes |
| Vulkan SDK | LunarG APT repo (`vulkan-sdk`, noble distro codename) |
| Windowing libs | `libx11-dev`, `libxrandr-dev`, `libxinerama-dev`, `libxcursor-dev`, `libxi-dev`, `libwayland-dev` |
| Editor | VS Code (recommended) with C/C++ and CMake Tools extensions |

### Paths

| Path | Purpose |
| --- | --- |
| `~/SapanaEngine` | Project repo |
| `~/SapanaEngine/third_party/DiligentEngine` | Diligent Engine submodule |
| `~/SapanaEngine/build/Linux` | Build output (once top-level CMake is set up) |

Default build config: **Debug** (`-DCMAKE_BUILD_TYPE=Debug`) — validation layers and assertions active.

## Setup (Ubuntu / Mint / Debian-based)

```bash
sudo apt update
sudo apt install build-essential gdb git cmake ninja-build
sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libwayland-dev

wget -qO- https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo tee /etc/apt/trusted.gpg.d/lunarg.asc
sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-noble.list https://packages.lunarg.com/vulkan/lunarg-vulkan-noble.list
sudo apt update
sudo apt install vulkan-sdk

vulkaninfo --summary   # verify

git clone --recursive <SapanaEngine repo URL> ~/SapanaEngine
```

The `--recursive` flag pulls in the DiligentEngine submodule automatically — no separate Diligent install is needed by team members.

**Other platforms:** swap `apt` / `noble` for your distro's package manager and LunarG codename; adjust entirely for Windows / macOS.
