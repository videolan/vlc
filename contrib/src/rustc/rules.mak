# rustc+cargo installation via rustup

RUST_VERSION=1.79.0

ifdef BUILD_RUST
PKGS_TOOLS += rustc

ifdef HAVE_CROSS_COMPILE
PKGS_TOOLS += rustc-cross
PKGS_ALL += rustc-cross
endif

ifneq ($(call system_tool_version, rustup --version, cat),)
RUSTUP = RUSTUP_HOME=$(RUSTUP_HOME) CARGO_HOME=$(CARGO_HOME) rustup
else
RUSTUP = . $(CARGO_HOME)/env && \
         RUSTUP_HOME=$(RUSTUP_HOME) CARGO_HOME=$(CARGO_HOME) rustup
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

.sum-rustc:
	touch $@

.sum-rustc-cross:
	touch $@

.rustc:
	+$(RUSTUP) set profile minimal
	+$(RUSTUP) default $(RUST_VERSION)
	touch $@

.rustc-cross:
	+$(RUSTUP) set profile minimal
	+$(RUSTUP) default $(RUST_VERSION)
	+$(RUSTUP) target add --toolchain $(RUST_VERSION) $(RUST_TARGET)
	touch $@
