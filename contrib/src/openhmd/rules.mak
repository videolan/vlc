# OpenHMD

OPENHMD_VERSION := fa50b693fc09c669bbfc7fd3c0627f73ffdba464
OPENHMD_GITURL = https://github.com/magwyz/OpenHMD.git

ifdef HAVE_LINUX
ifndef HAVE_ANDROID
DEPS_openhmd = hidapi
endif
endif

ifdef HAVE_WIN32
DEPS_openhmd = hidapi
endif

ifdef HAVE_ANDROID
OPENHMD_DRIVER_CONFIG = --disable-driver-oculus-rift --disable-driver-htc-vive --disable-driver-deepoon --disable-driver-psvr --disable-driver-nolo --disable-driver-external --disable-driver-wmr --enable-driver-android
endif

PKGS += openhmd

ifeq ($(call need_pkg,"openhmd"),)
PKGS_FOUND += openhmd
endif

$(TARBALLS)/openhmd-git.tar.xz:
	$(call download_git,$(OPENHMD_GITURL),,$(OPENHMD_VERSION))

.sum-openhmd: openhmd-git.tar.xz
	$(call check_githash,$(OPENHMD_VERSION))
	touch $@

openhmd: openhmd-git.tar.xz .sum-openhmd
	$(UNPACK)
	$(MOVE)

.openhmd: openhmd
	cd $< && ./autogen.sh
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --enable-static --disable-shared $(OPENHMD_DRIVER_CONFIG)
	cd $< && $(MAKE)
	cd $< && $(MAKE) install
	touch $@
