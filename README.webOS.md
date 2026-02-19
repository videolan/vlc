# VLC webOS Build Guide

This guide explains how to build VLC media player for LG webOS TVs (webOS 5.0+).

## Overview

VLC can be cross-compiled for webOS using the ARM toolchain and deployed as an IPK package to LG Smart TVs. This build is based on the successful approach used by Kodi for webOS.

## Validated Session Snapshot (Feb 2026)

This section captures the exact setup that was used in a working end-to-end simulator validation (install, launch, decode, and custom video output path).

### Simulator Image Used

- Release page: https://github.com/webosose/build-webos/releases/tag/v2.28.0
- Direct image artifact used: https://github.com/webosose/build-webos/releases/download/v2.28.0/webos-ose-2-28-0-qemux86-64.tar.bz2
- Local image filename used during setup: `webos-ose-2-28-0-qemux86-64.tar.bz2`

### VM Configuration Used (VirtualBox)

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
- NAT port forwarding:
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

### Exact Host-Side Access Pattern

```bash
# SSH into simulator (root)
ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -p 6622 root@127.0.0.1

# Install package to emulator profile configured in ares-cli
ares-install --device emulator org.videolan.vlc.webos_1.0.0_x86_64.ipk

# Launch app
ares-launch --device emulator org.videolan.vlc.webos
```

### Media Path Used During Validation

- Default/fallback media path in app: `/media/internal/videos/sample.mkv`
- Practical test media used: `sample_960x540.mkv` copied into simulator and played via the path above.

## Prerequisites

