# cargo/cargo-c installation via rustup

RUSTUP_VERSION=1.22.1
RUSTUP_URL=https://github.com/rust-lang/rustup/archive/$(RUSTUP_VERSION).tar.gz

RUSTUP = . $(CARGO_HOME)/env && \
	RUSTUP_HOME=$(RUSTUP_HOME) CARGO_HOME=$(CARGO_HOME) rustup

$(TARBALLS)/rustup-$(RUSTUP_VERSION).tar.gz:
	$(call download_pkg,$(RUSTUP_URL),rustup)

.sum-cargo: rustup-$(RUSTUP_VERSION).tar.gz

rustup: rustup-$(RUSTUP_VERSION).tar.gz .sum-cargo
	$(UNPACK)
	$(MOVE)

# When needed (when we have a Rust dependency not using cargo-c), the cargo-c
# installation should go in a different package
.cargo: rustup
	cd $< && RUSTUP_INIT_SKIP_PATH_CHECK=yes \
	  RUSTUP_HOME=$(RUSTUP_HOME) CARGO_HOME=$(CARGO_HOME) \
	  ./rustup-init.sh --no-modify-path -y
	$(RUSTUP) default stable
	$(RUSTUP) target add $(RUST_TARGET)
	unset PKG_CONFIG_LIBDIR PKG_CONFIG_PATH CFLAGS CPPFLAGS LDFLAGS; \
		$(CARGO) install cargo-c
	touch $@
