# Cargo/Rust specific makefile rules for VLC 3rd party libraries ("contrib")
# Copyright (C) 2003-2020 the VideoLAN team
#
# This file is under the same license as the vlc package.

# default in Debian bookworm
RUST_VERSION_MIN=1.63.0

ifdef HAVE_WIN32
ifdef HAVE_WINSTORE
RUST_TARGET_FLAGS += --uwp
endif
ifdef HAVE_UCRT
# does not work as Tier 2 before that
RUST_VERSION_MIN=1.79.0
endif
endif

ifdef HAVE_CLANG
RUST_TARGET_FLAGS += --llvm
endif

ifdef HAVE_DARWIN_OS
ifdef HAVE_TVOS
RUST_TARGET_FLAGS += --darwin=tvos
else ifdef HAVE_WATCHOS
RUST_TARGET_FLAGS += --darwin=watchos
else ifdef HAVE_XROS
RUST_TARGET_FLAGS += --darwin=xros
else ifdef HAVE_IOS
RUST_TARGET_FLAGS += --darwin=ios
else
RUST_TARGET_FLAGS += --darwin=macos
endif
ifdef HAVE_SIMULATOR
RUST_TARGET_FLAGS += --simulator
endif
endif

ifneq ($(findstring darwin,$(BUILD)),)
RUST_BUILD_FLAGS += --darwin=macos
endif

RUST_TARGET := $(shell $(SRC)/get-rust-target.sh $(RUST_TARGET_FLAGS) $(HOST) 2>/dev/null || echo FAIL)
RUST_HOST :=  $(shell $(SRC)/get-rust-target.sh $(RUST_BUILD_FLAGS) $(BUILD) 2>/dev/null || echo FAIL)

ifneq ($(RUST_HOST),FAIL)
# For now, VLC don't support Tier 3 platforms (ios 32bit, tvOS).
# Supporting a Tier 3 platform means building an untested rust toolchain.
# TODO Let's hope tvOS move from Tier 3 to Tier 2 before the VLC 4.0 release.
ifneq ($(RUST_TARGET),FAIL)
ifeq ($(call system_tool_matches_min, echo 'fn main() {}' | rustc --target=$(RUST_TARGET) --emit=dep-info - -o /dev/null 2>/dev/null && rustc --version,$(RUST_VERSION_MIN)),)
BUILD_RUST="1"
endif
endif
endif

RUSTFLAGS := -C panic=abort
ifndef WITH_OPTIMIZATION
CARGO_PROFILE := "dev"
RUSTFLAGS += -C opt-level=1
else
CARGO_PROFILE := "release"
RUSTFLAGS += -C opt-level=z
endif

ifdef HAVE_EMSCRIPTEN
RUSTFLAGS += -C target-feature=+atomics
endif

CARGO_ENV = TARGET_CC="$(CC)" TARGET_AR="$(AR)" TARGET_RANLIB="$(RANLIB)" \
	TARGET_CFLAGS="$(CFLAGS)" RUSTFLAGS="$(RUSTFLAGS)"
CARGO_ENV_NATIVE = TARGET_CC="$(BUILDCC)" TARGET_AR="$(BUILDAR)" TARGET_RANLIB="$(BUILDRANLIB)" \
	TARGET_CFLAGS="$(BUILDCFLAGS)"

ifdef VLC_DEPLOYMENT_TARGET
ifeq ($(HAVE_MACOSX),1)
CARGO_ENV += MACOSX_DEPLOYMENT_TARGET=$(VLC_DEPLOYMENT_TARGET)
else ifeq ($(HAVE_TVOS),1)
CARGO_ENV += TVOS_DEPLOYMENT_TARGET=$(VLC_DEPLOYMENT_TARGET)
else ifeq ($(HAVE_WATCHOS),1)
CARGO_ENV += WATCHOS_DEPLOYMENT_TARGET=$(VLC_DEPLOYMENT_TARGET)
else ifeq ($(HAVE_XROS),1)
CARGO_ENV += XROS_DEPLOYMENT_TARGET=$(VLC_DEPLOYMENT_TARGET)
else ifeq ($(HAVE_IOS),1)
CARGO_ENV += IPHONEOS_DEPLOYMENT_TARGET=$(VLC_DEPLOYMENT_TARGET)
endif
endif

CARGO = $(CARGO_ENV) cargo
CARGO_NATIVE = $(CARGO_ENV_NATIVE) cargo

CARGO_INSTALL_ARGS = --target=$(RUST_TARGET) --prefix=$(PREFIX) \
	--library-type staticlib --profile=$(CARGO_PROFILE)

ifeq ($(V),1)
CARGO_INSTALL_ARGS += --verbose
endif

# Use the .cargo-vendor source if present, otherwise use crates.io
CARGO_INSTALL_ARGS += \
	$(shell test -d $<-vendor && echo --frozen --offline || echo --locked)

CARGO_INSTALL = $(CARGO) install $(CARGO_INSTALL_ARGS)

CARGOC_INSTALL = $(CARGO) capi install $(CARGO_INSTALL_ARGS)

download_vendor = \
	rm $@.skip-hash; \
	$(call download,$(CONTRIB_VIDEOLAN)/$(2)/$(1)) || (\
               rm $@; \
               $(RM) -R vendor-$(2)-build; \
               mkdir -p vendor-$(2)-build && \
               tar xzfo $(TARBALLS)/$(3) -C vendor-$(2)-build --strip-components=1 && \
               cd vendor-$(2)-build && \
               $(CARGO_NATIVE) vendor --locked $(patsubst %.tar,%,$(basename $(notdir $(1)))) && \
               tar -jcf $(1) $(patsubst %.tar,%,$(basename $(notdir $(1)))) && \
               cd .. && \
               install vendor-$(2)-build/$(1) "$(TARBALLS)" && \
               $(RM) -R vendor-$(2)-build && \
               touch $@.skip-hash && \
               touch $@) || (\
               rm $@);

.sum-vendor-%: $(SRC)/%/vendor-SHA512SUMS
	$(foreach f,$(filter %.tar.bz2,$^), if test ! -f $(f).skip-hash; then \
		$(call checksum,$(SHA512SUM),vendor-SHA512,.sum-vendor-); \
	fi)
	touch $@

# Extract and move the vendor archive if the checksum is valid. Succeed even in
# case of error (download or checksum failed). In that case, the cargo-vendor
# archive won't be used (crates.io will be used directly).
.%-vendor: $(SRC)/%-vendor/SHA512SUMS
	$(RM) -R $(patsubst .%,%,$@)
	-$(call checksum,$(SHA512SUM),SHA512,.) \
		$(foreach f,$(filter %.tar.bz2,$^), && tar $(TAR_VERBOSE)xjfo $(f) && \
		  mv $(patsubst %.tar.bz2,%,$(notdir $(f))) $(patsubst .%,%,$@))
	touch $@

cargo_vendor_setup = \
	mkdir -p $1/.cargo; \
	echo "[source.crates-io]" > $1/.cargo/config.toml; \
	echo "replace-with = \"vendored-sources\"" >> $1/.cargo/config.toml; \
	echo "[source.vendored-sources]" >> $1/.cargo/config.toml; \
	echo "directory = \"../$2-vendor\"" >> $1/.cargo/config.toml; \
	echo "Using cargo vendor archive for $2";
