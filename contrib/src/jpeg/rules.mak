# jpeg

JPEG_VERSION := 2.0.8-esr
JPEG_URL := $(GITHUB)/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/$(JPEG_VERSION).tar.gz

$(TARBALLS)/libjpeg-turbo-$(JPEG_VERSION).tar.gz:
	$(call download_pkg,$(JPEG_URL),jpeg)

.sum-jpeg: libjpeg-turbo-$(JPEG_VERSION).tar.gz

jpeg: libjpeg-turbo-$(JPEG_VERSION).tar.gz .sum-jpeg
	$(UNPACK)
	$(MOVE)

JPEG_CONF:= -DENABLE_SHARED=OFF -DWITH_TURBOJPEG=OFF
ifndef HAVE_WIN32
# this should probably be a global setting for CMake targets
JPEG_CONF += -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=TRUE
endif

.jpeg: jpeg toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_PIC) $(CMAKE) $(JPEG_CONF)
	+$(CMAKEBUILD)
	+$(CMAKEBUILD) --target install
	touch $@
