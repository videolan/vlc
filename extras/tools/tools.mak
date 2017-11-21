# Copyright (C) 2003-2011 the VideoLAN team
#
# This file is under the same license as the vlc package.

include packages.mak

#
# common rules
#

AUTOCONF=$(PREFIX)/bin/autoconf
export AUTOCONF

ifeq ($(shell curl --version >/dev/null 2>&1 || echo FAIL),)
download = curl -f -L -- "$(1)" > "$@.tmp" && touch $@.tmp && mv $@.tmp $@
else ifeq ($(shell wget --version >/dev/null 2>&1 || echo FAIL),)
download = rm -f $@.tmp && \
	wget --passive -c -p -O $@.tmp "$(1)" && \
	touch $@.tmp && \
	mv $@.tmp $@
else ifeq ($(which fetch >/dev/null 2>&1 || echo FAIL),)
download = rm -f $@.tmp && \
	fetch -p -o $@.tmp "$(1)" && \
	touch $@.tmp && \
	mv $@.tmp $@
else
download = $(error Neither curl nor wget found!)
endif

download_pkg = $(call download,$(VIDEOLAN)/$(2)/$(lastword $(subst /, ,$(@)))) || \
	( $(call download,$(1)) && echo "Please upload package $(lastword $(subst /, ,$(@))) to our FTP" )  \
	&& grep $(@) SHA512SUMS| shasum -a 512 -c

UNPACK = $(RM) -R $@ \
    $(foreach f,$(filter %.tar.gz %.tgz,$^), && tar xvzf $(f)) \
    $(foreach f,$(filter %.tar.bz2,$^), && tar xvjf $(f)) \
    $(foreach f,$(filter %.tar.xz,$^), && tar xvJf $(f)) \
    $(foreach f,$(filter %.zip,$^), && unzip $(f))

UNPACK_DIR = $(patsubst %.tar,%,$(basename $(notdir $<)))
APPLY = (cd $(UNPACK_DIR) && patch -p1) <
MOVE = mv $(UNPACK_DIR) $@ && touch $@

#
# package rules
#

# yasm

yasm-$(YASM_VERSION).tar.gz:
	$(call download_pkg,$(YASM_URL),yasm)

