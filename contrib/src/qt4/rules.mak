# qt4

QT4_VERSION = 4.7.4
QT4_URL := http://download.qt.nokia.com/qt/source/qt-everywhere-opensource-src-$(QT4_VERSION).tar.gz

# FIXME : OSX build
ifdef HAVE_WIN32
PKGS += qt4
endif

ifeq ($(call need_pkg,"QtCore QtGui"),)
PKGS_FOUND += qt4
endif

$(TARBALLS)/qt-$(QT4_VERSION).tar.gz:
	$(call download,$(QT4_URL))

.sum-qt4: qt-$(QT4_VERSION).tar.gz

qt4: qt-$(QT4_VERSION).tar.gz .sum-qt4
	$(UNPACK)
	patch -p0 < $(SRC)/qt4/cross.patch
	mv qt-everywhere-opensource-src-4.7.4 $@ && touch $@

XTOOLS := XCC="$(CC)" XCXX="$(CXX)" XSTRIP="$(STRIP)" XAR="$(AR)"

.qt4: qt4
	cd $< && $(XTOOLS) ./configure -xplatform win32-g++ -static -release -fast -no-exceptions -no-stl -no-sql-sqlite -no-qt3support -no-gif -no-libmng -qt-libjpeg -no-libtiff -no-qdbus -no-openssl -no-webkit -sse -no-script -no-multimedia -no-phonon -opensource -no-scripttools -no-opengl -no-script -no-scripttools -no-declarative -no-declarative-debug -opensource -no-s60 -host-little-endian -confirm-license
	cd $< && $(MAKE) $(XTOOLS) sub-src
	cd $</src/plugins/imageformats/jpeg && $(MAKE) $(XTOOLS) # FIXME
	# BUILDING QT BUILD TOOLS
ifdef HAVE_CROSS_COMPILE
	cd $</src/tools; $(MAKE) clean; \
		for i in bootstrap uic rcc moc; \
			do (cd $$i; ../../../bin/qmake); \
		done; \
		../../../bin/qmake; \
		$(MAKE) $(XTOOLS)
endif
	# INSTALLING LIBRARIES
	for lib in QtGui QtCore QtNetwork QtXml; \
		do install -D -- $</lib/lib$${lib}.a "$(PREFIX)/lib/lib$${lib}.a"; \
	done
	# INSTALLING PLUGINS
	install -D -- $</plugins/imageformats/libqjpeg.a "$(PREFIX)/lib/libqjpeg.a"
	install -D -- $</plugins/accessible/libqtaccessiblewidgets.a "$(PREFIX)/lib/libqtaccessiblewidgets.a"
	for codec in cn jp kr tw; \
		do install -D -- $</plugins/codecs/libq$${codec}codecs.a "$(PREFIX)/lib/libq$${codec}codecs.a"; \
	done
	# INSTALLING CORE HEADERS
	cd $</src/corelib;    find . -type f -name '*.h' -exec install -D -- "{}" "$(PREFIX)/include/qt4/src/corelib/{}" \;
	cd $</include/QtCore; find . -maxdepth 1 -type f \( -name '*.h' -o -name 'Q*' \) -exec install -D -s --strip-program="$(abspath $(SRC)/qt4/fix_header.sh)" -- "{}" "$(PREFIX)/include/qt4/QtCore/{}" \;
	# INSTALLING GUI HEADERS
	cd $</src/gui; find . -type f -name '*.h' -exec install -D -- "{}" "$(PREFIX)/include/qt4/src/gui/{}" \;
	cd $</include/QtGui; find . -maxdepth 1 -type f \( -name '*.h' -o -name 'Q*' \) -exec install -D -s --strip-program="$(abspath $(SRC)/qt4/fix_header.sh)" -- "{}" "$(PREFIX)/include/qt4/QtGui/{}" \;
	# INSTALLING XML HEADERS
	cd $</src/xml;    find . -type f -name '*.h' -exec install -D -- "{}" "$(PREFIX)/include/qt4/src/xml/{}" \;
	cd $</include/QtXml; find . -maxdepth 1 -type f \( -name '*.h' -o -name 'Q*' \) -exec install -D -s --strip-program="$(abspath $(SRC)/qt4/fix_header.sh)" -- "{}" "$(PREFIX)/include/qt4/QtXml/{}" \;
	# INSTALLING NETWORK HEADERS
	cd $</src/network;    find . -type f -name '*.h' -exec install -D -- "{}" "$(PREFIX)/include/qt4/src/network/{}" \;
	cd $</include/QtNetwork; find . -maxdepth 1 -type f \( -name '*.h' -o -name 'Q*' \) -exec install -D -s --strip-program="$(abspath $(SRC)/qt4/fix_header.sh)" -- "{}" "$(PREFIX)/include/qt4/QtNetwork/{}" \;
	# INSTALLING PKGCONFIG FILES
	install -d "$(PREFIX)/lib/pkgconfig"
	cat $(SRC)/qt4/QtCore.pc.in | sed -e s/@@VERSION@@/$(QT4_VERSION)/ | sed -e 's|@@PREFIX@@|$(PREFIX)|' > "$(PREFIX)/lib/pkgconfig/QtCore.pc"
	cat $(SRC)/qt4/QtGui.pc.in | sed -e s/@@VERSION@@/$(QT4_VERSION)/ | sed -e 's|@@PREFIX@@|$(PREFIX)|' > "$(PREFIX)/lib/pkgconfig/QtGui.pc"
	# INSTALLING QT BUILD TOOLS
	install -d "$(PREFIX)/bin/"
	for i in rcc moc uic; \
		do cp $</bin/$$i* "$(PREFIX)/bin"; \
	done
	touch $@
