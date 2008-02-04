#!/bin/sh

cmake_dir=extras/buildsystem/cmake

if ! test -e configure.ac
then
   echo "***"
   echo "*** Error: You must run that script from the root vlc source tree"
   echo "***"
   exit -1
fi

echo "Installing CMakeLists.txt"
ln -sf $cmake_dir/CMakeLists/root_CMakeLists.txt CMakeLists.txt

echo "Installing src/CMakeLists.txt"
ln -sf ../$cmake_dir/CMakeLists/src_CMakeLists.txt src/CMakeLists.txt

echo "Installing include/config.h.cmake"
ln -sf ../$cmake_dir/config.h.cmake include/config.h.cmake

echo "Installing cmake/"
ln -sf $cmake_dir/include cmake

echo "Generating CMakeLists for modules/"
sh $cmake_dir/scripts/convert_modules_to_cmake.sh modules

