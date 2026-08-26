#!/usr/bin/env bash
# Incremental sandbox build + always sync assets (JSON/meshes) next to the binary.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD="${ROOT}/build/Linux"
OUT="${BUILD}/game/sandbox_cube"

cmake --build "${BUILD}" -j"$(nproc)" --target SapanaSandbox

# Covers JSON-only edits when CMake skips POST_BUILD because nothing recompiled.
mkdir -p "${OUT}"
cp -a "${ROOT}/game/sandbox_cube/assets/." "${OUT}/"
echo "Assets synced -> ${OUT}"
