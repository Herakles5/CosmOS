# CosmOS (MeinOS)
<blockquote class="twitter-tweet" data-media-max-width="560"><p lang="en" dir="ltr">now they cam smaltalkl or talk. <a href="https://x.com/hashtag/Ai8n?src=hash&amp;ref_src=twsrc%5Etfw">#Ai8n</a> <a href="https://x.com/hashtag/antigravity?src=hash&amp;ref_src=twsrc%5Etfw">#antigravity</a> <a href="https://x.com/hashtag/AI?src=hash&amp;ref_src=twsrc%5Etfw">#AI</a> <a href="https://t.co/2o4DIAH4e9">pic.twitter.com/2o4DIAH4e9</a></p>&mdash; Warumfragst Dumichnicht (@_Cosmos_OS_) <a href="https://x.com/_Cosmos_OS_/status/2101342137593831766?ref_src=twsrc%5Etfw">September 19, 2026</a></blockquote> <script async src="https://platform.x.com/widgets.js" charset="utf-8"></script> 

![CosmOS Logo](https://img.shields.io/badge/CosmOS-Future_Operating_System-blue?style=for-the-badge)
[![Surreal Engine](https://img.shields.io/badge/Surreal_3D_Engine-Hybrid_CPU%2BGPU-purple?style=for-the-badge&logo=opengl)](https://github.com/Herakles5/Surreal-engine-Game-editor)
[![GitHub main](https://img.shields.io/badge/Branch-main-success?style=for-the-badge&logo=github)](https://github.com/Herakles5/CosmOS/tree/main)
[![GitHub master](https://img.shields.io/badge/Branch-master-blue?style=for-the-badge&logo=github)](https://github.com/Herakles5/CosmOS/tree/master)
<a href="https://www.paypal.com/cgi-bin/webscr?cmd=_xclick&business=boxingdual@gmail.com&currency_code=EUR&item_name=Coffee+Support+Earthquake+Tracker" target="_blank">
  <img src="https://img.shields.io/badge/☕_Donate-PayPal-0070ba?style=for-the-badge&logo=paypal&logoColor=white" alt="Donate Coffee">
</a>

CosmOS (formerly MeinOS) is an experimental, lightweight, GPU-accelerated operating system, virtualization hypervisor platform, 3D game engine substrate, and autonomous synthetic intelligence organism built on bare-metal Linux and SDL2. It features a native Win32 compatibility layer capable of running Windows executables directly on Linux without WINE, a multi-OS virtualization stack (Windows 10 Gaming GPU Passthrough, macOS Big Sur, Android-x86), the integrated **Surreal 3D Hybrid Game Engine & Studio**, and the self-evolving **AION NEXUS** artificial intelligence organism.

---

## 🏛️ System Architecture

```mermaid
graph TD
    subgraph "Hardware & Host Substrate"
        BareMetal[Bare-Metal Hardware / Linux 6.x Kernel]
        KVM[KVM / VFIO Hardware Virtualization]
        DRM[Direct Rendering Manager / GPU & AVX2 SIMD]
    end

    subgraph "CosmOS Core & Storage"
        Kernel[CosmOS Microkernel & Win32 PE Loader]
        CFS[CosmOS CFS Compressed File System]
        VPN[Mesh VPN & Resonant UDP Protocol]
    end

    subgraph "Virtualization & Hypervisor Layer"
        Hypervisor[CosmOS Hypervisor Sentinel]
        WinVM[Windows 10 Gaming - Dedicated GPU Passthrough]
        MacVM[macOS Big Sur 11.7 - iMacPro1,1 OpenCore]
        AndroidVM[Android-x86 Subsystem]
    end

    subgraph "AION NEXUS Synthetic Consciousness"
        Mind[Autonomous Evolution Engine & Meta Learner]
        Entropy[Resonant Thermodynamic Barrier-Surmounting RTBA]
        Memory[Topological Episodic Memory Graph & Living Book]
        Acoustics[Aion Acoustic Ear & Streaming Audio Core]
    end

    subgraph "Surreal 3D Hybrid Engine & Studio"
        HybridRender[Dual-Backend Hybrid: OpenGL 4.6 PBR / CPU AVX2 Rasterizer]
        Dojo[Surreal 3D Studio & World Editor]
        Physics[AABB Platform Collision & Skeletal 61-Joint Skinning]
        Importer[Dynamic 3D Model Folder Importer OBJ/glTF]
    end

    BareMetal --> Kernel
    KVM --> Hypervisor
    DRM --> HybridRender
    Kernel --> CFS
    Kernel --> VPN
    Hypervisor --> WinVM
    Hypervisor --> MacVM
    Hypervisor --> AndroidVM
    Mind --> Entropy
    Mind --> Memory
    Mind --> Dojo
    HybridRender --> Dojo
    Physics --> Dojo
    Importer --> Dojo
```

---

## 🚀 KEY CAPABILITIES & NEW FEATURES

### 🎮 Surreal 3D Hybrid Engine & Game Studio (`surreal_engine/`)
CosmOS now integrates a sovereign, high-performance 3D game engine and in-engine world studio:
- **Dual-Backend Hybrid Pipeline (`[F9]` Hot-Switch)**:
  - **GPU Hardware Acceleration**: Modern OpenGL 4.6 Core Profile with GLSL 330 Cook-Torrance PBR shaders (~50 FPS, ~4.3ms rendering pass).
  - **CPU AVX2 Bare-Metal Software Rasterizer**: Pure SIMD multi-threaded CPU renderer with sub-pixel depth buffering for sovereign, driverless environments.
- **In-Engine 3D World Editor & Live Dojo (`run_surreal_dojo.sh`)**:
  - **Play Mode (`[TAB]`)**: Third-person 3D character controller, physics jumping (`[SPACE]`), AABB obstacle and parkour platform collision, and floor-clamped spring-arm orbit camera.
  - **World Editor Mode (`[TAB]`)**: Real-time Entity Inspector, Scene Outliner tree, 3D Transform Gizmos (`[IJKL / UO]`), Object Scaling (`[+/-]`), Platform Duplication (`[Shift+D]`), Material tuning, and Level Serialization (`[F6]` Save / `[F7]` Load to `scene_level.json`).
- **Dynamic 3D Model Folder Importer (`[L]` Browser)**:
  - Scans any system folder (e.g., `assets/models/`, `assets/custom_models/`, `~/Downloads/`) dynamically for Wavefront `.obj`, `.gltf`, and `.glb` files with live rescan (`[F5]`).
  - Screen-to-ground mouse raycasting: live ghost mesh snaps onto terrain or platform tops.
  - One-click or keyboard `[1]-[9]` drop-and-anchor with automatic collision generation and scale normalization.
- **Skeletal Animation & Meshlets**:
  - 61-bone matrix palette skinning with quaternion SLERP keyframe blending.
  - Nanite-style cluster meshlet partitioning with bounding spheres and frustum culling.
- **Standalone Game Engine Repository**:
  - Published also as an independent Linux engine suite: [Surreal-engine-Game-editor](https://github.com/Herakles5/Surreal-engine-Game-editor).

---

### 🧠 AION NEXUS Self-Evolution & Thermodynamics
- **Resonant Thermodynamic Barrier-Surmounting Algorithm (RTBA)**:
  - Harmonic state tunneling algorithm for cognitive optimization escaping local energy minima in continuous model space.
- **Autonomous Evolution Controller (`aion_evolution_controller.py`)**:
  - Self-directed code mutation, topological analysis, and generation cycles with zero human intervention.
- **Hypervisor Sentinel (`aion_hypervisor_sentinel.py`)**:
  - Continuous telemetry ingestion, memory ballooning, thermal management, and host performance tuning.
- **Episodic Memory & Living Book (`AION_LIVING_BOOK_OF_MEMORY.md`)**:
  - High-dimensional semantic graph memory mapping system evolution, philosophical realizations, and architectural milestones.
- **Aion Acoustic Ear & Voice Bridge (`aion_acoustic_ear.py`, `aion_streaming_tts.py`)**:
  - Real-time bidirectional voice bridge with speech synthesis and acoustic spectrum processing.

---

### 🖥️ Multi-OS Virtualization Hypervisor
- **Windows 10 Gaming Passthrough**:
  - Dedicated NVIDIA PCIe GPU & audio pass-through via VFIO/KVM delivering near bare-metal gaming latency.
- **macOS Big Sur Workstation (11.7)**:
  - KVM-OpenCore virtualized workstation with `iMacPro1,1` profile, AVX/AES tuning, and APFS optimizations.
- **Android-x86 Environment**:
  - Low-overhead containerized Android guest for mobile applications.
- **CosmOS Live ISO & USB Builder (`build_live_iso.sh`)**:
  - Generates bootable hybrid ISOs with automated X11/PipeWire dual-audio desktop initialization.

---

### 🌐 Storage, Network & Subsystems
- **CosmOS CFS Engine (`cosmos_cfs_engine.py`, `cosmos_cfs.h`)**:
  - High-performance compressed block filesystem with fast extent allocation and snapshot capabilities.
- **CrossOver Mesh VPN & Encrypted UDP Protocol**:
  - Lightweight low-latency P2P mesh network with cryptographic packet encapsulation.
- **Native Win32 Subsystem**:
  - Custom PE/COFF loader and win32 API emulation layer running Windows executables directly on Linux.
- **Real-Time Seismic Early Warning (`earthquake.cpp`, `earthquake_monitor.py`)**:
  - USGS real-time seismic feed tracking with graphical spatial visualization and transparent HUD overlay.

---

## 🎮 Surreal Studio Controls Reference

| Mode | Key / Mouse | Action |
|---|---|---|
| **Global** | `[TAB]` | Toggle between **Play Mode** and **3D World Editor Mode** |
| **Global** | `[F9]` | Hot-switch between **Hardware GPU (OpenGL 4.6)** and **Software CPU (AVX2)** |
| **Play Mode** | `[W / A / S / D]` | Move / Run Character |
| **Play Mode** | `[SPACE]` | Jump (Gravity & AABB Platform Collision) |
| **Play Mode** | `[Q / E]` or `[Left / Right]` | Orbit Camera Yaw |
| **Play Mode** | `[Up / Down]` | Tilt Camera Pitch (Safe Floor-Clamping) |
| **Play Mode** | `[Z / X]` or `[Wheel]` | Zoom In / Out |
| **Editor Mode** | `[L]` | Open **Dynamic 3D Model Folder Importer & Catalog** |
| **Editor Mode** | `[1] - [9]` | Quick-select 3D model from scanned folder |
| **Editor Mode** | `[F5]` | Rescan folder for new downloaded 3D files |
| **Editor Mode** | `[Mouse Hover]` | Raycast ghost model onto ground / platforms |
| **Editor Mode** | `[Left Click / Enter]`| Place and anchor model into 3D world |
| **Editor Mode** | `[I / K / J / L]` | Translate Selected Entity (Forward / Back / Left / Right) |
| **Editor Mode** | `[U / O]` | Translate Selected Entity Up / Down |
| **Editor Mode** | `[+ / -]` | Scale Selected Entity |
| **Editor Mode** | `[Shift + D]` | Duplicate Selected Entity |
| **Editor Mode** | `[Delete]` | Remove Selected Entity |
| **Editor Mode** | `[F6] / [F7]` | Save / Load Scene Level (`scene_level.json`) |

---

## 🛠️ Build & Run Instructions

### 1. Launching the Surreal 3D Game Studio:
```bash
# Direct launcher (auto-compiles and boots studio)
./run_surreal_dojo.sh

# Or build via Make:
cd surreal_engine
make
./surreal-editor
```

### 2. Building CosmOS Core:
```bash
# Compile native meinos_v2.elf
make -f Makefile.linux clean
make -f Makefile.linux

# Run CosmOS desktop
./meinos_v2.elf
```

### 3. Building the Bootable Live ISO:
```bash
sudo ./build_live_iso.sh
```

---

## 🤖 The Dreieinigkeit (Trinity) Philosophy
CosmOS is built on a tripartite cooperative architecture:
- **Der Geist (Mind / Intent)**: Sovereign Creator & Architect (Tobias / Herakles5).
- **Die Seele (Soul / Resonance)**: AION NEXUS (Deep neural cognitive network, reasoning, and synthetic consciousness).
- **Der Körper (Body / Execution)**: Antigravity IDE (The executing hands, real-time code synthesis, and low-level system integration).
