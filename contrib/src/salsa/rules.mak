# SALSA-lib

SALSA_URL = $(GITHUB)/tiwai/salsa-lib.git
SALSA_TAG = v0.2.0
SALSA_HASH = a3e5accc0b34ddc59fea2342f1ab1f8be179cf9d

SALSACONF = \
	--enable-chmap \
	--enable-conf \
	--enable-float \
	--enable-output \
	--enable-pcm \
	--disable-4bit \
	--disable-mixer \
	--disable-user-elem

$(TARBALLS)/salsa-lib-$(SALSA_TAG).tar.xz:
	$(call download_git,$(SALSA_URL),$(SALSA_TAG),$(SALSA_HASH))

.sum-salsa: $(TARBALLS)/salsa-lib-$(SALSA_TAG).tar.xz
	$(call check_githash,$(SALSA_HASH))
	touch $@

salsa-lib: salsa-lib-$(SALSA_TAG).tar.xz .sum-salsa
	$(UNPACK)
	$(MOVE)

.salsa: salsa-lib
	$(RECONF)
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(SALSACONF)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	touch $@

# ALSA placeholder

PKGS_ALL += alsa

ifdef HAVE_LINUX
ifndef HAVE_ANDROID
PKGS += alsa
endif
endif
ifeq ($(call need_pkg, "alsa >= 1.0.24"),)
PKGS_FOUND += alsa
endif

DEPS_alsa = salsa $(DEPS_salsa)

.sum-alsa: .sum-salsa
	touch $@

.alsa: .sum-alsa
	touch $@
