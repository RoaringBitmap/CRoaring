#!/usr/bin/env bash
# Run tools/run_wasm_differential_test.sh inside Docker (Emscripten + gcc + Node + wasm-objdump).
# Requires Docker only — no local emcc / wabt. Builds a small ephemeral image layout (pinned
# emscripten/emsdk + apt wabt) here — there is no Dockerfile checked into the repo.
#
# From repo root (or via path below):
#   bash tools/run-wasm-diff-docker.sh
#
# Env:
#   CROARING_WASM_DIFF_IMAGE — image tag (default croaring/wasm-diff:local)
#   DOCKER_PLATFORM          — default linux/amd64 (emsdk official image)
#   WASM_DIFF_SKIP_SIMD_ARTIFACT_GUARD, WASM_DIFF_SKIP_LLVM_WASM_INTRINSIC_GUARD —
#       forwarded into the container when set (local debugging only; forbidden in CI
#       via GITHUB_ACTIONS check inside run_wasm_differential_test.sh).

set -euo pipefail

SCRIPTPATH="$(cd "$(dirname "$0")" && pwd -P)"
ROOT="$(cd "$SCRIPTPATH/.." && pwd)"

IMAGE="${CROARING_WASM_DIFF_IMAGE:-croaring/wasm-diff:local}"
DOCKER_PLATFORM="${DOCKER_PLATFORM:-linux/amd64}"

command -v docker >/dev/null 2>&1 || {
  echo >&2 "Docker is required. See https://docs.docker.com/get-docker/"
  exit 1
}
docker info >/dev/null 2>&1 || {
  echo >&2 "Docker does not appear to be running (try 'docker info')."
  exit 1
}

BUILD_CTX="$(mktemp -d "${TMPDIR:-/tmp}/croaring-wasm-diff-docker-ctx.XXXXXX")"
trap 'rm -rf "${BUILD_CTX}"' EXIT

# Ephemeral Dockerfile: keep emscripten + wabt pin out of-tree (official image lacks wabt on PATH reliably).
cat >"${BUILD_CTX}/Dockerfile" <<'DOCKER_EOF'
FROM emscripten/emsdk:3.1.74

USER root
RUN apt-get update -qq \
  && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends wabt \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /src
DOCKER_EOF

echo "Building $IMAGE (pinned emscripten/emsdk:3.1.74 + wabt)..."
docker build --platform="$DOCKER_PLATFORM" -t "$IMAGE" "${BUILD_CTX}"

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
