# protobuf
PROTOBUF_VERSION := 2.6.0
PROTOBUF_URL := https://protobuf.googlecode.com/svn/rc/protobuf-$(PROTOBUF_VERSION).tar.bz2

PKGS += protobuf
ifeq ($(call need_pkg,"protobuf"),)
PKGS_FOUND += protobuf
endif

$(TARBALLS)/protobuf-$(PROTOBUF_VERSION).tar.bz2:
	$(call download,$(PROTOBUF_URL))

.sum-protobuf: protobuf-$(PROTOBUF_VERSION).tar.bz2

DEPS_protobuf = zlib $(DEPS_zlib)

protobuf: protobuf-$(PROTOBUF_VERSION).tar.bz2 .sum-protobuf
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
