# jpeg

JPEG_VERSION := 3.1.4.1
JPEG_URL := $(GITHUB)/libjpeg-turbo/libjpeg-turbo/releases/download/$(JPEG_VERSION)/libjpeg-turbo-$(JPEG_VERSION).tar.gz

ifdef BUILD_ENCODERS
PKGS += jpeg
endif

$(TARBALLS)/libjpeg-turbo-$(JPEG_VERSION).tar.gz:
	$(call download_pkg,$(JPEG_URL),jpeg)

.sum-jpeg: libjpeg-turbo-$(JPEG_VERSION).tar.gz

jpeg: libjpeg-turbo-$(JPEG_VERSION).tar.gz .sum-jpeg
	$(UNPACK)
	# disable SIMD coverage executable
	sed -i.orig 's,WITH_SIMD AND ENABLE_STATIC,FALSE,' $(UNPACK_DIR)/simd/CMakeLists.txt
	$(MOVE)

JPEG_CONF:= -DENABLE_SHARED=OFF -DWITH_TURBOJPEG=OFF -DWITH_TOOLS=OFF

.jpeg: jpeg toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(JPEG_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL) --component lib
	$(CMAKEINSTALL) --component include
	touch $@
