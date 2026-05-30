# Network Machine Inventory

Homelab machines available on the LAN, with host information and IP addresses.

_Last updated: 2026-05-30_

| Alias | Hostname (k3s node) | IP | CPU | RAM | GPU(s) | OS | Role | SSH |
|-------|---------------------|-----|-----|-----|--------|-----|------|-----|
| **ptow** | pandoratower | 192.168.1.8 | Ryzen 7 3800X (8c) | 64GB | RTX 5090 32GB | Ubuntu 25.10 | k3s control plane, hub, ComfyUI 5090 host | _(primary workstation, user `wolfgang`)_ |
| **pbox** | pandoras-box | 192.168.1.226 | Ryzen 9 9950X3D (16c) | 192GB | RTX PRO 6000 Blackwell 96GB | Ubuntu 25.10 | k3s worker, flagship GPU host, 3-slice MPS ComfyUI, NFS server | `ssh pandora@192.168.1.226` (passwordless) |
| **pstorm** | pandora-storm | 192.168.1.133 | i9-14900HX (24c/32t) | 64GB | RTX 5090 Laptop 24GB | Ubuntu 25.10 (native) | Training + preprocessing workstation | `ssh wolfgang@pandora-storm.local` (passwordless) |
| **swolf** | spark-wolf | 192.168.1.229 | Grace ARM (GB10) | 128GB unified | Blackwell GB10 | DGX OS (Ubuntu 24.04) | LLM inference (vLLM), SillyTavern | `ssh wolfgang@192.168.1.229` |
| **air** | pandora-air | 192.168.1.172 | Apple M2 (8c) | 8GB | none | Ubuntu 24.04 Asahi (aarch64) | HA k3s control-plane #2 (clamshell, CP-only) | `ssh wolfgang@192.168.1.172` (key; sudo password-gated) |
| **tank** | pandora-tank | 192.168.1.222 | Ryzen 5 5500 (6c) | 48GB | 2× RTX 5060 Ti 16GB | Ubuntu 26.04 | k3s worker, dual-GPU ComfyUI host (vram16 tier) | `ssh wolfgang@192.168.1.222` (passwordless + NOPASSWD sudo) |

## Notes

- **pstorm's IP** was `192.168.1.64` before 2026-05-10 — now `192.168.1.133`.
- **air** sits on a USB ethernet dongle in clamshell; sleep targets are masked so it stays online lid-closed. Do not `apt upgrade` it into a sleep regression.
