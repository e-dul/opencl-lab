# ── Deployment Demo — Multi-stage Dockerfile ──────────────────────────────────
#
# WHY build from repo root:
#   This module depends on common/ headers that live two levels above.
#   Docker COPY paths are relative to the build context, so the context must be
#   the repository root.
#
# Build command (from repo root):
#   docker build -f 04_Addons/4_3_Deployment/Dockerfile -t deployment_demo_test .
#
# Run command (from repo root):
#   mkdir -p docker_out
#
#   CPU fallback (PoCL — no GPU required):
#   docker run --rm \
#     -v $(pwd)/assets:/assets \
#     -v $(pwd)/docker_out:/output \
#     deployment_demo_test \
#     ./deployment_demo --input /assets/sample.bmp --output /output/output.bmp
#
#   AMD/Intel GPU passthrough (host driver ICD + DRI device):
#   docker run --rm \
#     --device /dev/dri \
#     -v /etc/OpenCL/vendors:/etc/OpenCL/vendors:ro \
#     -v $(pwd)/assets:/assets \
#     -v $(pwd)/docker_out:/output \
#     -e GPU=AMD \
#     deployment_demo_test \
#     ./deployment_demo --input /assets/sample.bmp --output /output/output.bmp

# ── Stage 1: Builder ──────────────────────────────────────────────────────────
# Full dev image: compiles deployment_demo from source.
FROM ubuntu:24.04 AS builder

# Avoid interactive prompts during apt installs.
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        git \
        ca-certificates \
        ocl-icd-opencl-dev \
        pocl-opencl-icd \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

# Copy common/ and vendor/ — needed by CMakeLists.txt include(../../common/common.cmake).
# WHY copy whole common/: FetchContent in common.cmake fetches stb + CLI11 at
# configure time over git; we need the cmake helpers present at configure start.
COPY common/ /src/common/

# Copy the module source.
COPY 04_Addons/4_3_Deployment/ /src/04_Addons/4_3_Deployment/

WORKDIR /src/04_Addons/4_3_Deployment
RUN rm -rf build \
    && cmake -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --config Release

# ── Stage 2: Runtime ──────────────────────────────────────────────────────────
# Lean image: only ICD loader, PoCL, the compiled binary, and its kernels.
# WHY pocl-opencl-icd: provides a CPU OpenCL device without hardware GPU,
# ensuring the binary runs in CI without driver passthrough.
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        ocl-icd-libopencl1 \
        pocl-opencl-icd \
    && rm -rf /var/lib/apt/lists/*

# Copy only the compiled binary and its kernels from the builder stage.
COPY --from=builder /src/04_Addons/4_3_Deployment/build/deployment_demo /app/deployment_demo
COPY --from=builder /src/04_Addons/4_3_Deployment/build/kernels         /app/kernels

WORKDIR /app

# Default command: process the mounted asset and write to the mounted output dir.
# Mount assets: -v <host_assets>:/assets
# Mount output: -v <host_out>:/output
CMD ["./deployment_demo", "--input", "/assets/sample.bmp", "--output", "/output/output.bmp"]
