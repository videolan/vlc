# live555

LIVE555_VERSION := 2026.06.24
LIVE555_FILE := live.$(LIVE555_VERSION).tar.gz
LIVEDOTCOM_URL := $(CONTRIB_VIDEOLAN)/live555/$(LIVE555_FILE)

ifdef HAVE_GCC
# older GCC doesn't support -std=c++20 required for std::atomic_flag
ifeq ($(call gcc_at_least, 10), true)
HAVE_LIVE555_CPP20=1
endif
else
ifdef HAVE_CLANG
# older CLANG doesn't support -std=c++20 required for std::atomic_flag
ifeq ($(call clang_at_least, 12), true)
HAVE_LIVE555_CPP20=1
endif
else
HAVE_LIVE555_CPP20=1
endif
endif

ifdef BUILD_NETWORK
ifdef GNUV3
PKGS += live555
endif
endif

ifeq ($(call need_pkg,"live555"),)
PKGS_FOUND += live555
endif

$(TARBALLS)/$(LIVE555_FILE):
	$(call download_pkg,$(LIVEDOTCOM_URL),live555)

.sum-live555: $(LIVE555_FILE)

LIVE_EXTRA_CFLAGS := $(EXTRA_CFLAGS) -fexceptions -DNO_OPENSSL=1 $(CFLAGS)

LIVE_TARGET = $(error live555 target not defined!)
ifdef HAVE_LINUX
LIVE_TARGET := linux
LIVE_EXTRA_CFLAGS += -DXLOCALE_NOT_USED
endif
ifdef HAVE_ANDROID
LIVE_EXTRA_CFLAGS += -DDISABLE_LOOPBACK_IP_ADDRESS_CHECK=1
endif
ifdef HAVE_WIN32
LIVE_TARGET := mingw
LIVE_EXTRA_CFLAGS += -DNO_GETIFADDRS=1
endif
ifdef HAVE_DARWIN_OS
LIVE_TARGET := macosx-catalina
endif
ifdef HAVE_BSD
LIVE_TARGET := freebsd
endif
ifdef HAVE_SOLARIS
ifeq ($(ARCH),x86_64)
LIVE_TARGET := solaris-64bit
else
LIVE_TARGET := solaris-32bit
endif
endif

live555: UNPACK_DIR=live
live555: $(LIVE555_FILE) .sum-live555
	rm -rf $(UNPACK_DIR)
	$(UNPACK)

	# Change permissions to patch and sed the source
	chmod -R u+w $(UNPACK_DIR)
	# Remove hardcoded cc, c++, ar variables
	sed -e 's%C_COMPILER%#C_COMPILER%' -e 's%CPLUSPLUS_COMPILER%#CPLUSPLUS_COMPILER%' -e 's%LIBRARY_LINK%#LIBRARY_LINK%' -i.orig $(UNPACK_DIR)/config.$(LIVE_TARGET)
	# Remove hardcoded --std=c+20 on un supported compilers
ifndef HAVE_LIVE555_CPP20
	sed -e 's%-std=c++20% -DNO_STD_LIB=1%' -i.orig $(UNPACK_DIR)/config.$(LIVE_TARGET)
endif
	# Add the Extra_CFLAGS to the config files
	sed -i.orig \
		-e 's%^\(COMPILE_OPTS.*\)$$%\1 '"$(LIVE_EXTRA_CFLAGS)%" $(UNPACK_DIR)/config.$(LIVE_TARGET)
	# We want 64bits offsets and PIC on Linux
	sed -e 's%-D_FILE_OFFSET_BITS=64%-D_FILE_OFFSET_BITS=64\ -fPIC\ -DPIC%' -i.orig $(UNPACK_DIR)/config.linux
	# Disable Locale for Solaris
	sed -e 's%-DSOLARIS%-DSOLARIS -DXLOCALE_NOT_USED%' -i.orig $(UNPACK_DIR)/config.solaris-*bit
ifdef HAVE_ANDROID
	# Disable locale on Android too
	sed -e 's%-DPIC%-DPIC -DNO_SSTREAM=1 -DLOCALE_NOT_USED -I$(ANDROID_NDK)/platforms/android-$(ANDROID_API)/arch-$(PLATFORM_SHORT_ARCH)/usr/include%' -i.orig $(UNPACK_DIR)/config.linux
endif
	# Add a pkg-config file
	$(APPLY) $(SRC)/live555/add-pkgconfig-file.patch
	# Expose Server:
	$(APPLY) $(SRC)/live555/expose_server_string.patch
	# FormatMessageA is available on all Windows versions, even WinRT
	$(APPLY) $(SRC)/live555/live555-formatmessage.patch
	# ifaddrs.h is supported since API level 24
	$(APPLY) $(SRC)/live555/android-no-ifaddrs.patch
	# Don't use unavailable off64_t functions
	$(APPLY) $(SRC)/live555/file-offset-bits-64.patch
	# add IPv4 inet_pton()/inet_ntop() helper on older Windows
	$(APPLY) $(SRC)/live555/live555-vista-inet.patch
	# fix Win32 time_tm constructor to build on older compilers
	sed -e 's,= tm{},= tm\(\),' -i.orig $(UNPACK_DIR)/liveMedia/RTSPCommon.cpp
	# disable code built/installed in unused folder
	sed -e 's,all: $$(,all: #,' -e 's,install: $$,install: #,' -e 's,install ,#install ,' -i.orig $(UNPACK_DIR)/testProgs/Makefile.tail
	sed -e 's,all: $$(,all: #,' -e 's,install: $$,install: #,' -e 's,install ,#install ,' -i.orig $(UNPACK_DIR)/mediaServer/Makefile.tail
	sed -e 's,all: $$(,all: #,' -e 's,install: $$,install: #,' -e 's,install ,#install ,' -i.orig $(UNPACK_DIR)/proxyServer/Makefile.tail
	sed -e 's,all: $$(,all: #,' -e 's,install: $$,install: #,' -e 's,install ,#install ,' -i.orig $(UNPACK_DIR)/hlsProxy/Makefile.tail
	$(MOVE)

LIVE555_ENV := $(HOSTVARS) C_COMPILER=$(CC) CPLUSPLUS_COMPILER=$(CXX) LIBRARY_LINK="$(AR) cr " \
	PREFIX=$(PREFIX) DESTDIR= LIBDIR=$(PREFIX)/lib

.live555: BUILD_DIR=$<
.live555: live555
	$(REQUIRE_GNUV3)
	cd $< && ./genMakefiles $(LIVE_TARGET)
	+$(MAKEBUILD) $(LIVE555_ENV)
	+$(MAKEBUILD) $(LIVE555_ENV) install
	+$(MAKEBUILD) $(LIVE555_ENV) install_shared_libraries
	touch $@
