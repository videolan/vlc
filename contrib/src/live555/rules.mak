# live555

LIVE555_FILE := live.2015.06.24.tar.gz
#LIVEDOTCOM_URL := http://live555.com/liveMedia/public/$(LIVE555_FILE)
LIVEDOTCOM_URL := $(CONTRIB_VIDEOLAN)/live555/$(LIVE555_FILE)

ifdef BUILD_NETWORK
PKGS += live555
endif

$(TARBALLS)/$(LIVE555_FILE):
	$(call download,$(LIVEDOTCOM_URL))

.sum-live555: $(LIVE555_FILE)

LIVE_TARGET = $(error live555 target not defined!)
ifdef HAVE_LINUX
LIVE_TARGET := linux
endif
ifdef HAVE_WIN32
LIVE_TARGET := mingw
endif
ifdef HAVE_DARWIN_OS
LIVE_TARGET := macosx
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

LIVE_EXTRA_CFLAGS := $(EXTRA_CFLAGS) -fexceptions

live555: $(LIVE555_FILE) .sum-live555
	rm -Rf live
	$(UNPACK)
	chmod -R u+w live
	(cd live && patch -fp1) < $(SRC)/live555/live555-nosignal.patch
	cd live && sed -e 's%cc%$(CC)%' -e 's%c++%$(CXX)%' -e 's%LIBRARY_LINK =.*ar%LIBRARY_LINK = $(AR)%' -i.orig config.$(LIVE_TARGET)
	cd live && sed -i.orig -e s/"libtool -s -o"/"ar cr"/g config.macosx*
	cd live && sed -i.orig \
		-e 's%$(CXX)%$(CXX)\ $(EXTRA_LDFLAGS)%' config.macosx
	cd live && sed -i.orig \
		-e 's%^\(COMPILE_OPTS.*\)$$%\1 '"$(LIVE_EXTRA_CFLAGS)%" config.*
	cd live && sed -e 's%-D_FILE_OFFSET_BITS=64%-D_FILE_OFFSET_BITS=64\ -fPIC\ -DPIC%' -i.orig config.linux
	cd live && sed -e 's%-DSOLARIS%-DSOLARIS -DXLOCALE_NOT_USED%' -i.orig config.solaris-*bit
ifdef HAVE_ANDROID
	cd live && sed -e 's%-DPIC%-DPIC -DNO_SSTREAM=1 -DLOCALE_NOT_USED -I$(ANDROID_NDK)/platforms/$(ANDROID_API)/arch-$(PLATFORM_SHORT_ARCH)/usr/include%' -i.orig config.linux
endif
	mv live $@
	touch $@

.live555: live555
	cd $< && ./genMakefiles $(LIVE_TARGET)
	cd $< && $(MAKE) $(HOSTVARS) -C groupsock \
			&& $(MAKE) $(HOSTVARS) -C liveMedia \
			&& $(MAKE) $(HOSTVARS) -C UsageEnvironment \
			&& $(MAKE) $(HOSTVARS) -C BasicUsageEnvironment
	mkdir -p -- "$(PREFIX)/lib" "$(PREFIX)/include"
	cp \
		$</groupsock/libgroupsock.a \
		$</liveMedia/libliveMedia.a \
		$</UsageEnvironment/libUsageEnvironment.a \
		$</BasicUsageEnvironment/libBasicUsageEnvironment.a \
		"$(PREFIX)/lib/"
	cp \
		$</groupsock/include/*.hh \
		$</groupsock/include/*.h \
		$</liveMedia/include/*.hh \
		$</UsageEnvironment/include/*.hh \
		$</BasicUsageEnvironment/include/*.hh \
		"$(PREFIX)/include/"
	touch $@
