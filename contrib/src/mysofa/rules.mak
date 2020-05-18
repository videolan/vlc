# Mysofa

MYSOFA_VERSION := 0.5
MYSOFA_URL = https://github.com/hoene/libmysofa/archive/v$(MYSOFA_VERSION).tar.gz

PKGS += mysofa

ifeq ($(call need_pkg,"libmysofa"),)
PKGS_FOUND += mysofa
endif

DEPS_mysofa += zlib $(DEPS_zlib)
ifdef HAVE_WIN32
DEPS_mysofa += pthreads $(DEPS_pthreads)
endif

$(TARBALLS)/libmysofa-$(MYSOFA_VERSION).tar.gz:
	$(call download_pkg,$(MYSOFA_URL),mysofa)

.sum-mysofa: libmysofa-$(MYSOFA_VERSION).tar.gz

mysofa: libmysofa-$(MYSOFA_VERSION).tar.gz .sum-mysofa
	$(UNPACK)
	$(MOVE)

.mysofa: mysofa toolchain.cmake
	cd $< && rm -f CMakeCache.txt
	cd $< && $(HOSTVARS) $(CMAKE) -DBUILD_TESTS=OFF
	cd $< && $(MAKE) install
	touch $@

