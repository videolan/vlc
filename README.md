# VLC media player

**VLC** is a libre and open source **media player** and **multimedia engine**,
focused on **playing everything**, and **running everywhere**.

**VLC** can play most multimedia files, discs, streams, devices and is able to
convert, encode, **stream** and manipulate streams into numerous formats.

VLC is used by many over the world, on numerous platforms, for very different use cases.

The **engine of VLC** can be embedded by 3rd party applications, and is called *libVLC*.

**VLC** is part of the [VideoLAN project](https://videolan.org) and
is developed and supported by a community of volunteers.

The VideoLAN project was started at the university [École Centrale Paris](https://www.centralesupelec.fr/) who
relicensed VLC under the GPLv2 license in February 2001. Since then, VLC has
been downloaded multi-billion times.

## License

**VLC** is released under the GPLv2 *(or later)* license.
*On some platforms, it is de facto GPLv3, because of dependencies licenses*.

**libVLC**, the engine is released under the LGPLv2 *(or later)* license. \
This allows embedding the engine in 3rd party applications, while letting them to be licensed under other licenses.

# Platforms

VLC is available on the following platforms:
- [Windows](https://www.videolan.org/vlc/download-windows.html) *(from 7 to all versions of 10)*, including UWP platforms
- [macOS](https://www.videolan.org/vlc/download-macosx.html) *(10.10 and more recent)*
- [GNU/Linux](https://www.videolan.org/vlc/#download) and affiliated
- \*BSD and affiliated
- [Android](https://www.videolan.org/vlc/download-android.html) *(4.2 and more recent)*, including Android TV and Android Auto
- [iOS](https://www.videolan.org/vlc/download-ios.html) *(9 and more recent)*, including AppleTV and iPadOS
- Haiku, OS/2 and a few others.

Not all platforms receive the same amount of care, due to our limited resources.

**Nota Bene**: [Android repository](https://code.videolan.org/videolan/vlc-android/) and
[iOS repository](https://code.videolan.org/videolan/vlc-ios/) are on different repository
than the main one.

# Contributing & Community

**VLC** is maintained by a community of people, and VideoLAN is not paying any of them.\
The community is composed of developers, helpers, maintainers, designers and writers that want
the open source project to thrive.

The main development of VLC is done in the C language, but this repository has also plenty of C++, Obj-C, asm, Rust.

Other repositories linked to vlc are done in Kotlin/Java [(Android)](https://code.videolan.org/videolan/vlc-android/),
in Swift [(iOS)](https://code.videolan.org/videolan/vlc-ios/), C# [(libVLCSharp)](https://code.videolan.org/videolan/libvlcsharp/)

We need help for the following tasks:
- coding
- packaging for Windows, macOS and Linux distributions
- technical writing for the documentation
- design
- support
- community management and communication.

Please reach us :)

We are on IRC, on the **#videolan** channel on *Freenode*.

## Contributions

Contributions are now done through Merge Requests on our [gitlab repository](https://code.videolan.org/videolan/vlc/).

CI, Discussions should be resolved before merging a Merge Request.

# libVLC

**libVLC** is an embeddable engine for 3rd party applications and frameworks.

It runs on the same platforms of VLC *(and sometimes on more platforms)* and can provide playback,
streaming and converting of multimedia files and stream.

**libVLC** has numerous bindings for other languagues, like C++, Python or C#.

# Support

## Links

You can found here several links that might help you:

- [VLC web site](http://www.videolan.org/vlc/)
- [Support](https://www.videolan.org/support/)
- [Forums](https://forum.videolan.org/)
- [Wiki](https://wiki.videolan.org/)
- [Developer's Corner](https://wiki.videolan.org/Developers_Corner)
- [VLC hacking guide](https://wiki.videolan.org/Hacker_Guide)
- [Bugtracker](https://code.videolan.org/videolan/vlc/-/issues)
- [VideoLAN web site](https://www.videolan.org/)

## Source Code sitemap
```
ABOUT-NLS          - Notes on the Free Translation Project.
AUTHORS            - VLC authors.
COPYING            - The GPL license.
COPYING.LIB        - The LGPL license.
INSTALL            - Installation and building instructions.
NEWS               - Important modifications between the releases.
README             - This file.
THANKS             - VLC contributors.

bin/               - VLC binaries.
bindings/          - libVLC bindings to other languages.
compat/            - compatibility library for operating systems missing
                     essential functionalities.
contrib/           - Facilities for retrieving external libraries and building
                     them for systems that don't have the right versions.
doc/               - Miscellaneous documentation.
extras/analyser    - Code analyser and editor specific files.
extras/buildsystem - different buildsystems specific files.
extras/misc        - Files that don't fit in the other extras/ categories.
extras/package     - VLC packaging specific files such as spec files.
extras/tools/      - Facilities for retrieving external building tools needed
                     for systems that don't have the right versions.
include/           - Header files.
lib/               - libVLC source code.
modules/           - VLC plugins and modules. Most of the code is here.
po/                - VLC translations.
share/             - Common Resources files.
src/               - libvlccore source code.
test/              - testing system.
```

