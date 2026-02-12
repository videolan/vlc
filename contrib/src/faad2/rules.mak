# faad2

FAAD2_VERSION := 2.11.2
FAAD2_URL := $(GITHUB)/knik0/faad2/archive/refs/tags/$(FAAD2_VERSION).tar.gz

ifeq ($(findstring $(ARCH),arm),)
# FAAD is a lot slower than lavc on ARM. Skip it.
ifdef GPL
PKGS += faad2
endif
endif

$(TARBALLS)/faad2-$(FAAD2_VERSION).tar.gz:
	$(call download_pkg,$(FAAD2_URL),faad2)

.sum-faad2: faad2-$(FAAD2_VERSION).tar.gz

faad2: faad2-$(FAAD2_VERSION).tar.gz .sum-faad2
	$(UNPACK)
ifndef HAVE_FPU
	$(APPLY) $(SRC)/faad2/faad2-fixed.patch
endif
	$(call pkg_static,"libfaad/faad2.pc.in")
	$(MOVE)

FAAD2_CONF := -DFAAD_APPLY_DRC=OFF -DFAAD_BUILD_CLI=OFF

.faad2: faad2 toolchain.cmake
	$(REQUIRE_GPL)
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(FAAD2_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
