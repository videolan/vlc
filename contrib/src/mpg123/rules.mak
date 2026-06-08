# mpg123
MPG123_VERSION := 1.33.6
MPG123_URL := $(SF)/mpg123/mpg123/$(MPG123_VERSION)/mpg123-$(MPG123_VERSION).tar.bz2

PKGS += mpg123
ifeq ($(call need_pkg,"libmpg123"),)
PKGS_FOUND += mpg123
endif

MPG123_CONF := -DBUILD_LIBOUT123=OFF -DBUILD_PROGRAMS=OFF

$(TARBALLS)/mpg123-$(MPG123_VERSION).tar.bz2:
	$(call download_pkg,$(MPG123_URL),mpg123)

.sum-mpg123: mpg123-$(MPG123_VERSION).tar.bz2

mpg123: mpg123-$(MPG123_VERSION).tar.bz2 .sum-mpg123
	$(UNPACK)
	$(call pkg_static,"libmpg123.pc.in")
	$(call pkg_static,"libsyn123.pc.in")
	# fix llvm-mingw ARM build
	$(APPLY) $(SRC)/mpg123/getcpuflags_arm.c.patch
	$(MOVE)

.mpg123: mpg123 toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) -S $</ports/cmake $(MPG123_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
