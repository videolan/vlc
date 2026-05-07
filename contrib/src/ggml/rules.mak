# ggml

GGML_VERSION := 0.10.0
GGML_URL := $(GITHUB)/ggml-org/ggml/archive/refs/tags/v$(GGML_VERSION).tar.gz

ifeq ($(call need_pkg,"ggml"),)
PKGS_FOUND += ggml
endif

$(TARBALLS)/ggml-$(GGML_VERSION).tar.gz:
	$(call download_pkg,$(GGML_URL),ggml)

.sum-ggml: ggml-$(GGML_VERSION).tar.gz

ggml: ggml-$(GGML_VERSION).tar.gz .sum-ggml
	$(UNPACK)
	# fix path to install ggml.pc
	sed -i.orig 's,DESTINATION share/pkgconfig,DESTINATION $${CMAKE_INSTALL_LIBDIR}/pkgconfig,' $(UNPACK_DIR)/CMakeLists.txt
	# add missing libraries
	sed -i.orig 's, -lggml$$, -lggml -lggml-base -lggml-cpu,' $(UNPACK_DIR)/ggml.pc.in
	$(MOVE)

GGML_CONF := \
	-DGGML_BUILD_TESTS=OFF \
	-DGGML_BUILD_EXAMPLES=OFF

.ggml: ggml toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(GGML_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
