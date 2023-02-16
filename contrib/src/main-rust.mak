# Cargo/Rust specific makefile rules for VLC 3rd party libraries ("contrib")
# Copyright (C) 2003-2020 the VideoLAN team
#
# This file is under the same license as the vlc package.

ifdef HAVE_WIN32
ifndef HAVE_WINSTORE
ifeq ($(HOST),i686-w64-mingw32)
RUST_TARGET = i686-pc-windows-gnu # ARCH is i386
else ifeq ($(HOST),x86_64-w64-mingw32)
RUST_TARGET = $(ARCH)-pc-windows-gnu
else
# Not supported on armv7/aarch64 yet
endif
endif
else ifdef HAVE_ANDROID
RUST_TARGET = $(HOST)
else ifdef HAVE_IOS
ifneq ($(ARCH),arm) # iOS 32bit is Tier 3
ifneq ($(ARCH),i386) # iOS 32bit is Tier 3
ifndef HAVE_TVOS # tvOS is Tier 3
RUST_TARGET = $(ARCH)-apple-ios
endif
endif
endif
else ifdef HAVE_MACOSX
ifneq ($(ARCH),aarch64) # macOS ARM-64 is unsupported
RUST_TARGET = $(ARCH)-apple-darwin
endif
else ifdef HAVE_SOLARIS
RUST_TARGET = x86_64-sun-solaris
else ifdef HAVE_LINUX
ifeq ($(HOST),arm-linux-gnueabihf)
RUST_TARGET = arm-unknown-linux-gnueabihf #add eabihf
else
ifeq ($(HOST),riscv64-linux-gnu)
RUST_TARGET = riscv64gc-unknown-linux-gnu
else
RUST_TARGET = $(ARCH)-unknown-linux-gnu
endif
endif
else ifdef HAVE_BSD
RUST_TARGET = $(HOST)
endif

# For now, VLC don't support Tier 3 platforms (ios 32bit, tvOS).
# Supporting a Tier 3 platform means building an untested rust toolchain.
# TODO Let's hope tvOS move from Tier 3 to Tier 2 before the VLC 4.0 release.
ifneq ($(RUST_TARGET),)
BUILD_RUST="1"
endif

RUSTUP_HOME= $(BUILDBINDIR)/.rustup
CARGO_HOME = $(BUILDBINDIR)/.cargo
CARGO_ENV = TARGET_CC="$(CC)" TARGET_AR="$(AR)" \
	TARGET_CFLAGS="$(CFLAGS)" RUSTFLAGS="-C panic=abort -C opt-level=z"

CARGO = . $(CARGO_HOME)/env && \
		RUSTUP_HOME=$(RUSTUP_HOME) CARGO_HOME=$(CARGO_HOME) $(CARGO_ENV) cargo

CARGO_INSTALL_ARGS = --target=$(RUST_TARGET) --prefix=$(PREFIX) \
	--library-type staticlib --release

# Use the .cargo-vendor source if present, otherwise use crates.io
CARGO_INSTALL_ARGS += \
	$(shell test -d $<-vendor && echo --frozen --offline || echo --locked)

CARGO_INSTALL = $(CARGO) install $(CARGO_INSTALL_ARGS)

CARGOC_INSTALL = $(CARGO) capi install $(CARGO_INSTALL_ARGS)

download_vendor = \
	$(call download,$(CONTRIB_VIDEOLAN)/$(2)/$(1)) || (\
               echo "" && \
               echo "WARNING: cargo vendor archive for $(1) not found" && \
               echo "" && \
               touch $@);

# Extract and move the vendor archive if the checksum is valid. Succeed even in
# case of error (download or checksum failed). In that case, the cargo-vendor
# archive won't be used (crates.io will be used directly).
.%-vendor: $(SRC)/%-vendor/SHA512SUMS
	$(RM) -R $(patsubst .%,%,$@)
	-$(call checksum,$(SHA512SUM),SHA512,.) \
		$(foreach f,$(filter %.tar.bz2,$^), && tar $(TAR_VERBOSE)xjfo $(f) && \
		  mv $(patsubst %.tar.bz2,%,$(notdir $(f))) $(patsubst .%,%,$@))
	touch $@

CARGO_VENDOR_SETUP = \
	if test -d $@-vendor; then \
		mkdir -p $(UNPACK_DIR)/.cargo; \
		echo "[source.crates-io]" > $(UNPACK_DIR)/.cargo/config.toml; \
		echo "replace-with = \"vendored-sources\"" >> $(UNPACK_DIR)/.cargo/config.toml; \
		echo "[source.vendored-sources]" >> $(UNPACK_DIR)/.cargo/config.toml; \
		echo "directory = \"../$@-vendor\"" >> $(UNPACK_DIR)/.cargo/config.toml; \
		echo "Using cargo vendor archive for $(UNPACK_DIR)"; \
	fi;
