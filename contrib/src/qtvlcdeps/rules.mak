# qtvlcdeps

DEPS_qtvlcdeps += qt $(DEPS_qt) qtsvg $(DEPS_qtsvg) qtshadertools $(DEPS_qtshadertools) qtdeclarative $(DEPS_qtdeclarative) qt5compat $(DEPS_qt5compat)

ifdef HAVE_LINUX
DEPS_qtvlcdeps += qtwayland $(DEPS_qtwayland)
endif

ifdef HAVE_WIN32
PKGS += qtvlcdeps
endif

ifeq ($(call need_pkg,"qtvlcdeps >= 0.1"),)
PKGS_FOUND += qtvlcdeps
endif

$(TARBALLS)/.dummy-qtvlcdeps:
	rm -f $(TARBALLS)/.dummy-qtvlcdeps
	cp -f $(SRC)/qtvlcdeps/dummy $(TARBALLS)/.dummy-qtvlcdeps

.sum-qtvlcdeps: .dummy-qtvlcdeps

QMAKE := $(PREFIX)/bin/qmake

.qtvlcdeps:
	rm -rf /tmp/vlc-qt-deps
	$(QMAKE) $(SRC)/qtvlcdeps -o /tmp/vlc-qt-deps/deps
	rm -f $(PREFIX)/lib/pkgconfig/qtvlcdeps.pc
	cp -f $(SRC)/qtvlcdeps/qtvlcdeps.pc.in $(PREFIX)/lib/pkgconfig/qtvlcdeps.pc
	QT_LIBS=$$(awk -F '=' '/LIBS/ {print $$2; exit}' /tmp/vlc-qt-deps/deps.Release); \
	sed -i "s|%1|$$QT_LIBS|g" $(PREFIX)/lib/pkgconfig/qtvlcdeps.pc
