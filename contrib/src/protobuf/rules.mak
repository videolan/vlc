# protobuf
PROTOBUF_VERSION := 3.1.0
PROTOBUF_URL := $(GITHUB)/google/protobuf/releases/download/v$(PROTOBUF_VERSION)/protobuf-cpp-$(PROTOBUF_VERSION).tar.gz

ifndef HAVE_TVOS
PKGS += protobuf protoc
PKGS_TOOLS += protoc
endif # !HAVE_TVOS
PKGS_ALL += protoc
ifeq ($(call need_pkg, "protobuf-lite = $(PROTOBUF_VERSION)"),)
PKGS_FOUND += protobuf
ifndef HAVE_CROSS_COMPILE
PKGS_FOUND += protoc
endif
endif

ifeq ($(shell $(HOST)-protoc --version 2>/dev/null | head -1 | sed s/'.* '//),$(PROTOBUF_VERSION))
PKGS_FOUND += protoc
endif
ifeq ($(shell protoc --version 2>/dev/null | head -1 | sed s/'.* '//),$(PROTOBUF_VERSION))
PKGS_FOUND += protoc
endif

$(TARBALLS)/protobuf-$(PROTOBUF_VERSION)-cpp.tar.gz:
	$(call download_pkg,$(PROTOBUF_URL),protobuf)

$(TARBALLS)/protoc-$(PROTOBUF_VERSION)-cpp.tar.gz: $(TARBALLS)/protobuf-$(PROTOBUF_VERSION)-cpp.tar.gz
	$(RM) -R "$@"
	cp "$<" "$@"

.sum-protobuf: protobuf-$(PROTOBUF_VERSION)-cpp.tar.gz

DEPS_protobuf = zlib $(DEPS_zlib)

PROTOBUFVARS := DIST_LANG="cpp"
PROTOCVARS := DIST_LANG="cpp"

PROTOCCONF += --enable-static --disable-shared

.sum-protoc: .sum-protobuf
	touch $@

protoc: protoc-$(PROTOBUF_VERSION)-cpp.tar.gz .sum-protoc
	$(RM) -Rf $@ $(UNPACK_DIR) && mkdir -p $(UNPACK_DIR)
	tar xvzfo "$<" -C $(UNPACK_DIR) --strip-components=1
	$(APPLY) $(SRC)/protobuf/protobuf-disable-gmock.patch
	$(APPLY) $(SRC)/protobuf/protobuf-fix-build.patch
	$(APPLY) $(SRC)/protobuf/include-algorithm.patch
	$(APPLY) $(SRC)/protobuf/protobuf-no-mingw-pthread.patch
	$(MOVE)

.protoc: protoc
	$(RECONF)
	$(MAKEBUILDDIR)
	cd $</_build && $(BUILDVARS) ../configure $(BUILDTOOLCONF) $(PROTOCVARS) $(PROTOCCONF)
	+$(MAKEBUILD) && $(MAKEBUILD) install
	touch $@

protobuf: protobuf-$(PROTOBUF_VERSION)-cpp.tar.gz .sum-protobuf
	$(UNPACK)
	mv protobuf-$(PROTOBUF_VERSION) protobuf-$(PROTOBUF_VERSION)-cpp
	$(APPLY) $(SRC)/protobuf/protobuf-disable-gmock.patch
	$(APPLY) $(SRC)/protobuf/dont-build-protoc.patch
	$(APPLY) $(SRC)/protobuf/protobuf-fix-build.patch
	$(APPLY) $(SRC)/protobuf/include-algorithm.patch
	$(APPLY) $(SRC)/protobuf/protobuf-no-mingw-pthread.patch
	$(MOVE)

.protobuf: protobuf
	$(RECONF)
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(PROTOBUFVARS)
	+$(MAKEBUILD) && $(MAKEBUILD) install
	touch $@
