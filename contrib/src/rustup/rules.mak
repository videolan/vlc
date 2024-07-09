# rustup

RUSTUP_VERSION := 1.27.1
RUSTUP_URL := $(GITHUB)/rust-lang/rustup/archive/refs/tags/$(RUSTUP_VERSION).tar.gz

ifdef BUILD_RUST
PKGS_TOOLS += rustup

ifneq ($(call system_tool_version, rustup --version, cat),)
PKGS_FOUND += rustup
endif

endif

$(TARBALLS)/rustup-$(RUSTUP_VERSION).tar.gz:
	$(call download_pkg,$(RUSTUP_URL),rustup)

.sum-rustup: rustup-$(RUSTUP_VERSION).tar.gz

rustup: rustup-$(RUSTUP_VERSION).tar.gz .sum-rustup
	$(UNPACK)
	$(MOVE)

# Test if we can use the host libssl library
ifeq ($(shell unset PKG_CONFIG_LIBDIR PKG_CONFIG_PATH; \
	pkg-config "openssl >= 1.0.1" 2>/dev/null || \
	pkg-config "libssl >= 2.5" 2>/dev/null || echo FAIL),)
CARGOC_FEATURES=
else
# Otherwise, let cargo build and statically link its own openssl
CARGOC_FEATURES=--features=cargo/vendored-openssl
endif

.rustup: rustup
	cd $< && RUSTUP_INIT_SKIP_PATH_CHECK=yes \
	  RUSTUP_HOME=$(RUSTUP_HOME) CARGO_HOME=$(CARGO_HOME) \
	  ./rustup-init.sh --no-modify-path -y --default-toolchain none
	touch $@
