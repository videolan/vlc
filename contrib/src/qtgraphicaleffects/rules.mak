# QtGraphicalEffects

QTGE_VERSION := 5.11.0
QTGE_URL := http://download.qt.io/official_releases/qt/5.11/$(QTGE_VERSION)/submodules/qtgraphicaleffects-everywhere-src-$(QTGE_VERSION).tar.xz

DEPS_qtgraphicaleffects += qtdeclarative $(DEPS_qtdeclarative)

ifdef HAVE_WIN32
ifeq ($(findstring $(ARCH), arm aarch64),)
# There is no opengl available on windows on these architectures.
PKGS += qtgraphicaleffects
endif
endif

ifeq ($(call need_pkg,"Qt5QuickControls2"),)
PKGS_FOUND += qtgraphicaleffects
endif

$(TARBALLS)/qtgraphicaleffects-$(QTGE_VERSION).tar.xz:
	$(call download,$(QTGE_URL))

.sum-qtgraphicaleffects: qtgraphicaleffects-$(QTGE_VERSION).tar.xz

qtgraphicaleffects: qtgraphicaleffects-$(QTGE_VERSION).tar.xz .sum-qtgraphicaleffects
	$(UNPACK)
	mv qtgraphicaleffects-everywhere-src-$(QTGE_VERSION) qtgraphicaleffects-$(QTGE_VERSION)
	$(MOVE)

.qtgraphicaleffects: qtgraphicaleffects
ifdef HAVE_CROSS_COMPILE
	cd $< && $(PREFIX)/bin/qmake
else
	cd $< && ../qt/bin/qmake
endif
	# Make && Install libraries
	cd $< && $(MAKE)
	cd $< && $(MAKE) -C src sub-effects-install_subtargets
	cd $(PREFIX)/qml/QtGraphicalEffects/ && cp libqtgraphicaleffectsplugin.a private/libqtgraphicaleffectsprivate.a $(PREFIX)/lib/
	rm -rf $(PREFIX)/qml
	cd $(PREFIX)/lib/pkgconfig; sed -i.orig \
		-e 's/ -lQt5QuickWidgets/ -lqtgraphicaleffectsplugin -lqtgraphicaleffectsprivate -lQt5QuickWidgets/' \
		Qt5QuickWidgets.pc
	touch $@
