# libxml2

LIBXML2_VERSION := 2.11.4
LIBXML2_URL := https://download.gnome.org/sources/libxml2/2.11/libxml2-$(LIBXML2_VERSION).tar.xz

PKGS += libxml2
ifeq ($(call need_pkg,"libxml-2.0"),)
PKGS_FOUND += libxml2
endif

$(TARBALLS)/libxml2-$(LIBXML2_VERSION).tar.xz:
	$(call download_pkg,$(LIBXML2_URL),libxml2)

.sum-libxml2: libxml2-$(LIBXML2_VERSION).tar.xz

LIBXML2_CONF = \
        -DLIBXML2_WITH_C14N=OFF \
        -DLIBXML2_WITH_ISO8859X=OFF \
        -DLIBXML2_WITH_SCHEMAS=OFF \
        -DLIBXML2_WITH_SCHEMATRON=OFF \
        -DLIBXML2_WITH_VALID=OFF \
        -DLIBXML2_WITH_WRITER=OFF \
        -DLIBXML2_WITH_XINCLUDE=OFF \
        -DLIBXML2_WITH_XPATH=OFF \
        -DLIBXML2_WITH_XPTR=OFF \
        -DLIBXML2_WITH_MODULES=OFF \
        -DLIBXML2_WITH_LEGACY=OFF \
        -DLIBXML2_WITH_ZLIB=OFF    \
        -DLIBXML2_WITH_ICONV=OFF   \
        -DLIBXML2_WITH_HTTP=OFF    \
        -DLIBXML2_WITH_FTP=OFF     \
        -DLIBXML2_WITH_REGEXPS=OFF \
        -DLIBXML2_WITH_PYTHON=OFF \
        -DLIBXML2_WITH_LZMA=OFF \
        -DLIBXML2_WITH_TESTS=OFF \
        -DLIBXML2_WITH_PROGRAMS=OFF

ifdef WITH_OPTIMIZATION
LIBXML2_CONF += -DLIBXML2_WITH_DEBUG=OFF
endif

libxml2: libxml2-$(LIBXML2_VERSION).tar.xz .sum-libxml2
	$(UNPACK)
	$(call pkg_static,"libxml-2.0.pc.in")
	$(MOVE)

.libxml2: libxml2 toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(LIBXML2_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
