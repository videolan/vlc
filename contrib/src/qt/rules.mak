# qtbase

QTBASE_VERSION := $(QTBASE_VERSION_MAJOR).1
# Insert potential -betaX suffix here:
QTBASE_VERSION_FULL := $(QTBASE_VERSION)
QTBASE_URL := $(QT)/$(QTBASE_VERSION_FULL)/submodules/qtbase-everywhere-src-$(QTBASE_VERSION_FULL).tar.xz

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

DEPS_qt = qt-tools harfbuzz $(DEPS_harfbuzz) jpeg $(DEPS_jpeg) png $(DEPS_png) zlib $(DEPS_zlib) vulkan-headers $(DEPS_vulkan-headers)
ifdef HAVE_WIN32
DEPS_qt += d3d12 $(DEPS_d3d12) dcomp $(DEPS_dcomp)
else
DEPS_qt += freetype2 $(DEPS_freetype2)
endif

ifeq ($(call need_pkg,"Qt6Core >= $(QTBASE_VERSION_MAJOR) Qt6Gui >= $(QTBASE_VERSION_MAJOR) Qt6Widgets >= $(QTBASE_VERSION_MAJOR)"),)
PKGS_FOUND += qt
endif
ifndef HAVE_CROSS_COMPILE
PKGS_FOUND += qt-tools
else ifdef QT_USES_SYSTEM_TOOLS
PKGS_FOUND += qt-tools
endif

$(TARBALLS)/qtbase-everywhere-src-$(QTBASE_VERSION_FULL).tar.xz:
	$(call download_pkg,$(QTBASE_URL),qt)

.sum-qt: qtbase-everywhere-src-$(QTBASE_VERSION_FULL).tar.xz

.sum-qt-tools: .sum-qt
	touch $@

qt: qtbase-everywhere-src-$(QTBASE_VERSION_FULL).tar.xz .sum-qt
	$(UNPACK)
	$(APPLY) $(SRC)/qt/0001-Windows-Tray-Icon-Set-NOSOUND.patch
	$(APPLY) $(SRC)/qt/0003-Revert-QMutex-remove-qmutex_win.cpp.patch
	$(APPLY) $(SRC)/qt/0004-Expose-QRhiImplementation-in-QRhi.patch
	$(APPLY) $(SRC)/qt/0005-Do-not-include-D3D12MemAlloc.h-in-header-file.patch
	$(APPLY) $(SRC)/qt/0006-Try-DCompositionCreateDevice3-first-if-available.patch
	$(APPLY) $(SRC)/qt/0007-Try-to-satisfy-Windows-7-compatibility.patch
	$(APPLY) $(SRC)/qt/0001-disable-precompiled-headers-when-forcing-WINVER-inte.patch
	$(MOVE)

ifeq ($(V),1)
QTBASE_CONFIG += -verbose
endif

ifdef HAVE_WIN32
QTBASE_CONFIG += -no-freetype -directwrite
else
QTBASE_CONFIG += -system-freetype
endif

ifdef HAVE_CROSS_COMPILE
# This is necessary to make use of qmake
QTBASE_PLATFORM := -device-option CROSS_COMPILE=$(HOST)-
endif

ifdef HAVE_WIN32
QTBASE_CONFIG += -no-feature-style-fusion
endif

ifdef ENABLE_PDB
QTBASE_CONFIG += -release -force-debug-info
else
QTBASE_CONFIG += -release
endif

QTBASE_CONFIG += -static -opensource -confirm-license -no-pkg-config -no-openssl \
    -no-gif -no-dbus -no-feature-zstd -no-feature-concurrent -no-feature-androiddeployqt \
	-no-feature-sql -no-feature-testlib -system-harfbuzz -system-libjpeg \
	-no-feature-xml -no-feature-printsupport -system-libpng -system-zlib -no-feature-network \
	-no-feature-movie -no-feature-pdf -no-feature-whatsthis -no-feature-lcdnumber \
	-no-feature-syntaxhighlighter -no-feature-undoview -no-feature-splashscreen \
	-no-feature-dockwidget -no-feature-statusbar -no-feature-statustip \
	-no-feature-keysequenceedit -no-feature-pkg-config \
	-nomake examples -prefix $(PREFIX) \
	-- -DCMAKE_TOOLCHAIN_FILE=$(abspath toolchain.cmake) $(QT_HOST_PATH)

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
