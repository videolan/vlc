# Compile VLC for Windows

There are multiple ways to compile VLC for Windows. All of them involve `gcc` or `llvm` with `mingw-w64`.
Compilation with MSVC or clang-cl is currently not supported.

# UNIX Shell

You will also need a UNIX shell to run the build process. On Windows you can either
use [Windows Subsystem for Linux (WSL)](https://learn.microsoft.com/en-us/windows/wsl/) (recommended) or [msys2](https://www.msys2.org/).
You may also build inside a Docker on [Linux](https://docs.docker.com/desktop/install/linux-install/)
or on [Windows](https://docs.docker.com/desktop/install/windows-install/).

The choice of which is best depends on your goals. In terms of build times, Docker is the fastest
then WSL and msys2 is the slowest.

Unless you use a Docker image, you will need to install multiple development packages that VLC
needs to build itself and its contribs.

* On Ubuntu/Debian (or WSL), they are the same tools installed in our [Docker Images](https://code.videolan.org/videolan/docker-images/-/blob/master/vlc-debian-win64/Dockerfile#L27):
```
sudo apt-get update -qq
sudo apt-get install -qqy \
    git wget bzip2 file libwine-dev unzip libtool libtool-bin libltdl-dev pkg-config ant \
    build-essential automake texinfo ragel yasm p7zip-full autopoint \
    gettext cmake zip wine nsis g++-mingw-w64-i686 curl gperf flex bison \
    libcurl4-gnutls-dev python3 python3-setuptools python3-mako python3-requests \
    gcc make procps ca-certificates \
    openjdk-11-jdk-headless nasm jq gnupg \
    meson autoconf
```

* On msys2 you should install similar packages:
```
pacman -Syu
pacman -S --needed git wget bzip2 file unzip libtool pkg-config \
    automake autoconf texinfo yasm p7zip \
    gettext cmake zip curl gperf flex bison \
    python3 python3-setuptools python3-mako \
    gcc make ca-certificates nasm gnupg patch help2man \
    ragel python3 meson
```
<!-- pacman -S ant autopoint nsis python3-requests jq openjdk-11-jdk-headless -->

# Toolchains

There are 2 toolchains supported to build VLC for Windows:
* mingw-w64 with gcc
* mingw-w64 with llvm

The `gcc` is the most common one as it comes as packages in Linux distributions and msys2.
The problem with the gcc toolchain is that if you need to debug your build with `gdb`.
It is very slow when you use breakpoints because for each of the 200+ DLLs loaded
during the VLC launch, it looks for your breakpoints.

The 'llvm' toolchain solves this issue by producing .pdb files. You then debug your
code with the Windows Debugger, even within Visual Studio.

## Install mingw-w64 LLVM

The 'llvm' toolchain is normally not found in Linux or msys2. You will need to install it yourself.
You can download prebuilt packages from https://github.com/mstorsjo/llvm-mingw. And then add the
place where you decompressed it, followed by `/bin`, in your `PATH`.

You also have a choice between `ucrt` and `msvcrt`. `ucrt` is the one you should use if you don't care about
support on Windows versions older than Windows 10. If you need to support older Windows versions you
should go with the `msvcrt` version. The official VLC builds use `msvcrt` for desktop builds, and
`ucrt` for Universal Windows Platform (UWP) builds.

* On Linux:
```sh
wget https://github.com/mstorsjo/llvm-mingw/releases/download/20220906/llvm-mingw-20220906-msvcrt-ubuntu-18.04-x86_64.tar.xz
tar xvf llvm-mingw-20220906-msvcrt-ubuntu-18.04-x86_64.tar.xz -C /opt
export PATH=/opt/llvm-mingw-20220906-msvcrt-ubuntu-18.04-x86_64/bin:$PATH
```

* On msys2, **use the mingw64 (blue) environment** (ie not msys (purple) the  or mingw32 (grey) environments):
```sh
wget https://github.com/mstorsjo/llvm-mingw/releases/download/20220906/llvm-mingw-20220906-msvcrt-x86_64.zip
unzip llvm-mingw-20220906-msvcrt-x86_64.zip -d /opt
export PATH=/opt/llvm-mingw-20220906-msvcrt-x86_64/bin:$PATH
```

Every time you build VLC, you will need to have the toolchain in your PATH.
A convenient way to setup your environment is to set command in a file and call
it when you start your build sesson:

Create toolchain.sh:
```sh
cat export PATH=/opt/llvm-mingw-20220906-msvcrt-x86_64/bin:$PATH > toolchain.sh
```

Use toolchain.sh to set the path to your compiler:
```sh
source toolchain.sh
```

* On Docker, you can use these 2 images:

  * msvcrt: `registry.videolan.org/vlc-debian-llvm-msvcrt:20221011232542`
  * ucrt:   `registry.videolan.org/vlc-debian-llvm-ucrt:20221012005047`

You can find the latest Docker images we use in [extras/ci/gitlab-ci.yml](/extras/ci/gitlab-ci.yml)

## Install mingw-w64 gcc

This is the default build environment of VLC when cross-compiling. This is also
the environment used to produce the official builds that everyone uses. The mingw-w64
toolchain is usually found in all Linux distributions and in msys2.
You can install it this way:

* On Linux:
```sh
sudo apt-get install -qqy \
        gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64 mingw-w64-tools
```

* On msys2:
```sh
pacman -S mingw-w64-x86_64-toolchain
> :: Repository mingw64
   1) mingw-w64-x86_64-binutils  2) mingw-w64-x86_64-crt-git  3) mingw-w64-x86_64-gcc  4) mingw-w64-x86_64-gcc-ada  5) mingw-w64-x86_64-gcc-fortran  6) mingw-w64-x86_64-gcc-libgfortran  7) mingw-w64-x86_64-gcc-libs  8) mingw-w64-x86_64-gcc-objc
   9) mingw-w64-x86_64-gdb  10) mingw-w64-x86_64-gdb-multiarch  11) mingw-w64-x86_64-headers-git  12) mingw-w64-x86_64-libgccjit  13) mingw-w64-x86_64-libmangle-git  14) mingw-w64-x86_64-libwinpthread-git  15) mingw-w64-x86_64-make  16) mingw-w64-x86_64-pkgconf
   17) mingw-w64-x86_64-tools-git  18) mingw-w64-x86_64-winpthreads-git  19) mingw-w64-x86_64-winstorecompat-git
> Enter a selection (default=all):

Type enter to select "all"
```


* On Docker, you can use the `registry.videolan.org/vlc-debian-win64:20221011230137` image.

You can find the latest Docker images we use in [extras/ci/gitlab-ci.yml](/extras/ci/gitlab-ci.yml)


# Getting VLC

This is the easy part. In your UNIX Shell type:

```sh
git config --global core.autocrlf false
git clone https://code.videolan.org/videolan/vlc.git
```

This will download the VLC source code into the `vlc` folder, in your current directory.

# Building VLC

Fully build in a separate folder `build`:

```sh
mkdir build
cd build
../vlc/extras/package/win32/build.sh -a x86_64
```

This will build
* VLC "extra tools" in case you are missing some in your environment.
* VLC "contribs", a hundred different libraries that VLC uses like FFmpeg, Qt, etc.
* VLC "core" (libvlccore.dll) the heart of VLC.
* VLC "modules" which contains all the possible extensions to add functionality to VLC.
* libvlc.dll a DLL to use VLC from external code with a stable API.

## Prebuilt contribs

You may not want to spend one or two hours building all the contribs if you're never going to
want to touch any of them. In that case you may want to reuse prebuilt binaries and save some time.
To build with prebuilt contribs you need to have a **matching compiler** and especially
a **matching C++ compiler**. So the ones available may not match your environment. In that case you
still need to build the contribs yourself.

The choice of compiler may depend on how you plan to debug your code. See the [Debugging
section](#debugging) below for more information.

## Building with gcc (11) prebuilt contribs

If your mingw-w64 compiler/toolchain is gcc 11 you can use these commands to build VLC
and reuse prebuilt contribs:

```sh
mkdir build
cd build
export VLC_CONTRIB_SHA="$(cd ../vlc; extras/ci/get-contrib-sha.sh)"
export VLC_PREBUILT_CONTRIBS_URL="https://artifacts.videolan.org/vlc/win64/vlc-contrib-x86_64-w64-mingw32-${VLC_CONTRIB_SHA}.tar.bz2"
../vlc/extras/package/win32/build.sh -a x86_64 -p
```

## Building with LLVM (13) prebuilt contribs

If your mingw-w64 compiler/toolchain is LLVM 13 you can use these commands to build VLC
and reuse prebuilt contribs. The name of the prebuilt tarball is the same, but the folder is different:

```sh
mkdir build
cd build
export VLC_CONTRIB_SHA="$(cd ../vlc; extras/ci/get-contrib-sha.sh)"
export VLC_PREBUILT_CONTRIBS_URL="https://artifacts.videolan.org/vlc/win64-llvm/vlc-contrib-x86_64-w64-mingw32-$VLC_CONTRIB_SHA.tar.bz2"
time ../vlc/extras/package/win32/build.sh -a x86_64 -p
```

## libvlc

You may be interested in just building libvlc without the desktop app. In that case
you can add `-z` to you build.sh call. It will save you some building time, especially
if you build contribs. And it will also avoid creating the Qt gui plugins you will never use.

```sh
mkdir build
cd build
../vlc/extras/package/win32/build.sh -a x86_64 -z
```

# Other options

## CPU Control

You may want to reduce the load on your CPU, and thus your memory, when building.
You can control the number of maximum threads used during compilation with the `JOBS`
environment variable.

Due to a missing feature in ninja, contribs built from Meson will use all your CPU's
regardless of the `JOBS` value. You can reduce the problem by allocating a single
CPU thread to each Meson contrib with the `MESON_BUILD` environment variable.

Here is an example limit the build to 8 threads and 1 per Meson/ninja contrib.

```sh
JOBS=8 MESON_BUILD="-j 1" ../vlc/extras/package/win32/build.sh -a x86_64
```

## Config Flags

When building VLC manually, it's possible to enable/disable certain features of
VLC or select/unselect some compilation configurations.
You can find the list of options when running :

```sh
../vlc/configure -h
```

You can pass all these options to `build.sh` using the `CONFIGFLAGS` environment
variable.
For example you can enable the address sanitizer with:

```sh
CONFIGFLAGS="--with-sanitizer=address" ../vlc/extras/package/win32/build.sh -a x86_64
```

## Contrib Flags

When building VLC manually, it's possible to select/unselect certain contribs or
enable/disable some features.
You can find the list of options when running:

```sh
../vlc/contribs/bootstrap -h
```

You can pass all these options to `build.sh` using the `CONTRIBFLAGS` environment
variable.
For example you can disable bluray building with:

```sh
CONTRIBFLAGS="--disable-bluray" ../vlc/extras/package/win32/build.sh -a x86_64
```

# Debugging

Most of the time, if you need to build VLC yourself, you will also need to debug it.
The way to debug VLC on Windows depends on the toolchain you are using. When building
with gcc you need to use `gdb`. When building with LLVM, you can use `gdb` or
the `Windows Debugger`.

The latter is much faster than gdb which is very slow to pick breakpoints in the
hundred of VLC DLL's.

## Debugging with Windows Debugger

By default VLC is built with debug symbols. But if you want to use the **`Windows Debugger`
with LLVM**, you need to build with `.pdb` files. You can do that by adding `-d` to
the `build.sh` call.

```sh
../vlc/extras/package/win32/build.sh -a x86_64 -d
```

This will create PDB files, but since we are building in a UNIX environment, the
paths written in the PDB are UNIX files. The debugger will fail to find the proper
source files when you are debugging. There are different ways to fix this. Both
involve mapping the UNIX paths to paths in your Windows machine. That also means
if you want to debug properly the code you are editing, it needs to be visible
in Windows. It's always the case in msys2. It can be the case with WSL also it's
faster in a WSL1 environment than in WSL2. In Docker it's also possible by
mapping a Windows folder to make it accessible in your Docker. In the end your
VLC source code **should be on a partition/drive that Windows can see**,
and thus the Windows Debugger.

You can set the UNIX to Windows mapping when enabling PDB's in `build.sh`. You need
to set the Windows path where the VLC sources are this way:

```sh
../vlc/extras/package/win32/build.sh -a x86_64 -D "c:/path/to/sources/vlc"
```

You can also set it in your environment `CFLAGS` and `CXXFLAGS` which you can also add
in your `toolchain.sh` described [above](#install-mingw-w64-llvm):

```sh
CFLAGS="-fdebug-prefix-map='/mnt/c/'='c:/'" CXXFLAGS="-fdebug-prefix-map='/mnt/c/'='c:/'" ../vlc/extras/package/win32/build.sh -a x86_64 -d
```

## VLC desktop debug

The VLC you download has all plugins structured in different folders, all in a `"plugins"` folder.
But when you build VLC this is not the case. VLC cannot be used that way. It needs to be
either *installed* with the following command:

```sh
cd win64; make package-win-common
```

You will end up with a `vlc-4.0.0-dev` folder with the vlc EXE's and DLL's in the
proper place. You can debug from there. But this way of building is much slower than
just building the files that have been modified.

A faster way to debug is to just build with `build.sh` or even just calling make
in the `win64` folder. In that case you need to setup your debugging environment to
pick `libvlccore.dll`, `libvlc.dll` and plugins that you built. `libvlccore.dll` is
found in `win64/src/.libs` and `libvlc.dll` is found in `win64/lib/.libs`. You need to
add these paths to your `PATH`.

```
PATH="c:\path\to\sources\vlc\win64\lib\.libs;c:\path\to\sources\vlc\win64\src\.libs;C:\Windows\System32;C:\Windows;C:\Windows\System32\downlevel"
```

And when you run vlc.exe, you also need to set the `VLC_PLUGIN_PATH` environment variable
to the folder with all the plugins you built:

```
VLC_PLUGIN_PATH=c:\path\to\sources\vlc\win64\modules
```

# Future

In the future it might be possible to build VLC with Meson. With a native Windows toolchain it might
be possible to have proper Windows paths when compiling, which makes it even easier to
develop (click on the file path on compilation error). It will also avoid mapping the
PDB paths of UNIX paths to Windows paths.
