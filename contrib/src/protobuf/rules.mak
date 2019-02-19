# protobuf
PROTOBUF_VERSION := 3.1.0
PROTOBUF_URL := https://github.com/google/protobuf/releases/download/v$(PROTOBUF_VERSION)/protobuf-cpp-$(PROTOBUF_VERSION).tar.gz

PKGS += protobuf
ifeq ($(call need_pkg, "protobuf-lite >= 3.1.0 protobuf-lite < 3.2.0"),)
PKGS_FOUND += protobuf
else
ifeq ($(findstring protobuf,$(PKGS_DISABLE)),)
# check we have a matching protoc to use
PROTOC = $(shell PATH="$(PATH)" which protoc)
ifeq ($(PROTOC),)
PROTOC = $(error protoc not found in PATH $(PATH) - $(SYS_PROTOC) - $(PROTOC))
else
# make sure the installed protoc is compatible with the version we want to build
SYS_PROTOC_VER = $(shell $(PROTOC) --version)
SYS_PROTOC = $(word $(words $(SYS_PROTOC_VER)) , $(SYS_PROTOC_VER))
ifneq ($(PROTOBUF_VERSION),$(SYS_PROTOC))
PROTOC = $(error $(PROTOC) version $(SYS_PROTOC) doesn't match the protobuf $(PROTOBUF_VERSION) we're building)
endif
endif
endif
endif

$(TARBALLS)/protobuf-$(PROTOBUF_VERSION)-cpp.tar.gz:
	$(call download_pkg,$(PROTOBUF_URL),protobuf)

.sum-protobuf: protobuf-$(PROTOBUF_VERSION)-cpp.tar.gz

DEPS_protobuf = zlib $(DEPS_zlib)
ifdef HAVE_WIN32
DEPS_protobuf += pthreads $(DEPS_pthreads)
endif

PROTOBUF_CONF = -DBUILD_SHARED_LIBS=OFF -Dprotobuf_BUILD_TESTS=OFF -Dprotobuf_BUILD_EXAMPLES=OFF

protobuf: protobuf-$(PROTOBUF_VERSION)-cpp.tar.gz .sum-protobuf
	$(UNPACK)
	mv protobuf-$(PROTOBUF_VERSION) protobuf-$(PROTOBUF_VERSION)-cpp
	$(APPLY) $(SRC)/protobuf/protobuf-win32.patch
	$(APPLY) $(SRC)/protobuf/protobuf-cmake-pkgconfig.patch
	$(MOVE)

.protobuf: protobuf toolchain.cmake
	cd $</ && mkdir -p $(HOST)
	cd $</ && $(HOSTVARS_PIC) $(CMAKE) -S cmake -B $(HOST) $(PROTOBUF_CONF)
	cd $</ && $(MAKE) -C $(HOST) && $(MAKE) -C $(HOST) install
	touch $@
