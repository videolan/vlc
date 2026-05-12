# matroska

MATROSKA_VERSION := 1.7.1
MATROSKA_URL := https://dl.matroska.org/downloads/libmatroska/libmatroska-$(MATROSKA_VERSION).tar.xz

PKGS += matroska

ifeq ($(call need_pkg,"libmatroska"),)
PKGS_FOUND += matroska
endif

DEPS_matroska = ebml $(DEPS_ebml)

$(TARBALLS)/libmatroska-$(MATROSKA_VERSION).tar.xz:
	$(call download_pkg,$(MATROSKA_URL),matroska)

.sum-matroska: libmatroska-$(MATROSKA_VERSION).tar.xz

matroska: libmatroska-$(MATROSKA_VERSION).tar.xz .sum-matroska
	$(UNPACK)
	$(call pkg_static,"libmatroska.pc.in")
	$(APPLY) $(SRC)/matroska/0001-KaxDefines-do-not-allow-infinite-sizes-on-all-Master.patch
	$(APPLY) $(SRC)/matroska/0001-KaxBlock-release-read-buffers-on-EndOfStream-error.patch
	$(APPLY) $(SRC)/matroska/0001-KaxBlock-rework-EBML-lacing-sizes-in-SCOPE_PARTIAL_D.patch
	$(APPLY) $(SRC)/matroska/0002-KaxBlock-throw-when-the-EBML-length-difference-gives.patch
	$(APPLY) $(SRC)/matroska/0003-KaxBlock-fix-reading-EBML-lacing-with-just-one-frame.patch
	$(APPLY) $(SRC)/matroska/0001-KaxBlock-expand-FrameNum-after-reading.patch
	$(MOVE)

.matroska: matroska toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) -DCMAKE_POLICY_VERSION_MINIMUM=3.5
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
