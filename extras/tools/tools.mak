# Copyright (C) 2003-2011 the VideoLAN team
#
# This file is under the same license as the vlc package.

include $(TOOLS)/packages.mak
TARBALLS := $(TOOLS)

#
# common rules
#

ifeq ($(shell command -v curl >/dev/null 2>&1 || echo FAIL),)
download = curl -f -L --retry 3 --output "$@" -- "$(1)"
else ifeq ($(shell command -v wget >/dev/null 2>&1 || echo FAIL),)
download = rm -f $@.tmp && \
	wget --passive -c -p -O $@.tmp "$(1)" && \
	touch $@.tmp && \
	mv $@.tmp $@
else ifeq ($(shell command -v fetch >/dev/null 2>&1 || echo FAIL),)
download = rm -f $@.tmp && \
	fetch -p -o $@.tmp "$(1)" && \
	touch $@.tmp && \
	mv $@.tmp $@
else
download = $(error Neither curl nor wget found!)
endif

ifeq ($(shell command -v sha512sum >/dev/null 2>&1 || echo FAIL),)
SHA512SUM = sha512sum -c
else ifeq ($(shell command -v shasum >/dev/null 2>&1 || echo FAIL),)
SHA512SUM = shasum -a 512 --check
else ifeq ($(shell command -v openssl >/dev/null 2>&1 || echo FAIL),)
SHA512SUM = openssl dgst -sha512
else
SHA512SUM = $(error SHA-512 checksumming not found!)
endif

download_pkg = $(call download,$(VIDEOLAN)/$(2)/$(lastword $(subst /, ,$(@)))) || \
	( $(call download,$(1)) && echo "Please upload this package $(lastword $(subst /, ,$(@))) to our FTP" )  \
	&& grep $(@) $(TOOLS)/SHA512SUMS| $(SHA512SUM) /dev/stdin

ifeq ($(V),1)
TAR_VERBOSE := v
endif

UNPACK = $(RM) -R $@ \
    $(foreach f,$(filter %.tar.gz %.tgz,$^), && tar $(TAR_VERBOSE)xzfo $(f)) \
    $(foreach f,$(filter %.tar.bz2,$^), && tar $(TAR_VERBOSE)xjfo $(f)) \
    $(foreach f,$(filter %.tar.xz,$^), && tar $(TAR_VERBOSE)xJfo $(f)) \
    $(foreach f,$(filter %.zip,$^), && unzip $(f))

UNPACK_DIR = $(patsubst %.tar,%,$(basename $(notdir $<)))
APPLY = (cd $(UNPACK_DIR) && patch -p1) <
MOVE = mv $(UNPACK_DIR) $@ && touch $@

#
# package rules
#

# nasm

nasm-$(NASM_VERSION).tar.gz:
	$(call download_pkg,$(NASM_URL),nasm)

