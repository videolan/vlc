# protobuf
PROTOBUF_VERSION := 3.4.1
PROTOBUF_URL := $(GITHUB)/google/protobuf/releases/download/v$(PROTOBUF_VERSION)/protobuf-cpp-$(PROTOBUF_VERSION).tar.gz

PKGS += protobuf
ifeq ($(call need_pkg, "protobuf-lite = $(PROTOBUF_VERSION)"),)
PKGS_FOUND += protobuf
endif

$(TARBALLS)/protobuf-$(PROTOBUF_VERSION)-cpp.tar.gz:
	$(call download_pkg,$(PROTOBUF_URL),protobuf)

.sum-protobuf: protobuf-$(PROTOBUF_VERSION)-cpp.tar.gz

DEPS_protobuf = zlib $(DEPS_zlib)

PROTOBUFVARS := DIST_LANG="cpp"

protobuf: protobuf-$(PROTOBUF_VERSION)-cpp.tar.gz .sum-protobuf
	$(UNPACK)
	$(RM) -Rf $(UNPACK_DIR)
	mv protobuf-$(PROTOBUF_VERSION) protobuf-$(PROTOBUF_VERSION)-cpp
	# don't build benchmarks and conformance
	sed -i.orig 's, conformance benchmarks,,' "$(UNPACK_DIR)/Makefile.am"
	sed -i.orig 's, benchmarks/Makefile conformance/Makefile,,' "$(UNPACK_DIR)/configure.ac"
	# don't use gmock or any sub project to configure
	sed -i.orig 's,AC_CONFIG_SUBDIRS,dnl AC_CONFIG_SUBDIRS,' "$(UNPACK_DIR)/configure.ac"
	# don't build protoc
	sed -i.orig 's,bin_PROGRAMS,#bin_PROGRAMS,' "$(UNPACK_DIR)/src/Makefile.am"
	sed -i.orig 's,BUILT_SOURCES,#BUILT_SOURCES,' "$(UNPACK_DIR)/src/Makefile.am"
	sed -i.orig 's,libprotobuf-lite.la libprotobuf.la libprotoc.la,libprotobuf-lite.la libprotobuf.la,' "$(UNPACK_DIR)/src/Makefile.am"
	# force include <algorithm>
	sed -i.orig 's,#ifdef _MSC_VER,#if 1,' "$(UNPACK_DIR)/src/google/protobuf/repeated_field.h"
	$(MOVE)

.protobuf: protobuf
	$(RECONF)
	cd $< && $(HOSTVARS) $(PROTOBUFVARS) ./configure $(HOSTCONF) --with-protoc="$(PROTOC)"
	$(MAKE) -C $< && $(MAKE) -C $< install
	touch $@
