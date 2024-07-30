# Qt5Compat

QT5COMPAT_VERSION := $(QTBASE_VERSION)
QT5COMPAT_URL := $(QT)/$(QT5COMPAT_VERSION)/submodules/qt5compat-everywhere-src-$(QT5COMPAT_VERSION).tar.xz

DEPS_qt5compat += qtdeclarative $(DEPS_qtdeclarative)

ifdef HAVE_WIN32
PKGS += qt5compat
endif

ifeq ($(call need_pkg,"Qt6Core5Compat >= $(QTBASE_VERSION_MAJOR)"),)
PKGS_FOUND += qt5compat
endif

$(TARBALLS)/qt5compat-everywhere-src-$(QT5COMPAT_VERSION).tar.xz:
	$(call download_pkg,$(QT5COMPAT_URL),qt)

.sum-qt5compat: qt5compat-everywhere-src-$(QT5COMPAT_VERSION).tar.xz

QT5_COMPAT_CONFIG := $(QT_CMAKE_CONFIG)
ifdef ENABLE_PDB
QT5_COMPAT_CONFIG += -DCMAKE_BUILD_TYPE=RelWithDebInfo
else
QT5_COMPAT_CONFIG += -DCMAKE_BUILD_TYPE=Release
endif

qt5compat: qt5compat-everywhere-src-$(QT5COMPAT_VERSION).tar.xz .sum-qt5compat
	$(UNPACK)
	$(APPLY) $(SRC)/qt5compat/0001-Revert-Auxiliary-commit-to-revert-individual-files-f.patch
	$(APPLY) $(SRC)/qt5compat/0002-Do-not-build-core5.patch
	$(MOVE)

.qt5compat: qt5compat toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(QT5_COMPAT_CONFIG)
	+PATH="$(PATH):$(PREFIX)/bin" $(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
