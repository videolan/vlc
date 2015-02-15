# qt

QT_VERSION = 5.3.0
QT_URL := http://download.qt-project.org/official_releases/qt/5.3/$(QT_VERSION)/submodules/qtbase-opensource-src-$(QT_VERSION).tar.xz

ifdef HAVE_MACOSX
#PKGS += qt
endif
ifdef HAVE_WIN32
PKGS += qt
endif

ifeq ($(call need_pkg,"QtCore QtGui"),)
ifeq ($(call need_pkg,"Qt5Core Qt5Gui Qt5Widgets"),)
PKGS_FOUND += qt
endif
endif

$(TARBALLS)/qt-$(QT_VERSION).tar.xz:
	$(call download,$(QT_URL))

.sum-qt: qt-$(QT_VERSION).tar.xz

qt: qt-$(QT_VERSION).tar.xz .sum-qt
	$(UNPACK)
	mv qtbase-opensource-src-$(QT_VERSION) qt-$(QT_VERSION)
	$(APPLY) $(SRC)/qt/Win32-AOT.patch
	$(MOVE)

ifdef HAVE_MACOSX
QT_PLATFORM := -platform darwin-g++
endif
ifdef HAVE_WIN32
QT_PLATFORM := -xplatform win32-g++ -device-option CROSS_COMPILE=$(HOST)-
endif

.qt: qt
	cd $< && ./configure $(QT_PLATFORM) -static -release -no-sql-sqlite -no-gif -qt-libjpeg -no-openssl -no-opengl -opensource -confirm-license
	cd $< && $(MAKE) sub-src
	# INSTALLING LIBRARIES
	for lib in Widgets Gui Core; \
		do install -D -- $</lib/libQt5$${lib}.a "$(PREFIX)/lib/libQt5$${lib}.a"; \
	done
	# INSTALLING PLUGINS
	install -D -- $</plugins/platforms/libqwindows.a "$(PREFIX)/lib/libqwindows.a"
	install -D -- $</plugins/accessible/libqtaccessiblewidgets.a "$(PREFIX)/lib/libqtaccessiblewidgets.a"
	# INSTALLING HEADERS
	for h in corelib gui widgets; \
		do (cd $</src/$${h} && find . -type f -name '*.h' -exec install -D -- "{}" "$(PREFIX)/include/qt5/src/$${h}/{}" \;) ; \
	done
	for h in Core Gui Widgets; \
		do (cd $</include/Qt$${h} && find . -maxdepth 1 -type f \( -name '*.h' -o -name 'Q*' \) -exec install -D -s --strip-program="$(abspath $(SRC)/qt/fix_header.sh)" -- "{}" "$(PREFIX)/include/qt5/Qt$${h}/{}" \;) ; \
	done
	mkdir -p "$(PREFIX)/include/qt5/qpa"
	echo "#include \"../src/gui/kernel/qplatformnativeinterface.h\"" > "$(PREFIX)/include/qt5/qpa/qplatformnativeinterface.h"
	# INSTALLING PKGCONFIG FILES
	install -d "$(PREFIX)/lib/pkgconfig"
	for i in Core Gui Widgets; \
		do cat $(SRC)/qt/Qt5$${i}.pc.in | sed -e s/@@VERSION@@/$(QT_VERSION)/ | sed -e 's|@@PREFIX@@|$(PREFIX)|' > "$(PREFIX)/lib/pkgconfig/Qt5$${i}.pc"; \
	done
	# BUILDING QT BUILD TOOLS
ifdef HAVE_CROSS_COMPILE
	cd $</include/QtCore; ln -sf $(QT_VERSION)/QtCore/private
	cd $</src/tools; $(MAKE) clean; \
		for i in bootstrap uic rcc moc; \
			do (cd $$i; echo $i && ../../../bin/qmake -spec win32-g++ ; $(MAKE) clean; $(MAKE)); \
		done
endif
	# INSTALLING QT BUILD TOOLS
	install -d "$(PREFIX)/bin/"
	for i in rcc moc uic; \
		do cp $</bin/$$i* "$(PREFIX)/bin"; \
	done
	touch $@
