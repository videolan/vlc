# rustc+cargo installation via rustup

RUST_VERSION=1.79.0
RUSTUP_VERSION := 1.27.1
RUSTUP_URL := $(GITHUB)/rust-lang/rustup/archive/refs/tags/$(RUSTUP_VERSION).tar.gz

RUSTUP = . $(CARGO_HOME)/env && \
	RUSTUP_HOME=$(RUSTUP_HOME) CARGO_HOME=$(CARGO_HOME) rustup

ifdef BUILD_RUST
PKGS_TOOLS += rustup rustc
PKGS_ALL += rustup

ifdef HAVE_CROSS_COMPILE
PKGS_TOOLS += rustc-cross
PKGS_ALL += rustc-cross
endif

ifneq ($(call system_tool_version, rustup --version, cat),)
PKGS_FOUND += rustup
RUSTUP = RUSTUP_HOME=$(RUSTUP_HOME) CARGO_HOME=$(CARGO_HOME) rustup
endif

ifneq ($(call system_tool_majmin, cargo --version),)
PKGS_FOUND += rustc
# TODO detect if the target is available
# PKGS_FOUND += rustc-cross
else
DEPS_rustc = rustup $(DEPS_rustup)
endif

endif

DEPS_rustc-cross = rustc $(DEPS_rustc) rustup $(DEPS_rustup)

$(TARBALLS)/rustup-$(RUSTUP_VERSION).tar.gz:
	$(call download_pkg,$(RUSTUP_URL),rustup)

.sum-rustup: rustup-$(RUSTUP_VERSION).tar.gz

.sum-rustc:
	touch $@

.sum-rustc-cross:
	touch $@

cargo: rustup-$(RUSTUP_VERSION).tar.gz .sum-rustup
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

.rustup: cargo
	cd $< && RUSTUP_INIT_SKIP_PATH_CHECK=yes \
	  RUSTUP_HOME=$(RUSTUP_HOME) CARGO_HOME=$(CARGO_HOME) \
	  ./rustup-init.sh --no-modify-path -y --default-toolchain none
	touch $@

.rustc: cargo
	+$(RUSTUP) set profile minimal
	+$(RUSTUP) default $(RUST_VERSION)
	touch $@

.rustc-cross: cargo
	+$(RUSTUP) set profile minimal
	+$(RUSTUP) default $(RUST_VERSION)
	+$(RUSTUP) target add --toolchain $(RUST_VERSION) $(RUST_TARGET)
	touch $@
