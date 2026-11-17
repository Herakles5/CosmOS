#!/bin/bash
# ==============================================================================
# SMASH CATS 3D: SURREAL EDITION - STANDALONE LINUX LAUNCHER
# Launches the C++20 Surreal 3D OpenGL 4.6 / AVX2 Smash Cats arena game.
# Automatically uses NVIDIA GeForce GTX 1060 6GB via PRIME Render Offload
# when no VFIO passthrough VM is active!
# ==============================================================================
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export DISPLAY="${DISPLAY:-:0}"
if [ -e /dev/nvidia0 ] && [ -e /sys/bus/pci/drivers/nvidia/0000:01:00.0 ]; then
    export __NV_PRIME_RENDER_OFFLOAD=1
    export __NV_PRIME_RENDER_OFFLOAD_PROVIDER=NVIDIA-G0
    export __GLX_VENDOR_LIBRARY_NAME=nvidia
    export __VK_LAYER_NV_optimus=NVIDIA_only
fi

if [ -x "${SCRIPT_DIR}/surreal_engine/surreal-smash-cats" ]; then
    cd "${SCRIPT_DIR}/surreal_engine" || exit 1
    exec ./surreal-smash-cats "$@"
elif command -v smash-cats >/dev/null 2>&1; then
    exec smash-cats "$@"
else
    echo "[ERROR] surreal-smash-cats binary not found. Run: make -C ${SCRIPT_DIR}/surreal_engine surreal-smash-cats"
    exit 1
fi
