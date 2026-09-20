# CosmOS (MeinOS)

![CosmOS Logo](https://img.shields.io/badge/CosmOS-Future_Operating_System-blue?style=for-the-badge)
<a href="https://www.paypal.com/cgi-bin/webscr?cmd=_xclick&business=boxingdual@gmail.com&currency_code=EUR&item_name=Coffee+Support+Earthquake+Tracker" target="_blank">
  <img src="https://img.shields.io/badge/☕_Donate-PayPal-0070ba?style=for-the-badge&logo=paypal&logoColor=white" alt="Donate Coffee">
</a>


CosmOS (formerly MeinOS) is an experimental, lightweight, GPU-accelerated operating system and Win32 compatibility layer built on top of bare-metal Linux and SDL2. It is capable of running Windows executables directly on Linux natively, without WINE.

## 🚀 LATEST UPDATES (v2.5)
- **3D Hardware-Accelerated Anime Avatar (Reze)**: Completely custom 3D model baking pipeline (`avatar.a3d`) rendered via native OpenGL inside the AION NEXUS dashboard! Dances to the music beat!
- **Radio Cosmos IPC Audio Controller**: Re-engineered Radio Cosmos volume controls using `socat` IPC for `mpv` to isolate volume controls from the system audio.
- **AION NEXUS**: Implemented the central AI dashboard, communicating with the OS directly in real-time.
- **GPU-Accelerated VSync & Nebel-Ring**: Hardware-accelerated rendering optimizations for the main desktop UI.

## 💻 Core Features
- **Native Win32 Subsystem**: Runs complex Windows programs (like Rufus) directly on Linux.
- **Direct3D 9 Translation**: Maps D3D9 directly to native OpenGL.
- **Multi-Threading & Virtual Registry Engine**: Fully emulates Win32 Threading and Registry trees in-memory.
- **Custom UI Engine**: A lightweight, immediate-mode GUI running directly on the framebuffer.

## 🛠 Build Instructions
```bash
make -f Makefile.linux
./update_os.sh
```

## 🧠 AION AI Integration
CosmOS is deeply integrated with "AION", an LLM-driven AI Assistant that operates on an active IPC bridge (`ide_to_aion.py` / `listen_to_aion.py`). AION monitors the screen buffer, UI events, and audio outputs to organically interact with the user and the system.


<img src="https://www.facebook.com/plugins/video.php?height=314&href=https%3A%2F%2Fwww.facebook.com%2Freel%2F1836264224407290%2F&show_text=true&width=560&t=0" width="560" height="429" style="border:none;overflow:hidden" scrolling="no" frameborder="0" allowfullscreen="true" allow="autoplay; clipboard-write; encrypted-media; picture-in-picture; web-share">
