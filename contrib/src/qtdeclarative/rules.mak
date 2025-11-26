# QtDeclarative

QTDECLARATIVE_VERSION := $(QTBASE_VERSION)
QTDECLARATIVE_URL := $(QT)/$(QTDECLARATIVE_VERSION)/submodules/qtdeclarative-everywhere-src-$(QTDECLARATIVE_VERSION).tar.xz

DEPS_qtdeclarative-tools = qt-tools $(DEPS_qt-tools) qtshadertools-tools $(DEPS_qtshadertools-tools)

DEPS_qtdeclarative = qt $(DEPS_qt) qtdeclarative-tools $(DEPS_qtdeclarative-tools)

ifdef HAVE_WIN32
PKGS += qtdeclarative
endif
ifneq ($(findstring qt,$(PKGS)),)
PKGS_TOOLS += qtdeclarative-tools
endif
PKGS_ALL += qtdeclarative-tools

ifeq ($(call need_pkg,"Qt6Qml >= $(QTBASE_VERSION_MAJOR) Qt6Quick >= $(QTBASE_VERSION_MAJOR) Qt6QuickControls2 >= $(QTBASE_VERSION_MAJOR) Qt6QuickLayouts >= $(QTBASE_VERSION_MAJOR) Qt6QmlWorkerScript >= $(QTBASE_VERSION_MAJOR)"),)
PKGS_FOUND += qtdeclarative
endif
ifndef HAVE_CROSS_COMPILE
PKGS_FOUND += qtdeclarative-tools
else ifdef QT_USES_SYSTEM_TOOLS
PKGS_FOUND += qtdeclarative-tools
endif

$(TARBALLS)/qtdeclarative-everywhere-src-$(QTDECLARATIVE_VERSION).tar.xz:
	$(call download_pkg,$(QTDECLARATIVE_URL),qt)

.sum-qtdeclarative: qtdeclarative-everywhere-src-$(QTDECLARATIVE_VERSION).tar.xz

.sum-qtdeclarative-tools: .sum-qtdeclarative
	touch $@

qtdeclarative: qtdeclarative-everywhere-src-$(QTDECLARATIVE_VERSION).tar.xz .sum-qtdeclarative
	$(UNPACK)
	$(APPLY) $(SRC)/qtdeclarative/0001-QtQml-do-not-care-about-patch-version-when-checking-.patch
	$(APPLY) $(SRC)/qtdeclarative/0001-QtQml-do-not-care-about-library-version-hash-when-ch.patch
	# disable unused CLI tools: qml, qmleasing, qmldom, qmlformat, qmltc
	sed -i.orig -e 's,add_subdirectory(qml),#add_subdirectory(qml),' $(UNPACK_DIR)/tools/CMakeLists.txt
	sed -i.orig -e 's,add_subdirectory(qmleasing),#add_subdirectory(qmleasing),' $(UNPACK_DIR)/tools/CMakeLists.txt
	sed -i.orig -e 's,add_subdirectory(qmldom),#add_subdirectory(qmldom),' $(UNPACK_DIR)/tools/CMakeLists.txt
	sed -i.orig -e 's,add_subdirectory(qmlformat),#add_subdirectory(qmlformat),' $(UNPACK_DIR)/tools/CMakeLists.txt
	sed -i.orig -e 's,add_subdirectory(qmltc),#add_subdirectory(qmltc),' $(UNPACK_DIR)/tools/CMakeLists.txt
	# disable QT labs feature we don't use
	sed -i.orig -e 's,add_subdirectory(labs),#add_subdirectory(labs),' $(UNPACK_DIR)/src/CMakeLists.txt
	# disable unused svgtoqml tool:
	sed -i.orig -e 's,add_subdirectory(svgtoqml),#add_subdirectory(svgtoqml),' $(UNPACK_DIR)/tools/CMakeLists.txt
	sed -i.orig -e 's,add_subdirectory(quickdialogs),#add_subdirectory(quickdialogs),' $(UNPACK_DIR)/src/CMakeLists.txt
	sed -i.orig -e 's,add_subdirectory(qmldom),#add_subdirectory(qmldom),' $(UNPACK_DIR)/src/CMakeLists.txt
	sed -i.orig -e 's,add_subdirectory(quickwidgets),#add_subdirectory(quickwidgets),' $(UNPACK_DIR)/src/CMakeLists.txt
	sed -i.orig -e 's,add_subdirectory(quickvectorimage),#add_subdirectory(quickvectorimage),' $(UNPACK_DIR)/src/CMakeLists.txt
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
	-DFEATURE_quickcontrols2_fusion=OFF \
	-DFEATURE_quickcontrols2_windows=OFF \
	-DFEATURE_qml_network=OFF \
	-DFEATURE_quick_animatedimage=OFF \
	-DFEATURE_quick_flipable=OFF \
	-DFEATURE_quick_sprite=OFF \
	-DFEATURE_quick_canvas=OFF \
	-DFEATURE_quick_path=OFF \
	-DFEATURE_quicktemplates2_calendar=OFF \
	-DQT_FEATURE_testlib=OFF

QT_DECLARATIVE_CONFIG := $(QT_DECLARATIVE_COMMON_CONFIG) \
	$(QT_CMAKE_CONFIG)
ifdef ENABLE_PDB
QT_DECLARATIVE_CONFIG += -DCMAKE_BUILD_TYPE=RelWithDebInfo
else
QT_DECLARATIVE_CONFIG += -DCMAKE_BUILD_TYPE=Release
endif

QT_DECLARATIVE_NATIVE_CONFIG := $(QT_DECLARATIVE_COMMON_CONFIG) \
	-DFEATURE_qml_animation=OFF \
	-DFEATURE_qml_delegate_model=OFF \
	-DFEATURE_qml_itemmodel=OFF \
	-DFEATURE_qml_object_model=OFF \
	-DFEATURE_qml_table_model=OFF \
	-DFEATURE_quick_shadereffect=OFF \
	-DCMAKE_TOOLCHAIN_FILE=$(QT_HOST_LIBS)/cmake/Qt6/qt.toolchain.cmake

.qtdeclarative-tools: BUILD_DIR=$</vlc_native
.qtdeclarative-tools: qtdeclarative
	$(CMAKECLEAN)
	$(BUILDVARS) $(CMAKE_NATIVE) $(QT_DECLARATIVE_NATIVE_CONFIG)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@

.qtdeclarative: qtdeclarative toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(QT_DECLARATIVE_CONFIG)
	+PATH="$(PATH):$(PREFIX)/bin" $(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
