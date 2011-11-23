# live555

#LIVEDOTCOM_URL := http://live555.com/liveMedia/public/live555-latest.tar.gz
LIVE555_FILE := live.2011.11.08.tar.gz
LIVEDOTCOM_URL := http://live555sourcecontrol.googlecode.com/files/$(LIVE555_FILE)

PKGS += live555

$(TARBALLS)/$(LIVE555_FILE):
	$(call download,$(LIVEDOTCOM_URL))

.sum-live555: $(LIVE555_FILE)

live555: $(LIVE555_FILE) .sum-live555
	rm -Rf live
	$(UNPACK)
	chmod -R u+w live
	patch -p0 < $(SRC)/live555/live-uselocale.patch
	patch -p0 < $(SRC)/live555/live-inet_ntop.patch
	patch -p0 < $(SRC)/live555/live-intptr.patch
ifndef HAVE_WIN32
ifndef HAVE_WINCE
	patch -p0 < $(SRC)/live555/live-getaddrinfo.patch
endif
endif
	patch -p0 < $(SRC)/live555/live-cloexec.patch
	mv live $@
	touch $@

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
ifdef HAVE_MACOSX
LIVE_TARGET := macosx
endif

.live555: live555
ifdef HAVE_WINCE
	cd $< && sed -e 's/-lws2_32/-lws2/g' -i.orig config.mingw
endif
ifdef HAVE_MACOSX
	cd $< && sed -i.orig -e s/"libtool -s -o"/"ar cr"/g config.macosx*
endif
	cd $< && sed \
		-e 's%-DBSD=1%-DBSD=1\ $(EXTRA_CFLAGS)\ $(EXTRA_LDFLAGS)%' \
		-e 's%cc%$(CC)%' \
		-e 's%c++%$(CXX)\ $(EXTRA_LDFLAGS)%' \
		-i.orig config.macosx
	cd $< && sed -e 's%-D_FILE_OFFSET_BITS=64%-D_FILE_OFFSET_BITS=64\ -fPIC\ -DPIC%' -i.orig config.linux
	cd $< && ./genMakefiles $(LIVE_TARGET)
	cd $< && $(MAKE) $(HOSTVARS)
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
