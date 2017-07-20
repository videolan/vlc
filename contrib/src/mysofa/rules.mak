# Mysofa

MYSOFA_VERSION := 65d92e6463537c9f7907b69c2c768a7c8c3a02b5
MYSOFA_GITURL = https://github.com/hoene/libmysofa.git

PKGS += mysofa

ifeq ($(call need_pkg,"mysofa"),)
PKGS_FOUND += mysofa
endif

DEPS_mysofa += pthreads zlib $(DEPS_pthreads) $(DEPS_zlib)

$(TARBALLS)/mysofa-git.tar.xz:
	$(call download_git,$(MYSOFA_GITURL),,$(MYSOFA_VERSION))

.sum-mysofa: mysofa-git.tar.xz
	$(call check_githash,$(MYSOFA_VERSION))
	touch $@

mysofa: mysofa-git.tar.xz .sum-mysofa
	$(UNPACK)
	$(MOVE)

.mysofa: mysofa toolchain.cmake
	-cd $< && rm CMakeCache.txt
	cd $< && $(HOSTVARS) $(CMAKE) -DBUILD_TESTS=OFF -DBUILD_SHARED_LIBS=OFF
	cd $< && $(MAKE) install
	touch $@

