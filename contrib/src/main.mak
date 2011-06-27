# Main makefile for VLC 3rd party libraries ("contrib")
# Copyright (C) 2003-2011 the VideoLAN team
#
# This file is under the same license as the vlc package.

all: install

ALL_PKGS := $(patsubst ../src/%/rules.mak,%,$(wildcard ../src/*/rules.mak))
SRC := ../src
TARBALLS := ../tarballs
DATE := $(shell date +%Y%m%d)

# Common download locations
GNU := http://ftp.gnu.org/gnu
SF := http://heanet.dl.sourceforge.net/sourceforge
VIDEOLAN := http://downloads.videolan.org/pub/videolan
CONTRIB_VIDEOLAN := $(VIDEOLAN)/testing/contrib

# bootstrap configuration
include config.mak

#
# Machine-dependent variables
#
PREFIX ?= ../hosts/$(HOST)
PREFIX := $(abspath $(PREFIX))
ifneq ($(HOST),$(BUILD))
HAVE_CROSS_COMPILE = 1
endif
ARCH := $(shell $(SRC)/get-arch.sh $(HOST))
ifneq ($(findstring $(ARCH),i386 sparc sparc64 ppc ppc64 x86_64),)
# This should be consistent with include/vlc_cpu.h
HAVE_FPU = 1
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
PKG_CONFIG_PATH :=
PKG_CONFIG_LIBDIR := /dev/null
export PKG_CONFIG_LIBDIR
endif
PKG_CONFIG_PATH += :$(PREFIX)/lib/pkgconfig
export PKG_CONFIG_PATH

ifndef GIT
ifeq ($(shell git --version >/dev/null 2>&1 || echo FAIL),)
GIT = git
endif
endif
GIT ?= $(error git not found!)

ifndef SVN
ifeq ($(shell svn --version >/dev/null 2>&1 || echo FAIL),)
SVN = svn
endif
endif
SVN ?= $(error subversion client (svn) not found!)

ifndef WGET
ifeq ($(shell wget --version >/dev/null 2>&1 || echo FAIL),)
WGET = wget --passive -c
endif
endif
ifndef WGET
ifeq ($(shell curl --version >/dev/null 2>&1 || echo FAIL),)
WGET = curl -L -O
endif
endif
WGET ?= $(error Neither wget not curl found!)

#
# Common helpers
#
HOSTVARS := CPPFLAGS="$(CPPFLAGS)"
HOSTVARS += CC="$(CC)"
HOSTVARS += CFLAGS="$(CFLAGS)"
HOSTVARS += CXX="$(CXX)"
HOSTVARS += CXXFLAGS="$(CXXFLAGS)"
HOSTVARS += LD="$(LD)"
HOSTVARS += LDFLAGS="$(LDFLAGS)"
HOSTVARS += AR="$(AR)"
HOSTVARS += RANLIB="$(RANLIB)"
HOSTVARS += STRIP="$(STRIP)"
HOSTVARS_AR += AR="$(AR) rcvu"

HOSTCONF := --prefix="$(PREFIX)"
HOSTCONF += --build="$(BUILD)" --host="$(HOST)" --target="$(HOST)"
HOSTCONF += --program-prefix=""
# libtool stuff:
HOSTCONF += --enable-static --disable-shared --disable-dependency-tracking
ifdef HAVE_WIN32
HOSTCONF += --without-pic
else
HOSTCONF += --with-pic
endif

DOWNLOAD = cd $(TARBALLS) && $(WGET) -nc
checksum = (cd $(TARBALLS) && $(1)sum -c -) < \
		$(SRC)/$(patsubst .sum-%,%,$@)/$(2)SUMS
CHECK_SHA256 = $(call checksum,sha512,SHA512)
CHECK_SHA512 = $(call checksum,sha512,SHA512)
untar = $(RM) -R $@; tar xv$(1)f $<
UNPACK_GZ = $(call untar,z)
UNPACK_BZ2 = $(call untar,j)
UNPACK_XZ = $(call untar,J)
UNPACK_ZIP = $(RM) -R $@; unzip $<

#
# Per-package build rules
#
include ../src/*/rules.mak

#
# Targets
#
ifneq ($(filter $(PKGS_DISABLE),$(PKGS_ENABLE)),)
$(error Same package(s) disabled and enabled at the same time)
endif
PKGS := $(filter-out $(PKGS_DISABLE),$(PKGS)) $(PKGS_ENABLE)

fetch: $(PKGS:%=.sum-%)
fetch-all: $(ALL_PKGS:%=.sum-%)
install: $(PKGS:%=.%)

mostlyclean:
	-$(RM) $(ALL_PKGS:%=.%) $(ALL_PKGS:%=.sum-%)
	-$(RM) toolchain.cmake
	-$(RM) -R "$(PREFIX)"
	-find -maxdepth 1 -type d '!' -name . -exec $(RM) -R '{}' ';'

clean: mostlyclean
	-$(RM) $(TARBALLS)/*.*

distclean: clean
	$(RM) config.mak
	unlink Makefile

package: install
	(cd $(PREFIX)/.. && \
	tar cvJ $(notdir $(PREFIX))/) > ../vlc-contrib-$(HOST)-$(DATE).tar.xz

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
	echo "set(CMAKE_CXX_FLAGS $(CFLAGS)" >> $@
	echo "set(CMAKE_LD_FLAGS $(LDFLAGS))" >> $@
endif
	echo "set(CMAKE_C_COMPILER $(CC))" >> $@
	echo "set(CMAKE_CXX_COMPILER $(CXX))" >> $@
	echo "set(CMAKE_FIND_ROOT_PATH $(PREFIX))" >> $@
	echo "set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)" >> $@
	echo "set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)" >> $@
	echo "set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)" >> $@

.DELETE_ON_ERROR:
