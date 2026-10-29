#!/bin/bash
# ==============================================================================
# AION SURREAL 3D ENGINE - LIVE DOJO LAUNCHER
# Launches the interactive 3D PBR Dojo window on the Linux desktop (:0)
# ==============================================================================
cd /root/coding/MeinOS/surreal_engine
export DISPLAY=:0
./surreal_live_dojo "$@"

