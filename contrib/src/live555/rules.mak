# live555

LIVEDOTCOM_URL := http://live555.com/liveMedia/public/live555-latest.tar.gz

PKGS += live555

$(TARBALLS)/live555-latest.tar.gz:
	$(call download,$(LIVEDOTCOM_URL))

.sum-live555: live555-latest.tar.gz

live555: live555-latest.tar.gz .sum-live555
	$(UNPACK)
	patch -p0 < $(SRC)/live555/live-uselocale.patch
	patch -p0 < $(SRC)/live555/live-inet_ntop.patch
ifdef HAVE_WIN64
	patch -p0 < $(SRC)/live555/live-win64.patch
endif
ifndef HAVE_WIN32
ifndef HAVE_WINCE
	patch -p0 < $(SRC)/live555/live-getaddrinfo.patch
endif
endif
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
		"$(PREFIX)/lib"
	cp \
		$</groupsock/include/*.hh \
		$</groupsock/include/*.h \
		$</liveMedia/include/*.hh \
        	$</UsageEnvironment/include/*.hh \
        	$</BasicUsageEnvironment/include/*.hh \
		"$(PREFIX)/include"
	touch $@
