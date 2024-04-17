# protobuf
PROTOBUF_MAJPACKAGE := 3
PROTOBUF_MAJVERSION := 21
PROTOBUF_REVISION := 1
PROTOBUF_VERSION := $(PROTOBUF_MAJVERSION).$(PROTOBUF_REVISION)
PROTOBUF_PACKAGE := $(PROTOBUF_MAJPACKAGE).$(PROTOBUF_MAJVERSION).$(PROTOBUF_REVISION)
PROTOBUF_URL := $(GITHUB)/protocolbuffers/protobuf/releases/download/v$(PROTOBUF_VERSION)/protobuf-cpp-$(PROTOBUF_PACKAGE).tar.gz

ifndef HAVE_TVOS
PKGS += protobuf protoc
PKGS_TOOLS += protoc
PKGS.tools += protoc
endif # !HAVE_TVOS
PKGS_ALL += protoc
ifeq ($(call need_pkg, "protobuf-lite = $(PROTOBUF_VERSION)"),)
PKGS_FOUND += protobuf
ifndef HAVE_CROSS_COMPILE
PKGS_FOUND += protoc
endif
endif
PKGS.tools.protoc.path = $(BUILDBINDIR)/protoc

ifeq ($(call system_tool_majmin, protoc --version),$(PROTOBUF_MAJVERSION))
PKGS_FOUND += protoc
endif

$(TARBALLS)/protobuf-$(PROTOBUF_PACKAGE).tar.gz:
	$(call download_pkg,$(PROTOBUF_URL),protobuf)

$(TARBALLS)/protoc-$(PROTOBUF_VERSION)-cpp.tar.gz: $(TARBALLS)/protobuf-$(PROTOBUF_PACKAGE).tar.gz
	$(RM) -R "$@"
	cp "$<" "$@"

.sum-protobuf: protobuf-$(PROTOBUF_PACKAGE).tar.gz

DEPS_protobuf = zlib $(DEPS_zlib)

PROTOBUF_COMMON_CONF := -Dprotobuf_BUILD_TESTS=OFF -Dprotobuf_DEBUG_POSTFIX:STRING=
PROTOBUF_CONF := $(PROTOBUF_COMMON_CONF) -Dprotobuf_BUILD_PROTOC_BINARIES=OFF
PROTOC_CONF := $(PROTOBUF_COMMON_CONF) -Dprotobuf_BUILD_PROTOC_BINARIES=ON

.sum-protoc: .sum-protobuf
	touch $@

protoc: protoc-$(PROTOBUF_VERSION)-cpp.tar.gz .sum-protoc
	# extract in a different directory as it may run at the same time as the protobug extraction
	$(RM) -Rf $@ $(UNPACK_DIR) && mkdir -p $(UNPACK_DIR)
	tar $(TAR_VERBOSE)xzfo "$<" -C $(UNPACK_DIR) --strip-components=1
	$(APPLY) $(SRC)/protobuf/0001-Fix-9947-make-the-ABI-identical-between-debug-and-no.patch
	# add a dummy install command to disable some installation
	sed -i.old '1s;^;function (noinstall ...)\nendfunction()\n;' $(UNPACK_DIR)/cmake/install.cmake
	# don't install pkg-config files (on top of the target ones)
	sed -i.orig -e 's,install(FILES ,noinstall(FILES ,' $(UNPACK_DIR)/cmake/install.cmake
	# don't install cmake exports/targets/folders except protoc
	sed -i.orig -e 's,install(EXPORT ,noinstall(EXPORT ,' $(UNPACK_DIR)/cmake/install.cmake
	sed -i.orig -e 's,install(DIRECTORY ,noinstall(DIRECTORY ,' $(UNPACK_DIR)/cmake/install.cmake
	sed -i.orig -e 's,install(TARGETS ,noinstall(TARGETS ,' $(UNPACK_DIR)/cmake/install.cmake
	sed -i.orig -e 's,noinstall(TARGETS protoc,install(TARGETS protoc,' $(UNPACK_DIR)/cmake/install.cmake
	# disable libprotobuf-lite
	# sed -i.orig -e 's,libprotobuf-lite, ,' $(UNPACK_DIR)/cmake/install.cmake
	# sed -i.orig -e 's,include(libprotobuf-lite,#include(libprotobuf-lite,' $(UNPACK_DIR)/cmake/CMakeLists.txt
	$(MOVE)

.protoc: BUILD_DIR=$</vlc_native
.protoc: protoc
	$(CMAKECLEAN)
	$(BUILDVARS) $(CMAKE_NATIVE) $(PROTOC_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@

protobuf: protobuf-$(PROTOBUF_PACKAGE).tar.gz .sum-protobuf
	$(UNPACK)
	$(APPLY) $(SRC)/protobuf/0001-Fix-9947-make-the-ABI-identical-between-debug-and-no.patch
	# add a dummy install command to disable some installation
	sed -i.old '1s;^;function (noinstall ...)\nendfunction()\n;' $(UNPACK_DIR)/cmake/install.cmake
	# don't build libprotoc
	sed -i.orig -e 's,include(libprotoc,#include(libprotoc,' $(UNPACK_DIR)/cmake/CMakeLists.txt
	# don't build protoc
	sed -i.orig -e 's,include(protoc,#include(protoc,' $(UNPACK_DIR)/cmake/CMakeLists.txt
	# don't install libprotoc
	sed -i.orig -e 's, libprotoc protoc, ,' $(UNPACK_DIR)/cmake/install.cmake
	sed -i.orig -e 's, libprotoc, ,' $(UNPACK_DIR)/cmake/install.cmake
	# don't install protoc
	sed -i.orig -e 's,install(TARGETS protoc,noinstall(TARGETS protoc,' $(UNPACK_DIR)/cmake/install.cmake
	# force include <algorithm>
	sed -i.orig 's,#ifdef _MSC_VER,#if 1,' "$(UNPACK_DIR)/src/google/protobuf/repeated_field.h"
	$(MOVE)

.protobuf: protobuf toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS) $(CMAKE) $(PROTOBUF_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
