# OpenCV 4

OPENCV4_VERSION := 4.4.0
OPENCV4_URL := $(GITHUB)/opencv/opencv/archive/$(OPENCV4_VERSION).tar.gz

ifndef HAVE_WINSTORE # uses winrt APIs not working with mingw
PKGS += opencv4
endif
ifeq ($(call need_pkg,"opencv4 >= 4.0.0"),)
PKGS_FOUND += opencv4
endif

DEPS_opencv4 = zlib $(DEPS_zlib) jpeg $(DEPS_jpeg) png $(DEPS_png)
ifneq ($(findstring protobuf,$(PKGS)),)
DEPS_opencv4 = protobuf $(DEPS_protobuf)
endif

$(TARBALLS)/opencv-$(OPENCV4_VERSION).tar.gz:
	$(call download_pkg,$(OPENCV4_URL),opencv4)

.sum-opencv4: opencv-$(OPENCV4_VERSION).tar.gz

opencv4: opencv-$(OPENCV4_VERSION).tar.gz .sum-opencv4
	$(UNPACK)
	# fix build with newer CMake
	sed -i.orig 's,cmake_minimum_required(VERSION 2.8.12.2),cmake_minimum_required(VERSION 3.5),' $(UNPACK_DIR)/cmake/OpenCVGenPkgconfig.cmake
	# enable pkg-config on all configurations
	sed -i.orig 's,if(MSVC OR IOS),if(0),' $(UNPACK_DIR)/cmake/OpenCVGenPkgconfig.cmake
	# always install pkgconfig file
	sed -i.orig 's,if(UNIX AND NOT ANDROID),if(1),' $(UNPACK_DIR)/cmake/OpenCVGenPkgconfig.cmake
	# fix ARM intrin.h case
	sed -i.orig 's,Intrin.h,intrin.h,g' $(UNPACK_DIR)/modules/core/include/opencv2/core/cv_cpu_dispatch.h
	sed -i.orig 's,Intrin.h,intrin.h,' $(UNPACK_DIR)/cmake/checks/cpu_neon.cpp
	sed -i.orig 's,Intrin.h,intrin.h,' $(UNPACK_DIR)/modules/flann/include/opencv2/flann/dist.h
	$(call pkg_static,"cmake/templates/opencv-XXX.pc.in")
	$(MOVE)

# only enable necessary pkgs
OPENCV4_ENV =

OPENCV4_CONF := \
	-DBUILD_LIST=core,imgproc,imgcodecs,objdetect \
	-DOPENCV_GENERATE_PKGCONFIG=ON \
	-DBUILD_EXAMPLES=OFF \
	-DBUILD_TESTS=OFF \
	-DBUILD_ANDROID_EXAMPLES=OFF \
	-DBUILD_ANDROID_PROJECTS=OFF \
	-DBUILD_PERF_TESTS=OFF \
	-DBUILD_DOCS=OFF \
	-DBUILD_opencv_apps=OFF \
	-DWITH_GTK=OFF \
	-DWITH_QT=OFF \
	-DWITH_OPENGL=OFF \
	-DWITH_OPENCL=OFF \
	-DWITH_LAPACK=OFF \
	-DWITH_EIGEN=OFF \
	-DWITH_FFMPEG=OFF \
	-DWITH_GSTREAMER=OFF \
	-DWITH_V4L=OFF \
	-DWITH_1394=OFF \
	-DWITH_LIBV4L=OFF \
	-DWITH_AVFOUNDATION=OFF \
	-DWITH_IPP=OFF \
	-DWITH_WEBP=OFF \
	-DWITH_OPENJPEG=OFF \
	-DWITH_JASPER=OFF \
	-DWITH_TIFF=OFF \
	-DWITH_COCOA=OFF \
	-DBUILD_PROTOBUF=OFF \
	-DWITH_CAROTENE=OFF \
	-DWITH_ADE=OFF \
	-DBUILD_ZLIB=OFF \
	-DBUILD_PNG=OFF \
	-DBUILD_JPEG=OFF \
	-DBUILD_ITT=OFF \
	-DOPENCV_FORCE_FUNCTIONS_SECTIONS=ON
# OpenCV's adds -ffunction-sections/-fdata-sections by default, but skips for static
# iOS/Android builds. Force it on

# NEON mandatory on aarch64
ifneq ($(findstring aarch64,$(HOST)),)
OPENCV4_CONF += \
	-DHAVE_CPU_NEON_SUPPORT:BOOL=ON \
	-DCPU_BASELINE=NEON \
	-DENABLE_NEON=ON
endif

# intrin_wasm.hpp requires -msimd128 to compile, but not provided
ifneq ($(findstring emscripten,$(HOST)),)
OPENCV4_CONF += -DCV_ENABLE_INTRINSICS=OFF
endif

ifdef HAVE_CROSS_COMPILE
# force detecting contribs with pkg-config
OPENCV4_ENV += PKG_CONFIG_LIBDIR="$(PKG_CONFIG_PATH)"
endif

.opencv4: opencv4 toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(OPENCV4_ENV) $(CMAKE) $(OPENCV4_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@