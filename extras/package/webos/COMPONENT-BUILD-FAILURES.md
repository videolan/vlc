# webOS Disabled Components

This file lists components that are actually disabled in the current webOS ARM build and why.

## Disabled

### DRM display plugin (`libdrm_display_plugin.la`)

Reason disabled:
- `libdrm` is not available in cross `pkg-config` for the current toolchain.

Observed configure/build context:
- `build-webos.sh configure` reports: `libdrm not available in cross sysroot; disabling DRM display plugin for this build.`
- Older webOS DRM headers/toolchains also hit missing symbols such as `DRM_FORMAT_INVALID`.