### Required Software
- Ubuntu 22.04+ (64-bit) or similar Linux distribution
- LG webOS TV (webOS 5.0 or newer)
- LG Developer account (https://webostv.developer.lge.com/)
- Developer Mode app installed on your TV
- ares-cli tools for IPK packaging

### Toolchain Options

**Option 1: buildroot-nc4 (Recommended)**
```bash
cd $HOME
git clone https://github.com/openlgtv/buildroot-nc4
cd buildroot-nc4
make webos_tv_defconfig
make sdk -j$(nproc)
```
This will create the toolchain in `output/host/`.

**Option 2: LG Official Toolchain**
Download from https://opensource.lge.com/ (enter your TV model number)
```bash
mkdir -p $HOME/kodi-dev
tar xzvf arm-webos-linux-gnueabi_sdk-buildroot.tar.gz -C $HOME/kodi-dev
```

### Install ares-cli Tools
Download from: https://webostv.developer.lge.com/develop/tools/cli-installation

Add your webOS device:
```bash
ares-setup-device -s
```
- Username: `prisoner`
- Password: (leave blank)
- Port: 9922

## Building VLC

### 1. Get VLC Source
```bash
cd $HOME
git clone https://code.videolan.org/videolan/vlc.git
cd vlc
```

### 2. Run Build Script
```bash
export WEBOS_TOOLCHAIN="$HOME/buildroot-nc4/output/host"
./build-webos.sh deps
./build-webos.sh configure
```

The script will:
- Verify toolchain installation
- Build VLC dependencies via contrib system
- Configure VLC with webOS-specific flags
- Apply common webOS post-config fixes

### 3. Compile VLC
```bash
make -C $HOME/vlc-webos-build -j$(nproc)
```

### Build Script Actually Used in This Session

The reproducible command path for core VLC cross-build setup came from `build-webos.sh` in this repository:

```bash
cd $HOME/code/vlc
WEBOS_TOOLCHAIN="$HOME/kodi-dev/arm-webos-linux-gnueabi_sdk-buildroot" ./build-webos.sh deps
WEBOS_TOOLCHAIN="$HOME/kodi-dev/arm-webos-linux-gnueabi_sdk-buildroot" ./build-webos.sh configure
make -C "$HOME/vlc-webos-build" -j$(nproc)
```

`build-webos.sh` includes the webOS-specific post-config fixes used in this effort (notably `-ldl` linking and selective plugin disabling in generated makefiles).

## App Build + Packaging Scripts Used

For simulator bring-up, the native webOS app wrapper scripts in `vlc-webos-app/` were used.

### Build (`vlc-webos-app/build.sh`)

- Supports `TARGET_ARCH=arm` and `TARGET_ARCH=x86_64`
- Links `vlc-player` against bundled `libvlc`/`libvlccore`
- Links Wayland/EGL/GLES libraries for custom video output path

Commands used:

```bash
cd /home/alien/code/vlc-webos-app
TARGET_ARCH=x86_64 VLC_DEPLOY_PATH=/home/alien/code/vlc/vlc-webos-deploy-x86_64 ./build.sh
```

### Package (`vlc-webos-app/package.sh`)

- Creates IPK payload under `package/`
- Copies VLC libs/plugins with `cp -a` (important for plugin cache timestamp consistency)
- Adds x86_64 runtime deps discovered during triage (Matroska + FFmpeg + swscale dependency trees)
- Generates `run.sh` that exports:
  - `LD_LIBRARY_PATH`
  - `VLC_PLUGIN_PATH`
  - `XDG_RUNTIME_DIR` / `WAYLAND_DISPLAY` (when Wayland socket exists)
  - `VLC_WEBOS_CUSTOM_VOUT=1` by default on Wayland-enabled simulator

Commands used:

```bash
cd /home/alien/code/vlc-webos-app
TARGET_ARCH=x86_64 VLC_DEPLOY_PATH=/home/alien/code/vlc/vlc-webos-deploy-x86_64 ./package.sh
ares-install --device emulator --remove org.videolan.vlc.webos || true
ares-install --device emulator org.videolan.vlc.webos_1.0.0_x86_64.ipk
```

## Manual Configuration

If you prefer manual configuration instead of using the build script:

```bash
# Set environment
export WEBOS_TOOLCHAIN="$HOME/kodi-dev/arm-webos-linux-gnueabi_sdk-buildroot"
export PATH="${WEBOS_TOOLCHAIN}/bin:${PATH}"
export VLC_DEPS_DIR="$HOME/vlc-webos-deps"

# Configure VLC
./configure \
    --host=arm-webos-linux-gnueabi \
    --prefix=/media/developer/apps/usr/palm/applications/org.videolan.vlc \
    --with-sysroot=${WEBOS_TOOLCHAIN}/sysroot \
    --disable-dbus \
    --disable-xcb \
    --disable-wayland \
    --disable-libdrm \
    --disable-qt \
    --disable-skins2 \
    --enable-run-as-root \
    CFLAGS="-march=armv7-a -mfloat-abi=softfp -mfpu=neon" \
    CXXFLAGS="-march=armv7-a -mfloat-abi=softfp -mfpu=neon"

make -j$(nproc)
```

## Disabled Features for webOS

The following desktop Linux features are disabled for webOS compatibility:

- **dbus** - Not available on webOS
- **xcb/X11** - No X server on webOS
- **wayland** - Not supported
- **libdrm/KMS** - Requires different approach
- **Qt/skins2** - GUI needs webOS-specific implementation
- **PulseAudio/ALSA** - Different audio system
- **udev** - Not available
- **Avahi/mDNS** - Service discovery not supported

## Known Issues and TODO

## Findings and Lessons Learned (from this porting session)

1. **ABI mismatch in libVLC media constructor can hard-crash early**
  - Symptom: crash around `libvlc_media_new_path/location`.
  - Fix pattern: ABI-aware dispatch/fallback when creating media objects.

2. **Plugin loading success is necessary but not sufficient**
  - MKV demux worked only after bundling missing runtime libs (`libebml.so.5`, `libmatroska.so.7`).
  - H.264 decode path required FFmpeg runtime deps from `libavcodec_plugin.so`.
  - Chroma conversion path required `libswscale` dependency chain.

3. **Preserving plugin/cache timestamps matters**
  - Using `cp -a` avoided subtle plugin cache mismatch behavior seen with plain copy.

4. **Wayland environment must be explicit in this runtime model**
  - `XDG_RUNTIME_DIR` and `WAYLAND_DISPLAY` were required to initialize custom video output reliably.

5. **Custom memory vout (`vmem`) is fragile**
  - Callback buffer format/stride mismatches can lead to decoder-thread crashes.
  - Thread ownership for EGL contexts must be handled carefully.
  - Treat this as a bring-up path; a dedicated webOS Wayland/vout integration is the long-term fix.

6. **“No UI/no playback” can still mean decode is progressing**
  - Log milestones such as `using video decoder module "avcodec"` and `Received first picture` helped separate decode issues from composition/presentation issues.

## Restarting on a Newer Version (Checklist)

When resuming work with a newer VLC branch, newer webOS OSE image, or new toolchain:

1. **Pin versions first**
  - Record: VLC commit/tag, simulator image version, toolchain version, and ares-cli version.

2. **Recreate baseline VM**
  - Start from a known OSE qemux86-64 image and re-apply the VM parameters from this document.
  - Recreate NAT forwards (`6622`, `9998`, `9223`) before debugging app issues.

3. **Rebuild deploy libs from clean state**
  - Re-run `build-webos.sh deps` + `configure` and clean build output.
  - Validate `libvlc.so`, `libvlccore.so`, and plugin tree are present in deploy directory.

4. **Package with dependency audit**
  - Run `ldd` on critical plugins (`libmkv`, `libavcodec_plugin.so`, `libswscale_plugin.so`) and ensure all non-system dependencies are bundled in app `vlc/lib`.

5. **Smoke-test in this order**
  - Process starts and stays alive.
  - MKV demux opens sample file.
  - Decoder module selection (expect `avcodec` in current setup).
  - First-frame indicator in logs.
  - Visible composition in simulator.

6. **Keep logs and kernel messages paired per run**
  - Clear and capture `dmesg` for each attempt to avoid chasing stale crashes.

7. **Prefer incremental changes**
  - Change one axis at a time (toolchain, image, VLC commit, app wrapper) to isolate regressions quickly.

### Video Output
- **Good News**: VLC already has Wayland support with EGL backend!
- **Challenge**: Need to add webOS-specific Wayland protocol extensions
- VLC has the following components ready:
  - ✅ Wayland client support (`xdg-shell`, `wl_shell`)
  - ✅ EGL context management (`egl_wl` plugin)
  - ✅ OpenGL and OpenGL ES rendering
  - ✅ Wayland protocol scanner integration
  - ❌ webOS-specific protocols (`wayland-webos-foreign`, `wayland-webos-shell`)
  - ❌ Starfish media framework integration (HW acceleration)
  
**Implementation Strategy:**
1. Add webOS Wayland protocol definitions (similar to Kodi's approach)
2. Create `WinSystemWaylandWebOS` or extend existing Wayland vout
3. Integrate webOS exported surface for video overlay
4. Add Starfish codec/renderer support for hardware acceleration
5. Use Luna services for power management integration

### Audio Output  
- webOS uses different audio subsystem than desktop Linux
- Need to investigate webOS audio APIs

### GUI
- VLC's Qt interface won't work
- May need to create webOS-specific UI or use VLC's web interface
- Could potentially use libVLC headless with external control

## Packaging for webOS

### Create IPK Structure
```bash
mkdir -p package/org.videolan.vlc
cp -r bin lib share package/org.videolan.vlc/
```

### Create appinfo.json
```json
{
  "id": "org.videolan.vlc",
  "version": "4.0.0",
  "vendor": "VideoLAN",
  "type": "native",
  "main": "vlc",
  "title": "VLC Media Player",
  "icon": "icon.png"
}
```

### Package with ares-cli
```bash
ares-package package/org.videolan.vlc
```

### Install on TV
```bash
ares-install org.videolan.vlc_*.ipk -d <your-tv-name>
```

## Debugging

### SSH Access
```bash
ssh -oHostKeyAlgorithms=+ssh-rsa -p 9922 prisoner@<tv-ip> bash -i
```

### View Logs
```bash
cd /media/developer/apps/usr/palm/applications/org.videolan.vlc
tail -f .vlc/vlc-log.txt
```

### Trace Dependencies
```bash
strace -o trace.log ./vlc --help
```

## Architecture Details

- **Target**: arm-webos-linux-gnueabi
- **Architecture**: ARMv7-A
- **FPU**: NEON
- **Float ABI**: softfp
- **Kernel**: Linux 5.4.96 (webOS 7)
- **glibc**: 2.31
- **Install Path**: `/media/developer/apps/usr/palm/applications/<app-id>/`

## References

- VLC Git: https://code.videolan.org/videolan/vlc
- buildroot-nc4: https://github.com/openlgtv/buildroot-nc4
- Kodi webOS: https://github.com/webosbrew/xbmc-webos
- LG Developer: https://webostv.developer.lge.com/
- ares-cli tools: https://webostv.developer.lge.com/develop/tools/cli-installation

## Contributing

This webOS port is experimental. Contributions welcome for:
- Video output module implementation
- Audio output integration
- GUI/control interface
- Testing on different webOS versions
- Performance optimization

## License

VLC is licensed under GPLv2 or later. See COPYING file in VLC source.
