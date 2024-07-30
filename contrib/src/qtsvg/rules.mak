# qtsvg

QTSVG_VERSION := $(QTBASE_VERSION)
QTSVG_URL := $(QT)/$(QTSVG_VERSION)/submodules/qtsvg-everywhere-src-$(QTSVG_VERSION).tar.xz

DEPS_qtsvg += qt $(DEPS_qt)

ifdef HAVE_WIN32
PKGS += qtsvg
endif

ifeq ($(call need_pkg,"Qt6Svg >= $(QTBASE_VERSION_MAJOR)"),)
PKGS_FOUND += qtsvg
endif

$(TARBALLS)/qtsvg-everywhere-src-$(QTSVG_VERSION).tar.xz:
	$(call download_pkg,$(QTSVG_URL),qt)

.sum-qtsvg: qtsvg-everywhere-src-$(QTSVG_VERSION).tar.xz

QTSVG_CONFIG := $(QT_CMAKE_CONFIG)
ifdef ENABLE_PDB
QTSVG_CONFIG += -DCMAKE_BUILD_TYPE=RelWithDebInfo
else
QTSVG_CONFIG += -DCMAKE_BUILD_TYPE=Release
endif

qtsvg: qtsvg-everywhere-src-$(QTSVG_VERSION).tar.xz .sum-qtsvg
	$(UNPACK)
	$(MOVE)

.qtsvg: qtsvg toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(QTSVG_CONFIG)
	+PATH="$(PATH):$(PREFIX)/bin" $(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