nasm: nasm-$(NASM_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.buildnasm: nasm
	cd $<; ./configure --prefix=$(PREFIX)
	+$(MAKE) -C $<
	+$(MAKE) -C $< install
	touch $@

CLEAN_FILE += .buildnasm
CLEAN_PKG += nasm
DISTCLEAN_PKG += nasm-$(NASM_VERSION).tar.gz

# cmake

cmake-$(CMAKE_VERSION).tar.gz:
	$(call download_pkg,$(CMAKE_URL),cmake)

cmake: cmake-$(CMAKE_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.buildcmake: cmake
	cd $<; ./configure --prefix=$(PREFIX) $(CMAKEFLAGS) --no-qt-gui -- -DCMAKE_USE_OPENSSL:BOOL=OFF -DBUILD_TESTING:BOOL=OFF
	+$(MAKE) -C $<
	+$(MAKE) -C $< install
	touch $@

CLEAN_FILE += .buildcmake
CLEAN_PKG += cmake
DISTCLEAN_PKG += cmake-$(CMAKE_VERSION).tar.gz

# help2man
help2man-$(HELP2MAN_VERSION).tar.xz:
	$(call download_pkg,$(HELP2MAN_URL),help2man)

help2man: help2man-$(HELP2MAN_VERSION).tar.xz .tar
	$(UNPACK)
	$(MOVE)

.buildhelp2man: help2man
	cd $<; ./configure --prefix=$(PREFIX)
	+$(MAKE) -C $<
	+$(MAKE) -C $< install
	touch $@

CLEAN_FILE += .buildhelp2man
CLEAN_PKG += help2man
DISTCLEAN_PKG += help2man-$(HELP2MAN_VERSION).tar.xz

# libtool

libtool-$(LIBTOOL_VERSION).tar.gz:
	$(call download_pkg,$(LIBTOOL_URL),libtool)

libtool: libtool-$(LIBTOOL_VERSION).tar.gz
	$(UNPACK)
	(cd $(UNPACK_DIR) && chmod u+w build-aux/ltmain.sh)
	$(APPLY) $(TOOLS)/libtool-2.4.7-bitcode.patch
	$(APPLY) $(TOOLS)/libtool-2.5.4-clang-libs.patch
	$(APPLY) $(TOOLS)/libtool-2.4.7-lpthread.patch
	$(APPLY) $(TOOLS)/libtool-2.5.4-embed-bitcode.patch
	$(MOVE)

.buildlibtool: libtool .automake .help2man
	(cd $(UNPACK_DIR) && autoreconf -fv)
	cd $<; ./configure --prefix=$(PREFIX)
	+$(MAKE) -C $<
	+$(MAKE) -C $< install
	ln -sf libtool $(PREFIX)/bin/glibtool
	ln -sf libtoolize $(PREFIX)/bin/glibtoolize
	touch $@

CLEAN_PKG += libtool
DISTCLEAN_PKG += libtool-$(LIBTOOL_VERSION).tar.gz
CLEAN_FILE += .buildlibtool

# GNU tar (with xz support)

tar-$(TAR_VERSION).tar.bz2:
	$(call download_pkg,$(TAR_URL),tar)

tar: tar-$(TAR_VERSION).tar.bz2
	$(UNPACK)
	$(MOVE)

.buildtar: .xz tar
	cd $<; ./configure --prefix=$(PREFIX)
	+$(MAKE) -C $<
	+$(MAKE) -C $< install
	touch $@

CLEAN_PKG += tar
DISTCLEAN_PKG += tar-$(TAR_VERSION).tar.bz2
CLEAN_FILE += .buildtar

# xz

xz-$(XZ_VERSION).tar.bz2:
	$(call download_pkg,$(XZ_URL),xz)

xz: xz-$(XZ_VERSION).tar.bz2
	$(UNPACK)
	$(MOVE)

.buildxz: xz
	cd $<; ./configure --prefix=$(PREFIX)
	+$(MAKE) -C $<
	+$(MAKE) -C $< install
	rm $(PREFIX)/lib/pkgconfig/liblzma.pc
	touch $@

CLEAN_PKG += xz
DISTCLEAN_PKG += xz-$(XZ_VERSION).tar.bz2
CLEAN_FILE += .buildxz

# config.guess

config.guess: $(TOOLS)/config.guess-$(CONFIGGUESS_VERSION)
	# CONFIGGUESS_URL=https://git.savannah.gnu.org/gitweb/?p=config.git;a=blob_plain;f=config.guess;hb=$(CONFIGGUESS_VERSION)
	cp -f $< $@

config.sub: $(TOOLS)/config.sub-$(CONFIGSUB_VERSION)
	# CONFIGSUB_URL=https://git.savannah.gnu.org/gitweb/?p=config.git;a=blob_plain;f=config.sub;hb=$(CONFIGSUB_VERSION)
	cp -f $< $@

.buildconfigguess: config.guess config.sub
	# install in a dummy autoconf so that VLC contribs pick it
	install -d           "$(PREFIX)/share/autoconf-vlc/build-aux"
	install config.guess "$(PREFIX)/share/autoconf-vlc/build-aux"
	install config.sub   "$(PREFIX)/share/autoconf-vlc/build-aux"
	touch $@

# autoconf

autoconf-$(AUTOCONF_VERSION).tar.gz:
	$(call download_pkg,$(AUTOCONF_URL),autoconf)

autoconf: autoconf-$(AUTOCONF_VERSION).tar.gz .configguess
	$(UNPACK)
	@-cp config.guess $(UNPACK_DIR)/build-aux
	@-cp config.sub $(UNPACK_DIR)/build-aux
	$(MOVE)

.buildautoconf: autoconf .pkg-config .m4
	cd $<; ./configure --prefix=$(PREFIX)
	+$(MAKE) -C $<
	+$(MAKE) -C $< install
	touch $@

CLEAN_FILE += .buildautoconf
CLEAN_PKG += autoconf
DISTCLEAN_PKG += autoconf-$(AUTOCONF_VERSION).tar.gz

# automake

automake-$(AUTOMAKE_VERSION).tar.gz:
	$(call download_pkg,$(AUTOMAKE_URL),automake)

automake: automake-$(AUTOMAKE_VERSION).tar.gz .configguess
	$(UNPACK)
	$(APPLY) $(TOOLS)/automake-disable-documentation.patch
	$(APPLY) $(TOOLS)/automake-clang.patch
	@-cp config.guess $(UNPACK_DIR)/lib
	@-cp config.sub $(UNPACK_DIR)/lib
	$(MOVE)

.buildautomake: automake .autoconf
	(cd $<; ./bootstrap)
	cd $<; ./configure --prefix=$(PREFIX)
	+$(MAKE) -C $<
	+$(MAKE) -C $< install
	touch $@

CLEAN_FILE += .buildautomake
CLEAN_PKG += automake
DISTCLEAN_PKG += automake-$(AUTOMAKE_VERSION).tar.gz

# m4

m4-$(M4_VERSION).tar.gz:
	$(call download_pkg,$(M4_URL),m4)

m4: m4-$(M4_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.buildm4: m4
	cd $<; ./configure --prefix=$(PREFIX)
	+$(MAKE) -C $<
	+$(MAKE) -C $< install
	touch $@

CLEAN_FILE += .buildm4
CLEAN_PKG += m4
DISTCLEAN_PKG += m4-$(M4_VERSION).tar.gz

# pkg-config

pkg-config-$(PKGCFG_VERSION).tar.gz:
	$(call download_pkg,$(PKGCFG_URL),pkgconfiglite)

pkgconfig: pkg-config-$(PKGCFG_VERSION).tar.gz
	$(UNPACK)
	mv pkg-config-lite-$(PKGCFG_VERSION) pkg-config-$(PKGCFG_VERSION)
	$(APPLY) $(TOOLS)/pkg-config-stdc23-port.patch
	$(MOVE)

.buildpkg-config: pkgconfig
	cd $<; ./configure --prefix=$(PREFIX) --disable-shared --enable-static --disable-dependency-tracking
	+$(MAKE) -C $<
	+$(MAKE) -C $< install
	touch $@

CLEAN_FILE += .buildpkg-config
CLEAN_PKG += pkgconfig
DISTCLEAN_PKG += pkg-config-$(PKGCFG_VERSION).tar.gz

# GNU sed

sed-$(SED_VERSION).tar.bz2:
	$(call download_pkg,$(SED_URL),sed)

sed: sed-$(SED_VERSION).tar.bz2
	$(UNPACK)
	$(MOVE)

.buildsed: sed
	cd $<; ./configure --prefix=$(PREFIX)
	+$(MAKE) -C $<
	+$(MAKE) -C $< install
	touch $@

CLEAN_PKG += sed
DISTCLEAN_PKG += sed-$(SED_VERSION).tar.bz2
CLEAN_FILE += .buildsed

# Apache ANT

apache-ant-$(ANT_VERSION).tar.bz2:
	$(call download_pkg,$(ANT_URL),ant)

ant: apache-ant-$(ANT_VERSION).tar.bz2
	$(UNPACK)
	$(MOVE)

.buildant: ant
	(mkdir -p $(PREFIX)/bin && cp $</bin/* $(PREFIX)/bin/)
	(mkdir -p $(PREFIX)/lib && cp $</lib/* $(PREFIX)/lib/)
	touch $@

CLEAN_PKG += ant
DISTCLEAN_PKG += apache-ant-$(ANT_VERSION).tar.bz2
CLEAN_FILE += .buildant


#
# GNU bison
#

bison-$(BISON_VERSION).tar.xz:
	$(call download_pkg,$(BISON_URL),bison)

bison: bison-$(BISON_VERSION).tar.xz .tar
	$(UNPACK)
	$(MOVE)

.buildbison: bison
	cd $<; ./configure --prefix=$(PREFIX)
	+$(MAKE) -C $<
	+$(MAKE) -C $< install
	touch $@

CLEAN_PKG += bison
DISTCLEAN_PKG += bison-$(BISON_VERSION).tar.xz
CLEAN_FILE += .buildbison

#
# GNU flex
#

flex-$(FLEX_VERSION).tar.gz:
	$(call download_pkg,$(FLEX_URL),flex)

flex: flex-$(FLEX_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.buildflex: flex
	cd $<; ./configure --prefix=$(PREFIX)
	+$(MAKE) -C $<
	+$(MAKE) -C $< install
	touch $@

CLEAN_PKG += flex
DISTCLEAN_PKG += flex-$(FLEX_VERSION).tar.gz
CLEAN_FILE += .buildflex



#
# GNU gettext
#

GETTEXT_CONF = \
	--disable-relocatable \
	--disable-java \
	--disable-native-java \
	--disable-csharp \
	--disable-d \
	--disable-go \
	--disable-modula2 \
	--disable-openmp \
	--without-emacs \
	--without-included-libxml \
	--without-git \
	--without-cvs

gettext-$(GETTEXT_VERSION).tar.gz:
	$(call download_pkg,$(GETTEXT_URL),gettext)

gettext: gettext-$(GETTEXT_VERSION).tar.gz
	$(UNPACK)
	$(APPLY) $(TOOLS)/gettext-no-iconv.patch
	$(MOVE)

.buildgettext: gettext
	cd $<; ./configure --prefix=$(PREFIX) $(GETTEXT_CONF)
	+$(MAKE) -C $< EXAMPLESFILES= EXAMPLESDIRS= TESTS=
	+$(MAKE) -C $< EXAMPLESFILES= EXAMPLESDIRS= TESTS= install
	touch $@

CLEAN_PKG += gettext
DISTCLEAN_PKG += gettext-$(GETTEXT_VERSION).tar.gz
CLEAN_FILE += .buildgettext

#
# meson build
#

meson-$(MESON_VERSION).tar.gz:
	$(call download_pkg,$(MESON_URL),meson)

meson: meson-$(MESON_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.buildmeson: meson
	mkdir -p $(PREFIX)/bin
	printf "#!/bin/sh\n\npython3 $(abspath .)/meson/meson.py \"\$$@\"\n" > $(PREFIX)/bin/meson
	chmod +x $(PREFIX)/bin/meson
	touch $@

CLEAN_PKG += meson
DISTCLEAN_PKG += meson-$(MESON_VERSION).tar.gz
CLEAN_FILE += .buildmeson

#
# ninja build
#

ninja-$(NINJA_VERSION).tar.gz:
	$(call download_pkg,$(NINJA_URL),ninja)

ninja: UNPACK_DIR=ninja-$(NINJA_BUILD_NAME)
ninja: ninja-$(NINJA_VERSION).tar.gz
	$(UNPACK)
	$(APPLY) $(TOOLS)/ninja-1.11.1-replace-pipes-quote-with-shlex-quote.patch
	$(APPLY) $(TOOLS)/0001-CanonicalizePath-Remove-kMaxComponents-limit.patch
	$(APPLY) $(TOOLS)/0002-CanonicalizePath-fix-a-b-._foo-a-replacement.patch
	$(MOVE)

.buildninja: ninja
	(cd $<; python3 ./configure.py --bootstrap && mv ninja $(PREFIX)/bin/)
	touch $@

CLEAN_PKG += ninja
DISTCLEAN_PKG += ninja-$(NINJA_VERSION).tar.gz
CLEAN_FILE += .buildninja

# gperf

gperf-$(GPERF_VERSION).tar.gz:
	$(call download_pkg,$(GPERF_URL),gperf)

gperf: gperf-$(GPERF_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.buildgperf: gperf
	cd $<; ./configure --prefix=$(PREFIX)
	+$(MAKE) -C $<
	+$(MAKE) -C $< install
	touch $@

CLEAN_PKG += gperf
DISTCLEAN_PKG += gperf-$(GPERF_VERSION).tar.gz
CLEAN_FILE += .buildgperf

#
#
#

fetch-all: $(DISTCLEAN_PKG)

clean:
	rm -fr $(CLEAN_FILE) $(CLEAN_PKG) build/

distclean: clean
	rm -fr $(DISTCLEAN_PKG)

.PHONY: all clean distclean

.DELETE_ON_ERROR:
