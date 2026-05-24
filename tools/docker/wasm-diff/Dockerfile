# Emscripten + WABT for tools/run_wasm_differential_test.sh (native vs wasm SIMD digests).
# Pin version for reproducibility; bump when CI EMSDK advances.
FROM emscripten/emsdk:3.1.74

USER root
RUN apt-get update -qq \
  && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends wabt \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /src
