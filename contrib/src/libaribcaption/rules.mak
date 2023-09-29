LIBARIBCC_VERSION := 1.1.1
LIBARIBCC_URL := $(GITHUB)/xqq/libaribcaption/archive/refs/tags/v$(LIBARIBCC_VERSION).tar.gz

PKGS += libaribcaption
ifeq ($(call need_pkg,"libaribcaption"),)
PKGS_FOUND += libaribcaption
endif

ifdef HAVE_ANDROID
LIBARIBCC_WITH_FONTCONFIG = 0
else
ifdef HAVE_DARWIN_OS
LIBARIBCC_WITH_FONTCONFIG = 0
else
ifdef HAVE_WIN32
LIBARIBCC_WITH_FONTCONFIG = 0
else
LIBARIBCC_WITH_FONTCONFIG = 1
endif
endif
endif

DEPS_libaribcaption = freetype2 $(DEPS_freetype2)

ifeq ($(LIBARIBCC_WITH_FONTCONFIG), 1)
DEPS_libaribcaption += fontconfig $(DEPS_fontconfig)
endif

$(TARBALLS)/libaribcaption-$(LIBARIBCC_VERSION).tar.gz:
	${call download_pkg,$(LIBARIBCC_URL),libaribcaption}

.sum-libaribcaption: libaribcaption-$(LIBARIBCC_VERSION).tar.gz

libaribcaption: libaribcaption-$(LIBARIBCC_VERSION).tar.gz .sum-libaribcaption
	$(UNPACK)
	$(MOVE)

LIBARIBCC_CONF := \
	-DARIBCC_NO_EXCEPTIONS:BOOL=ON \
	-DARIBCC_NO_RTTI:BOOL=ON

ifdef HAVE_ANDROID
LIBARIBCC_CONF += -DARIBCC_IS_ANDROID:BOOL=ON
endif

LIBARIBCC_CONF += -DARIBCC_USE_FREETYPE:BOOL=ON

ifeq ($(LIBARIBCC_WITH_FONTCONFIG), 1)
LIBARIBCC_CONF += -DARIBCC_USE_FONTCONFIG:BOOL=ON
else
LIBARIBCC_CONF += -DARIBCC_USE_FONTCONFIG:BOOL=OFF
endif


.libaribcaption: libaribcaption toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS) $(CMAKE) $(LIBARIBCC_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
