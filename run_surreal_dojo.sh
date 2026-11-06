#!/bin/bash
# ==============================================================================
# AION SURREAL 3D ENGINE - LIVE DOJO LAUNCHER
# Launches the interactive 3D PBR Dojo window on the Linux desktop (:0)
# Automatically uses NVIDIA GeForce GTX 1060 6GB via PRIME Render Offload
# when no VFIO passthrough VM is running!
# ==============================================================================
cd /root/coding/MeinOS/surreal_engine
export DISPLAY=:0
if [ -e /dev/nvidia0 ] && [ -e /sys/bus/pci/drivers/nvidia/0000:01:00.0 ]; then
    export __NV_PRIME_RENDER_OFFLOAD=1
    export __NV_PRIME_RENDER_OFFLOAD_PROVIDER=NVIDIA-G0
    export __GLX_VENDOR_LIBRARY_NAME=nvidia
    export __VK_LAYER_NV_optimus=NVIDIA_only
fi
./surreal_live_dojo "$@"
