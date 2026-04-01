#!/usr/bin/env bash
# appimage.sh — Package deployment as a Linux AppImage
#
# WHY AppImage: a single self-contained file that runs on any Linux distro
# without installation, matching the "runs on the customer's machine" goal.
#
# Prerequisites (must be in PATH before running this script):
#   - linuxdeploy       https://github.com/linuxdeploy/linuxdeploy/releases
#   - appimagetool      https://github.com/AppImage/AppImageKit/releases
#
# Usage:
#   cd 04_Addons/4_3_Deployment
#   cmake -B build && cmake --build build
#   bash appimage.sh
#
# Output: deployment-${ARCH}.AppImage

set -euo pipefail

# Architecture detection — substituted into download URLs and output filename.
ARCH=$(uname -m)

BINARY="build/deployment"
KERNELS_SRC="build/kernels"
APPDIR="AppDir"

# ── Tool bootstrap ────────────────────────────────────────────────────────────
# Auto-download linuxdeploy and appimagetool into the working directory if not
# already in PATH. APPIMAGE_EXTRACT_AND_RUN=1 bypasses FUSE (needed in Docker
# or environments without /dev/fuse).

export APPIMAGE_EXTRACT_AND_RUN=1

if ! command -v linuxdeploy &>/dev/null; then
    if [ ! -f "./linuxdeploy" ]; then
        echo "[bootstrap] linuxdeploy not found — downloading to ./linuxdeploy ..."
        wget -q --show-progress \
            -O linuxdeploy \
            "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${ARCH}.AppImage"
        chmod +x linuxdeploy
    fi
    export PATH="$PWD:$PATH"
fi

if ! command -v appimagetool &>/dev/null; then
    if [ ! -f "./appimagetool" ]; then
        echo "[bootstrap] appimagetool not found — downloading to ./appimagetool ..."
        wget -q --show-progress \
            -O appimagetool \
            "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-${ARCH}.AppImage"
        chmod +x appimagetool
    fi
    export PATH="$PWD:$PATH"
fi

if [ ! -f "$BINARY" ]; then
    echo "ERROR: binary not found at $BINARY — run cmake -B build && cmake --build build first."
    exit 1
fi

# ── Build AppDir structure ────────────────────────────────────────────────────

echo "[1/4] Creating AppDir structure..."
rm -rf "$APPDIR"
mkdir -p "${APPDIR}/usr/bin"
# Kernels are data files: install under share/ so the binary can find them
# via a runtime search path if needed.
mkdir -p "${APPDIR}/usr/share/deployment/kernels"
mkdir -p "${APPDIR}/usr/share/applications"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/256x256/apps"

# Copy binary
cp "$BINARY" "${APPDIR}/usr/bin/deployment"

# Copy OpenCL kernel source files next to the binary so the runtime path
# (bin_dir / "kernels/") resolves correctly inside the AppImage squashfs mount.
cp -r "${KERNELS_SRC}/." "${APPDIR}/usr/bin/kernels/"

# Also install under share/ for FHS compliance.
cp -r "${KERNELS_SRC}/." "${APPDIR}/usr/share/deployment/kernels/"

# WHY .desktop + icon: appimagetool requires both to build the AppImage.
# The desktop file declares the app entry point; the icon is embedded in the
# AppImage and shown in launchers.
cat > "${APPDIR}/usr/share/applications/deployment.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=deployment
Exec=deployment
Icon=deployment
Categories=Utility;
EOF

# Minimal 1×1 PNG icon (avoids requiring ImageMagick or external assets).
# Generated with: python3 -c "import base64,sys; sys.stdout.buffer.write(base64.b64decode(...))"
printf '\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR\x00\x00\x00\x01\x00\x00\x00\x01\x08\x02\x00\x00\x00\x90wS\xde\x00\x00\x00\x0cIDATx\x9cc\xf8\x0f\x00\x00\x01\x01\x00\x05\x18\xd8N\x00\x00\x00\x00IEND\xaeB`\x82' \
    > "${APPDIR}/usr/share/icons/hicolor/256x256/apps/deployment.png"

echo "[2/4] Deploying shared libraries (excluding libOpenCL.so.1)..."
# WHY --exclude-library libOpenCL.so.1: bundling the ICD loader silently bypasses
# the host GPU driver — see Deployment.md §Concept: The ICD Loader for the full explanation.
linuxdeploy \
    --appdir "$APPDIR" \
    --executable "${APPDIR}/usr/bin/deployment" \
    --exclude-library libOpenCL.so.1

echo "[3/4] Bundling AppImage..."
appimagetool "$APPDIR" "deployment-${ARCH}.AppImage"

echo "[4/4] Done: deployment-${ARCH}.AppImage"
