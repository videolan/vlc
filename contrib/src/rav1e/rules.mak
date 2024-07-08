# rav1e

RAV1E_VERSION := 0.7.1
RAV1E_URL := https://crates.io/api/v1/crates/rav1e/$(RAV1E_VERSION)/download

ifdef BUILD_RUST
ifdef BUILD_ENCODERS
# Rav1e is not linking correctly on iOS arm64
ifndef HAVE_IOS
PKGS += rav1e
PKGS_ALL += rav1e-vendor
endif
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
DEPS_rav1e-vendor = rustc-cross $(DEPS_rustc-cross)
DEPS_rav1e = rustc-cross $(DEPS_rustc-cross)
else
DEPS_rav1e-vendor = rustc $(DEPS_rustc)
DEPS_rav1e = rustc $(DEPS_rustc)
endif
DEPS_rav1e += rav1e-vendor $(DEPS_rav1e-vendor) cargo-c $(DEPS_cargo-c)

# rav1e-vendor

rav1e-vendor-build: .rustc
	$(RM) -R $@
	mkdir -p $@
	tar xzfo $(TARBALLS)/rav1e-$(RAV1E_VERSION).tar.gz -C $@ --strip-components=1
	cd $@ && $(CARGO) vendor --locked rav1e-$(RAV1E_VERSION)-vendor
	cd $@ && tar -jcf rav1e-$(RAV1E_VERSION)-vendor.tar.bz2 rav1e-$(RAV1E_VERSION)-vendor
	install $@/rav1e-$(RAV1E_VERSION)-vendor.tar.bz2 "$(TARBALLS)"
	# cd $@ && sha512sum rav1e-$(RAV1E_VERSION)-vendor.tar.bz2 > SHA512SUMS
	# install $@/SHA512SUMS $(SRC)/rav1e-vendor/SHA512SUMS
	$(RM) -R $@

$(TARBALLS)/rav1e-$(RAV1E_VERSION)-vendor.tar.bz2: .sum-rav1e
	-$(call download_vendor,rav1e-$(RAV1E_VERSION)-vendor.tar.bz2,rav1e)
	# if the vendor tarball doesn't exist yet, we build it
	if test ! -s "$@"; then $(RM) -R rav1e-vendor-build; $(MAKE) rav1e-vendor-build; fi

.sum-rav1e-vendor: rav1e-$(RAV1E_VERSION)-vendor.tar.bz2
	touch $@

rav1e-vendor: rav1e-$(RAV1E_VERSION)-vendor.tar.bz2 .sum-rav1e-vendor
	$(UNPACK)
	$(MOVE)

.rav1e-vendor: rav1e-vendor
	touch $@

# rav1e

rav1e: rav1e-$(RAV1E_VERSION).tar.gz .sum-rav1e
	$(UNPACK)
	$(call cargo_vendor_setup,$(UNPACK_DIR),$@)
	$(MOVE)

.rav1e: rav1e
	+cd $< && $(CARGOC_INSTALL) --no-default-features $(RAV1E_FEATURES)
# No gcc in Android NDK25
ifdef HAVE_ANDROID
	sed -i -e 's/ -lgcc//g' $(PREFIX)/lib/pkgconfig/rav1e.pc
endif
	touch $@
