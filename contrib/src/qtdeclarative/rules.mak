# QtDeclarative

QTDECLARATIVE_VERSION_MAJOR := 6.6
QTDECLARATIVE_VERSION := $(QTDECLARATIVE_VERSION_MAJOR).2
QTDECLARATIVE_URL := $(QT)/$(QTDECLARATIVE_VERSION_MAJOR)/$(QTDECLARATIVE_VERSION)/submodules/qtdeclarative-everywhere-src-$(QTDECLARATIVE_VERSION).tar.xz

DEPS_qtdeclarative-tools := qt-tools $(DEPS_qt-tools) qtshadertools-tools $(DEPS_qtshadertools-tools)

DEPS_qtdeclarative = qt $(DEPS_qt) qtshadertools $(DEPS_qtshadertools)
ifdef HAVE_CROSS_COMPILE
DEPS_qtdeclarative += qtdeclarative-tools $(DEPS_qtdeclarative-tools) qtshadertools-tools $(DEPS_qtshadertools-tools) spirv-tools $(DEPS_spirv-tools)
endif

ifdef HAVE_WIN32
PKGS += qtdeclarative
endif
ifneq ($(findstring qt,$(PKGS)),)
PKGS_TOOLS += qtdeclarative-tools
endif
PKGS_ALL += qtdeclarative-tools

ifeq ($(call need_pkg,"Qt6Qml >= 6.6 Qt6Quick >= 6.6 Qt6QuickControls2 >= 6.6 Qt6QuickDialogs2 >= 6.6 Qt6QuickLayouts >= 6.6"),)
PKGS_FOUND += qtdeclarative
endif
ifndef HAVE_CROSS_COMPILE
PKGS_FOUND += qtdeclarative-tools
else ifeq ($(call system_tool_majmin, qmlcachegen --version),$(QTDECLARATIVE_VERSION_MAJOR))
PKGS_FOUND += qtshadertools-tools
endif

$(TARBALLS)/qtdeclarative-everywhere-src-$(QTDECLARATIVE_VERSION).tar.xz:
	$(call download_pkg,$(QTDECLARATIVE_URL),qt)

.sum-qtdeclarative: qtdeclarative-everywhere-src-$(QTDECLARATIVE_VERSION).tar.xz

.sum-qtdeclarative-tools: .sum-qtdeclarative
	touch $@

qtdeclarative: qtdeclarative-everywhere-src-$(QTDECLARATIVE_VERSION).tar.xz .sum-qtdeclarative
	$(UNPACK)
	$(APPLY) $(SRC)/qtdeclarative/0001-Fix-incorrect-library-inclusion.patch
	$(MOVE)

QT_DECLARATIVE_COMMON_CONFIG := \
	-DFEATURE_qml_debug=OFF \
	-DFEATURE_qml_profiler=OFF \
	-DFEATURE_quick_designer=OFF \
	-DFEATURE_quick_particles=OFF \
	-DFEATURE_qml_preview=OFF \
	-DFEATURE_quickcontrols2_imagine=OFF \
	-DFEATURE_quickcontrols2_material=OFF \
	-DFEATURE_quickcontrols2_universal=OFF \
	-DFEATURE_quickcontrols2_macos=OFF \
	-DFEATURE_quickcontrols2_ios=OFF \
	-DFEATURE_qml_network=OFF

QT_DECLARATIVE_CONFIG := $(QT_DECLARATIVE_COMMON_CONFIG) \
	-DCMAKE_TOOLCHAIN_FILE=$(PREFIX)/lib/cmake/Qt6/qt.toolchain.cmake \
	-DQT_HOST_PATH=$(BUILDPREFIX)
ifdef ENABLE_PDB
QT_DECLARATIVE_CONFIG += -DCMAKE_BUILD_TYPE=RelWithDebInfo
else
QT_DECLARATIVE_CONFIG += -DCMAKE_BUILD_TYPE=Release
endif

QT_DECLARATIVE_NATIVE_CONFIG := $(QT_DECLARATIVE_COMMON_CONFIG) \
	-DFEATURE_qml_animation=OFF \
	-DFEATURE_qml_delegate_model=OFF \
	-DFEATURE_qml_itemmodel=OFF \
	-DFEATURE_qml_object-model=OFF \
	-DFEATURE_qml_table-model=OFF \
	-DFEATURE_quick_shadereffect=OFF \
	-DFEATURE_quick_path=OFF \
	-DCMAKE_TOOLCHAIN_FILE=$(BUILDPREFIX)/lib/cmake/Qt6/qt.toolchain.cmake \

.qtdeclarative-tools: BUILD_DIR=$</vlc_native
.qtdeclarative-tools: qtdeclarative
	$(CMAKECLEAN)
	$(BUILDVARS) $(CMAKE_NATIVE) $(QT_DECLARATIVE_NATIVE_CONFIG)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@

.qtdeclarative: qtdeclarative toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS) $(CMAKE) $(QT_DECLARATIVE_CONFIG)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
