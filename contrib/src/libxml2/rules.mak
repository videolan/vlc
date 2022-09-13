# libxml2

LIBXML2_VERSION := 2.9.14
LIBXML2_URL := https://download.gnome.org/sources/libxml2/2.9/libxml2-$(LIBXML2_VERSION).tar.xz

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
        -DLIBXML2_WITH_DOCB=OFF    \
        -DLIBXML2_WITH_REGEXPS=OFF \
        -DLIBXML2_WITH_PYTHON=OFF \
        -DLIBXML2_WITH_LZMA=OFF \
        -DLIBXML2_WITH_TESTS=OFF \
        -DLIBXML2_WITH_PROGRAMS=OFF

ifdef WITH_OPTIMIZATION
LIBXML2_CONF += -DLIBXML2_WITH_DEBUG=OFF
endif

XMLCONF += CFLAGS="$(CFLAGS) -DLIBXML_STATIC"

libxml2: libxml2-$(LIBXML2_VERSION).tar.xz .sum-libxml2
	$(UNPACK)
	# fix pkg-config file using an unset variable
	sed -e 's,"\\\$${pcfiledir}/$${PACKAGE_RELATIVE_PATH}","$${CMAKE_INSTALL_PREFIX}",' -i.orig  "$(UNPACK_DIR)/CMakeLists.txt"
ifdef HAVE_WINSTORE
	$(APPLY) $(SRC)/libxml2/nogetcwd.patch
endif
	$(call pkg_static,"libxml-2.0.pc.in")
	$(MOVE)

.libxml2: libxml2 toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS) $(CMAKE) $(LIBXML2_CONF)
	+$(CMAKEBUILD)
	+$(CMAKEBUILD) --target install
	touch $@
