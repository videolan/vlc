# faad2

FAAD2_VERSION := 2.10.0
FAAD2_VERSION_FIXED := $(subst .,_,$(FAAD2_VERSION))
FAAD2_URL := $(GITHUB)/knik0/faad2/archive/$(FAAD2_VERSION_FIXED).tar.gz

ifeq ($(findstring $(ARCH),arm),)
# FAAD is a lot slower than lavc on ARM. Skip it.
ifdef GPL
PKGS += faad2
endif
endif

$(TARBALLS)/faad2-$(FAAD2_VERSION_FIXED).tar.gz:
	$(call download_pkg,$(FAAD2_URL),faad2)

.sum-faad2: faad2-$(FAAD2_VERSION_FIXED).tar.gz

faad2: faad2-$(FAAD2_VERSION_FIXED).tar.gz .sum-faad2
	$(UNPACK)
ifndef HAVE_FPU
	$(APPLY) $(SRC)/faad2/faad2-fixed.patch
endif
	$(APPLY) $(SRC)/faad2/faad2-disable-drc.patch
	$(APPLY) $(SRC)/faad2/faad2-fix-71wPCEmapping.patch
	$(APPLY) $(SRC)/faad2/faad2-add-define.patch
	cd $(UNPACK_DIR) && $(CC) -iquote . -E - </dev/null || sed -i 's/-iquote /-I/' libfaad/Makefile.am
	$(MOVE)

FAAD2_CONF := --without-drm

.faad2: faad2
	$(REQUIRE_GPL)
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(FAAD2_CONF)
	cd $< && sed -i.orig "s/shrext_cmds/shrext/g" libtool
	$(MAKE) -C $< -C libfaad install
	touch $@
