# qtbase

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
	$(APPLY) $(SRC)/qt/0001-Do-not-link-D3D9.patch
	$(MOVE)

ifdef HAVE_WIN32
QTBASE_CONFIG += -DFEATURE_freetype=OFF -DFEATURE_directwrite=ON -DFEATURE_directwrite3=ON
else
QTBASE_CONFIG += -DFEATURE_freetype=ON -DFEATURE_system_freetype=ON
endif

ifdef HAVE_CROSS_COMPILE
# This is necessary to make use of qmake
QTBASE_CONFIG += -DQT_QMAKE_DEVICE_OPTIONS:STRING=CROSS_COMPILE=$(HOST)-
endif

ifdef HAVE_WIN32
QTBASE_CONFIG += -DFEATURE_style_fusion=OFF
endif

ifdef ENABLE_PDB
QTBASE_CONFIG += -DCMAKE_BUILD_TYPE=RelWithDebInfo
else
QTBASE_CONFIG += -DCMAKE_BUILD_TYPE=Release
endif

QTBASE_COMMON_CONFIG := -DFEATURE_pkg_config=OFF -DINPUT_openssl=no \
	-DFEATURE_dbus=OFF -DFEATURE_zstd=OFF -DFEATURE_concurrent=OFF -DFEATURE_androiddeployqt=OFF \
	-DFEATURE_sql=OFF -DFEATURE_testlib=OFF \
	-DFEATURE_xml=OFF -DFEATURE_printsupport=OFF -DFEATURE_network=OFF \
	-DFEATURE_pdf=OFF \
	-DQT_BUILD_EXAMPLES=OFF

QTBASE_CONFIG += $(QTBASE_COMMON_CONFIG) \
    -DFEATURE_gif=OFF \
	-DFEATURE_harfbuzz=ON -DFEATURE_system_harfbuzz=ON -DFEATURE_jpeg=ON -DFEATURE_system_jpeg=ON \
	-DFEATURE_png=ON -DFEATURE_system_png=ON -DFEATURE_zlib=ON -DFEATURE_system_zlib=ON \
	-DFEATURE_movie=OFF -DFEATURE_whatsthis=OFF -DFEATURE_lcdnumber=OFF \
	-DFEATURE_syntaxhighlighter=OFF -DFEATURE_undoview=OFF -DFEATURE_splashscreen=OFF \
	-DFEATURE_dockwidget=OFF -DFEATURE_statusbar=OFF -DFEATURE_statustip=OFF \
	-DFEATURE_keysequenceedit=OFF \
	-DCMAKE_TOOLCHAIN_FILE=$(abspath toolchain.cmake) $(QT_HOST_PATH)

QTBASE_NATIVE_CONFIG := $(QTBASE_COMMON_CONFIG) -DQT_BUILD_TESTS=FALSE \
	-DFEATURE_accessibility=OFF -DFEATURE_widgets=OFF \
	-DFEATURE_vnc=OFF -DFEATURE_linuxfb=OFF -DFEATURE_xlib=OFF \
	-DFEATURE_vulkan=OFF -DFEATURE_imageformatplugin=OFF \
	-DFEATURE_xkbcommon=OFF -DFEATURE_evdev=OFF -DFEATURE_sessionmanager=OFF -DFEATURE_png=OFF \
	-DFEATURE_glib=OFF -DFEATURE_icu=OFF \
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
	$(HOSTVARS_CMAKE) $(CMAKE) $(QTBASE_CONFIG)
	+PATH="$(PATH):$(PREFIX)/bin" $(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
