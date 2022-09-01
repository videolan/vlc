LIBARIBCC_HASH := fab6e2a857dbda2eabca5c9b53d7a67e5c00c626
LIBARIBCC_VERSION := git-$(LIBARIBCC_HASH)
LIBARIBCC_GITURL := https://github.com/xqq/libaribcaption.git

PKGS += libaribcaption
ifeq ($(call need_pkg,"libaribcaption"),)
PKGS_FOUND += libaribcaption
endif

LIBARIBCC_WITH_FREETYPE = 1

ifdef HAVE_ANDROID
LIBARIBCC_WITH_FONTCONFIG = 0
LIBARIBCC_WITH_DIRECTWRITE = 0
else
ifdef HAVE_DARWIN_OS
LIBARIBCC_WITH_FONTCONFIG = 0
LIBARIBCC_WITH_DIRECTWRITE = 0
else
ifdef HAVE_WIN32
LIBARIBCC_WITH_FONTCONFIG = 0
LIBARIBCC_WITH_DIRECTWRITE = 1
else
LIBARIBCC_WITH_FONTCONFIG = 1
LIBARIBCC_WITH_DIRECTWRITE = 0
endif
endif
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
	-DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON \
	-DARIBCC_NO_EXCEPTIONS:BOOL=ON \
	-DARIBCC_NO_RTTI:BOOL=ON

ifdef HAVE_ANDROID
LIBARIBCC_CONF += -DARIBCC_IS_ANDROID:BOOL=ON
endif

ifeq ($(LIBARIBCC_WITH_FREETYPE), 1)
DEPS_libaribcaption += freetype2 $(DEPS_freetype2)
LIBARIBCC_CONF += -DARIBCC_USE_FREETYPE:BOOL=ON
else
LIBARIBCC_CONF += -DARIBCC_USE_FREETYPE:BOOL=OFF
endif

ifeq ($(LIBARIBCC_WITH_FONTCONFIG), 1)
DEPS_libaribcaption += fontconfig $(DEPS_fontconfig)
LIBARIBCC_CONF += -DARIBCC_USE_FONTCONFIG:BOOL=ON
else
LIBARIBCC_CONF += -DARIBCC_USE_FONTCONFIG:BOOL=OFF
endif

ifeq ($(LIBARIBCC_WITH_DIRECTWRITE), 1)
LIBARIBCC_CONF += -DARIBCC_USE_DIRECTWRITE:BOOL=ON
endif


.libaribcaption: libaribcaption toolchain.cmake
	cd $< && rm -f CMakeCache.txt
	cd $< && $(HOSTVARS_PIC) $(CMAKE) $(LIBARIBCC_CONF)
	+$(CMAKEBUILD) $< --target install
	touch $@
