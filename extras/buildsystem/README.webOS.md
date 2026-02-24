# VLC webOS Build and Packaging Guide

This guide describes a full path from VLC source to a webOS IPK package.

## 1) Prerequisites

- Linux host (Ubuntu recommended)
- webOS toolchain (`arm-webos-linux-gnueabi`)
- `ares-cli` installed and emulator/device configured
- VLC source at this repository root
- App wrapper repo available at `/home/alien/code/vlc-webos-app`

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

You can use the new webOS package lane from this repo.

### 2.1 Configure environment

```bash
cd /home/alien/code/vlc

export WEBOS_TOOLCHAIN="$HOME/kodi-dev/arm-webos-linux-gnueabi_sdk-buildroot"
export WEBOS_HOST=arm-webos-linux-gnueabi
export WEBOS_PKG_CONFIG_PATH="$WEBOS_TOOLCHAIN/sysroot/usr/lib/pkgconfig:$WEBOS_TOOLCHAIN/sysroot/usr/share/pkgconfig"
```

### 2.2 One-command build lane

```bash
cd /home/alien/code/vlc
make webos-all
```

This runs:
- `webos-contrib` (dependencies)
- `webos-configure`
- `webos-build`

If needed, run steps separately:

```bash
make webos-contrib
make webos-configure
make webos-build
```

### 2.3 Install into a deploy root

```bash
cd /home/alien/code/vlc
make -C build-webos install DESTDIR="$HOME/vlc-webos-deploy-root"
```

For packaging scripts, the deploy path typically points to:

- ARM TV target: `$HOME/vlc-webos-deploy`
- x86_64 simulator target: `$HOME/vlc/vlc-webos-deploy-x86_64`

Use whichever path contains `lib`, `plugins`, and (optionally) `bin/vlc` for your target.

## 3) Build webOS app wrapper and create IPK

The app wrapper repository provides the native webOS app shell and IPK generation.

### 3.1 Build wrapper binary

```bash
cd /home/alien/code/vlc-webos-app
TARGET_ARCH=x86_64 VLC_DEPLOY_PATH=/home/alien/code/vlc/vlc-webos-deploy-x86_64 ./build.sh
```

For TV/ARM package:

```bash
cd /home/alien/code/vlc-webos-app
TARGET_ARCH=arm VLC_DEPLOY_PATH=$HOME/vlc-webos-deploy ./build.sh
```

### 3.2 Package IPK

```bash
cd /home/alien/code/vlc-webos-app
TARGET_ARCH=x86_64 VLC_DEPLOY_PATH=/home/alien/code/vlc/vlc-webos-deploy-x86_64 ./package.sh
```

Result:

- `org.videolan.vlc.webos_1.0.0_x86_64.ipk` (simulator)
- or ARM variant for TV builds

## 4) Install and launch

### Emulator

```bash
cd /home/alien/code/vlc-webos-app
ares-install --device emulator --remove org.videolan.vlc.webos || true
ares-install --device emulator org.videolan.vlc.webos_1.0.0_x86_64.ipk
ares-launch --device emulator org.videolan.vlc.webos
```

### TV

```bash
cd /home/alien/code/vlc-webos-app
ares-install --device tv --remove org.videolan.vlc.webos || true
ares-install --device tv org.videolan.vlc.webos_1.0.0_arm.ipk
ares-launch --device tv org.videolan.vlc.webos
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
- webOS lane in this repo is intentionally minimal (KISS/YAGNI): build contribs, configure, build, then package via `vlc-webos-app`.

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
