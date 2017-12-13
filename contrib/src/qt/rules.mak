# Qt

QT_VERSION := 5.6.3
QT_URL := https://download.qt.io/archive/qt/5.6/$(QT_VERSION)/submodules/qtbase-opensource-src-$(QT_VERSION).tar.xz

ifdef HAVE_MACOSX
#PKGS += qt
endif
ifdef HAVE_WIN32
PKGS += qt
endif

ifeq ($(call need_pkg,"Qt5Core Qt5Gui Qt5Widgets"),)
PKGS_FOUND += qt
endif

$(TARBALLS)/qt-$(QT_VERSION).tar.xz:
	$(call download_pkg,$(QT_URL),qt)

.sum-qt: qt-$(QT_VERSION).tar.xz

qt: qt-$(QT_VERSION).tar.xz .sum-qt
	$(UNPACK)
	mv qtbase-opensource-src-$(QT_VERSION) qt-$(QT_VERSION)
	$(APPLY) $(SRC)/qt/0001-Windows-QPA-Reimplement-calculation-of-window-frames_56.patch
	$(APPLY) $(SRC)/qt/0002-Windows-QPA-Use-new-EnableNonClientDpiScaling-for-Wi_56.patch
	$(APPLY) $(SRC)/qt/0003-QPA-prefer-lower-value-when-rounding-fractional-scaling.patch
	$(APPLY) $(SRC)/qt/0004-qmake-don-t-limit-command-line-length-when-not-actua.patch
	$(APPLY) $(SRC)/qt/0005-harfbuzz-Fix-building-for-win64-with-clang.patch
	$(APPLY) $(SRC)/qt/0006-moc-Initialize-staticMetaObject-with-the-highest-use.patch
	$(APPLY) $(SRC)/qt/0007-Only-define-QT_FASTCALL-on-x86_32.patch
	$(APPLY) $(SRC)/qt/0008-Skip-arm-pixman-drawhelpers-on-windows-just-like-on-.patch
	$(APPLY) $(SRC)/qt/0009-mkspecs-Add-a-win32-clang-g-mkspec-for-clang-targeti.patch
	$(APPLY) $(SRC)/qt/systray-no-sound.patch
	$(MOVE)

ifdef HAVE_MACOSX
QT_PLATFORM := -platform darwin-g++
endif
ifdef HAVE_WIN32
ifdef HAVE_CLANG
QT_SPEC := win32-clang-g++
else
QT_SPEC := win32-g++
endif
QT_PLATFORM := -xplatform $(QT_SPEC) -device-option CROSS_COMPILE=$(HOST)-
endif

QT_CONFIG := -static -opensource -confirm-license -no-pkg-config \
	-no-sql-sqlite -no-gif -qt-libjpeg -no-openssl -no-opengl -no-dbus \
	-no-qml-debug -no-audio-backend -no-sql-odbc -no-pch \
	-no-compile-examples -nomake examples

QT_CONFIG += -release

.qt: qt
	cd $< && ./configure $(QT_PLATFORM) $(QT_CONFIG) -prefix $(PREFIX)
	# Make && Install libraries
	cd $< && $(MAKE)
	cd $< && $(MAKE) -C src sub-corelib-install_subtargets sub-gui-install_subtargets sub-widgets-install_subtargets sub-platformsupport-install_subtargets sub-zlib-install_subtargets sub-bootstrap-install_subtargets
	# Install tools
	cd $< && $(MAKE) -C src sub-moc-install_subtargets sub-rcc-install_subtargets sub-uic-install_subtargets
	# Install plugins
	cd $< && $(MAKE) -C src/plugins sub-platforms-install_subtargets
	mv $(PREFIX)/plugins/platforms/libqwindows.a $(PREFIX)/lib/ && rm -rf $(PREFIX)/plugins
	# Move includes to match what VLC expects
	mkdir -p $(PREFIX)/include/QtGui/qpa
	cp $(PREFIX)/include/QtGui/$(QT_VERSION)/QtGui/qpa/qplatformnativeinterface.h $(PREFIX)/include/QtGui/qpa
	# Clean Qt mess
	rm -rf $(PREFIX)/lib/libQt5Bootstrap*
	# Fix .pc files to remove debug version (d)
	cd $(PREFIX)/lib/pkgconfig; for i in Qt5Core.pc Qt5Gui.pc Qt5Widgets.pc; do sed -i -e 's/d\.a/.a/g' -e 's/d $$/ /' $$i; done
	# Fix Qt5Gui.pc file to include qwindows (QWindowsIntegrationPlugin) and Qt5Platform Support
	cd $(PREFIX)/lib/pkgconfig; sed -i -e 's/ -lQt5Gui/ -lqwindows -lQt5PlatformSupport -lQt5Gui/g' Qt5Gui.pc
ifdef HAVE_CROSS_COMPILE
	# Building Qt build tools for Xcompilation
	cd $</include/QtCore; $(LN_S)f $(QT_VERSION)/QtCore/private private
	cd $<; $(MAKE) -C qmake
	cd $<; $(MAKE) install_qmake install_mkspecs
	cd $</src/tools; \
	for i in bootstrap uic rcc moc; \
		do (cd $$i; echo $$i && ../../../bin/qmake -spec $(QT_SPEC) && $(MAKE) clean && $(MAKE) CC=$(HOST)-gcc CXX=$(HOST)-g++ LINKER=$(HOST)-g++ LIB="$(HOST)-ar -rc" && $(MAKE) install); \
	done
endif
	touch $@
