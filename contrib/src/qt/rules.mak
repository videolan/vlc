# Qt

QT_VERSION_MAJOR := 5.11
QT_VERSION := $(QT_VERSION_MAJOR).0
# Insert potential -betaX suffix here:
QT_VERSION_FULL := $(QT_VERSION)
QT_URL := https://download.qt.io/official_releases/qt/$(QT_VERSION_MAJOR)/$(QT_VERSION_FULL)/submodules/qtbase-everywhere-src-$(QT_VERSION_FULL).tar.xz

ifdef HAVE_MACOSX
#PKGS += qt
endif
ifdef HAVE_WIN32
PKGS += qt
endif

ifeq ($(call need_pkg,"Qt5Core Qt5Gui Qt5Widgets"),)
PKGS_FOUND += qt
endif

$(TARBALLS)/qt-$(QT_VERSION_FULL).tar.xz:
	$(call download,$(QT_URL))

.sum-qt: qt-$(QT_VERSION_FULL).tar.xz

qt: qt-$(QT_VERSION_FULL).tar.xz .sum-qt
	$(UNPACK)
	mv qtbase-everywhere-src-$(QT_VERSION_FULL) qt-$(QT_VERSION_FULL)
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/qt/0001-Windows-QPA-prefer-lower-value-when-rounding-fractio.patch
	$(APPLY) $(SRC)/qt/0002-Windows-QPA-Disable-systray-notification-sounds.patch
	$(APPLY) $(SRC)/qt/0003-configure-Treat-win32-clang-g-the-same-as-win32-g.patch
	$(APPLY) $(SRC)/qt/0004-qmake-Fix-building-with-lld-with-mingw-makefiles.patch
ifndef HAVE_WIN64
	$(APPLY) $(SRC)/qt/0001-disable-qt_random_cpu.patch
endif
endif
	$(MOVE)

QT_OPENGL := -opengl desktop

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
ifneq ($(findstring $(ARCH), arm aarch64),)
# There is no opengl available on windows on these architectures.
QT_OPENGL := -no-opengl
endif
endif

QT_CONFIG := -static -opensource -confirm-license -no-pkg-config \
	-no-sql-sqlite -no-gif -qt-libjpeg -no-openssl $(QT_OPENGL) -no-dbus \
	-no-sql-odbc -no-pch \
	-no-compile-examples -nomake examples -qt-zlib

QT_CONFIG += -release

.qt: qt
	cd $< && ./configure $(QT_PLATFORM) $(QT_CONFIG) -prefix $(PREFIX)
	# Make && Install libraries
	cd $< && $(MAKE)
	cd $< && $(MAKE) -C src sub-corelib-install_subtargets sub-gui-install_subtargets sub-widgets-install_subtargets sub-platformsupport-install_subtargets sub-zlib-install_subtargets sub-bootstrap-install_subtargets sub-network-install_subtargets sub-testlib-install_subtargets
	# Install tools
	cd $< && $(MAKE) -C src sub-moc-install_subtargets sub-rcc-install_subtargets sub-uic-install_subtargets
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
	# Fix .pc files to remove debug version (d)
	cd $(PREFIX)/lib/pkgconfig; for i in Qt5Core.pc Qt5Gui.pc Qt5Widgets.pc Qt5Test.pc Qt5Network.pc; do sed -i.orig -e 's/d\.a/.a/g' -e 's/d $$/ /' $$i; done
	# Fix Qt5Gui.pc file to include qwindows (QWindowsIntegrationPlugin) and platform support libraries
	cd $(PREFIX)/lib/pkgconfig; sed -i.orig -e 's/ -lQt5Gui/ -lqwindows -lqjpeg -luxtheme -ldwmapi -lQt5ThemeSupport -lQt5FontDatabaseSupport -lQt5EventDispatcherSupport -lQt5WindowsUIAutomationSupport -lqtfreetype -lQt5Gui/g' Qt5Gui.pc
	# Fix Qt5Widget.pc file to include qwindowsvistastyle before Qt5Widget, as it depends on it
	cd $(PREFIX)/lib/pkgconfig; sed -i.orig -e 's/ -lQt5Widget/ -lqwindowsvistastyle -lQt5Widget/' Qt5Widgets.pc
endif
	# Install a qmake with correct paths set
	cd $<; $(MAKE) sub-qmake-qmake-aux-pro-install_subtargets install_mkspecs
	touch $@
