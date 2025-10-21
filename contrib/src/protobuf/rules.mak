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

PROTOBUF_COMMON_CONF := -Dprotobuf_BUILD_TESTS=OFF -Dprotobuf_DEBUG_POSTFIX:STRING= -DCMAKE_POLICY_VERSION_MINIMUM=3.5
PROTOBUF_CONF := $(PROTOBUF_COMMON_CONF)

protobuf: protobuf-$(PROTOBUF_VERSION)-cpp.tar.gz .sum-protobuf
	$(UNPACK)
	$(RM) -Rf $(UNPACK_DIR)
	mv protobuf-$(PROTOBUF_VERSION) protobuf-$(PROTOBUF_VERSION)-cpp
	$(APPLY) $(SRC)/protobuf/0001-don-t-build-and-install-protoc-libprotoc.patch
	# don't build libprotoc
	sed -i.orig -e 's,include(libprotoc,#include(libprotoc,' $(UNPACK_DIR)/cmake/CMakeLists.txt
	# don't build protoc
	sed -i.orig -e 's,include(protoc,#include(protoc,' $(UNPACK_DIR)/cmake/CMakeLists.txt
	# force include <algorithm>
	sed -i.orig 's,#ifdef _MSC_VER,#if 1,' "$(UNPACK_DIR)/src/google/protobuf/repeated_field.h"
	$(MOVE)

.protobuf: protobuf toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) -S $</cmake $(PROTOBUF_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
