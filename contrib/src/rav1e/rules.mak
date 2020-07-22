# rav1e

RAV1E_VERSION := 0.3.4
RAV1E_URL := https://github.com/xiph/rav1e/archive/v$(RAV1E_VERSION).tar.gz

ifdef BUILD_RUST
ifdef BUILD_ENCODERS
PKGS += rav1e
endif
endif

ifeq ($(call need_pkg,"rav1e"),)
PKGS_FOUND += rav1e
endif

$(TARBALLS)/rav1e-$(RAV1E_VERSION).tar.gz:
	$(call download_pkg,$(RAV1E_URL),rav1e)

.sum-rav1e: rav1e-$(RAV1E_VERSION).tar.gz

RAV1E_FEATURES=--features=asm

rav1e: rav1e-$(RAV1E_VERSION).tar.gz .sum-rav1e .rav1e-vendor
	$(UNPACK)
	$(CARGO_VENDOR_SETUP)
	$(MOVE)

.rav1e: rav1e .cargo
	cd $< && $(CARGOC_INSTALL) --no-default-features $(RAV1E_FEATURES)
	touch $@