yasm: yasm-$(YASM_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.yasm: yasm
	(cd $<; ./configure --prefix=$(PREFIX) && $(MAKE) && $(MAKE) install)
	touch $@

CLEAN_FILE += .yasm
CLEAN_PKG += yasm
DISTCLEAN_PKG += yasm-$(YASM_VERSION).tar.gz

# cmake

cmake-$(CMAKE_VERSION).tar.gz:
	$(call download_pkg,$(CMAKE_URL),cmake)

cmake: cmake-$(CMAKE_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.cmake: cmake
	(cd $<; ./configure --prefix=$(PREFIX) $(CMAKEFLAGS) && $(MAKE) && $(MAKE) install)
	touch $@

CLEAN_FILE += .cmake
CLEAN_PKG += cmake
DISTCLEAN_PKG += cmake-$(CMAKE_VERSION).tar.gz

# libtool

libtool-$(LIBTOOL_VERSION).tar.gz:
	$(call download_pkg,$(LIBTOOL_URL),libtool)

libtool: libtool-$(LIBTOOL_VERSION).tar.gz
	$(UNPACK)
	$(APPLY) libtool-2.4.2-bitcode.patch
	$(APPLY) libtool-2.4.2-san.patch
	$(MOVE)

.libtool: libtool .automake
	(cd $<; ./configure --prefix=$(PREFIX) && $(MAKE) && $(MAKE) install)
	ln -sf libtool $(PREFIX)/bin/glibtool
	ln -sf libtoolize $(PREFIX)/bin/glibtoolize
	touch $@

CLEAN_PKG += libtool
DISTCLEAN_PKG += libtool-$(LIBTOOL_VERSION).tar.gz
CLEAN_FILE += .libtool

# GNU tar (with xz support)

tar-$(TAR_VERSION).tar.bz2:
	$(call download_pkg,$(TAR_URL),tar)

tar: tar-$(TAR_VERSION).tar.bz2
	$(UNPACK)
	$(MOVE)

.tar: tar
	(cd $<; ./configure --prefix=$(PREFIX) && $(MAKE) && $(MAKE) install)
	touch $@

CLEAN_PKG += tar
DISTCLEAN_PKG += tar-$(TAR_VERSION).tar.bz2
CLEAN_FILE += .tar

# xz

xz-$(XZ_VERSION).tar.bz2:
	$(call download_pkg,$(XZ_URL),xz)

xz: xz-$(XZ_VERSION).tar.bz2
	$(UNPACK)
	$(MOVE)

.xz: xz
	(cd $<; ./configure --prefix=$(PREFIX) && $(MAKE) && $(MAKE) install && rm $(PREFIX)/lib/pkgconfig/liblzma.pc)
	touch $@

CLEAN_PKG += xz
DISTCLEAN_PKG += xz-$(XZ_VERSION).tar.bz2
CLEAN_FILE += .xz

# autoconf

autoconf-$(AUTOCONF_VERSION).tar.gz:
	$(call download_pkg,$(AUTOCONF_URL),autoconf)

autoconf: autoconf-$(AUTOCONF_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.autoconf: autoconf .pkg-config
	(cd $<; ./configure --prefix=$(PREFIX) && $(MAKE) && $(MAKE) install)
	touch $@

CLEAN_FILE += .autoconf
CLEAN_PKG += autoconf
DISTCLEAN_PKG += autoconf-$(AUTOCONF_VERSION).tar.gz

# automake

automake-$(AUTOMAKE_VERSION).tar.gz:
	$(call download_pkg,$(AUTOMAKE_URL),automake)

automake: automake-$(AUTOMAKE_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.automake: automake .autoconf
	(cd $<; ./configure --prefix=$(PREFIX) && $(MAKE) && $(MAKE) install)
	touch $@

CLEAN_FILE += .automake
CLEAN_PKG += automake
DISTCLEAN_PKG += automake-$(AUTOMAKE_VERSION).tar.gz

# m4

m4-$(M4_VERSION).tar.gz:
	$(call download_pkg,$(M4_URL),m4)

m4: m4-$(M4_VERSION).tar.gz
	$(UNPACK)
	$(APPLY) bison-macOS-c41f233c.patch
	$(APPLY) bison-macOS-7df04f9.patch
	$(MOVE)

.m4: m4
	(cd $<; ./configure --prefix=$(PREFIX) && $(MAKE) && $(MAKE) install)
	touch $@

CLEAN_FILE += .m4
CLEAN_PKG += m4
DISTCLEAN_PKG += m4-$(M4_VERSION).tar.gz

# pkg-config

pkg-config-$(PKGCFG_VERSION).tar.gz:
	$(call download_pkg,$(PKGCFG_URL),pkgconfiglite)

pkgconfig: pkg-config-$(PKGCFG_VERSION).tar.gz
	$(UNPACK)
	mv pkg-config-lite-$(PKGCFG_VERSION) pkg-config-$(PKGCFG_VERSION)
	$(MOVE)

.pkg-config: pkgconfig
	(cd pkgconfig; ./configure --prefix=$(PREFIX) --disable-shared --enable-static && $(MAKE) && $(MAKE) install)
	touch $@

CLEAN_FILE += .pkg-config
CLEAN_PKG += pkgconfig
DISTCLEAN_PKG += pkg-config-$(PKGCFG_VERSION).tar.gz

# gas-preprocessor
gas-preprocessor-$(GAS_VERSION).tar.gz:
	$(call download_pkg,$(GAS_URL),gas-preprocessor)

gas: gas-preprocessor-$(GAS_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.gas: gas
	mkdir -p $(PREFIX)/bin
	cp gas/gas-preprocessor.pl $(PREFIX)/bin/
	touch $@

CLEAN_FILE += .gas
CLEAN_PKG += gas
DISTCLEAN_PKG += gas-preprocessor-$(GAS_VERSION).tar.gz

# Ragel State Machine Compiler
ragel-$(RAGEL_VERSION).tar.gz:
	$(call download_pkg,$(RAGEL_URL),ragel)

ragel: ragel-$(RAGEL_VERSION).tar.gz
	$(UNPACK)
	$(APPLY) ragel-6.8-javacodegen.patch
	$(MOVE)


.ragel: ragel
	(cd ragel; ./configure --prefix=$(PREFIX) --disable-shared --enable-static && $(MAKE) && $(MAKE) install)
	touch $@

CLEAN_FILE += .ragel
CLEAN_PKG += ragel
DISTCLEAN_PKG += ragel-$(RAGEL_VERSION).tar.gz

# GNU sed

sed-$(SED_VERSION).tar.bz2:
	$(call download_pkg,$(SED_URL),sed)

sed: sed-$(SED_VERSION).tar.bz2
	$(UNPACK)
	$(MOVE)

.sed: sed
	(cd $<; ./configure --prefix=$(PREFIX) && $(MAKE) && $(MAKE) install)
	touch $@

CLEAN_PKG += sed
DISTCLEAN_PKG += sed-$(SED_VERSION).tar.bz2
CLEAN_FILE += .sed

# Apache ANT

apache-ant-$(ANT_VERSION).tar.bz2:
	$(call download_pkg,$(ANT_URL),ant)

ant: apache-ant-$(ANT_VERSION).tar.bz2
	$(UNPACK)
	$(MOVE)

.ant: ant
	(mkdir -p $(PREFIX)/bin && cp $</bin/* $(PREFIX)/bin/)
	(mkdir -p $(PREFIX)/lib && cp $</lib/* $(PREFIX)/lib/)
	touch $@

CLEAN_PKG += ant
DISTCLEAN_PKG += apache-ant-$(ANT_VERSION).tar.bz2
CLEAN_FILE += .ant


# Protobuf Protoc

protobuf-$(PROTOBUF_VERSION).tar.gz:
	$(call download_pkg,$(PROTOBUF_URL),protobuf)

protobuf: protobuf-$(PROTOBUF_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.protoc: protobuf
	(cd $< && ./configure --prefix="$(PREFIX)" --disable-shared --enable-static && $(MAKE) && $(MAKE) install)
	(find $(PREFIX) -name 'protobuf*.pc' -exec rm -f {} \;)
	touch $@

CLEAN_PKG += protobuf
DISTCLEAN_PKG += protobuf-$(PROTOBUF_VERSION).tar.gz
CLEAN_FILE += .protoc

#
# GNU bison
#

bison-$(BISON_VERSION).tar.xz:
	$(call download_pkg,$(BISON_URL),bison)

bison: bison-$(BISON_VERSION).tar.xz
	$(UNPACK)
	$(APPLY) bison-macOS-c41f233c.patch
	$(APPLY) bison-macOS-7df04f9.patch
	$(MOVE)

.bison: bison
	(cd $<; ./configure --prefix=$(PREFIX) && $(MAKE) && $(MAKE) install)
	touch $@

CLEAN_PKG += bison
DISTCLEAN_PKG += bison-$(BISON_VERSION).tar.xz
CLEAN_FILE += .bison

#
# GNU flex
#

flex-$(FLEX_VERSION).tar.gz:
	$(call download_pkg,$(FLEX_URL),flex)

flex: flex-$(FLEX_VERSION).tar.gz
	$(UNPACK)
	$(MOVE)

.flex: flex
	(cd $<; ./configure --prefix=$(PREFIX) && $(MAKE) && $(MAKE) install)
	touch $@

CLEAN_PKG += flex
DISTCLEAN_PKG += flex-$(FLEX_VERSION).tar.gz
CLEAN_FILE += .flex


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
