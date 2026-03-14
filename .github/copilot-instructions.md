# GitHub Copilot Instructions for VLC Media Player

## Project Overview

VLC is a libre and open source media player and multimedia engine focused on playing everything and running everywhere. It's part of the VideoLAN project, licensed under GPLv2 (or later), with libVLC under LGPLv2 (or later).

## Development Languages

- **Primary**: C (main development)
- **Additional**: C++, Objective-C, Assembly, Rust
- **Related projects**: Kotlin/Java (Android), Swift (iOS), C# (libVLCSharp)

## Code Style and Standards

- Use GCC version 5 or higher, or Clang/LLVM version 3.4 or higher
- Follow the existing code patterns in the modules
- Maintain compatibility across multiple platforms (Windows 7+, macOS 10.10+, GNU/Linux, BSD, Android 4.2+, iOS 9+)

## Build System

VLC uses autotools for building:
1. Bootstrap: `./bootstrap` (if no configure script exists)
2. Configure: `./configure` (use `--enable-debug` for debugging)
3. Build: `make`
4. Install: `make install` (optional - VLC can run from build directory with `./vlc`)

## Repository Structure

- `bin/` - VLC binaries
- `lib/` - libVLC source code (embeddable engine)
- `src/` - libvlccore source code
- `modules/` - VLC plugins and modules (most code is here)
- `include/` - Header files
- `compat/` - Compatibility library for missing OS functionalities
- `contrib/` - External libraries for systems without right versions
- `test/` - Testing system
- `po/` - Translations
- `share/` - Common resource files
- `doc/` - Documentation

## Key Considerations

### Cross-Platform Compatibility
- Code must work across Windows, macOS, Linux, BSD, Android, iOS, and other platforms
- Use compatibility layer (`compat/`) for OS-specific functionality
- Be mindful of platform-specific limitations

