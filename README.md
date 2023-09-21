# VLC Media Player: Play Everything, Everywhere

**VLC** is not just a media player; it's a versatile, open-source multimedia engine. It's designed with one mission: to play everything, everywhere.

With **VLC**, you can effortlessly handle a wide range of multimedia files, discs, streams, and devices. It goes beyond mere playback; it can convert, encode, stream, and manipulate content into various formats.

This powerful tool finds its users across the globe, serving diverse purposes on numerous platforms.

If you need to integrate this engine into your own applications, look no further than *libVLC*.

**VLC** is a proud member of the [VideoLAN project](https://videolan.org), and its development is fostered by a dedicated community of volunteers.

The VideoLAN project began at [École Centrale Paris](https://www.centralesupelec.fr/) when VLC was relicensed under the GPLv2 license in February 2001. Since then, it has been downloaded billions of times.

## Licensing

**VLC** is distributed under the GPLv2 *(or later)* license. On certain platforms, it effectively operates under GPLv3 due to dependencies.

For those interested in integrating the engine into third-party applications, **libVLC** is available under the LGPLv2 *(or later)* license, allowing flexibility in licensing for embedded use.

# Supported Platforms

VLC caters to a wide range of platforms, including:

- [Windows] *(7 and later, including UWP platforms and all versions of Windows 10)*
- [macOS] *(10.10 and later)*
- [GNU/Linux] and affiliated distributions
- [BSD] and related systems
- [Android] *(4.2 and later)*, including Android TV and Android Auto
- [iOS] *(9 and later)*, including AppleTV and iPadOS
- Haiku, OS/2, and more.

[Windows]: https://www.videolan.org/vlc/download-windows.html
[macOS]: https://www.videolan.org/vlc/download-macosx.html
[GNU/Linux]: https://www.videolan.org/vlc/#download
[BSD]: https://www.videolan.org/vlc/download-freebsd.html
[Android]: https://www.videolan.org/vlc/download-android.html
[iOS]: https://www.videolan.org/vlc/download-ios.html

Please note that our resources vary across platforms.

**Nota Bene**: The [Android app](https://code.videolan.org/videolan/vlc-android/) and [iOS app](https://code.videolan.org/videolan/vlc-ios/) have separate repositories from the main one.

# Contributing & Community

**VLC** is the result of a vibrant community effort, with no monetary compensation. Our community consists of developers, helpers, maintainers, designers, and writers, all working to ensure the project's success.

While VLC's core development is primarily in C, our repository contains code in various languages, including C++, Obj-C, asm, and Rust.

Associated repositories include projects in Kotlin/Java [(Android)](https://code.videolan.org/videolan/vlc-android/), Swift [(iOS)](https://code.videolan.org/videolan/vlc-ios/), and C# [(libVLCSharp)](https://code.videolan.org/videolan/libvlcsharp/).

We welcome contributions in the following areas:
- Coding
- Packaging for Windows, macOS, and Linux distributions
- Technical writing for documentation
- Design
- Support
- Community management and communication.

Please consider contributing to our thriving community :)

You can find us on IRC in the **#videolan** channel on *[Libera.chat]*.

[Libera.chat]: https://libera.chat

## Contributions

Contributions are now managed through Merge Requests on our [GitLab repository](https://code.videolan.org/videolan/vlc/). We encourage resolving CI and discussions before merging.

# libVLC: Power for Third-Party Applications

**libVLC** is an embeddable engine tailored for third-party applications and frameworks. It runs on the same platforms as VLC and often more, providing seamless playback, streaming, and multimedia file conversion.

In addition, **libVLC** boasts extensive bindings for other programming languages, such as C++, Python, and C#.

# Support and Resources

## Useful Links

For more information and support, explore the following links:

- [VLC website](https://www.videolan.org/vlc/)
- [Support](https://www.videolan.org/support/)
- [Forums](https://forum.videolan.org/)
- [Wiki](https://wiki.videolan.org/)
- [Developer's Corner](https://wiki.videolan.org/Developers_Corner)
- [VLC hacking guide](https://wiki.videolan.org/Hacker_Guide)
- [Bugtracker](https://code.videolan.org/videolan/vlc/-/issues)
- [VideoLAN website](https://www.videolan.org/)

## Source Code Sitemap

Explore the repository's structure for detailed information:

- **ABOUT-NLS**: Notes on the Free Translation Project.
- **AUTHORS**: List of VLC authors.
- **COPYING**: The GPL license.
- **COPYING.LIB**: The LGPL license.
- **INSTALL**: Installation and building instructions.
- **NEWS**: Important modifications between releases.
- **README**: Project summary.
- **THANKS**: Acknowledgments to VLC contributors.

Additional directories:

- **bin/**: VLC binaries.
- **bindings/**: libVLC bindings to other languages.
- **compat/**: Compatibility library for operating systems lacking essential functionalities.
- **contrib/**: Facilities for retrieving external libraries and building them for systems with incompatible versions.
- **doc/**: Miscellaneous documentation.
- **extras/analyser**: Code analyzer and editor-specific files.
- **extras/buildsystem**: Files specific to different build systems.
- **extras/misc**: Files that don't fit in other extras/ categories.
- **extras/package**: VLC packaging-specific files, such as spec files.
- **extras/tools/**: Facilities for retrieving external building tools needed for systems lacking the correct versions.
- **include/**: Header files.
- **lib/**: libVLC source code.
- **modules/**: VLC plugins and modules; most of the code resides here.
- **po/**: VLC translations.
- **share/**: Common resource files.
- **src/**: libvlccore source code.
- **test/**: Testing system.

Explore and contribute to VLC's codebase through our [GitLab repository](https://code.videolan.org/videolan/vlc/).
