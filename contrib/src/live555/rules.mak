# live555

#LIVEDOTCOM_URL := http://live555.com/liveMedia/public/live555-latest.tar.gz
LIVE555_FILE := live.2012.09.13.tar.gz
LIVEDOTCOM_URL := http://download.videolan.org/pub/contrib/live555/$(LIVE555_FILE)

PKGS += live555

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
ifdef HAVE_WINCE
LIVE_TARGET := mingw
endif
ifdef HAVE_DARWIN_OS
LIVE_TARGET := macosx
else
ifdef HAVE_BSD
LIVE_TARGET := freebsd
endif
endif

LIVE_EXTRA_CFLAGS := $(EXTRA_CFLAGS) -fexceptions

live555: $(LIVE555_FILE) .sum-live555
	rm -Rf live
	$(UNPACK)
	chmod -R u+w live
ifdef HAVE_WINCE
	cd live && sed -e 's/-lws2_32/-lws2/g' -i.orig config.mingw
endif
	cd live && sed -e 's%cc%$(CC)%' -e 's%c++%$(CXX)%' -e 's%LIBRARY_LINK =.*ar%LIBRARY_LINK = $(AR)%' -i.orig config.$(LIVE_TARGET)
	cd live && sed -i.orig -e s/"libtool -s -o"/"ar cr"/g config.macosx*
	cd live && sed -i.orig \
		-e 's%$(CXX)%$(CXX)\ $(EXTRA_LDFLAGS)%' config.macosx
	cd live && sed -i.orig \
		-e 's%^\(COMPILE_OPTS.*\)$$%\1 '"$(LIVE_EXTRA_CFLAGS)%" config.*
	cd live && sed -e 's%-D_FILE_OFFSET_BITS=64%-D_FILE_OFFSET_BITS=64\ -fPIC\ -DPIC%' -i.orig config.linux
ifdef HAVE_ANDROID
	cd live && sed -e 's%-DPIC%-DPIC -DNO_SSTREAM=1 -DLOCALE_NOT_USED -I$(ANDROID_NDK)/platforms/android-9/arch-$(PLATFORM_SHORT_ARCH)/usr/include%' -i.orig config.linux
	patch -p0 < $(SRC)/live555/android.patch
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
