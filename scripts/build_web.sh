#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

EMSDK="${EMSDK:-${HOME}/Development/emsdk}"
if [[ ! -f "${EMSDK}/emsdk_env.sh" ]]; then
    echo "Emscripten SDK not found at '${EMSDK}'." >&2
    echo "Set EMSDK to the directory containing emsdk_env.sh." >&2
    exit 1
fi

# emsdk_env.sh configures emcc, em++, and the CMake toolchain environment.
export EMSDK_QUIET=1
source "${EMSDK}/emsdk_env.sh" >/dev/null

PRESET="${CADMIUM_WEB_PRESET:-emscripten}"
BUILD_DIR="${PROJECT_DIR}/build/emscripten"
if [[ -z "${CADMIUM_WEB_PRESET:-}" && -f "${PROJECT_DIR}/CMakeUserPresets.json" ]]; then
    if grep -q '"name"[[:space:]]*:[[:space:]]*"local-emscripten"' "${PROJECT_DIR}/CMakeUserPresets.json"; then
        PRESET="local-emscripten"
        BUILD_DIR="${PROJECT_DIR}/build/local-emscripten"
    fi
elif [[ "${PRESET}" == "local-emscripten" ]]; then
    BUILD_DIR="${PROJECT_DIR}/build/local-emscripten"
fi

DIST_DIR="${CADMIUM_WEB_DIST_DIR:-${PROJECT_DIR}/dist/web}"
ARCHIVE="${PROJECT_DIR}/dist/cadmium-web.zip"

if [[ ! -f "${BUILD_DIR}/CMakeCache.txt" || "${CADMIUM_WEB_RECONFIGURE:-0}" == "1" ]]; then
    echo "Configuring Cadmium Web build with preset '${PRESET}'..."
    cmake --preset "${PRESET}" -S "${PROJECT_DIR}"
else
    echo "Using configured Cadmium Web build in '${BUILD_DIR}'."
fi

echo "Building Cadmium Web target..."
cmake --build "${BUILD_DIR}" --target cadmium --config Release

echo "Staging upload files in '${DIST_DIR}'..."
cmake -E rm -rf "${DIST_DIR}"
cmake -E make_directory "${DIST_DIR}"
for artifact in index.html index.js index.wasm; do
    source_file="${BUILD_DIR}/bin/${artifact}"
    if [[ ! -f "${source_file}" ]]; then
        echo "Expected build artifact not found: ${source_file}" >&2
        exit 1
    fi
    cmake -E copy_if_different "${source_file}" "${DIST_DIR}/${artifact}"
done

cmake -E rm -f "${ARCHIVE}"
(
    cd "${DIST_DIR}"
    cmake -E tar cf "${ARCHIVE}" --format=zip index.html index.js index.wasm
)

echo "Web build ready:"
echo "  Directory: ${DIST_DIR}"
echo "  Archive:   ${ARCHIVE}"
