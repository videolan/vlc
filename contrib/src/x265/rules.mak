# x265

#X265_GITURL := https://github.com/videolan/x265
X265_VERSION := 2.9
X265_SNAPURL := https://bitbucket.org/multicoreware/x265_git/downloads/x265_$(X265_VERSION).tar.gz

ifdef BUILD_ENCODERS
ifdef GPL
PKGS += x265
endif
endif

DEPS_x265 :=
ifdef HAVE_WINSTORE
# x265 uses LoadLibraryEx
DEPS_x265 += alloweduwp $(DEPS_alloweduwp)
endif

ifeq ($(call need_pkg,"x265 >= 0.6"),)
PKGS_FOUND += x265
endif

$(TARBALLS)/x265-git.tar.xz:
	$(call download_git,$(X265_GITURL))

$(TARBALLS)/x265_$(X265_VERSION).tar.gz:
	$(call download_pkg,$(X265_SNAPURL),x265)

.sum-x265: x265_$(X265_VERSION).tar.gz

x265: x265_$(X265_VERSION).tar.gz .sum-x265
	$(UNPACK)
	$(APPLY) $(SRC)/x265/x265-ldl-linking.patch
	$(APPLY) $(SRC)/x265/x265-no-pdb-install.patch
	$(APPLY) $(SRC)/x265/x265-enable-detect512.patch
	$(APPLY) $(SRC)/x265/0001-api-use-LoadLibraryExA-instead-of-LoadLibraryA.patch
	$(APPLY) $(SRC)/x265/0001-threadpool-disable-group-affinity-in-UWP-builds.patch
	$(call pkg_static,"source/x265.pc.in")
	$(MOVE)

X265_CONF := -DENABLE_SHARED=OFF -DENABLE_CLI=OFF

.x265: x265 toolchain.cmake
	$(REQUIRE_GPL)
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) -S $</source $(X265_CONF)
	+$(CMAKEBUILD)
	sed -e s/'[^ ]*clang_rt[^ ]*'//g -i.orig "$(BUILD_DIR)/x265.pc"
	$(CMAKEINSTALL)
	touch $@
