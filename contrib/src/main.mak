# Main makefile for VLC 3rd party libraries ("contrib")
# Copyright (C) 2003-2011 the VideoLAN team
#
# This file is under the same license as the vlc package.

all: install

SRC := $(TOPSRC)/src
SRC_BUILT := $(TOPSRC_BUILT)/src
TARBALLS := $(TOPSRC)/tarballs
VLC_TOOLS ?= $(TOPSRC)/../extras/tools/build

PATH :=$(abspath $(VLC_TOOLS)/bin):$(PATH)
export PATH

PKGS_ALL := $(patsubst $(SRC)/%/rules.mak,%,$(wildcard $(SRC)/*/rules.mak))
DATE := $(shell date +%Y%m%d)
VPATH := $(TARBALLS)

# Common download locations
GNU ?= http://ftp.gnu.org/gnu
SF := https://netcologne.dl.sourceforge.net/
VIDEOLAN := http://downloads.videolan.org/pub/videolan
CONTRIB_VIDEOLAN := http://downloads.videolan.org/pub/contrib
GITHUB := https://github.com/

#
# Machine-dependent variables
#

PREFIX ?= $(TOPDST)/$(HOST)
PREFIX := $(abspath $(PREFIX))
BUILDPREFIX ?= $(TOPDST)
BUILDPREFIX := $(abspath $(BUILDPREFIX))
BUILDBINDIR ?= $(BUILDPREFIX)/bin
ifneq ($(HOST),$(BUILD))
HAVE_CROSS_COMPILE = 1
endif
ARCH := $(shell $(SRC)/get-arch.sh $(HOST))

ifeq ($(ARCH)-$(HAVE_WIN32),x86_64-1)
HAVE_WIN64 := 1
endif
ifeq ($(ARCH)-$(HAVE_WIN32),aarch64-1)
HAVE_WIN64 := 1
endif

need_pkg = $(shell $(PKG_CONFIG) $(1) || echo 1)

ifeq ($(findstring mingw32,$(BUILD)),mingw32)
MSYS_BUILD := 1
endif
ifeq ($(findstring msys,$(BUILD)),msys)
MSYS_BUILD := 1
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
NM ?= nm
RANLIB ?= ranlib
STRIP ?= strip
WIDL ?= widl
WINDRES ?= windres
PKG_CONFIG ?= pkg-config
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
NM ?= $(HOST)-nm
RANLIB ?= $(HOST)-ranlib
STRIP ?= $(HOST)-strip
WIDL ?= $(HOST)-widl
WINDRES ?= $(HOST)-windres

# On Debian x86_64-w64-mingw32-pkg-config exists, runs but returns an error when checking packages
ifeq ($(shell unset PKG_CONFIG_LIBDIR; $(HOST)-pkg-config --version 1>/dev/null 2>/dev/null || echo FAIL),)
PKG_CONFIG ?= $(HOST)-pkg-config
else
# Use the regular pkg-config and set some PKG_CONFIG_LIBDIR ourselves
PKG_CONFIG = pkg-config
ifeq ($(findstring $(origin PKG_CONFIG_LIBDIR),undefined),)
# an extra PKG_CONFIG_LIBDIR was provided, use it prioritarily
PKG_CONFIG_LIBDIR := $(PKG_CONFIG_LIBDIR):/usr/$(HOST)/lib/pkgconfig:/usr/lib/$(HOST)/pkgconfig
else
PKG_CONFIG_LIBDIR := /usr/$(HOST)/lib/pkgconfig:/usr/lib/$(HOST)/pkgconfig
endif
export PKG_CONFIG_LIBDIR
need_pkg = $(shell PKG_CONFIG_LIBDIR=$(PKG_CONFIG_LIBDIR) $(PKG_CONFIG) $(1) || echo 1)
endif

endif

ifdef HAVE_ANDROID
ifneq ($(findstring $(origin CC),undefined default),)
CC :=  clang
endif
ifneq ($(findstring $(origin CXX),undefined default),)
CXX := clang++
endif
endif

# -fno-stack-check is a workaround for a possible
# bug in Xcode 11 or macOS 10.15+
ifdef HAVE_DARWIN_OS
EXTRA_CFLAGS += -fno-stack-check
XCODE_FLAGS += OTHER_CFLAGS=-fno-stack-check
endif

ifdef HAVE_MACOSX
EXTRA_CXXFLAGS += -stdlib=libc++
ifeq ($(ARCH),x86_64)
EXTRA_CFLAGS += -m64
EXTRA_LDFLAGS += -m64
else
EXTRA_CFLAGS += -m32
EXTRA_LDFLAGS += -m32
endif

XCODE_FLAGS += -arch $(ARCH)

endif

CCAS=$(CC) -c

ifdef HAVE_IOS
ifdef HAVE_NEON
AS=perl $(abspath $(VLC_TOOLS)/bin/gas-preprocessor.pl) $(CC)
CCAS=gas-preprocessor.pl $(CC) -c
endif
endif

LN_S = ln -s
ifdef HAVE_WIN32
ifneq ($(shell $(CC) $(CFLAGS) -E -dM -include _mingw.h - < /dev/null | grep -E __MINGW64_VERSION_MAJOR),)
HAVE_MINGW_W64 := 1
MINGW_W64_VERSION := $(shell $(CC) $(CFLAGS) -E -dM -include _mingw.h - < /dev/null | grep -E 'define\s__MINGW64_VERSION_MAJOR' | sed -e 's/\#define\s__MINGW64_VERSION_MAJOR\s//')
HAVE_MINGW64_V8 := $(shell [ $(MINGW_W64_VERSION) -gt 7 ] && echo true)
endif
ifndef HAVE_CROSS_COMPILE
LN_S = cp -R
endif
endif

ifdef HAVE_SOLARIS
ifeq ($(ARCH),x86_64)
EXTRA_CFLAGS += -m64
EXTRA_LDFLAGS += -m64
else
EXTRA_CFLAGS += -m32
EXTRA_LDFLAGS += -m32
endif
endif

ifdef HAVE_WINSTORE
EXTRA_CFLAGS += -DWINSTORECOMPAT
EXTRA_LDFLAGS += -lwindowsappcompat
endif

ifneq ($(findstring clang, $(shell $(CC) --version 2>/dev/null)),)
HAVE_CLANG := 1
endif

cppcheck = $(shell $(CC) $(CFLAGS) -E -dM - < /dev/null | grep -E $(1))

EXTRA_CFLAGS += -I$(PREFIX)/include
CPPFLAGS := $(CPPFLAGS) $(EXTRA_CFLAGS)
CFLAGS := $(CFLAGS) $(EXTRA_CFLAGS)
CXXFLAGS := $(CXXFLAGS) $(EXTRA_CFLAGS) $(EXTRA_CXXFLAGS)
LDFLAGS := $(LDFLAGS) -L$(PREFIX)/lib $(EXTRA_LDFLAGS)

ifdef ENABLE_PDB
ifdef HAVE_CLANG
ifneq ($(findstring $(ARCH),i686 x86_64),)
CFLAGS := $(CFLAGS) -gcodeview
CXXFLAGS := $(CXXFLAGS) -gcodeview
endif
endif
endif

# Do not export those! Use HOSTVARS.

# Do the FPU detection, after we have figured out our compilers and flags.
ifneq ($(findstring $(ARCH),aarch64 i386 ppc ppc64 sparc sparc64 x86_64),)
# This should be consistent with include/vlc_cpu.h
HAVE_FPU = 1
else ifneq ($(findstring $(ARCH),arm),)
ifneq ($(call cppcheck, __VFP_FP__)),)
ifeq ($(call cppcheck, __SOFTFP__),)
HAVE_FPU = 1
endif
endif
else ifneq ($(call cppcheck, __mips_hard_float),)
HAVE_FPU = 1
endif

ACLOCAL_AMFLAGS += -I$(PREFIX)/share/aclocal
ifneq ($(wildcard $(VLC_TOOLS)/share/aclocal/*),)
ACLOCAL_AMFLAGS += -I$(abspath $(VLC_TOOLS)/share/aclocal)
endif
export ACLOCAL_AMFLAGS

#########
# Tools #
#########

ifdef HAVE_CROSS_COMPILE
# This inhibits .pc file from within the cross-compilation toolchain sysroot.
# Hopefully, nobody ever needs that.
PKG_CONFIG_PATH := /usr/share/pkgconfig
endif
PKG_CONFIG_PATH := $(PREFIX)/lib/pkgconfig:$(PKG_CONFIG_PATH)
ifeq ($(findstring mingw32,$(BUILD)),mingw32)
PKG_CONFIG_PATH := $(shell cygpath -pm ${PKG_CONFIG_PATH})
endif
export PKG_CONFIG_PATH

ifndef GIT
ifeq ($(shell git --version >/dev/null 2>&1 || echo FAIL),)
GIT = git
endif
endif
GIT ?= $(error git not found)

ifndef SVN
ifeq ($(shell svn --version >/dev/null 2>&1 || echo FAIL),)
SVN = svn
endif
endif
SVN ?= $(error subversion client (svn) not found)

ifeq ($(shell curl --version >/dev/null 2>&1 || echo FAIL),)
download = curl -f -L -- "$(1)" > "$@"
else ifeq ($(shell wget --version >/dev/null 2>&1 || echo FAIL),)
download = (rm -f $@.tmp && \
	wget --passive -c -p -O $@.tmp "$(1)" && \
	touch $@.tmp && \
	mv $@.tmp $@ )
else ifeq ($(which fetch >/dev/null 2>&1 || echo FAIL),)
download = (rm -f $@.tmp && \
	fetch -p -o $@.tmp "$(1)" && \
	touch $@.tmp && \
	mv $@.tmp $@)
else
download = $(error Neither curl nor wget found)
endif

download_pkg = $(call download,$(CONTRIB_VIDEOLAN)/$(2)/$(lastword $(subst /, ,$(@)))) || \
	( $(call download,$(1)) && echo "Please upload this package $(lastword $(subst /, ,$(@))) to our FTP" )

ifeq ($(shell which xz >/dev/null 2>&1 || echo FAIL),)
XZ = xz
else
XZ ?= $(error XZ (LZMA) compressor not found)
endif

ifeq ($(shell sha512sum --version >/dev/null 2>&1 || echo FAIL),)
SHA512SUM = sha512sum --check
else ifeq ($(shell shasum --version >/dev/null 2>&1 || echo FAIL),)
SHA512SUM = shasum -a 512 --check
else ifeq ($(shell openssl version >/dev/null 2>&1 || echo FAIL),)
SHA512SUM = openssl dgst -sha512
else
SHA512SUM = $(error SHA-512 checksumming not found)
endif

ifeq ($(shell protoc --version >/dev/null 2>&1 || echo FAIL),)
PROTOC = protoc
else
PROTOC ?= $(error Protobuf compiler (protoc) not found)
endif

#
# Common helpers
#
HOSTCONF := --prefix="$(PREFIX)"
HOSTCONF += --datarootdir="$(PREFIX)/share"
HOSTCONF += --includedir="$(PREFIX)/include"
HOSTCONF += --libdir="$(PREFIX)/lib"
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
	AR="$(AR)" CCAS="$(CCAS)" RANLIB="$(RANLIB)" STRIP="$(STRIP)" \
	PATH="$(PREFIX)/bin:$(PATH)" \
	PKG_CONFIG="$(PKG_CONFIG)"

HOSTVARS_MESON := $(HOSTTOOLS) \
	CPPFLAGS="$(CPPFLAGS)" \
	CFLAGS="$(CFLAGS)" \
	CXXFLAGS="$(CXXFLAGS)" \
	LDFLAGS="$(LDFLAGS)"

# Add these flags after Meson consumed the CFLAGS/CXXFLAGS
# as when setting those for Meson, it would apply to tests
# and cause the check if symbols have underscore prefix to
# incorrectly report they have not, even if they have.
ifndef WITH_OPTIMIZATION
CFLAGS := $(CFLAGS) -g -O0
CXXFLAGS := $(CXXFLAGS) -g -O0
else
CFLAGS := $(CFLAGS) -g -O2
CXXFLAGS := $(CXXFLAGS) -g -O2
endif

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
	rm -Rf -- "$(@:.tar.xz=)" && \
	$(GIT) init --bare "$(@:.tar.xz=)" && \
	(cd "$(@:.tar.xz=)" && \
	$(GIT) remote add origin "$(1)" && \
	$(GIT) fetch origin "$(2)") && \
	(cd "$(@:.tar.xz=)" && \
	$(GIT) archive --prefix="$(notdir $(@:.tar.xz=))/" \
		--format=tar "$(3)") > "$(@:.xz=)" && \
	echo "$(3) $(@)" > "$(@:.tar.xz=.githash)" && \
	rm -Rf -- "$(@:.tar.xz=)" && \
	$(XZ) --stdout "$(@:.xz=)" > "$@.tmp" && \
	rm -f "$(@:.xz=)" && \
	mv -f -- "$@.tmp" "$@"
check_githash = \
	h=`sed -e "s,^\([0-9a-fA-F]\{40\}\) .*/$(notdir $<),\1,g" \
		< "$(<:.tar.xz=.githash)"` && \
	test "$$h" = "$1"

checksum = \
	$(foreach f,$(filter $(TARBALLS)/%,$^), \
		grep -- " $(f:$(TARBALLS)/%=%)$$" \
			"$(SRC)/$(patsubst .sum-%,%,$@)/$(2)SUMS" &&) \
	(cd $(TARBALLS) && $(1) /dev/stdin) < \
		"$(SRC)/$(patsubst .sum-%,%,$@)/$(2)SUMS"
CHECK_SHA512 = $(call checksum,$(SHA512SUM),SHA512)
UNPACK = $(RM) -R $@ \
	$(foreach f,$(filter %.tar.gz %.tgz,$^), && tar xvzfo $(f)) \
	$(foreach f,$(filter %.tar.bz2,$^), && tar xvjfo $(f)) \
	$(foreach f,$(filter %.tar.xz,$^), && tar xvJfo $(f)) \
	$(foreach f,$(filter %.zip,$^), && unzip $(f))
UNPACK_DIR = $(patsubst %.tar,%,$(basename $(notdir $<)))
APPLY = (cd $(UNPACK_DIR) && patch -fp1) <
pkg_static = (cd $(UNPACK_DIR) && $(SRC_BUILT)/pkg-static.sh $(1))
MOVE = mv $(UNPACK_DIR) $@ && touch $@

AUTOMAKE_DATA_DIRS=$(foreach n,$(foreach n,$(subst :, ,$(shell echo $$PATH)),$(abspath $(n)/../share)),$(wildcard $(n)/automake*))
UPDATE_AUTOCONFIG = for dir in $(AUTOMAKE_DATA_DIRS); do \
		if test -f "$${dir}/config.sub" -a -f "$${dir}/config.guess"; then \
			cp "$${dir}/config.sub" "$${dir}/config.guess" $(UNPACK_DIR); \
			break; \
		fi; \
	done

ifdef HAVE_DARWIN_OS
AUTORECONF = AUTOPOINT=true autoreconf
else
AUTORECONF = autoreconf
endif
RECONF = mkdir -p -- $(PREFIX)/share/aclocal && \
	cd $< && $(AUTORECONF) -fiv $(ACLOCAL_AMFLAGS)
CMAKE = cmake . -DCMAKE_TOOLCHAIN_FILE=$(abspath toolchain.cmake) \
		-DCMAKE_INSTALL_PREFIX=$(PREFIX) $(CMAKE_GENERATOR) \
		-DBUILD_SHARED_LIBS:BOOL=OFF
ifdef HAVE_WIN32
CMAKE += -DCMAKE_DEBUG_POSTFIX:STRING=
endif
ifdef MSYS_BUILD
CMAKE := PKG_CONFIG_LIBDIR="$(PKG_CONFIG_PATH)" $(CMAKE)
CMAKE += -DCMAKE_LINK_LIBRARY_SUFFIX:STRING=.a
endif

ifeq ($(V),1)
CMAKE += -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON
endif

MESONFLAGS = --default-library static --prefix "$(PREFIX)" --backend ninja \
	-Dlibdir=lib
ifndef WITH_OPTIMIZATION
MESONFLAGS += --buildtype debug
else
MESONFLAGS += --buildtype debugoptimized
endif

ifdef HAVE_CROSS_COMPILE
# When cross-compiling meson uses the env vars like
# CC, CXX, etc. and CFLAGS, CXXFLAGS, etc. for the
# build machine compiler and not like most other
# buildsystems for the host compilation. Therefore
# we clear the enviornment variables using the env
# command, except PATH, which is needed.
# The values of the mentioned relevant env variables
# are passed for the host compilation using the
# generated crossfile, so everything should work as
# expected.
MESONFLAGS += --cross-file $(abspath crossfile.meson)
MESON = env -i PATH="$(PREFIX)/bin:$(PATH)" PKG_CONFIG_LIBDIR="$(PKG_CONFIG_LIBDIR)" \
	PKG_CONFIG_PATH="$(PKG_CONFIG_PATH)" \
	meson -Dpkg_config_libdir="$(PKG_CONFIG_LIBDIR)" \
	-Dpkg_config_path="$(PKG_CONFIG_PATH)" \
	$(MESONFLAGS)

else
MESON = meson $(MESONFLAGS)
endif

ifdef GPL
REQUIRE_GPL =
else
REQUIRE_GPL = @echo "Package \"$<\" requires the GPL license." >&2; exit 1
endif
ifdef GNUV3
REQUIRE_GNUV3 =
else
REQUIRE_GNUV3 = \
	@echo "Package \"$<\" requires the version 3 of GNU licenses." >&2; \
	exit 1
endif

#
# Per-package build rules
#
PKGS_FOUND :=
include $(SRC)/*/rules.mak

ifeq ($(PKGS_DISABLE), all)
PKGS :=
endif
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
	-$(RM) crossfile.meson
	-$(RM) -R "$(PREFIX)"
	-$(RM) -R "$(BUILDBINDIR)"
	-$(RM) -R */

clean: mostlyclean
	-$(RM) $(TARBALLS)/*.*

distclean: clean
	$(RM) config.mak
	unlink Makefile

PREBUILT_URL=http://download.videolan.org/pub/videolan/contrib/$(HOST)/vlc-contrib-$(HOST)-latest.tar.bz2

vlc-contrib-$(HOST)-latest.tar.bz2:
	$(call download,$(PREBUILT_URL))

prebuilt: vlc-contrib-$(HOST)-latest.tar.bz2
	$(RM) -r $(PREFIX)
	-$(UNPACK)
	mv $(HOST) $(PREFIX)
	cd $(PREFIX) && $(abspath $(SRC))/change_prefix.sh
ifdef HAVE_WIN32
ifndef HAVE_CROSS_COMPILE
	$(RM) `find $(PREFIX)/bin | file -f- | grep ELF | awk -F: '{print $$1}' | xargs`
endif
endif

package: install
	rm -Rf tmp/
	mkdir -p tmp/
	cp -R $(PREFIX) tmp/
	# remove useless files
	cd tmp/$(notdir $(PREFIX)); \
		cd share; rm -Rf man doc gtk-doc info lua projectM; cd ..; \
		rm -Rf man sbin etc lib/lua lib/sidplay
	cd tmp/$(notdir $(PREFIX)) && $(abspath $(SRC))/change_prefix.sh $(PREFIX) @@CONTRIB_PREFIX@@
ifneq ($(notdir $(PREFIX)),$(HOST))
	(cd tmp && mv $(notdir $(PREFIX)) $(HOST))
endif
	(cd tmp && tar c $(HOST)/) | bzip2 -c > ../vlc-contrib-$(HOST)-$(DATE).tar.bz2

list:
	@echo All packages:
	@echo '  $(PKGS_ALL)' | tr " " "\n" | sort | tr "\n" " " |fmt
	@echo Distribution-provided packages:
	@echo '  $(PKGS_FOUND)' | tr " " "\n" | sort | tr "\n" " " |fmt
	@echo Automatically selected packages:
	@echo '  $(PKGS_AUTOMATIC)' | tr " " "\n" | sort | tr "\n" " " |fmt
	@echo Manually deselected packages:
	@echo '  $(PKGS_DISABLE)' | tr " " "\n" | sort | tr "\n" " " |fmt
	@echo Manually selected packages:
	@echo '  $(PKGS_ENABLE)' | tr " " "\n" | sort | tr "\n" " " |fmt
	@echo Depended-on packages:
	@echo '  $(PKGS_DEPS)' | tr " " "\n" | sort | tr "\n" " " |fmt
	@echo To-be-built packages:
	@echo '  $(PKGS)' | tr " " "\n" | sort | tr "\n" " " |fmt

help:
	@cat $(SRC)/help.txt

.PHONY: all fetch fetch-all install mostlyclean clean distclean package list help prebuilt

CMAKE_SYSTEM_NAME =
ifdef HAVE_WIN32
CMAKE_SYSTEM_NAME = Windows
ifdef HAVE_VISUALSTUDIO
ifdef HAVE_WINSTORE
CMAKE_SYSTEM_NAME = WindowsStore
endif
ifdef HAVE_WINDOWSPHONE
CMAKE_SYSTEM_NAME = WindowsPhone
endif
endif
endif
ifdef HAVE_DARWIN_OS
CMAKE_SYSTEM_NAME = Darwin
endif

ifdef HAVE_ANDROID
CFLAGS += -DANDROID_NATIVE_API_LEVEL=$(ANDROID_API)
endif

# CMake toolchain
toolchain.cmake:
	$(RM) $@
ifndef WITH_OPTIMIZATION
	echo "set(CMAKE_BUILD_TYPE Debug)" >> $@
else
	echo "set(CMAKE_BUILD_TYPE RelWithDebInfo)" >> $@
endif
	echo "set(CMAKE_SYSTEM_PROCESSOR $(ARCH))" >> $@
	if test -n "$(CMAKE_SYSTEM_NAME)"; then \
		echo "set(CMAKE_SYSTEM_NAME $(CMAKE_SYSTEM_NAME))" >> $@; \
	fi;
ifdef HAVE_WIN32
ifdef HAVE_CROSS_COMPILE
	echo "set(CMAKE_RC_COMPILER $(WINDRES))" >> $@
endif
endif
ifdef HAVE_DARWIN_OS
	echo "set(CMAKE_C_FLAGS \"$(CFLAGS)\")" >> $@
	echo "set(CMAKE_CXX_FLAGS \"$(CXXFLAGS)\")" >> $@
	echo "set(CMAKE_LD_FLAGS \"$(LDFLAGS)\")" >> $@
ifdef HAVE_IOS
	echo "set(CMAKE_OSX_SYSROOT $(IOS_SDK))" >> $@
else
	echo "set(CMAKE_OSX_SYSROOT $(MACOSX_SDK))" >> $@
endif
endif
	echo "set(CMAKE_AR $(AR) CACHE FILEPATH \"Archiver\")" >> $@
ifdef HAVE_CROSS_COMPILE
	echo "set(_CMAKE_TOOLCHAIN_PREFIX $(HOST)-)" >> $@
ifdef HAVE_ANDROID
# cmake will overwrite our --sysroot with a native (host) one on Darwin
# Set it to "" right away to short-circuit this behaviour
	echo "set(CMAKE_CXX_SYSROOT_FLAG \"\")" >> $@
	echo "set(CMAKE_C_SYSROOT_FLAG \"\")" >> $@
endif
endif
	echo "set(CMAKE_C_COMPILER $(CC))" >> $@
	echo "set(CMAKE_CXX_COMPILER $(CXX))" >> $@
ifdef MSYS_BUILD
	echo "set(CMAKE_FIND_ROOT_PATH `cygpath -m $(PREFIX)`)" >> $@
else
	echo "set(CMAKE_FIND_ROOT_PATH $(PREFIX))" >> $@
endif
	echo "set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)" >> $@
ifdef HAVE_CROSS_COMPILE
	echo "set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)" >> $@
	echo "set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)" >> $@
endif

MESON_SYSTEM_NAME =
ifdef HAVE_WIN32
	MESON_SYSTEM_NAME = windows
else
ifdef HAVE_DARWIN_OS
	MESON_SYSTEM_NAME = darwin
else
ifdef HAVE_ANDROID
	MESON_SYSTEM_NAME = android
else
ifdef HAVE_LINUX
	# android has also system = linux and defines HAVE_LINUX
	MESON_SYSTEM_NAME = linux
else
	$(error "No meson system name known for this target")
endif
endif
endif
endif

crossfile.meson: $(SRC)/gen-meson-crossfile.py
	$(HOSTVARS_MESON) \
	WINDRES="$(WINDRES)" \
	PKG_CONFIG="$(PKG_CONFIG)" \
	HOST_SYSTEM="$(MESON_SYSTEM_NAME)" \
	HOST_ARCH="$(subst i386,x86,$(ARCH))" \
	HOST="$(HOST)" \
	$(SRC)/gen-meson-crossfile.py $@
	cat $@

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
