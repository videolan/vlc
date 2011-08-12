#
# Configure script
#
#   used by VLCKit.xcodeproj

if test "x$SYMROOT" = "x"; then
    echo " This script is bound to be launched by VLCKit.xcodeproj, not you"
    exit 1
fi

if test "$ACTION" = "clean"; then
    rm -Rf $VLC_BUILD_DIR
    exit 0
fi

# Contruct the vlc_build_dir
mkdir -p $VLC_BUILD_DIR
cd $VLC_BUILD_DIR

# Contruct the argument list
echo "Building for $ARCHS with sdk=\"$SDKROOT\" in $VLC_BUILD_DIR"


args="--disable-nls $args"

# Mac OS X related options
args="--disable-macosx-defaults $args"
args="--disable-macosx $args" # Disable old gui/macosx
args="--disable-macosx-vlc-app $args" # Don't build old vlc.app

args="--with-macosx-version-min=10.6 $args"

# optional modules
args="--enable-faad $args"
args="--enable-flac $args"
args="--enable-theora $args"
args="--enable-shout $args"
args="--enable-caca $args"
args="--enable-vcdx $args"
args="--enable-twolame $args"
args="--enable-realrtsp $args"
args="--enable-libass $args"

# disabled stuff
args="--disable-ncurses $args"
args="--disable-httpd $args"
args="--disable-vlm $args"
args="--disable-skins2 $args"
args="--disable-glx $args"
args="--disable-xvideo $args"
args="--disable-xcb $args"
args="--disable-sdl $args"
args="--disable-sdl-image $args"
args="--disable-visual $args"

if test "x$SDKROOT" != "x"
then
	args="--with-macosx-sdk=$SDKROOT $args"
fi

# Debug Flags
if test "$CONFIGURATION" = "Debug"; then
	args="--enable-debug $args"
fi

top_srcdir="$VLC_SRC_DIR"

# 64 bits switches
for arch in $ARCHS; do
    this_args="$args"

    # where to install
    this_args="--prefix=${VLC_BUILD_DIR}/$arch/vlc_install_dir $this_args"

    input="$top_srcdir/configure"
    output="$arch/Makefile"
    echo `pwd`"/${output}"
    if test -e ${output} && test ${output} -nt ${input}; then
        echo "No need to re-run configure for $arch"
        continue;
    fi

    # Contruct the vlc_build_dir/$arch
    mkdir -p $arch
    cd $arch

    echo "Running [$arch] configure $this_args"
    if test $arch = "x86_64"; then
        export CFLAGS="-m64 -arch x86_64"
        export CXXFLAGS="-m64 -arch x86_64"
        export OBJCFLAGS="-m64 -arch x86_64"
        export CPPFLAGS="-m64 -arch x86_64"
        this_args="--with-contrib=${VLC_SRC_DIR}/extras/contrib/hosts/x86_64-apple-darwin10 $this_args"
        $top_srcdir/configure --build=x86_64-apple-darwin10 $this_args
    fi
    if test $arch = "i386"; then
        export CFLAGS="-m32 -arch i686"
        export CXXFLAGS="-m32 -arch i686"
        export OBJCFLAGS="-m32 -arch i686"
        export CPPFLAGS="-m32 -arch i686"
        this_args="--with-contrib=${VLC_SRC_DIR}/extras/contrib/hosts/i386-apple-darwin10 $this_args"
        $top_srcdir/configure --build=i686-apple-darwin10 $this_args
    fi
    if test $arch = "ppc"; then
        export CFLAGS="-m32 -arch ppc"
        export CXXFLAGS="-m32 -arch ppc"
        export OBJCFLAGS="-m32 -arch ppc"
        export CPPFLAGS="-m32 -arch ppc"
        this_args="--with-contrib=${VLC_SRC_DIR}/extras/contrib/hosts/powerpc-apple-darwin9 $this_args"
        $top_srcdir/configure --build=powerpc-apple-darwin9 $this_args
    fi
    cd ..
done
