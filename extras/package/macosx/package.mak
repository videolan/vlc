if HAVE_DARWIN
if BUILD_MACOSX_VLC_APP
# Create the MacOS X app
noinst_DATA = VLC.app
endif
endif

# This is just for development purposes.
# The resulting VLC-dev.app will only run in this tree.
VLC-dev.app: VLC-tmp
	rm -Rf $@
	cp -R VLC-tmp $@
	$(INSTALL) -m 0755 $(top_builddir)/bin/.libs/vlc $@/Contents/MacOS/VLC
	$(LN_S) -f ../../../modules $@/Contents/MacOS/plugins

# VLC.app for packaging and giving it to your friends
# use package-macosx to get a nice dmg
VLC.app: VLC-tmp
	rm -Rf $@
	cp -R VLC-tmp $@
	PRODUCT="$@" ACTION="release-makefile" src_dir=$(srcdir) build_dir=$(top_builddir) sh $(srcdir)/extras/package/macosx/build-package.sh
	find $@ -type d -exec chmod ugo+rx '{}' \;
	find $@ -type f -exec chmod ugo+r '{}' \;
	rm -Rf $@/Contents/Frameworks/BGHUDAppKit.framework/Versions/A/Resources/BGHUDAppKitPlugin.ibplugin
	rm -Rf $@/Contents/Frameworks/BGHUDAppKit.framework/Versions/A/Resources/README.textile


VLC-tmp: vlc
	$(AM_V_GEN)for i in src lib share; do \
		(cd $$i && $(MAKE) $(AM_MAKEFLAGS) install $(silentstd)); \
	done
	rm -Rf "$(top_builddir)/tmp" "$@"
	mkdir -p "$(top_builddir)/tmp/extras/package/macosx"
	cd $(srcdir)/extras/package/macosx; cp -R Resources README.MacOSX.rtf $(abs_top_builddir)/tmp/extras/package/macosx/
	mkdir -p $(abs_top_builddir)/tmp/extras/package/macosx/vlc.xcodeproj/
	sed -e s,../../../contrib,$(CONTRIB_DIR),g $(srcdir)/extras/package/macosx/vlc.xcodeproj/project.pbxproj \
        > $(abs_top_builddir)/tmp/extras/package/macosx/vlc.xcodeproj/project.pbxproj
	REVISION=`(git --git-dir=$(srcdir)/.git describe --always || echo exported)` && \
	    sed "s/#REVISION#/$$REVISION/g" $(top_builddir)/extras/package/macosx/Info.plist \
        > $(top_builddir)/tmp/extras/package/macosx/Info.plist
	xcrun plutil -convert binary1 $(top_builddir)/tmp/extras/package/macosx/Info.plist
	cp -R $(top_builddir)/extras/package/macosx/Resources $(top_builddir)/tmp/extras/package/macosx/
	cd "$(srcdir)"; cp AUTHORS COPYING THANKS $(abs_top_builddir)/tmp/
	mkdir -p $(top_builddir)/tmp/modules/audio_output
	mkdir -p $(top_builddir)/tmp/modules/gui/macosx
	cd "$(srcdir)/modules/gui/macosx/" && cp *.h *.m $(abs_top_builddir)/tmp/modules/gui/macosx/
	cd $(top_builddir)/tmp/extras/package/macosx && \
		xcodebuild -target vlc SYMROOT=../../../build DSTROOT=../../../build $(silentstd)
	cp -R $(top_builddir)/tmp/build/Default/VLC.bundle $@
	mkdir -p $@/Contents/Frameworks && cp -R $(CONTRIB_DIR)/Growl.framework $@/Contents/Frameworks/
	mkdir -p $@/Contents/MacOS/share/locale/
	cp -r "$(prefix)/lib/vlc/lua" "$(prefix)/share/vlc/lua" $@/Contents/MacOS/share/
	mkdir -p $@/Contents/MacOS/include/
	(cd "$(prefix)/include" && $(AMTAR) -c --exclude "plugins" vlc) | $(AMTAR) -x -C $@/Contents/MacOS/include/
	$(INSTALL) -m 644 $(srcdir)/share/vlc512x512.png $@/Contents/MacOS/share/vlc512x512.png
	cat $(top_srcdir)/po/LINGUAS | while read i; do \
	  $(INSTALL) -d $@/Contents/MacOS/share/locale/$${i}/LC_MESSAGES ; \
	  $(INSTALL) $(srcdir)/po/$${i}.gmo $@/Contents/MacOS/share/locale/$${i}/LC_MESSAGES/vlc.mo; \
	  mkdir -p $@/Contents/Resources/$${i}.lproj/ ; \
	  $(LN_S) -f ../English.lproj/InfoPlist.strings ../English.lproj/MainMenu.nib \
		$@/Contents/Resources/$${i}.lproj/ ; \
	done
	printf "APPLVLC#" >| $@/Contents/PkgInfo

package-macosx: VLC.app
	mkdir -p "$(top_builddir)/vlc-$(VERSION)/Goodies/"
	cp -R "$(top_builddir)/VLC.app" "$(top_builddir)/vlc-$(VERSION)/VLC.app"
	cd $(srcdir); cp AUTHORS COPYING README THANKS NEWS $(abs_top_builddir)/vlc-$(VERSION)/Goodies/
	cp $(srcdir)/extras/package/macosx/README.MacOSX.rtf $(top_builddir)/vlc-$(VERSION)/Read\ Me.rtf
	$(LN_S) -f /Applications $(top_builddir)/vlc-$(VERSION)/
	rm -f "$(top_builddir)/vlc-$(VERSION)-rw.dmg"
	hdiutil create -verbose -srcfolder "$(top_builddir)/vlc-$(VERSION)" "$(top_builddir)/vlc-$(VERSION)-rw.dmg" -scrub -format UDRW
	mkdir -p ./mount
	hdiutil attach -readwrite -noverify -noautoopen -mountRoot ./mount "vlc-$(VERSION)-rw.dmg"
	-osascript "$(srcdir)"/extras/package/macosx/dmg_setup.scpt "vlc-$(VERSION)"
	hdiutil detach ./mount/"vlc-$(VERSION)"
# Make sure the image is not writable
# Note: We can't directly create a read only dmg as we do the bless stuff
	rm -f "$(top_builddir)/vlc-$(VERSION).dmg"
	hdiutil convert "$(top_builddir)/vlc-$(VERSION)-rw.dmg" -format UDBZ -o "$(top_builddir)/vlc-$(VERSION).dmg"
	ls -l "$(top_builddir)/vlc-$(VERSION).dmg"
	rm -f "$(top_builddir)/vlc-$(VERSION)-rw.dmg"
	rm -rf "$(top_builddir)/vlc-$(VERSION)"

package-macosx-zip: VLC.app
	mkdir -p $(top_builddir)/vlc-$(VERSION)/Goodies/
	cp -R $(top_builddir)/VLC.app $(top_builddir)/vlc-$(VERSION)/VLC.app
	cd $(srcdir); cp -R AUTHORS COPYING README THANKS NEWS $(abs_top_builddir)/vlc-$(VERSION)/Goodies/
	cp $(srcdir)/extras/package/macosx/README.MacOSX.rtf $(top_builddir)/vlc-$(VERSION)/Read\ Me.rtf
	zip -r -y -9 $(top_builddir)/vlc-$(VERSION).zip $(top_builddir)/vlc-$(VERSION)
	rm -rf "$(top_builddir)/vlc-$(VERSION)"

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

.PHONY: package-macosx package-macosx-zip package-translations
