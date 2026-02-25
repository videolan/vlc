# VLC webOS Build and Packaging Guide

This guide describes a full in-repository path from VLC source to a webOS IPK package.

## 1) Prerequisites

- Linux host (Ubuntu recommended)
- webOS toolchain (`arm-webos-linux-gnueabi`)
- `ares-cli` installed and emulator/device configured
- VLC source at this repository root
- `ares-cli` tools (`ares-package`, `ares-install`, `ares-launch`) available in `PATH`

Toolchain sources:

- buildroot-nc4 (recommended): `https://github.com/openlgtv/buildroot-nc4`
	- Typical generated SDK path after `make sdk`: `$HOME/buildroot-nc4/output/host`
- LG official SDK downloads: `https://opensource.lge.com/`
	- Typical extracted path: `$HOME/kodi-dev/arm-webos-linux-gnueabi_sdk-buildroot`

Optional (for simulator): webOS OSE qemux86-64 with SSH forwarding (host `6622` -> guest `22`).

Simulator image (VM/VMDK source and version used):

- webOS OSE release images: `https://github.com/webosose/build-webos/releases`
- Session-tested release page: `https://github.com/webosose/build-webos/releases/tag/v2.28.0`
- Session-tested artifact used: `https://github.com/webosose/build-webos/releases/download/v2.28.0/webos-ose-2-28-0-qemux86-64.tar.bz2`
- Local filename used during validation: `webos-ose-2-28-0-qemux86-64.tar.bz2`

After extracting this archive, import/attach the provided disk image in your VM manager (VirtualBox/QEMU) and apply the port forwards used in this guide.

## 2) Build VLC (core/libs/plugins)

Use the repository helper script for an end-to-end ARM build + install tree.

### 2.0 SDK bootstrap (optional, automated)

If the SDK/toolchain is not installed yet, `build-webos.sh` can download/extract it first:

```bash
cd /home/alien/code/vlc
WEBOS_SDK_URL="<sdk-archive-url>" ./build-webos.sh sdk
```

You can also point to a local SDK archive:

```bash
cd /home/alien/code/vlc
WEBOS_SDK_ARCHIVE="$HOME/kodi-dev/arm-webos-linux-gnueabi_sdk-buildroot-x86_64.tar.gz" ./build-webos.sh sdk
```

Defaults:
- extraction directory: `~/kodi-dev` (override with `SDK_DOWNLOAD_DIR`)
- auto-download behavior in normal modes: enabled (`AUTO_SDK_DOWNLOAD=1`)

### 2.1 Configure environment

```bash
cd /home/alien/code/vlc

export WEBOS_TOOLCHAIN="$HOME/kodi-dev/arm-webos-linux-gnueabi_sdk-buildroot"
export WEBOS_HOST=arm-webos-linux-gnueabi
export WEBOS_PKG_CONFIG_PATH="$WEBOS_TOOLCHAIN/sysroot/usr/lib/pkgconfig:$WEBOS_TOOLCHAIN/sysroot/usr/share/pkgconfig"

# Recommended for packaging into vlc-webos-app:
export PREFIX=/
export DEPLOY_DIR=/home/alien/code/vlc/vlc-webos-deploy
```

### 2.2 Build + install in one command

```bash
cd /home/alien/code/vlc
WEBOS_TOOLCHAIN="$HOME/kodi-dev/arm-webos-linux-gnueabi_sdk-buildroot" PREFIX=/ DEPLOY_DIR=/home/alien/code/vlc/vlc-webos-deploy ./build-webos.sh all
```

This performs:
- contrib deps build
- VLC configure
- VLC compile
- install into `DEPLOY_DIR`

### 2.3 Build and package IPK from this repository

```bash
cd /home/alien/code/vlc
make webos-ipk
```

This target:
- installs runtime files to `vlc-webos-deploy`
- stages a webOS package payload
- runs `ares-package`
- emits `org.videolan.vlc.webos_1.0.0_arm.ipk` in `webos-package/`

One-command full flow (deps + build + IPK):

```bash
cd /home/alien/code/vlc
make webos-all-ipk
```

