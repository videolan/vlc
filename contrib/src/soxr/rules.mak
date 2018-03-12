# SoXR

SOXR_VERSION := 0.1.3
SOXR_URL := http://vorboss.dl.sourceforge.net/project/soxr/soxr-$(SOXR_VERSION)-Source.tar.xz

ifeq ($(call need_pkg,"soxr >= 0.1"),)
PKGS_FOUND += soxr
endif
DEPS_soxr = ffmpeg $(DEPS_ffmpeg)

$(TARBALLS)/soxr-$(SOXR_VERSION)-Source.tar.xz:
	$(call download_pkg,$(SOXR_URL),soxr)

.sum-soxr: soxr-$(SOXR_VERSION)-Source.tar.xz

soxr: soxr-$(SOXR_VERSION)-Source.tar.xz .sum-soxr
	$(UNPACK)
	$(MOVE)

.soxr: soxr toolchain.cmake
	rm -f $</CMakeCache.txt
	cd $< && $(HOSTVARS_PIC) $(CMAKE) \
		-DBUILD_SHARED_LIBS=OFF \
		-DBUILD_EXAMPLES=OFF \
		-DBUILD_TESTS=OFF \
		-DWITH_LSR_BINDINGS=OFF \
		-DWITH_OPENMP=OFF \
		-DWITH_AVFFT=ON \
		-Wno-dev $(CMAKE_GENERATOR)
	cd $< && $(MAKE) install
	touch $@
