# OpenCV 4

OPENCV4_VERSION := 4.4.0
OPENCV4_URL := $(GITHUB)/opencv/opencv/archive/$(OPENCV4_VERSION).tar.gz

ifneq ($(findstring opencv4,$(PKGS_ENABLE)),)
PKGS += opencv4
ifeq ($(call need_pkg,"opencv4 >= 4.0.0"),)
PKGS_FOUND += opencv4
endif
endif

DEPS_opencv4 = sam3 $(DEPS_sam3)

$(TARBALLS)/opencv-$(OPENCV4_VERSION).tar.gz:
	$(call download_pkg,$(OPENCV4_URL),opencv4)

.sum-opencv4: opencv-$(OPENCV4_VERSION).tar.gz

opencv4: opencv-$(OPENCV4_VERSION).tar.gz .sum-opencv4
	$(UNPACK)
	$(MOVE)

# only enable necessary pkgs
OPENCV4_CONF := \
	-DBUILD_LIST=core,imgproc,imgcodecs \
	-DOPENCV_GENERATE_PKGCONFIG=ON \
	-DBUILD_EXAMPLES=OFF \
	-DBUILD_TESTS=OFF \
	-DBUILD_ANDROID_EXAMPLES=OFF \
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
	-DWITH_COCOA=OFF

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

.opencv4: opencv4 toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(OPENCV4_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@