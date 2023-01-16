# cargo/cargo-c installation via rustup

RUST_VERSION=1.66.1
CARGOC_VERSION=0.9.15
RUSTUP_VERSION=1.25.1
RUSTUP_URL=$(GITHUB)/rust-lang/rustup/archive/$(RUSTUP_VERSION).tar.gz

RUSTUP = . $(CARGO_HOME)/env && \
	RUSTUP_HOME=$(RUSTUP_HOME) CARGO_HOME=$(CARGO_HOME) rustup

$(TARBALLS)/rustup-$(RUSTUP_VERSION).tar.gz:
	$(call download_pkg,$(RUSTUP_URL),rustup)

.sum-cargo: rustup-$(RUSTUP_VERSION).tar.gz

rustup: rustup-$(RUSTUP_VERSION).tar.gz .sum-cargo
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

# When needed (when we have a Rust dependency not using cargo-c), the cargo-c
# installation should go in a different package
.cargo: rustup
	cd $< && RUSTUP_INIT_SKIP_PATH_CHECK=yes \
	  RUSTUP_HOME=$(RUSTUP_HOME) CARGO_HOME=$(CARGO_HOME) \
	  ./rustup-init.sh --no-modify-path -y --default-toolchain $(RUST_VERSION)
	+$(RUSTUP) default $(RUST_VERSION)
	+$(RUSTUP) target add $(RUST_TARGET)
	unset PKG_CONFIG_LIBDIR PKG_CONFIG_PATH CFLAGS CPPFLAGS LDFLAGS; \
		$(CARGO) install --locked $(CARGOC_FEATURES) cargo-c --version $(CARGOC_VERSION)
	touch $@
