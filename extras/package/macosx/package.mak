macos_destdir=$(abs_top_builddir)/macos-install


if HAVE_DARWIN
noinst_DATA = pseudo-bundle
endif

# Symlink a pseudo-bundle
pseudo-bundle:
	$(MKDIR_P) $(top_builddir)/bin/Contents/Resources/
	$(LN_S) -nf $(abs_top_builddir)/modules/gui/macosx/UI $(top_builddir)/bin/Contents/Resources/Base.lproj
	$(LN_S) -nf $(abs_top_builddir)/share/macosx/Info.plist $(top_builddir)/bin/Contents/Info.plist
	$(LN_S) -nf $(CONTRIB_DIR)/Frameworks
	cd $(top_builddir)/bin/Contents/Resources/ && find $(abs_top_srcdir)/modules/gui/macosx/Resources/ -type f -exec $(LN_S) -f {} \;

macos-install:
	rm -Rf "$(macos_destdir)"
	mkdir "$(macos_destdir)"
	DESTDIR="$(macos_destdir)" $(MAKE) install
	touch "$(macos_destdir)"

# VLC.app for packaging and giving it to your friends
# use package-macosx to get a nice dmg
VLC.app: macos-install
	rm -Rf $@
	## Copy Contents
	cp -R "$(macos_destdir)$(datadir)/macosx/" $@
	## Copy .strings file and .nib files
	cp -R "$(top_builddir)/modules/gui/macosx/UI" $@/Contents/Resources/Base.lproj
	## Copy Info.plist and convert to binary
	cp -R "$(top_builddir)/share/macosx/Info.plist" $@/Contents/
	xcrun plutil -convert binary1 $@/Contents/Info.plist
	## Create Frameworks dir and copy required ones
	mkdir -p $@/Contents/Frameworks
if HAVE_SPARKLE
	cp -R "$(CONTRIB_DIR)/Frameworks/Sparkle.framework" $@/Contents/Frameworks
endif
if HAVE_BREAKPAD
	cp -R "$(CONTRIB_DIR)/Frameworks/Breakpad.framework" $@/Contents/Frameworks
endif
	mkdir -p $@/Contents/Resources/share/
	mkdir -p $@/Contents/MacOS/
if BUILD_LUA
	## Copy lua scripts
	cp -r "$(macos_destdir)$(pkgdatadir)/lua" $@/Contents/Resources/share/
	cp -r "$(macos_destdir)$(pkglibexecdir)/lua" $@/Contents/Frameworks/
endif
	## HRTFs
	cp -r "$(srcdir)/share/hrtfs" $@/Contents/Resources/share/
	## Copy translations
	-cp -a "$(macos_destdir)$(datadir)/locale" $@/Contents/Resources/share/
	printf "APPLVLC#" >| $@/Contents/PkgInfo
	## Copy libs
	cp -a "$(macos_destdir)$(libdir)"/libvlc*.dylib $@/Contents/Frameworks/
	## Copy plugins
	mkdir -p $@/Contents/Frameworks/plugins
	find "$(macos_destdir)$(pkglibdir)/plugins" -name 'lib*_plugin.dylib' -maxdepth 2 -exec cp -a {} $@/Contents/Frameworks/plugins \;
	## Copy libbluray jar
	-cp -a "$(CONTRIB_DIR)"/share/java/libbluray*.jar $@/Contents/Frameworks/plugins/
	## Install binary
	cp "$(macos_destdir)$(prefix)/bin/vlc" $@/Contents/MacOS/VLC
	install_name_tool -rpath "$(libdir)" "@executable_path/../Frameworks/" $@/Contents/MacOS/VLC
	## Generate plugin cache
	VLC_LIB_PATH="$@/Contents/Frameworks" bin/vlc-cache-gen $@/Contents/Frameworks/plugins
	find $@ -type d -exec chmod ugo+rx '{}' \;
	find $@ -type f -exec chmod ugo+r '{}' \;

package-macosx-sdk: macos-install
	rm -f "$(top_builddir)/vlc-macos-sdk-$(VERSION).tar.gz"
	tar -cf - --exclude "share/macosx" -C "$(macos_destdir)" . \
		| gzip -c > "$(top_builddir)/vlc-macos-sdk-$(VERSION).tar.gz"

package-macosx: VLC.app
	rm -f "$(top_builddir)/vlc-$(VERSION).dmg"
if HAVE_DMGBUILD
	@echo "Packaging fancy DMG using dmgbuild"
	cd "$(top_srcdir)/extras/package/macosx/dmg" && dmgbuild -s "dmg_settings.py" \
		-D app="$(abs_top_builddir)/VLC.app" "VLC Media Player" "$(abs_top_builddir)/vlc-$(VERSION).dmg"
