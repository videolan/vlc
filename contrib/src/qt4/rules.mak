# qt4

QT4_VERSION = 4.8.5
QT4_URL := http://download.qt-project.org/official_releases/qt/4.8/$(QT4_VERSION)/qt-everywhere-opensource-src-$(QT4_VERSION).tar.gz

ifdef HAVE_MACOSX
#PKGS += qt4
endif
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
	mv qt-everywhere-opensource-src-$(QT4_VERSION) qt-$(QT4_VERSION)
	$(APPLY) $(SRC)/qt4/cross.patch
	$(APPLY) $(SRC)/qt4/styles.patch
	$(APPLY) $(SRC)/qt4/chroot.patch
	$(APPLY) $(SRC)/qt4/imageformats.patch
	$(APPLY) $(SRC)/qt4/win64.patch
	$(MOVE)

ifdef HAVE_MACOSX
QT_PLATFORM := -platform darwin-g++
endif
ifdef HAVE_WIN32
QT_PLATFORM := -xplatform win32-g++ -device-option CROSS_COMPILE=$(HOST)-
endif

.qt4: qt4
	cd $< && ./configure $(QT_PLATFORM) -static -release -fast -no-exceptions -no-stl -no-sql-sqlite -no-qt3support -no-gif -no-libmng -qt-libjpeg -no-libtiff -no-qdbus -no-openssl -no-webkit -sse -no-script -no-multimedia -no-phonon -opensource -no-scripttools -no-opengl -no-script -no-scripttools -no-declarative -no-declarative-debug -opensource -no-s60 -host-little-endian -confirm-license
	cd $< && $(MAKE) sub-src
	# BUILDING QT BUILD TOOLS
ifdef HAVE_CROSS_COMPILE
	cd $</src/tools; $(MAKE) clean; \
		for i in bootstrap uic rcc moc; \
			do (cd $$i; ../../../bin/qmake); \
		done; \
		../../../bin/qmake; \
		$(MAKE)
endif
	# INSTALLING LIBRARIES
	for lib in QtGui QtCore QtNetwork QtXml; \
		do install -D -- $</lib/lib$${lib}.a "$(PREFIX)/lib/lib$${lib}.a"; \
	done
	# INSTALLING PLUGINS
	install -D -- $</plugins/imageformats/libqjpeg.a "$(PREFIX)/lib/libqjpeg.a"
	install -D -- $</plugins/accessible/libqtaccessiblewidgets.a "$(PREFIX)/lib/libqtaccessiblewidgets.a"
	# INSTALLING HEADERS
	for h in corelib gui xml network; \
		do (cd $</src/$${h} && find . -type f -name '*.h' -exec install -D -- "{}" "$(PREFIX)/include/qt4/src/$${h}/{}" \;) ; \
	done
	for h in Core Gui Xml Network; \
		do (cd $</include/Qt$${h} && find . -maxdepth 1 -type f \( -name '*.h' -o -name 'Q*' \) -exec install -D -s --strip-program="$(abspath $(SRC)/qt4/fix_header.sh)" -- "{}" "$(PREFIX)/include/qt4/Qt$${h}/{}" \;) ; \
	done
	# INSTALLING PKGCONFIG FILES
	install -d "$(PREFIX)/lib/pkgconfig"
	for i in Core Gui; \
		do cat $(SRC)/qt4/Qt$${i}.pc.in | sed -e s/@@VERSION@@/$(QT4_VERSION)/ | sed -e 's|@@PREFIX@@|$(PREFIX)|' > "$(PREFIX)/lib/pkgconfig/Qt$${i}.pc"; \
	done
	# INSTALLING QT BUILD TOOLS
	install -d "$(PREFIX)/bin/"
	for i in rcc moc uic; \
		do cp $</bin/$$i* "$(PREFIX)/bin"; \
	done
	touch $@
