# protobuf
PROTOBUF_VERSION := 2.6.1
PROTOBUF_URL := https://github.com/google/protobuf/releases/download/v$(PROTOBUF_VERSION)/protobuf-$(PROTOBUF_VERSION).tar.gz

PKGS += protobuf
ifeq ($(call need_pkg,"protobuf"),)
PKGS_FOUND += protobuf
endif

$(TARBALLS)/protobuf-$(PROTOBUF_VERSION).tar.gz:
	$(call download,$(PROTOBUF_URL))

.sum-protobuf: protobuf-$(PROTOBUF_VERSION).tar.gz

DEPS_protobuf = zlib $(DEPS_zlib)

protobuf: protobuf-$(PROTOBUF_VERSION).tar.gz .sum-protobuf
	$(UNPACK)
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/protobuf/win32.patch
endif
	$(MOVE)

.protobuf: protobuf
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --with-protoc=protoc
	cd $< && $(MAKE) && $(MAKE) install
	touch $@