else !HAVE_DMGBUILD
	@echo "Packaging non-fancy DMG"
	## Create directory for DMG contents
	mkdir -p "$(top_builddir)/vlc-$(VERSION)"
	## Copy contents
	cp -Rp "$(top_builddir)/VLC.app" "$(top_builddir)/vlc-$(VERSION)/VLC.app"
	## Symlink to Applications so users can easily drag-and-drop the App to it
	$(LN_S) -f /Applications "$(top_builddir)/vlc-$(VERSION)/"
	## Create DMG
	hdiutil create -srcfolder "$(top_builddir)/vlc-$(VERSION)" -volname "VLC Media Player" \
		-format UDBZ -fs HFS+ -o "$(top_builddir)/vlc-$(VERSION).dmg"
	## Cleanup
	rm -rf "$(top_builddir)/vlc-$(VERSION)"
endif

package-macosx-zip: VLC.app
	rm -f "$(top_builddir)/vlc-$(VERSION).zip"
	mkdir -p $(top_builddir)/vlc-$(VERSION)/Goodies/
	cp -Rp $(top_builddir)/VLC.app $(top_builddir)/vlc-$(VERSION)/VLC.app
	cd $(srcdir); cp -R AUTHORS COPYING README THANKS NEWS $(abs_top_builddir)/vlc-$(VERSION)/Goodies/
	zip -r -y -9 $(top_builddir)/vlc-$(VERSION).zip $(top_builddir)/vlc-$(VERSION)
	rm -rf "$(top_builddir)/vlc-$(VERSION)"

package-macosx-release:
	rm -f "$(top_builddir)/vlc-$(VERSION)-release.zip"
	mkdir -p $(top_builddir)/vlc-$(VERSION)-release
	cp -Rp $(top_builddir)/VLC.app $(top_builddir)/vlc-$(VERSION)-release/
	cp $(srcdir)/extras/package/macosx/dmg/* $(top_builddir)/vlc-$(VERSION)-release/
	cp "$(srcdir)/extras/package/macosx/codesign.sh" $(top_builddir)/vlc-$(VERSION)-release/
	cp "$(srcdir)/extras/package/macosx/vlc-hardening.entitlements" $(top_builddir)/vlc-$(VERSION)-release/
	cp "$(pkglibexecdir)/vlc-cache-gen" $(top_builddir)/vlc-$(VERSION)-release/
	install_name_tool -add_rpath "@executable_path/VLC.app/Contents/Frameworks" $(top_builddir)/vlc-$(VERSION)-release/vlc-cache-gen
	zip -r -y -9 $(top_builddir)/vlc-$(VERSION)-release.zip $(top_builddir)/vlc-$(VERSION)-release
	rm -rf "$(top_builddir)/vlc-$(VERSION)-release"

package-translations:
	mkdir -p "$(srcdir)/vlc-translations-$(VERSION)"
	for i in `cat "$(top_srcdir)/po/LINGUAS"`; do \
	  cp "$(srcdir)/po/$${i}.po" "$(srcdir)/vlc-translations-$(VERSION)/" ; \
	done
	cp "$(srcdir)/doc/translations.txt" "$(srcdir)/vlc-translations-$(VERSION)/README.txt"

	echo "#!/bin/sh" >>"$(srcdir)/vlc-translations-$(VERSION)/convert.po.sh"
	echo "" >>"$(srcdir)/vlc-translations-$(VERSION)/convert.po.sh"
	echo 'if test $$# != 1; then' >>"$(srcdir)/vlc-translations-$(VERSION)/convert.po.sh"
	echo "	echo \"Usage: convert-po.sh <.po file>\"" >>"$(srcdir)/vlc-translations-$(VERSION)/convert.po.sh"
	echo "	exit 1" >>"$(srcdir)/vlc-translations-$(VERSION)/convert.po.sh"
	echo "fi" >>"$(srcdir)/vlc-translations-$(VERSION)/convert.po.sh"
	echo "" >>"$(srcdir)/vlc-translations-$(VERSION)/convert.po.sh"
	echo 'msgfmt --statistics -o vlc.mo $$1' >>"$(srcdir)/vlc-translations-$(VERSION)/convert.po.sh"

	$(AMTAR) chof - $(srcdir)/vlc-translations-$(VERSION) \
	  | GZIP=$(GZIP_ENV) gzip -c >$(srcdir)/vlc-translations-$(VERSION).tar.gz

.PHONY: package-macosx package-macosx-zip package-macosx-release package-translations pseudo-bundle macos-install package-macosx-sdk

###############################################################################
# Mac OS X project
###############################################################################

EXTRA_DIST += \
	extras/package/macosx/env.build.sh \
	extras/package/macosx/build.sh \
	extras/package/macosx/codesign.sh \
	extras/package/macosx/configure.sh \
	extras/package/macosx/dmg/dmg_settings.py \
	extras/package/macosx/dmg/disk_image.icns \
	extras/package/macosx/dmg/background.tiff \
	extras/package/macosx/asset_sources/vlc_app_icon.svg \
	extras/package/macosx/VLC.entitlements \
	extras/package/macosx/VLC.xcodeproj/project.pbxproj
