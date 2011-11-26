# Main makefile for VLC 3rd party libraries ("contrib")
# Copyright (C) 2003-2011 the VideoLAN team
#
# This file is under the same license as the vlc package.

all: install

# bootstrap configuration
include config.mak

TOPSRC ?= ../../contrib
TOPDST ?= ..
SRC := $(TOPSRC)/src
TARBALLS := $(TOPSRC)/tarballs

PATH :=$(abspath ../../extras/tools/build/bin):$(PATH)
export PATH

PKGS_ALL := $(patsubst $(SRC)/%/rules.mak,%,$(wildcard $(SRC)/*/rules.mak))
DATE := $(shell date +%Y%m%d)
VPATH := $(TARBALLS)

# Common download locations
GNU := http://ftp.gnu.org/gnu
SF := http://heanet.dl.sourceforge.net/sourceforge
VIDEOLAN := http://downloads.videolan.org/pub/videolan
CONTRIB_VIDEOLAN := $(VIDEOLAN)/testing/contrib

#
# Machine-dependent variables
#
PREFIX ?= $(TOPDST)/$(HOST)
PREFIX := $(abspath $(PREFIX))
ifneq ($(HOST),$(BUILD))
HAVE_CROSS_COMPILE = 1
endif
ARCH := $(shell $(SRC)/get-arch.sh $(HOST))
ifneq ($(findstring $(ARCH),i386 sparc sparc64 ppc ppc64 x86_64),)
# This should be consistent with include/vlc_cpu.h
HAVE_FPU = 1
endif
ifeq ($(ARCH)-$(HAVE_WIN32),x86_64-1)
HAVE_WIN64 := 1
endif

ifdef HAVE_CROSS_COMPILE
need_pkg = 1
else
need_pkg = $(shell $(PKG_CONFIG) $(1) || echo 1)
endif

#
# Default values for tools
#
ifndef HAVE_CROSS_COMPILE
ifneq ($(findstring $(origin CC),undefined default),)
CC := gcc
endif
ifneq ($(findstring $(origin CXX),undefined default),)
CXX := g++
endif
ifneq ($(findstring $(origin LD),undefined default),)
LD := ld
endif
ifneq ($(findstring $(origin AR),undefined default),)
AR := ar
endif
ifneq ($(findstring $(origin RANLIB),undefined default),)
RANLIB := ranlib
endif
ifneq ($(findstring $(origin STRIP),undefined default),)
STRIP := strip
endif
else
ifneq ($(findstring $(origin CC),undefined default),)
CC := $(HOST)-gcc
endif
ifneq ($(findstring $(origin CXX),undefined default),)
CXX := $(HOST)-g++
endif
ifneq ($(findstring $(origin LD),undefined default),)
LD := $(HOST)-ld
endif
ifneq ($(findstring $(origin AR),undefined default),)
AR := $(HOST)-ar
endif
ifneq ($(findstring $(origin RANLIB),undefined default),)
RANLIB := $(HOST)-ranlib
endif
ifneq ($(findstring $(origin STRIP),undefined default),)
STRIP := $(HOST)-strip
endif
endif

ifdef HAVE_MACOSX
MACOSX_SDK=/Developer/SDKs/MacOSX$(OSX_VERSION).sdk
CC=gcc-4.2
CXX=g++-4.2
AR=ar
LD=ld
STRIP=strip
RANLIB=ranlib
EXTRA_CFLAGS += -isysroot $(MACOSX_SDK) -mmacosx-version-min=$(OSX_VERSION)
EXTRA_LDFLAGS += -Wl,-syslibroot,$(MACOSX_SDK) -mmacosx-version-min=$(OSX_VERSION) -isysroot $(MACOSX_SDK)
#TODO ppc
ifeq ($(ARCH),x86_64)
EXTRA_CFLAGS += -m64
EXTRA_LDFLAGS += -m64
else
EXTRA_CFLAGS += -m32
EXTRA_LDFLAGS += -m32
endif

XCODE_FLAGS = -sdk macosx$(OSX_VERSION)
ifeq ($(xcodebuild -version 2>/dev/null | tee /dev/null|head -1|cut -d\  -f2|cut -d. -f1), 3)
# XCode 3 doesn't support -arch
else
XCODE_FLAGS += -arch $(ARCH)
endif

endif


EXTRA_CFLAGS += -I$(PREFIX)/include
CPPFLAGS := $(CPPFLAGS) $(EXTRA_CFLAGS)
CFLAGS := $(CFLAGS) $(EXTRA_CFLAGS)
CXXFLAGS := $(CXXFLAGS) $(EXTRA_CFLAGS)
EXTRA_LDFLAGS += -L$(PREFIX)/lib
LDFLAGS := $(LDFLAGS) $(EXTRA_LDFLAGS)
# Do not export those! Use HOSTVARS.

ACLOCAL_AMFLAGS += -I$(PREFIX)/share/aclocal
export ACLOCAL_AMFLAGS

PKG_CONFIG ?= pkg-config
ifdef HAVE_CROSS_COMPILE
# This inhibits .pc file from within the cross-compilation toolchain sysroot.
# Hopefully, nobody ever needs that.
PKG_CONFIG_PATH := /usr/share/pkgconfig
PKG_CONFIG_LIBDIR := /usr/$(HOST)/lib/pkgconfig
export PKG_CONFIG_LIBDIR
endif
PKG_CONFIG_PATH := $(PKG_CONFIG_PATH):$(PREFIX)/lib/pkgconfig
export PKG_CONFIG_PATH

ifndef GIT
ifeq ($(shell git --version >/dev/null 2>&1 || echo FAIL),)
GIT = git
endif
endif
GIT ?= $(error git not found!)

ifndef AUTOPOINT
AUTOPOINT := "$(PREFIX)/bin/autopoint"
export AUTOPOINT
endif

ifndef SVN
ifeq ($(shell svn --version >/dev/null 2>&1 || echo FAIL),)
SVN = svn
endif
endif
SVN ?= $(error subversion client (svn) not found!)

ifeq ($(shell curl --version >/dev/null 2>&1 || echo FAIL),)
download = curl -f -L -- "$(1)" > "$@"
else ifeq ($(shell wget --version >/dev/null 2>&1 || echo FAIL),)
download = rm -f $@.tmp && \
	wget --passive -c -p -O $@.tmp "$(1)" && \
	touch $@.tmp && \
	mv $@.tmp $@
else
download = $(error Neither curl nor wget found!)
endif

ifeq ($(shell gzcat --version >/dev/null 2>&1 || echo FAIL),)
ZCAT = gzcat
else ifeq ($(shell zcat --version >/dev/null 2>&1 || echo FAIL),)
ZCAT = zcat
else
ZCAT ?= $(error Gunzip client (zcat) not found!)
endif

ifeq ($(shell sha512sum --version >/dev/null 2>&1 || echo FAIL),)
SHA512SUM = sha512sum
else ifeq ($(shell shasum --version >/dev/null 2>&1 || echo FAIL),)
SHA512SUM = shasum -a 512
else
SHA512SUM = $(error SHA-512 checksumming not found!)
endif

#
# Common helpers
#
HOSTCONF := --prefix="$(PREFIX)"
HOSTCONF += --build="$(BUILD)" --host="$(HOST)" --target="$(HOST)"
HOSTCONF += --program-prefix=""
# libtool stuff:
HOSTCONF += --enable-static --disable-shared --disable-dependency-tracking
ifdef HAVE_WIN32
HOSTCONF += --without-pic
PIC :=
else
HOSTCONF += --with-pic
PIC := -fPIC
endif

HOSTTOOLS := \
	CC="$(CC)" CXX="$(CXX)" LD="$(LD)" \
	AR="$(AR)" RANLIB="$(RANLIB)" STRIP="$(STRIP)" \
	PATH="$(PREFIX)/bin:$(PATH)"
HOSTVARS := $(HOSTTOOLS) \
	CPPFLAGS="$(CPPFLAGS)" \
	CFLAGS="$(CFLAGS)" \
	CXXFLAGS="$(CXXFLAGS)" \
	LDFLAGS="$(LDFLAGS)"
HOSTVARS_PIC := $(HOSTTOOLS) \
	CPPFLAGS="$(CPPFLAGS) $(PIC)" \
	CFLAGS="$(CFLAGS) $(PIC)" \
	CXXFLAGS="$(CXXFLAGS) $(PIC)" \
	LDFLAGS="$(LDFLAGS)"

download_git = \
	rm -Rf $(@:.tar.xz=) && \
	$(GIT) clone $(2:%=--branch %) $(1) $(@:.tar.xz=) && \
	(cd $(@:.tar.xz=) && $(GIT) checkout $(3:%= %)) && \
	rm -Rf $(@:%.tar.xz=%)/.git && \
	(cd $(dir $@) && \
	 tar cvJ $(notdir $(@:.tar.xz=))) > $@ && \
	rm -Rf $(@:.tar.xz=)
checksum = \
	$(foreach f,$(filter $(TARBALLS)/%,$^), \
		grep -- " $(f:$(TARBALLS)/%=%)$$" \
			"$(SRC)/$(patsubst .sum-%,%,$@)/$(2)SUMS" &&) \
	(cd $(TARBALLS) && $(1) --check /dev/stdin) < \
		"$(SRC)/$(patsubst .sum-%,%,$@)/$(2)SUMS"
CHECK_SHA512 = $(call checksum,$(SHA512SUM),SHA512)
UNPACK = $(RM) -R $@ \
	$(foreach f,$(filter %.tar.gz %.tgz,$^), && tar xvzf $(f)) \
	$(foreach f,$(filter %.tar.bz2,$^), && tar xvjf $(f)) \
	$(foreach f,$(filter %.tar.xz,$^), && tar xvJf $(f)) \
	$(foreach f,$(filter %.zip,$^), && unzip $(f))
UNPACK_DIR = $(basename $(basename $(notdir $<)))
APPLY = (cd $(UNPACK_DIR) && patch -p1) <
MOVE = mv $(UNPACK_DIR) $@ && touch $@

RECONF = mkdir -p -- $(PREFIX)/share/aclocal && \
	cd $< && autoreconf -fiv $(ACLOCAL_AMFLAGS)
CMAKE = cmake . -DCMAKE_TOOLCHAIN_FILE=$(abspath toolchain.cmake) \
		-DCMAKE_INSTALL_PREFIX=$(PREFIX)

#
# Per-package build rules
#
PKGS_FOUND :=
include $(SRC)/*/rules.mak

#
# Targets
#
ifneq ($(filter $(PKGS_DISABLE),$(PKGS_ENABLE)),)
$(error Same package(s) disabled and enabled at the same time)
endif
# Apply automatic selection (= remove distro packages):
PKGS_AUTOMATIC := $(filter-out $(PKGS_FOUND),$(PKGS))
# Apply manual selection (from bootstrap):
PKGS_MANUAL := $(sort $(PKGS_ENABLE) $(filter-out $(PKGS_DISABLE),$(PKGS_AUTOMATIC)))
# Resolve dependencies:
PKGS_DEPS := $(filter-out $(PKGS_FOUND) $(PKGS_MANUAL),$(sort $(foreach p,$(PKGS_MANUAL),$(DEPS_$(p)))))
PKGS := $(sort $(PKGS_MANUAL) $(PKGS_DEPS))

fetch: $(PKGS:%=.sum-%)
fetch-all: $(PKGS_ALL:%=.sum-%)
install: $(PKGS:%=.%)

mostlyclean:
	-$(RM) $(foreach p,$(PKGS_ALL),.$(p) .sum-$(p) .dep-$(p))
	-$(RM) toolchain.cmake
	-$(RM) -R "$(PREFIX)"
	-$(RM) -R */

clean: mostlyclean
	-$(RM) $(TARBALLS)/*.*

distclean: clean
	$(RM) config.mak
	unlink Makefile

PREBUILT_URL=ftp://ftp.videolan.org/pub/videolan/contrib/$(HOST)/vlc-contrib-$(HOST)-latest.tar.bz2

vlc-contrib-$(HOST)-latest.tar.bz2:
	$(call download,$(PREBUILT_URL))

prebuilt: vlc-contrib-$(HOST)-latest.tar.bz2
	$(UNPACK) && mv $(HOST) $(TOPDST)
	cd $(TOPDST)/$(HOST) && ./change_prefix.sh

package: install
	rm -Rf tmp/
	mkdir -p tmp/
	cp -r $(PREFIX) tmp/
	# remove useless files
	cd tmp/$(notdir $(PREFIX)); \
		cd share; rm -Rf man doc gtk-doc info lua projectM gettext; cd ..; \
		rm -Rf man sbin etc lib/lua lib/sidplay
	cp $(SRC)/change_prefix.sh tmp/$(notdir $(PREFIX))/
	cd tmp/$(notdir $(PREFIX)) && ./change_prefix.sh $(PREFIX) @@CONTRIB_PREFIX@@
	(cd tmp && tar c $(notdir $(PREFIX))/) | bzip2 -c > ../vlc-contrib-$(HOST)-$(DATE).tar.bz2

list:
	@echo All packages:
	@echo '  $(PKGS_ALL)' | fmt
	@echo Distribution-provided packages:
	@echo '  $(PKGS_FOUND)' | fmt
	@echo Automatically selected packages:
	@echo '  $(PKGS_AUTOMATIC)' | fmt
	@echo Manually deselected packages:
	@echo '  $(PKGS_DISABLE)' | fmt
	@echo Manually selected packages:
	@echo '  $(PKGS_ENABLE)' | fmt
	@echo Depended-on packages:
	@echo '  $(PKGS_DEPS)' | fmt
	@echo To-be-built packages:
	@echo '  $(PKGS)' | fmt

.PHONY: all fetch fetch-all install mostlyclean clean distclean package list prebuilt

# CMake toolchain
toolchain.cmake:
	$(RM) $@
ifdef HAVE_WIN32
	echo "set(CMAKE_SYSTEM_NAME Windows)" >> $@
	echo "set(CMAKE_RC_COMPILER $(HOST)-windres)" >> $@
endif
ifdef HAVE_MACOSX
	echo "set(CMAKE_SYSTEM_NAME Darwin)" >> $@
	echo "set(CMAKE_C_FLAGS $(CFLAGS))" >> $@
	echo "set(CMAKE_CXX_FLAGS $(CFLAGS))" >> $@
	echo "set(CMAKE_LD_FLAGS $(LDFLAGS))" >> $@
endif
	echo "set(CMAKE_C_COMPILER $(CC))" >> $@
	echo "set(CMAKE_CXX_COMPILER $(CXX))" >> $@
	echo "set(CMAKE_FIND_ROOT_PATH $(PREFIX))" >> $@
	echo "set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)" >> $@
	echo "set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)" >> $@
	echo "set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)" >> $@

# Default pattern rules
.sum-%: $(SRC)/%/SHA512SUMS
	$(CHECK_SHA512)
	touch $@

.sum-%:
	$(error Download and check target not defined for $*)

# Dummy dependency on found packages
$(patsubst %,.dep-%,$(PKGS_FOUND)): .dep-%:
	touch $@

# Real dependency on missing packages
$(patsubst %,.dep-%,$(filter-out $(PKGS_FOUND),$(PKGS_ALL))): .dep-%: .%
	touch -r $< $@

.SECONDEXPANSION:

# Dependency propagation (convert 'DEPS_foo = bar' to '.foo: .bar')
$(foreach p,$(PKGS_ALL),.$(p)): .%: $$(foreach d,$$(DEPS_$$*),.dep-$$(d))

.DELETE_ON_ERROR:
