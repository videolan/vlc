# cargo-c installation via cargo

CARGOC_VERSION := 0.9.29

ifdef BUILD_RUST
# PKGS_TOOLS += cargo-c

ifneq ($(call system_tool_majmin, cargo-capi --version),)
PKGS_FOUND += cargo-c
endif

endif

.sum-cargo-c:
	touch $@

# Test if we can use the host libssl library
ifeq ($(shell unset PKG_CONFIG_LIBDIR PKG_CONFIG_PATH; \
	pkg-config "openssl >= 1.0.1" 2>/dev/null || \
	pkg-config "libssl >= 2.5" 2>/dev/null || echo FAIL),)
CARGOC_FEATURES=
else
# Otherwise, let cargo build and statically link its own openssl
CARGOC_FEATURES=--features=cargo/vendored-openssl
endif

.cargo-c:
	+unset PKG_CONFIG_LIBDIR PKG_CONFIG_PATH CFLAGS CPPFLAGS LDFLAGS; \
		$(CARGO_NATIVE) install --target=$(RUST_HOST) --locked $(CARGOC_FEATURES) cargo-c --version $(CARGOC_VERSION)
	touch $@
