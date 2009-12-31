#
# Configure script
#
#   used by VLCKit.xcodeproj

if test "x$SYMROOT" = "x"; then
    echo " This script is bound to be launched by VLCKit.xcodeproj, not you"
    exit 1
fi

if test "$ACTION" = "clean"; then
    rm -Rf $SYMROOT/vlc_build_dir
    exit 0
fi

# Contruct the vlc_build_dir
mkdir -p $SYMROOT/vlc_build_dir
cd $SYMROOT/vlc_build_dir

# Contruct the argument list
echo "Building for $ARCHS with sdk=\"$SDKROOT\""


args="--disable-nls $args"

# Mac OS X related options
args="--disable-macosx-defaults $args"
args="--disable-macosx $args" # Disable old gui/macosx
args="--disable-macosx-vlc-app $args" # Don't build old vlc.app

args="--with-macosx-version-min=10.5 $args"

# optional modules
args="--enable-faad $args"
args="--enable-flac $args"
args="--enable-theora $args"
args="--enable-shout $args"
args="--enable-cddax $args"
args="--enable-caca $args"
args="--enable-vcdx $args"
args="--enable-twolame $args"
args="--enable-realrtsp $args"
args="--enable-libass $args"
args="--enable-asademux $args"

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

# where to install
args="--prefix=$SYMROOT/vlc_build_dir/vlc_install_dir $args"

if test "x$SDKROOT" != "x"
then
	args="--with-macosx-sdk=$SDKROOT $args"
fi

# Debug Flags
if test "$CONFIGURATION" = "Debug"; then
	args="--enable-debug $args"
else
	args="--enable-release $args"
fi

archcount=0

# 64 bits switches
for a in $ARCHS; do
	archs_args="-arch $a $archs_args"
    let archcount=archcount+1
done

if test $archcount -ge 2; then
    echo "****"
    echo "WARNING: You are building a VLC with --disable-dependency-tracking"
    echo "         because FAT architecture are not compatible with "
    echo "         disable-dependency-tracking."
    echo ""
    echo "         You will have to do a clean build manually."
    echo ""
    echo "To disable this message, build only one architecture."
    echo ""
    echo "****"
    args="--disable-dependency-tracking $args"
fi

echo "Running [$archs_args] configure $args"
top_srcdir="$SRCROOT/../../.."
CFLAGS="$archs_args" CXXFLAGS="$archs_args" OBJCFLAGS="$archs_args" exec $top_srcdir/configure $args