# Mysofa

MYSOFA_VERSION := 1.2.1
MYSOFA_URL = $(GITHUB)/hoene/libmysofa/archive/v$(MYSOFA_VERSION).tar.gz

ifeq ($(call need_pkg,"libmysofa"),)
PKGS_FOUND += mysofa
endif

DEPS_mysofa += zlib $(DEPS_zlib)
ifdef HAVE_WIN32
DEPS_mysofa += winpthreads $(DEPS_winpthreads)
endif

$(TARBALLS)/libmysofa-$(MYSOFA_VERSION).tar.gz:
	$(call download_pkg,$(MYSOFA_URL),mysofa)

.sum-mysofa: libmysofa-$(MYSOFA_VERSION).tar.gz

mysofa: libmysofa-$(MYSOFA_VERSION).tar.gz .sum-mysofa
	$(UNPACK)
	$(APPLY) $(SRC)/mysofa/0001-Only-link-with-MATH-library-if-it-s-found.patch
	$(APPLY) $(SRC)/mysofa/0002-Only-link-with-ZLib-library-if-it-s-found.patch
	$(call pkg_static,"libmysofa.pc.cmake")
	$(MOVE)

MYSOFA_CONF := -DBUILD_TESTS=OFF \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5

.mysofa: mysofa toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(MYSOFA_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
