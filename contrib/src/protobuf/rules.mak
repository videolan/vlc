# protobuf
PROTOBUF_VERSION := 3.1.0
PROTOBUF_URL := https://github.com/google/protobuf/releases/download/v$(PROTOBUF_VERSION)/protobuf-cpp-$(PROTOBUF_VERSION).tar.gz

PKGS += protobuf
ifeq ($(call need_pkg, "protobuf-lite >= 3.1.0 protobuf-lite < 3.2.0"),)
PKGS_FOUND += protobuf
else
# check we have a matching protoc to use
PROTOC_ABSPATH = $(shell PATH="$(PATH)" which protoc)
ifeq ($(PROTOC_ABSPATH),)
PROTOC = $(error protoc not found (search path: $(PATH)))
else
# make sure the installed protoc is compatible with the version we want to build
SYS_PROTOC_VER = $(shell $(PROTOC_ABSPATH) --version)
SYS_PROTOC_VERSION = $(word $(words $(SYS_PROTOC_VER)) , $(SYS_PROTOC_VER))
ifneq ($(PROTOBUF_VERSION),$(SYS_PROTOC_VERSION))
PROTOC = $(error protoc system version $(SYS_PROTOC_VERSION) and required version $(PROTOBUF_VERSION) do not match)
else
PROTOC = $(PROTOC_ABSPATH)
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

protobuf: protobuf-$(PROTOBUF_VERSION)-cpp.tar.gz .sum-protobuf
	$(UNPACK)
	mv protobuf-$(PROTOBUF_VERSION) protobuf-$(PROTOBUF_VERSION)-cpp
	$(APPLY) $(SRC)/protobuf/dont-build-protoc.patch
	$(APPLY) $(SRC)/protobuf/protobuf-win32.patch
	$(MOVE)

.protobuf: protobuf
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --with-protoc="$(PROTOC)"
	cd $< && $(MAKE) && $(MAKE) install
	touch $@
