# qtshadertools
# required for Qt5Compat, and for qtdeclarative.

QTSHADERTOOLS_VERSION_MAJOR := 6.6
QTSHADERTOOLS_VERSION := $(QTSHADERTOOLS_VERSION_MAJOR).2
QTSHADERTOOLS_URL := $(QT)/$(QTSHADERTOOLS_VERSION_MAJOR)/$(QTSHADERTOOLS_VERSION)/submodules/qtshadertools-everywhere-src-$(QTSHADERTOOLS_VERSION).tar.xz

DEPS_qtshadertools += qt $(DEPS_qt) spirv-tools $(DEPS_spirv-tools)
ifdef HAVE_WIN32
DEPS_qtshadertools += fxc2 $(DEPS_fxc2)
endif

ifdef HAVE_WIN32
#PKGS += qtshadertools
endif

ifeq ($(call need_pkg,"Qt6ShaderTools >= 6.6"),)
PKGS_FOUND += qtshadertools
endif

$(TARBALLS)/qtshadertools-everywhere-src-$(QTSHADERTOOLS_VERSION).tar.xz:
	$(call download,$(QTSHADERTOOLS_URL))

.sum-qtshadertools: qtshadertools-everywhere-src-$(QTSHADERTOOLS_VERSION).tar.xz

qtshadertools: qtshadertools-everywhere-src-$(QTSHADERTOOLS_VERSION).tar.xz .sum-qtshadertools
	$(UNPACK)
	$(APPLY) $(SRC)/qtshadertools/0001-Use-fxc2-through-wine-instead-of-fxc.patch
	$(MOVE)

.qtshadertools: qtshadertools toolchain.cmake
ifdef HAVE_CROSS_COMPILE
	# Native
	$(CMAKECLEAN)
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && $(BUILDVARS) $(BUILDPREFIX)/bin/qt-configure-module $(BUILD_SRC)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)

	# qsb
	ln -sf $(BUILDPREFIX)/bin/qsb $(PREFIX)/bin/qsb
endif
	$(CMAKECLEAN)
	mkdir -p $(BUILD_DIR)
	+cd $(BUILD_DIR) && $(PREFIX)/bin/qt-configure-module $(BUILD_SRC)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)

	touch $@
