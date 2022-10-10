LIBARIBCC_HASH := fab6e2a857dbda2eabca5c9b53d7a67e5c00c626
LIBARIBCC_VERSION := git-$(LIBARIBCC_HASH)
LIBARIBCC_GITURL := $(GITHUB)/xqq/libaribcaption.git

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

$(TARBALLS)/libaribcaption-$(LIBARIBCC_VERSION).tar.xz:
	$(call download_git,$(LIBARIBCC_GITURL),,$(LIBARIBCC_HASH))

.sum-libaribcaption: libaribcaption-$(LIBARIBCC_VERSION).tar.xz
	$(call check_githash,$(LIBARIBCC_HASH))
	touch $@

libaribcaption: libaribcaption-$(LIBARIBCC_VERSION).tar.xz .sum-libaribcaption
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
	$(HOSTVARS_PIC) $(CMAKE) $(LIBARIBCC_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
