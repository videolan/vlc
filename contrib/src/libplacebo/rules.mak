# libplacebo

PLACEBO_VERSION := 0.1.0
PLACEBO_URL := https://github.com/haasn/libplacebo/archive/v$(PLACEBO_VERSION).tar.gz
PLACEBO_ARCHIVE = libplacebo-$(PLACEBO_VERSION).tar.gz

PLACEBOCONF := --prefix="$(PREFIX)" \
	--libdir lib \
	--default-library static

$(TARBALLS)/$(PLACEBO_ARCHIVE):
	$(call download_pkg,$(PLACEBO_URL),libplacebo)

.sum-libplacebo: $(PLACEBO_ARCHIVE)

libplacebo: $(PLACEBO_ARCHIVE) .sum-libplacebo
	$(UNPACK)
	$(MOVE)

.libplacebo: libplacebo
	cd $< && rm -rf ./build
	cd $< && $(HOSTVARS) meson $(PLACEBOCONF) build
	cd $< && cd build && ninja install
	touch $@
