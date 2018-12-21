# OpenHMD

OPENHMD_VERSION := 80d51bea575a5bf71bb3a0b9683b80ac3146596a
OPENHMD_GITURL = https://github.com/OpenHMD/OpenHMD.git
OPENHMD_BRANCH = master

OPENHMD_DRIVERS = rift,vive,deepoon,psvr,nolo,external

ifdef HAVE_LINUX
ifndef HAVE_ANDROID
DEPS_openhmd = hidapi
endif
endif

ifdef HAVE_WIN32
DEPS_openhmd = hidapi
endif

ifdef HAVE_ANDROID
D
#OPENHMD_DRIVER_CONFIG = --disable-driver-oculus-rift --disable-driver-htc-vive --disable-driver-deepoon --disable-driver-psvr --disable-driver-nolo --disable-driver-external --disable-driver-wmr --enable-driver-android
OPENHMD_DRIVERS = android
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

OPENHMD_CONFIG = \
	-Ddrivers="$(OPENHMD_DRIVERS)" \
	-Dexamples= 

.openhmd: openhmd
	cd $< && rm -rf build
	cd $< && $(HOSTVARS_MESON) $(MESON) build $(OPENHMD_CONFIG)
	cd $< && ninja -C build install
	touch $@
