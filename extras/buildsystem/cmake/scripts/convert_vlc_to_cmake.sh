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
rm -f CMakeLists.txt
ln -sf $cmake_dir/CMakeLists/root_CMakeLists.txt CMakeLists.txt

echo "Installing src/CMakeLists.txt"
rm -f src/CMakeLists.txt
ln -sf ../$cmake_dir/CMakeLists/src_CMakeLists.txt src/CMakeLists.txt

echo "Removing old modules/gui/qt4/CMakeLists.txt"
rm -f modules/gui/qt4/CMakeLists.txt

echo "Installing modules/CMakeLists.txt"
rm -f modules/CMakeLists.txt
ln -s ../$cmake_dir/CMakeLists/modules_CMakeLists.txt modules/CMakeLists.txt

echo "Installing po/CMakeLists.txt"
rm -f po/CMakeLists.txt
ln -s ../$cmake_dir/CMakeLists/po_CMakeLists.txt po/CMakeLists.txt

echo "Installing include/config.h.cmake"
rm -f include/config.h.cmake
ln -sf ../$cmake_dir/config.h.cmake include/config.h.cmake

echo "Installing cmake/"
rm -f cmake
ln -sf $cmake_dir/include cmake

echo "Generating CMakeLists for modules/"
sh $cmake_dir/scripts/convert_modules_to_cmake.sh modules

echo "Installing modules/gui/qt4/CMakeLists.txt"
rm -f modules/gui/qt4/CMakeLists.txt
ln -sf ../../../$cmake_dir/CMakeLists/qt4_CMakeLists.txt modules/gui/qt4/CMakeLists.txt

