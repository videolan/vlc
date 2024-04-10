# qtvlcdeps

DEPS_qtvlcdeps += qt $(DEPS_qt) qtsvg $(DEPS_qtsvg) qtdeclarative $(DEPS_qtdeclarative) qt5compat $(DEPS_qt5compat)

ifdef HAVE_LINUX
DEPS_qtvlcdeps += qtwayland $(DEPS_qtwayland)
endif

ifneq ($(findstring qt,$(PKGS)),)
PKGS += qtvlcdeps
endif

ifeq ($(call need_pkg,"qtvlcdeps >= 0.1"),)
PKGS_FOUND += qtvlcdeps
endif

QT_VLC_DEP_SOURCES := Imports.qml Imports.qrc qtvlcdeps.pc.in vlcdeps.pro

.sum-qtvlcdeps:
	touch $@

qtvlcdeps: UNPACK_DIR=qtvlcdeps-unpack
qtvlcdeps:
	$(RM) -R $@ && mkdir -p $(UNPACK_DIR)
	for f in $(QT_VLC_DEP_SOURCES) ; do \
	  cp -f $(SRC)/qtvlcdeps/$$f $(UNPACK_DIR) ; \
	done
	$(MOVE)

.qtvlcdeps: qtvlcdeps
	rm -rf $</Makefile.Release
	$(BUILDPREFIX)/bin/qmake6 -qtconf $(PREFIX)/bin/target_qt.conf $(SRC)/qtvlcdeps -o $<
	QT_LIBS=$$(awk -F '=' '/LIBS/ {print $$2; exit}' $</Makefile.Release); \
	  cat $</qtvlcdeps.pc.in                         | \
	  sed "s|%1|$$QT_LIBS|"                          | \
	  sed "s|$(PREFIX)/lib/|$$\{libdir\}/|g"         | \
	  sed "s|$(PREFIX)/plugins/|$$\{pluginsdir\}/|g" | \
	  sed "s|$(PREFIX)/qml/|$$\{qmldir\}/|g"         | \
	  sed "s|$(PREFIX)/|$$\{prefix\}/|g"             | \
	  sed "s|@@CONTRIB_PREFIX@@|$(PREFIX)|" > $</qtvlcdeps.pc
	install -d $(PREFIX)/lib/pkgconfig && install $</qtvlcdeps.pc $(PREFIX)/lib/pkgconfig
	touch $@
