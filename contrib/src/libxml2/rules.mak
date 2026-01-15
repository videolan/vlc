# libxml2

LIBXML2_VERSION := 2.15.1
LIBXML2_URL := https://download.gnome.org/sources/libxml2/2.15/libxml2-$(LIBXML2_VERSION).tar.xz

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
        -DLIBXML2_WITH_ZLIB=OFF    \
        -DLIBXML2_WITH_ICONV=OFF   \
        -DLIBXML2_WITH_REGEXPS=OFF \
        -DLIBXML2_WITH_TESTS=OFF \
        -DLIBXML2_WITH_PROGRAMS=OFF

ifdef WITH_OPTIMIZATION
LIBXML2_CONF += -DLIBXML2_WITH_DEBUG=OFF
endif

libxml2: libxml2-$(LIBXML2_VERSION).tar.xz .sum-libxml2
	$(UNPACK)
	$(APPLY) $(SRC)/libxml2/0001-threads-don-t-force-_WIN32_WINNT-to-Vista-if-it-s-se.patch
	$(APPLY) $(SRC)/libxml2/0002-globals-don-t-use-destructor-in-UWP-builds.patch
	$(call pkg_static,"libxml-2.0.pc.in")
	$(MOVE)

.libxml2: libxml2 toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(LIBXML2_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
