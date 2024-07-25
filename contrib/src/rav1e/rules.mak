# rav1e

RAV1E_VERSION := 0.7.1
RAV1E_URL := https://crates.io/api/v1/crates/rav1e/$(RAV1E_VERSION)/download

ifdef BUILD_RUST
ifdef BUILD_ENCODERS
PKGS += rav1e
PKGS_ALL += vendor-rav1e
endif
endif

ifeq ($(call need_pkg,"rav1e"),)
PKGS_FOUND += rav1e
endif

$(TARBALLS)/rav1e-$(RAV1E_VERSION).tar.gz:
	$(call download_pkg,$(RAV1E_URL),rav1e)

.sum-rav1e: rav1e-$(RAV1E_VERSION).tar.gz

RAV1E_FEATURES=--features=asm

# we may not need cargo if the tarball is downloaded, but it will be needed by rav1e anyway
ifdef HAVE_CROSS_COMPILE
DEPS_rav1e = rustc-cross $(DEPS_rustc-cross)
else
DEPS_rav1e = rustc $(DEPS_rustc)
endif
DEPS_vendor-rav1e = rustc $(DEPS_rustc)
DEPS_rav1e += vendor-rav1e $(DEPS_vendor-rav1e) cargo-c $(DEPS_cargo-c)

# vendor-rav1e

$(TARBALLS)/rav1e-$(RAV1E_VERSION)-vendor.tar.bz2: .sum-rav1e .rustc
	$(call download_vendor,rav1e-$(RAV1E_VERSION)-vendor.tar.bz2,rav1e,rav1e-$(RAV1E_VERSION).tar.gz)

.sum-vendor-rav1e: rav1e-$(RAV1E_VERSION)-vendor.tar.bz2

rav1e-vendor: rav1e-$(RAV1E_VERSION)-vendor.tar.bz2 .sum-vendor-rav1e
	$(UNPACK)
	$(MOVE)

.vendor-rav1e: rav1e-vendor
	touch $@

# rav1e

rav1e: rav1e-$(RAV1E_VERSION).tar.gz .sum-rav1e
	$(UNPACK)
	$(call cargo_vendor_setup,$(UNPACK_DIR),$@)
	$(MOVE)

.rav1e: rav1e
	+cd $< && $(CARGOC_INSTALL) --target-dir vlc_build --no-default-features $(RAV1E_FEATURES)
# No gcc in Android NDK25
ifdef HAVE_ANDROID
	sed -i -e 's/ -lgcc//g' $(PREFIX)/lib/pkgconfig/rav1e.pc
endif
	touch $@