### libVLC Engine
- libVLC is embeddable and must maintain API stability
- LGPLv2 license allows 3rd party embedding
- Consider impact on external bindings (C++, Python, C#, etc.)

### Performance
- VLC handles multimedia playback requiring efficient code
- Consider memory usage and CPU performance
- Optimize for real-time processing where applicable

### Modularity
- Most functionality is in plugins/modules
- Keep modules independent and reusable
- Follow existing module patterns

## Contribution Guidelines

- Contributions via Merge Requests on GitLab (https://code.videolan.org/videolan/vlc/)
- Resolve CI and discussions before merge
- Test on multiple platforms when possible
- Update documentation as needed

## Debugging

- Build with debug symbols: `make distclean ; ./configure --enable-debug`
- VLC can be launched from build directory without installation
- Check `test/` directory for testing utilities

## Community Resources

- IRC: #videolan on Libera.chat
- Forums: https://forum.videolan.org/
- Wiki: https://wiki.videolan.org/
- Bugtracker: https://code.videolan.org/videolan/vlc/-/issues
- Developer's Corner: https://wiki.videolan.org/Developers_Corner
- Hacking Guide: https://wiki.videolan.org/Hacker_Guide

## webOS Platform Support (Target Platform)

VLC is being adapted to run on LG webOS TVs (webOS 5.0+). Key considerations:

### Build Environment
- **Toolchain**: ARM cross-compilation using `buildroot-nc4` or LG official toolchains from https://opensource.lge.com/
- **Target**: `arm-webos-linux-gnueabi` (32-bit ARM)
- **Architecture**: ARMv7-A with NEON support (`-march=armv7-a -mfloat-abi=softfp -mfpu=neon`)
- **Kernel**: Linux 5.4.96 with glibc 2.31 (webOS 7 configuration)
- **Build system**: Use VLC's existing autotools/contrib system for cross-compilation

### Platform Constraints
- **No systemd/dbus**: webOS has limited system services compared to desktop Linux
- **No X11/Wayland**: Requires native webOS graphics layer (DirectFB or webOS compositor)
- **Sandboxed apps**: Install path is `/media/developer/apps/usr/palm/applications/<app-id>/`
- **Package format**: IPK (not DEB/RPM) - requires `ares-cli` tools for packaging and deployment
- **32-bit only**: Must build as 32-bit even on 64-bit development machines

### Development Requirements
- LG developer account for accessing developer mode
- Developer Mode app installed on webOS TV
- `ares-cli` tools for packaging, installation, and debugging
- SSH access via port 9922 (username: `prisoner`, no password)

### Configuration Recommendations
```bash
./configure --host=arm-webos-linux-gnueabi \
            --with-toolchain=/path/to/arm-webos-linux-gnueabi_sdk-buildroot \
            --prefix=/media/developer/apps/usr/palm/applications/org.videolan.vlc \
            --enable-debug=no \
            --disable-dbus \
            --disable-xcb \
            --disable-wayland \
            --disable-libdrm \
            --without-x \
            PKG_CONFIG_PATH=/path/to/vlc-webos-deps/lib/pkgconfig
```

### Known Build Issues & Solutions

#### C++ Compatibility Issues
- **Problem**: webOS C++ stdlib declares `aligned_alloc` as `noexcept`, but VLC's compat layer doesn't match
- **Solution**: Add `noexcept` specifier to `aligned_alloc` in `include/vlc_fixups.h`:
  ```c
  #ifdef __cplusplus
  void *aligned_alloc(size_t, size_t) noexcept;
  #else
  void *aligned_alloc(size_t, size_t);
  #endif
  ```

#### O_TMPFILE Support
- **Problem**: Older webOS kernels don't define `O_TMPFILE` constant
- **Solution**: Guard `O_TMPFILE` usage in `src/linux/filesystem.c`:
  ```c
  #ifdef O_TMPFILE
  fd = open(path, O_TMPFILE | O_RDWR | O_EXCL, 0600);
  #endif
  ```

#### libglibc_polyfills Linking
- **Problem**: Toolchain's `libglibc_polyfills.a` requires `-ldl` for `dlsym`/`dlopen`
- **Solution**: Add `-ldl` to LIBS in configure or Makefile:
  ```bash
  sed -i 's/^LIBS = -lanl/LIBS = -lanl -ldl/' Makefile modules/Makefile
  ```

#### DRM Display Plugin
- **Problem**: webOS kernel headers are too old, missing `DRM_FORMAT_INVALID` and modern DRM ioctls
- **Solution**: Disable the DRM display plugin in `modules/Makefile`:
  ```bash
  sed -i 's/^am__append_351 = libdrm_display_plugin.la/#&/' modules/Makefile
  ```

#### Fontconfig/Freetype Dependencies
- **Problem**: Static fontconfig requires expat which may not be in contrib dependencies
- **Solution**: Either build expat as dependency, or disable freetype plugin:
  ```bash
  sed -i 's/^am__append_207 = libfreetype_plugin.la/#&/' modules/Makefile
  ```

#### Binary Linking (rpath)
- **Problem**: vlc and vlc-cache-gen binaries fail to link due to missing libvlccore at link time
- **Solution**: Add libvlccore to LDADD and set proper rpath in `bin/Makefile`:
  ```makefile
  vlc_LDFLAGS = -Wl,-rpath,$(abs_top_builddir)/src/.libs -Wl,-rpath,$(abs_top_builddir)/lib/.libs
  vlc_cache_gen_LDADD = ... ../src/libvlccore.la
  ```

### Build Environment Tips
- **PATH order matters**: Put `/usr/bin` before toolchain to avoid broken toolchain meson/python
- **In-source builds**: Easier than out-of-tree for generated files and Makefile modifications
- **Parallel builds**: Use `make -j4` for faster compilation (adjust for CPU cores)
- **Build time**: Full build takes ~10-15 minutes; libraries only ~10-15 seconds after fixes

### Video Output Challenges
- Standard Linux video output modules (X11, Wayland, DRM) won't work
- Need to implement webOS-specific video output module in `modules/video_output/`
- May require DirectFB or proprietary webOS compositor integration
- Hardware acceleration path needs investigation
- **Current status**: Audio-only playback possible; video requires custom output module

### Reference Implementation
- Kodi successfully runs on webOS 5+ using similar approach
- See xbmc-webos repository for working webOS build system example
- Kodi disables: dbus, CEC, pipewire, udmabuf for webOS builds
- Study Kodi's video output implementation for webOS compositor integration

## When Suggesting Code

1. Prefer C for core functionality
2. Maintain existing code style and conventions
3. Consider cross-platform compatibility
4. Add appropriate error handling
5. Include relevant comments for complex logic
6. Consider performance implications for multimedia processing
7. Check if compatibility functions exist in `compat/` before implementing OS-specific code
8. Follow GPL/LGPL licensing requirements in suggestions
9. **For webOS**: Avoid dependencies on desktop Linux services (dbus, systemd, X11, Wayland)
10. **For webOS**: Consider ARM performance and 32-bit constraints

## Successful webOS Build Checklist

When helping with webOS builds, verify these steps:

1. ✅ **Dependencies**: All contrib libraries built for ARM (ffmpeg, aom, etc.)
2. ✅ **Configure**: Proper --host, --prefix, --disable flags for webOS
3. ✅ **Compatibility fixes**: 
   - aligned_alloc noexcept in vlc_fixups.h
   - O_TMPFILE guards in filesystem.c
   - LIBS += -ldl in Makefiles
4. ✅ **Plugin conflicts**: DRM and freetype plugins disabled if needed
5. ✅ **Binary linking**: rpath and libvlccore.la added to bin/Makefile
6. ✅ **Build verification**: Check libvlccore.so, libvlc.so, and plugin .so files exist
7. ⚠️ **Video output**: Remind user that custom video output module is needed for GUI playback
