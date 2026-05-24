#!/usr/bin/env bash
# Run tools/run_wasm_differential_test.sh inside Docker (Emscripten + gcc + Node + wasm-objdump).
# Requires Docker only — no local emcc / wabt.
#
# From repo root (or via path below):
#   bash tools/run-wasm-diff-docker.sh
#
# Env:
#   CROARING_WASM_DIFF_IMAGE — image tag (default croaring/wasm-diff:local)
#   DOCKER_PLATFORM          — default linux/amd64 (emsdk official image)
#   WASM_DIFF_SKIP_SIMD_ARTIFACT_GUARD, WASM_DIFF_SKIP_LLVM_WASM_INTRINSIC_GUARD — forwarded when non-empty.

set -euo pipefail

SCRIPTPATH="$(cd "$(dirname "$0")" && pwd -P)"
ROOT="$(cd "$SCRIPTPATH/.." && pwd)"

IMAGE="${CROARING_WASM_DIFF_IMAGE:-croaring/wasm-diff:local}"
DOCKERFILE="$SCRIPTPATH/docker/wasm-diff/Dockerfile"
DOCKER_PLATFORM="${DOCKER_PLATFORM:-linux/amd64}"

command -v docker >/dev/null 2>&1 || {
  echo >&2 "Docker is required. See https://docs.docker.com/get-docker/"
  exit 1
}
docker info >/dev/null 2>&1 || {
  echo >&2 "Docker does not appear to be running (try 'docker info')."
  exit 1
}

echo "Building $IMAGE ($(basename "$DOCKERFILE"))..."
docker build --platform="$DOCKER_PLATFORM" -t "$IMAGE" -f "$DOCKERFILE" "$SCRIPTPATH/docker/wasm-diff"

DOCKER_OPTS=(--rm)
if [ -t 0 ]; then
  DOCKER_OPTS+=(-it)
fi

echo "Running wasm differential test in container..."
DOCKER_ENV=(
  -e CC=gcc
  -e TMPDIR=/tmp
)
# Forward optional SIMD attestation env into the container.
[[ -n "${WASM_DIFF_SKIP_SIMD_ARTIFACT_GUARD-}" ]] && DOCKER_ENV+=(-e "WASM_DIFF_SKIP_SIMD_ARTIFACT_GUARD=$WASM_DIFF_SKIP_SIMD_ARTIFACT_GUARD")
[[ -n "${WASM_DIFF_SKIP_LLVM_WASM_INTRINSIC_GUARD-}" ]] && DOCKER_ENV+=(-e "WASM_DIFF_SKIP_LLVM_WASM_INTRINSIC_GUARD=$WASM_DIFF_SKIP_LLVM_WASM_INTRINSIC_GUARD")

docker run "${DOCKER_OPTS[@]}" \
  --platform="$DOCKER_PLATFORM" \
  -v "$ROOT":"$ROOT":Z \
  -w "$ROOT" \
  -u "$(id -u):$(id -g)" \
  "${DOCKER_ENV[@]}" \
  "$IMAGE" \
  bash "$ROOT/tools/run_wasm_differential_test.sh"
