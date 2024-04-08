# qtbase

QTBASE_VERSION_MAJOR := 6.6
QTBASE_VERSION := $(QTBASE_VERSION_MAJOR).2
# Insert potential -betaX suffix here:
QTBASE_VERSION_FULL := $(QTBASE_VERSION)
QTBASE_URL := $(QT)/$(QTBASE_VERSION_MAJOR)/$(QTBASE_VERSION_FULL)/submodules/qtbase-everywhere-src-$(QTBASE_VERSION_FULL).tar.xz

ifdef HAVE_MACOSX
#PKGS += qt
endif
ifdef HAVE_WIN32
PKGS += qt
endif
ifneq ($(findstring qt,$(PKGS)),)
PKGS_TOOLS += qt-tools
endif
PKGS_ALL += qt-tools

DEPS_qt = qt-tools freetype2 $(DEPS_freetype2) harfbuzz $(DEPS_harfbuzz) jpeg $(DEPS_jpeg) png $(DEPS_png) zlib $(DEPS_zlib) vulkan-headers $(DEPS_vulkan-headers)
ifdef HAVE_WIN32
DEPS_qt += d3d12 $(DEPS_d3d12) dcomp $(DEPS_dcomp)
endif

ifeq ($(call need_pkg,"Qt6Core >= 6.6 Qt6Gui >= 6.6 Qt6Widgets >= 6.6 Qt6Network >= 6.6"),)
PKGS_FOUND += qt
endif
ifndef HAVE_CROSS_COMPILE
PKGS_FOUND += qt-tools
endif
ifeq ($(shell qt-cmake --version 2>/dev/null | head -1 | sed s/'.* '// | cut -d '.' -f -2),3.22)
PKGS_FOUND += qt-tools
endif

$(TARBALLS)/qtbase-everywhere-src-$(QTBASE_VERSION_FULL).tar.xz:
	$(call download_pkg,$(QTBASE_URL), qt)

.sum-qt: qtbase-everywhere-src-$(QTBASE_VERSION_FULL).tar.xz

.sum-qt-tools: .sum-qt
	touch $@

qt: qtbase-everywhere-src-$(QTBASE_VERSION_FULL).tar.xz .sum-qt
	$(UNPACK)
	$(APPLY) $(SRC)/qt/0001-CMake-Place-resources-into-static-libraries-not-obje.patch
	$(APPLY) $(SRC)/qt/0002-Windows-Tray-Icon-Set-NOSOUND.patch
	$(APPLY) $(SRC)/qt/0003-Try-to-generate-pkgconfig-pc-files-in-static-build.patch
	$(APPLY) $(SRC)/qt/0004-Revert-QMutex-remove-qmutex_win.cpp.patch
	$(APPLY) $(SRC)/qt/0005-Expose-QRhiImplementation-in-QRhi.patch
	$(APPLY) $(SRC)/qt/0006-Do-not-include-D3D12MemAlloc.h-in-header-file.patch
	$(APPLY) $(SRC)/qt/0007-Try-DCompositionCreateDevice3-first-if-available.patch
	$(APPLY) $(SRC)/qt/0008-Try-to-satisfy-Windows-7-compatibility.patch
	$(APPLY) $(SRC)/qt/0001-disable-precompiled-headers-when-forcing-WINVER-inte.patch
	$(APPLY) $(SRC)/qt/0001-QTypeInfo-Add-a-missing-include.patch
	$(APPLY) $(SRC)/qt/0002-QStringTokenizer-Add-a-missing-include.patch
	$(APPLY) $(SRC)/qt/0003-CMake-Fix-a-misplaced-in-pkg-config-files.patch
	$(MOVE)

ifeq ($(V),1)
QTBASE_CONFIG += -verbose
endif

ifdef HAVE_CROSS_COMPILE
# This is necessary to make use of qmake
QTBASE_PLATFORM := -device-option CROSS_COMPILE=$(HOST)-
endif

QTBASE_CONFIG += -static -opensource -confirm-license -opengl desktop -no-pkg-config -no-openssl \
    -no-gif -no-dbus -no-feature-zstd -no-feature-concurrent -no-feature-androiddeployqt \
	-no-feature-sql -no-feature-testlib -system-freetype -system-harfbuzz -system-libjpeg \
	-no-feature-xml -no-feature-printsupport -system-libpng -system-zlib -no-feature-network \
	-nomake examples -prefix $(PREFIX) -qt-host-path $(BUILDPREFIX) \
	-- -DCMAKE_TOOLCHAIN_FILE=$(abspath toolchain.cmake)

ifdef ENABLE_PDB
QTBASE_CONFIG += -DCMAKE_BUILD_TYPE=RelWithDebInfo
else
QTBASE_CONFIG += -DCMAKE_BUILD_TYPE=Release
endif

QTBASE_NATIVE_CONFIG := -DQT_BUILD_EXAMPLES=FALSE -DQT_BUILD_TESTS=FALSE -DFEATURE_pkg_config=OFF \
	-DFEATURE_accessibility=OFF -DFEATURE_widgets=OFF -DFEATURE_printsupport=OFF -DFEATURE_androiddeployqt=OFF \
	-DFEATURE_xml=OFF -DFEATURE_network=OFF -DFEATURE_vnc=OFF -DFEATURE_linuxfb=OFF -DFEATURE_xlib=OFF \
	-DFEATURE_sql=OFF -DFEATURE_testlib=OFF -DFEATURE_pdf=OFF -DFEATURE_vulkan=OFF -DFEATURE_imageformatplugin=OFF \
	-DFEATURE_zstd=OFF -DFEATURE_xkbcommon=OFF -DFEATURE_evdev=OFF -DFEATURE_sessionmanager=OFF -DFEATURE_png=OFF \
	-DFEATURE_dbus=OFF -DINPUT_openssl=no -DFEATURE_concurrent=OFF -DFEATURE_glib=OFF -DFEATURE_icu=OFF \
	-DFEATURE_texthtmlparser=OFF -DFEATURE_cssparser=OFF -DFEATURE_textodfwriter=OFF -DFEATURE_textmarkdownreader=OFF \
	-DFEATURE_textmarkdownwriter=OFF -DINPUT_libb2=no -DFEATURE_harfbuzz=OFF -DFEATURE_freetype=OFF -DINPUT_opengl=no

.qt-tools: BUILD_DIR=$</vlc_native
.qt-tools: qt
	$(CMAKECLEAN)
	$(BUILDVARS) $(CMAKE_NATIVE) $(QTBASE_NATIVE_CONFIG)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@

.qt: qt toolchain.cmake
	$(CMAKECLEAN)
	mkdir -p $(BUILD_DIR)

	# Configure qt, build and run cmake
	+cd $(BUILD_DIR) && ../configure $(QTBASE_PLATFORM) $(QTBASE_CONFIG)

	# Build
	+$(CMAKEBUILD)

	# Install
	$(CMAKEINSTALL)

	touch $@
