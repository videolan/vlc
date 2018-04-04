# aom
AOM_HASH := 3715bf25db5cdedbd8b49560903bad02d911c62f
AOM_VERSION := v1.0.0-$(AOM_HASH)
AOM_GITURL := https://aomedia.googlesource.com/aom/+archive/$(AOM_HASH).tar.gz

PKGS += aom
ifeq ($(call need_pkg,"aom"),)
PKGS_FOUND += aom
endif

$(TARBALLS)/aom-$(AOM_VERSION).tar.gz:
	$(call download,$(AOM_GITURL))

.sum-aom: aom-$(AOM_VERSION).tar.gz
	$(warning $@ not implemented)
	touch $@

aom: aom-$(AOM_VERSION).tar.gz .sum-aom
	rm -Rf $@-$(AOM_VERSION) $@
	mkdir -p $@-$(AOM_VERSION)
	tar xvzf "$<" -C $@-$(AOM_VERSION)
	$(MOVE)

DEPS_aom =

AOM_LDFLAGS := $(LDFLAGS)

AOM_CONF := \
	-DCONFIG_RUNTIME_CPU_DETECT=1 \
	-DENABLE_DOCS=OFF \
	-DENABLE_EXAMPLES=OFF \
	-DENABLE_TOOLS=OFF \
	-DCONFIG_UNIT_TESTS=0 \
	-DENABLE_TESTS=OFF \
	-DCONFIG_INSTALL_BINS=0 \
	-DCONFIG_INSTALL_DOCS=0 \
	-DCONFIG_DEPENDENCY_TRACKING=0

ifndef BUILD_ENCODERS
AOM_CONF += -DCONFIG_AV1_ENCODER=0
endif

ifndef HAVE_WIN32
AOM_CONF += -DCONFIG_PIC=1
endif

# libaom doesn't allow in-tree builds
.aom: aom toolchain.cmake
	cd $< && mkdir -p aom_build
	cd $</aom_build && LDFLAGS="$(AOM_LDFLAGS)" $(HOSTVARS) $(CMAKE) ../ $(AOM_CONF)
	cd $</aom_build && $(MAKE)
	cd $</aom_build && ../../../../contrib/src/pkg-static.sh aom.pc
	cd $</aom_build && $(MAKE) install
	touch $@
