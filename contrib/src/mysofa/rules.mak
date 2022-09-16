# Mysofa

MYSOFA_VERSION := 0.5
MYSOFA_URL = $(GITHUB)/hoene/libmysofa/archive/v$(MYSOFA_VERSION).tar.gz

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

MYSOFA_CONF := -DBUILD_TESTS=OFF

.mysofa: mysofa toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS) $(CMAKE) $(MYSOFA_CONF)
	+$(CMAKEBUILD) --target install
	touch $@

