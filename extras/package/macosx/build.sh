#!/bin/sh
set -e

info()
{
    echo " [build] $1"
}

usage()
{
cat << EOF
usage: $0 [options]

OPTIONS:
   -h            Show some help
EOF

}

spushd()
{
    pushd "$1" > /dev/null
}

spopd()
{
    popd > /dev/null
}

while getopts "h" OPTION
do
     case $OPTION in
         h)
             usage
             exit 1
             ;;
     esac
done
shift $(($OPTIND - 1))

if [ "x$1" != "x" ]; then
    usage
    exit 1
fi

info "Building VLC for the Mac OS X"

spushd `dirname $0`/../../..
vlcroot=`pwd`
spopd

builddir="${vlcroot}/build-macosx"

export CC=/Developer/usr/bin/llvm-gcc-4.2
export CXX=/Developer/usr/bin/llvm-g++-4.2
export OBJC=/Developer/usr/bin/llvm-gcc-4.2

spushd "${vlcroot}/extras/contrib"
./bootstrap
make
spopd

mkdir -p "${builddir}"

if ! [ -e "${vlcroot}/configure" ]; then
    ${vlcroot}/bootstrap
fi

spushd "${builddir}"

# Run configure only upon changes.
if [ "${vlcroot}/configure" -nt config.log ]; then
  ${vlcroot}/configure \
      --enable-debug
fi

core_count=`sysctl -n machdep.cpu.core_count`
let jobs=$core_count+1

info "Running make -j$jobs"

make -j$jobs
spopd

