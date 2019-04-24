# Qt

QT_VERSION_MAJOR := 5.12
QT_VERSION := $(QT_VERSION_MAJOR).2
# Insert potential -betaX suffix here:
QT_VERSION_FULL := $(QT_VERSION)
QT_URL := https://download.qt.io/official_releases/qt/$(QT_VERSION_MAJOR)/$(QT_VERSION_FULL)/submodules/qtbase-everywhere-src-$(QT_VERSION_FULL).tar.xz

ifdef HAVE_MACOSX
#PKGS += qt
endif
ifdef HAVE_WIN32
PKGS += qt
ifdef HAVE_CROSS_COMPILE
DEPS_qt += wine-headers fxc2 $(DEPS_fxc2)
endif
endif

ifeq ($(call need_pkg,"Qt5Core >= 5.11 Qt5Gui Qt5Widgets"),)
PKGS_FOUND += qt
endif

$(TARBALLS)/qtbase-everywhere-src-$(QT_VERSION_FULL).tar.xz:
	$(call download_pkg,$(QT_URL),qt)

.sum-qt: qtbase-everywhere-src-$(QT_VERSION_FULL).tar.xz

qt: qtbase-everywhere-src-$(QT_VERSION_FULL).tar.xz .sum-qt
	$(UNPACK)
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/qt/0001-Windows-QPA-prefer-lower-value-when-rounding-fractio.patch
	$(APPLY) $(SRC)/qt/0002-Windows-QPA-Disable-systray-notification-sounds.patch
	$(APPLY) $(SRC)/qt/0004-Fix-PMurHash.c-mingw-clang-64-bit-compilation.patch
ifndef HAVE_WIN64
	$(APPLY) $(SRC)/qt/0001-disable-qt_random_cpu.patch
endif
	$(APPLY) $(SRC)/qt/0006-ANGLE-don-t-use-msvc-intrinsics-when-crosscompiling-.patch
	$(APPLY) $(SRC)/qt/0007-ANGLE-remove-static-assert-that-can-t-be-evaluated-b.patch
	$(APPLY) $(SRC)/qt/0008-ANGLE-disable-ANGLE_STD_ASYNC_WORKERS-when-compiling.patch

ifdef HAVE_CROSS_COMPILE
	$(APPLY) $(SRC)/qt/0003-allow-cross-compilation-of-angle-with-wine.patch
else
	cd $(UNPACK_DIR); for i in QtFontDatabaseSupport QtWindowsUIAutomationSupport QtEventDispatcherSupport QtCore; do \
		sed -i -e 's,"../../../../../src,"../src,g' include/$$i/$(QT_VERSION)/$$i/private/*.h; done
endif

endif
	$(MOVE)


ifdef HAVE_WIN32
QT_OPENGL := -angle
else
QT_OPENGL := -opengl desktop
endif

ifdef HAVE_MACOSX
QT_SPEC := darwin-g++
endif

ifdef HAVE_WIN32

ifdef HAVE_CLANG
QT_SPEC := win32-clang-g++
else
QT_SPEC := win32-g++
endif

ifdef HAVE_CROSS_COMPILE
QT_PLATFORM := -xplatform $(QT_SPEC) -device-option CROSS_COMPILE=$(HOST)-
else
ifneq ($(QT_SPEC),)
QT_PLATFORM := -platform $(QT_SPEC)
endif
endif

endif

QT_CONFIG := -static -opensource -confirm-license -no-pkg-config \
	-no-sql-sqlite -no-gif -qt-libjpeg -no-openssl $(QT_OPENGL) -no-dbus \
	-no-vulkan -no-sql-odbc -no-pch \
	-no-compile-examples -nomake examples -qt-zlib

QT_CONFIG += -release

ifdef HAVE_MINGW_W64
QT_CONFIG += -no-direct2d
endif

ENV_VARS := $(HOSTVARS) DXSDK_DIR=$(PREFIX)/bin

.qt: qt
	cd $< && $(ENV_VARS) ./configure $(QT_PLATFORM) $(QT_CONFIG) -prefix $(PREFIX) -I $(PREFIX)/include
	# Make && Install libraries
	cd $< && $(ENV_VARS) $(MAKE)
	cd $< && $(MAKE) -C src sub-corelib-install_subtargets sub-gui-install_subtargets sub-widgets-install_subtargets sub-platformsupport-install_subtargets sub-zlib-install_subtargets sub-bootstrap-install_subtargets sub-network-install_subtargets sub-testlib-install_subtargets
	# Install tools
	cd $< && $(MAKE) -C src sub-moc-install_subtargets sub-rcc-install_subtargets sub-uic-install_subtargets sub-qlalr-install_subtargets
	# Install plugins
	cd $< && $(MAKE) -C src/plugins sub-platforms-install_subtargets
ifdef HAVE_WIN32
	cd $< && $(MAKE) -C src/plugins sub-imageformats-install_subtargets
	mv $(PREFIX)/plugins/imageformats/libqjpeg.a $(PREFIX)/lib/
	mv $(PREFIX)/plugins/platforms/libqwindows.a $(PREFIX)/lib/ && rm -rf $(PREFIX)/plugins
	# Vista styling
	cd $< && $(MAKE) -C src -C plugins sub-styles-install_subtargets
	mv $(PREFIX)/plugins/styles/libqwindowsvistastyle.a $(PREFIX)/lib/ && rm -rf $(PREFIX)/plugins
	# Move includes to match what VLC expects
	mkdir -p $(PREFIX)/include/QtGui/qpa
	cp $(PREFIX)/include/QtGui/$(QT_VERSION)/QtGui/qpa/qplatformnativeinterface.h $(PREFIX)/include/QtGui/qpa
	# Clean Qt mess
	rm -rf $(PREFIX)/lib/libQt5Bootstrap* $</lib/libQt5Bootstrap*
	# Fix .pc files
	for i in Qt5Core Qt5Gui Qt5Widgets Qt5Test Qt5Network ; do $(SRC)/qt/FixQtPcFiles.sh $(PREFIX)/lib/$$i.prl $(PREFIX)/lib/pkgconfig/$$i.pc; done
	# Fix Qt5Gui.pc file to include qwindows (QWindowsIntegrationPlugin) and platform support libraries
	cd $(PREFIX)/lib/pkgconfig; sed -i.orig -e 's/ -lQt5Gui/ -lqwindows -lqjpeg -luxtheme -ldwmapi -lwtsapi32 -lQt5ThemeSupport -lQt5FontDatabaseSupport -lQt5EventDispatcherSupport -lQt5WindowsUIAutomationSupport -lqtfreetype -lQt5Gui/g' Qt5Gui.pc
	# Fix Qt5Widget.pc file to include qwindowsvistastyle before Qt5Widget, as it depends on it
	cd $(PREFIX)/lib/pkgconfig; sed -i.orig -e 's/ -lQt5Widget/ -lqwindowsvistastyle -lQt5Widget/' Qt5Widgets.pc
	# Use ANGLE OpenGL provided by Qt
	cd $(PREFIX)/lib/pkgconfig; sed -i.orig -e '/^Cflags:/ s#$$# -I$${includedir}/QtANGLE#' \
		-e 's/-llibGLESv2/-llibGLESv2 -ld3d9 -ltranslator -lpreprocessor/g' Qt5Gui.pc
endif
	# Install a qmake with correct paths set
	cd $<; $(MAKE) sub-qmake-qmake-aux-pro-install_subtargets install_mkspecs
	touch $@
