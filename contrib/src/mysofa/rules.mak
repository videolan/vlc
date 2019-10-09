# Mysofa

MYSOFA_VERSION := 0.5
MYSOFA_URL = https://github.com/hoene/libmysofa/archive/v$(MYSOFA_VERSION).tar.gz

PKGS += mysofa

ifeq ($(call need_pkg,"libmysofa"),)
PKGS_FOUND += mysofa
endif

MYSOFA_CFLAGS   := $(CFLAGS)
MYSOFA_CXXFLAGS := $(CXXFLAGS)
DEPS_mysofa += zlib $(DEPS_zlib)
ifdef HAVE_WIN32
DEPS_mysofa += pthreads $(DEPS_pthreads)
MYSOFA_CFLAGS   += -DPTW32_STATIC_LIB
MYSOFA_CXXFLAGS += -DPTW32_STATIC_LIB
endif

$(TARBALLS)/libmysofa-$(MYSOFA_VERSION).tar.gz:
	$(call download_pkg,$(MYSOFA_URL),mysofa)

.sum-mysofa: libmysofa-$(MYSOFA_VERSION).tar.gz

mysofa: libmysofa-$(MYSOFA_VERSION).tar.gz .sum-mysofa
	$(UNPACK)
	$(MOVE)

.mysofa: mysofa toolchain.cmake
	cd $< && rm -f CMakeCache.txt
	cd $< && $(HOSTVARS) CFLAGS="$(MYSOFA_CFLAGS)" CXXFLAGS="$(MYSOFA_CXXFLAGS)" $(CMAKE) -DBUILD_TESTS=OFF -DBUILD_SHARED_LIBS=OFF
	cd $< && $(MAKE) install
	touch $@