If needed, run steps separately:

```bash
WEBOS_TOOLCHAIN="$HOME/kodi-dev/arm-webos-linux-gnueabi_sdk-buildroot" ./build-webos.sh deps
WEBOS_TOOLCHAIN="$HOME/kodi-dev/arm-webos-linux-gnueabi_sdk-buildroot" PREFIX=/ ./build-webos.sh configure
WEBOS_TOOLCHAIN="$HOME/kodi-dev/arm-webos-linux-gnueabi_sdk-buildroot" ./build-webos.sh build
WEBOS_TOOLCHAIN="$HOME/kodi-dev/arm-webos-linux-gnueabi_sdk-buildroot" DEPLOY_DIR=/home/alien/code/vlc/vlc-webos-deploy ./build-webos.sh install
```

### 2.4 Alternate make-target lane (manual steps)

```bash
cd /home/alien/code/vlc
make webos-contrib
make webos-configure
make webos-build
make webos-install
make webos-ipk
```

For deploy/install paths, typical locations are:

- ARM TV target: `$HOME/vlc-webos-deploy`
- x86_64 simulator target: `$HOME/vlc/vlc-webos-deploy-x86_64`

The in-repo packager (`extras/package/webos/package.sh`) auto-detects either a flat deploy tree or a nested tree under install prefix.

## 3) Install and launch

### TV

```bash
cd /home/alien/code/vlc
ares-install --device tv --remove org.videolan.vlc.webos || true
ares-install --device tv webos-package/org.videolan.vlc.webos_1.0.0_arm.ipk
ares-launch --device tv org.videolan.vlc.webos
```

### Emulator

```bash
cd /home/alien/code/vlc
ares-install --device emulator --remove org.videolan.vlc.webos || true
ares-install --device emulator webos-package/org.videolan.vlc.webos_1.0.0_arm.ipk
ares-launch --device emulator org.videolan.vlc.webos
```

## 4) Legacy external wrapper flow (optional)

If you still want to use the older external wrapper repository:

```bash
cd /home/alien/code/vlc-webos-app
TARGET_ARCH=arm VLC_DEPLOY_PATH=/home/alien/code/vlc/vlc-webos-deploy ./package.sh
```

## 5) Troubleshooting quick checks

- App installed but black screen on emulator globally: relaunch Home

```bash
ares-launch --device emulator com.webos.app.home
```

- Verify deployed launcher on emulator:

```bash
ssh -p 6622 root@127.0.0.1 'sed -n "1,140p" /media/developer/apps/usr/palm/applications/org.videolan.vlc.webos/run.sh'
```

- If install fails due stale state or disk pressure, remove app and retry.

## 6) Notes

- Android app build files are in a separate project: `vlc-android`.
- macOS packaging is under `extras/package/macosx` and `extras/package/apple`.
- webOS lane in this repo is intentionally minimal (KISS/YAGNI): build contribs, configure, build, install, then package with `extras/package/webos/package.sh`.

## Appendix A) VM settings used (validated session)

VM name: `webos-ose-emu`

- Guest OS type: `Other Linux (64-bit)`
- Chipset: `piix3`
- Firmware: `BIOS`
- CPUs: `2`
- RAM: `4096 MB`
- Graphics controller: `VMSVGA`
- VRAM: `64 MB`
- 3D acceleration: `on`
- NIC1: `NAT`, adapter type `82540EM`

NAT port forwards:

- `host 6622 -> guest 22` (SSH)
- `host 9998 -> guest 9998` (enact)
- `host 9223 -> guest 9223` (inspector)

Machine-readable VirtualBox excerpt:

```text
ostype="Other Linux (64-bit)"
memory=4096
vram=64
chipset="piix3"
firmware="BIOS"
cpus=2
graphicscontroller="vmsvga"
accelerate3d="on"
nic1="nat"
nictype1="82540EM"
Forwarding(0)="enact,tcp,,9998,,9998"
Forwarding(1)="inspector,tcp,,9223,,9223"
Forwarding(2)="ssh,tcp,,6622,,22"
```
