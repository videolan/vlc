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
endif
ifeq ($(shell qml --version 2>/dev/null | head -1 | sed s/'.* '// | cut -d '.' -f -2),$(QTDECLARATIVE_VERSION_MAJOR))
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

QT_DECLARATIVE_CONFIG := \
	--no-feature-quick-designer \
	--no-feature-quick-particles \
	--no-feature-qml-preview \
	--no-feature-quickcontrols2-imagine \
	--no-feature-quickcontrols2-material \
	--no-feature-quickcontrols2-universal \
	--no-feature-quickcontrols2-macos \
	--no-feature-quickcontrols2-ios \
	--no-feature-qml-network

QT_DECLARATIVE_NATIVE_CONFIG := $(QT_DECLARATIVE_CONFIG) \
	--no-feature-qml-animation \
	--no-feature-qml-delegate-model \
	--no-feature-qml-itemmodel \
	--no-feature-qml-object-model \
	--no-feature-qml-table-model \
	--no-feature-quick-particles \
	--no-feature-quick-shadereffect \
	--no-feature-quick-path \
	--no-feature-qml-network

QT_DECLARATIVE_FEATURES := -DFEATURE_qml_debug=OFF -DFEATURE_qml_profiler=OFF

.qtdeclarative-tools: BUILD_DIR=$</vlc_native
.qtdeclarative-tools: qtdeclarative
	$(CMAKECLEAN)
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && $(BUILDVARS) $(BUILDPREFIX)/bin/qt-configure-module $(BUILD_SRC) $(QT_DECLARATIVE_NATIVE_CONFIG) -- $(QT_DECLARATIVE_FEATURES)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@

.qtdeclarative: qtdeclarative toolchain.cmake
	mkdir -p $(PREFIX)/libexec
	$(CMAKECLEAN)
	mkdir -p $(BUILD_DIR)
	+cd $(BUILD_DIR) && $(PREFIX)/bin/qt-configure-module $(BUILD_SRC) $(QT_DECLARATIVE_CONFIG) -- $(QT_DECLARATIVE_FEATURES)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)

	touch $@
