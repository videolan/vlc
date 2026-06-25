# live555

LIVE555_VERSION := 2022.07.14
LIVE555_FILE := live.$(LIVE555_VERSION).tar.gz
LIVEDOTCOM_URL := $(CONTRIB_VIDEOLAN)/live555/$(LIVE555_FILE)

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
LIVE_TARGET := macosx-bigsur
else
ifdef HAVE_BSD
LIVE_TARGET := freebsd
endif
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
	sed -e 's%cc%$(CC)%' -e 's%c++%$(CXX)%' -e 's%LIBRARY_LINK =.*ar%LIBRARY_LINK = $(AR)%' -i.orig $(UNPACK_DIR)/config.$(LIVE_TARGET)
	# Replace libtool -s by ar cr for macOS only
	sed -i.orig -e s/"libtool -s -o"/"ar cr"/g $(UNPACK_DIR)/config.macosx*
	# Add Extra LDFLAGS for macOS
	sed -i.orig -e 's%$(CXX)%$(CXX)\ $(EXTRA_LDFLAGS)%' $(UNPACK_DIR)/config.macosx*
	# Add CXXFLAGS for macOS (force libc++)
	sed -i.orig -e 's%^\(CPLUSPLUS_FLAGS.*\)$$%\1 '"$(CXXFLAGS)%" $(UNPACK_DIR)/config.macosx*
	# Add the Extra_CFLAGS to all config files
	sed -i.orig \
		-e 's%^\(COMPILE_OPTS.*\)$$%\1 '"$(LIVE_EXTRA_CFLAGS)%" $(UNPACK_DIR)/config.*
	# We want 64bits offsets and PIC on Linux
	sed -e 's%-D_FILE_OFFSET_BITS=64%-D_FILE_OFFSET_BITS=64\ -fPIC\ -DPIC%' -i.orig $(UNPACK_DIR)/config.linux
	# Disable Locale for Solaris
	sed -e 's%-DSOLARIS%-DSOLARIS -DXLOCALE_NOT_USED%' -i.orig $(UNPACK_DIR)/config.solaris-*bit
ifdef HAVE_ANDROID
	# Disable locale on Android too
	sed -e 's%-DPIC%-DPIC -DNO_SSTREAM=1 -DLOCALE_NOT_USED -I$(ANDROID_NDK)/platforms/android-$(ANDROID_API)/arch-$(PLATFORM_SHORT_ARCH)/usr/include%' -i.orig $(UNPACK_DIR)/config.linux
endif
	# Patch for MSG_NOSIGNAL
	$(APPLY) $(SRC)/live555/live555-nosignal.patch
	# Add a pkg-config file
	$(APPLY) $(SRC)/live555/add-pkgconfig-file.patch
	# Expose Server:
	$(APPLY) $(SRC)/live555/expose_server_string.patch
	# Fix creating static libs on mingw
	$(APPLY) $(SRC)/live555/mingw-static-libs.patch
	# FormatMessageA is available on all Windows versions, even WinRT
	$(APPLY) $(SRC)/live555/live555-formatmessage.patch
	# ifaddrs.h is supported since API level 24
	$(APPLY) $(SRC)/live555/android-no-ifaddrs.patch
	# Don't use unavailable off64_t functions
	$(APPLY) $(SRC)/live555/file-offset-bits-64.patch
	sed -i.orig "s,LIBRARY_LINK =.*,LIBRARY_LINK = $(AR) cr ,g" $(UNPACK_DIR)/config.macosx*
	$(MOVE)

LIVE555_SUBDIRS=groupsock liveMedia UsageEnvironment BasicUsageEnvironment

.live555: live555
	$(REQUIRE_GNUV3)
	cd $< && for subdir in $(LIVE555_SUBDIRS); do \
		echo "PREFIX = $(PREFIX)" >> $$subdir/Makefile.head && \
		echo "LIBDIR = $(PREFIX)/lib" >> $$subdir/Makefile.head ; done
	cd $< && echo "LIBDIR = $(PREFIX)/lib" >> Makefile.head && \
		echo "PREFIX = $(PREFIX)" >> Makefile.head
	cd $< && ./genMakefiles $(LIVE_TARGET)
	cd $< && for subdir in $(LIVE555_SUBDIRS); do $(MAKE) $(HOSTVARS) -C $$subdir; done
	cd $< && for subdir in $(LIVE555_SUBDIRS); do $(MAKE) $(HOSTVARS) -C $$subdir install; done
	$(MAKE) -C $< install_shared_libraries
	touch $@
