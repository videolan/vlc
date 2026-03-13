# VLC webOS Build and Packaging Guide

This guide describes a full in-repository path from VLC source to a webOS IPK package.

## 1) Prerequisites

- Linux host (Ubuntu recommended)
- webOS toolchain (`arm-webos-linux-gnueabi`)
- `ares-cli` tools (`ares-package`, `ares-install`, `ares-launch`) available in `PATH`

### 1.1 Install development environment

Use the helper mode to install host build dependencies on Ubuntu/Debian:

```bash
cd /vlc
extras/package/webos/build-webos.sh install-dev-env
```

Equivalent commands:

```bash
sudo apt update
sudo apt install -y \
	ca-certificates \
	build-essential pkg-config autoconf automake libtool flex bison \
	cmake ninja-build meson gettext python3-pip gawk nasm yasm m4 \
	xz-utils unzip rsync file
sudo update-ca-certificates
```

Toolchain sources:

- buildroot-nc4 (recommended): `https://github.com/openlgtv/buildroot-nc4`
	- Typical generated SDK path after `make sdk`: `$HOME/buildroot-nc4/output/host`
- LG official SDK downloads: `https://opensource.lge.com/`
	- Typical extracted path: `$HOME/kodi-dev/arm-webos-linux-gnueabi_sdk-buildroot`

## 2) Build VLC (core/libs/plugins)

Use the repository helper script for an end-to-end ARM build + install tree.

By default, all generated artifacts are now stored under a single in-repo directory:

- `<vlc-src>/.webos/build` (BUILD_DIR)
- `<vlc-src>/.webos/deps` (DEPS_PREFIX)
- `<vlc-src>/.webos/build/sdk` (SDK download/extract)
- `<vlc-src>/.webos/qt6` (Qt host/target trees)
- `<vlc-src>/.webos/deploy` (install DESTDIR)
- `<vlc-src>/.webos/package` (IPK output + staging)

Override all of these at once with `WEBOS_WORK_ROOT=/custom/path`.

### 2.0 SDK bootstrap (optional, automated)

If the SDK/toolchain is not installed yet, `build-webos.sh` can download/extract it first:

```bash
cd /vlc
WEBOS_SDK_URL="<sdk-archive-url>" extras/package/webos/build-webos.sh sdk
```

You can also point to a local SDK archive:

```bash
cd /vlc
WEBOS_SDK_ARCHIVE="$HOME/kodi-dev/arm-webos-linux-gnueabi_sdk-buildroot-x86_64.tar.gz" extras/package/webos/build-webos.sh sdk
```

### 2.1 Build + install in one command

```bash
cd /home/alien/code/vlc
extras/package/webos/build-webos.sh all
```

This performs:
- contrib deps build
- VLC configure
- VLC compile
- install into `<vlc-src>/.webos/deploy`

Notes:
- `WEBOS_TOOLCHAIN` is auto-detected from `<vlc-src>/.webos/build/sdk` when available.
- Qt6 host/target paths are auto-detected from `<vlc-src>/.webos/qt6` (or `~/qt6-webos` fallback).
- Keep `PREFIX` default unless you have a custom app sandbox path.

### 2.2 Build and package IPK

One-command full flow (deps + build + IPK):

```bash
cd /home/alien/code/vlc
make webos-all-ipk
```

Or run steps separately:

```bash
extras/package/webos/build-webos.sh deps
extras/package/webos/build-webos.sh configure
extras/package/webos/build-webos.sh build
extras/package/webos/build-webos.sh install
make webos-ipk
```

The IPK is emitted as `.webos/package/org.videolan.vlc.webos_1.0.0_arm.ipk`.

## 3) Install and launch

```bash
cd /home/alien/code/vlc
ares-install --device tv --remove org.videolan.vlc.webos || true
ares-install --device tv .webos/package/org.videolan.vlc.webos_1.0.0_arm.ipk
ares-launch --device tv org.videolan.vlc.webos
```

## 4) Troubleshooting

- If install fails due to stale state or disk pressure, remove the app and retry.
- Verify the deployed launcher:

```bash
ssh root@<tv-ip> 'cat /media/developer/apps/usr/palm/applications/org.videolan.vlc.webos/run.sh'
```

## 5) Notes

- Android app build files are in a separate project: `vlc-android`.
- macOS packaging is under `extras/package/macosx` and `extras/package/apple`.
- webOS lane in this repo: build contribs, configure, build, install, package with `extras/package/webos/package.sh`.

## 6) "Kodi way" compositor notes (Qt UI + webOS Wayland)

VLC on webOS keeps the Qt UI path, while display integration follows the compositor/Wayland model.

- UI remains in `modules/gui/qt`.
- Display should prefer Wayland/webOS compositor integration over DRM/KMS assumptions.
- `extras/package/webos/build-webos.sh` now probes for webOS Wayland extension protocol XMLs (`webos-shell.xml`) in:
	- `WEBOS_WAYLAND_WEBOS_PROTOCOLS_DIR`
	- `WEBOS_WAYLAND_WEBOS_PREFIX/share/wayland-webos`
	- `${DEPS_PREFIX}/share/wayland-webos`
	- `${SYSROOT}/usr/share/wayland-webos`

Optional environment variables:

- `WEBOS_WAYLAND_WEBOS_PREFIX`: prefix containing webOS wayland extension pkgconfig/protocol files.
- `WEBOS_WAYLAND_WEBOS_PROTOCOLS_DIR`: direct path to protocol XML directory.

The probe is informational for now (non-fatal) so existing Qt + Wayland builds remain unchanged, while preparing for native webOS protocol integration in future video/window modules.
