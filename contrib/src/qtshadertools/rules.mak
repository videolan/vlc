# qtshadertools
# required for Qt5Compat, and for qtdeclarative.

QTSHADERTOOLS_VERSION_MAJOR := 6.6
QTSHADERTOOLS_VERSION := $(QTSHADERTOOLS_VERSION_MAJOR).2
QTSHADERTOOLS_URL := $(QT)/$(QTSHADERTOOLS_VERSION_MAJOR)/$(QTSHADERTOOLS_VERSION)/submodules/qtshadertools-everywhere-src-$(QTSHADERTOOLS_VERSION).tar.xz

DEPS_qtshadertools-tools := qt-tools $(DEPS_qt-tools)

DEPS_qtshadertools += qt $(DEPS_qt) spirv-tools
ifdef HAVE_CROSS_COMPILE
DEPS_qtshadertools += qtshadertools-tools $(DEPS_qtshadertools-tools)
endif
ifdef HAVE_WIN32
DEPS_qtshadertools += fxc2 $(DEPS_fxc2)
endif

ifdef HAVE_WIN32
PKGS += qtshadertools
endif
ifneq ($(findstring qt,$(PKGS)),)
PKGS_TOOLS += qtshadertools-tools
endif
PKGS_ALL += qtshadertools-tools

ifeq ($(call need_pkg,"Qt6ShaderTools >= 6.6"),)
PKGS_FOUND += qtshadertools
endif
ifndef HAVE_CROSS_COMPILE
PKGS_FOUND += qtshadertools-tools
else ifeq ($(call system_tool_majmin, qsb --version),$(QTSHADERTOOLS_VERSION_MAJOR))
PKGS_FOUND += qtshadertools-tools
endif

$(TARBALLS)/qtshadertools-everywhere-src-$(QTSHADERTOOLS_VERSION).tar.xz:
	$(call download,$(QTSHADERTOOLS_URL))

.sum-qtshadertools: qtshadertools-everywhere-src-$(QTSHADERTOOLS_VERSION).tar.xz

.sum-qtshadertools-tools: .sum-qtshadertools
	touch $@

QT_SHADETOOLS_CONFIG := -DCMAKE_TOOLCHAIN_FILE=$(PREFIX)/lib/cmake/Qt6/qt.toolchain.cmake -DQT_HOST_PATH=$(BUILDPREFIX)
ifdef ENABLE_PDB
QT_SHADETOOLS_CONFIG += -DCMAKE_BUILD_TYPE=RelWithDebInfo
else
QT_SHADETOOLS_CONFIG += -DCMAKE_BUILD_TYPE=Release
endif

QT_SHADETOOLS_NATIVE_CONFIG := -DCMAKE_TOOLCHAIN_FILE=$(BUILDPREFIX)/lib/cmake/Qt6/qt.toolchain.cmake

qtshadertools: qtshadertools-everywhere-src-$(QTSHADERTOOLS_VERSION).tar.xz .sum-qtshadertools
	$(UNPACK)
	$(APPLY) $(SRC)/qtshadertools/0001-Use-fxc2-through-wine-instead-of-fxc.patch
	$(APPLY) $(SRC)/qtshadertools/0001-Backport-a-change-to-SPIRV-Cross-to-add-a-missing-in.patch
	$(MOVE)

.qtshadertools-tools: BUILD_DIR=$</vlc_native
.qtshadertools-tools: qtshadertools
	$(CMAKECLEAN)
	$(BUILDVARS) $(CMAKE_NATIVE) $(QT_SHADETOOLS_NATIVE_CONFIG)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@

.qtshadertools: qtshadertools toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS) $(CMAKE) $(QT_SHADETOOLS_CONFIG)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
