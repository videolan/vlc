if HAVE_DARWIN
if BUILD_MACOSX_VLC_APP
# Create the MacOS X app
noinst_DATA = VLC.app
endif
endif

# This is just for development purposes.
# The resulting VLC.app will only run in this tree.
VLC.app: VLC-tmp.app
	rm -Rf $@
	cp -R VLC-tmp.app $@
	$(INSTALL) -m 0755 $(top_builddir)/bin/.libs/vlc $@/Contents/MacOS/VLC
	$(LN_S) -f ../../../modules $@/Contents/MacOS/plugins
	rm -Rf VLC-tmp.app

# VLC-release.app for packaging and giving it to your friends
# use package-macosx to get a nice dmg
VLC-release.app: VLC-tmp.app
	rm -Rf $@
	cp -R VLC-tmp.app $@
	PRODUCT="$@" ACTION="release-makefile" src_dir=$(srcdir) build_dir=$(top_builddir) sh $(srcdir)/projects/macosx/framework/Pre-Compile.sh
	find $@ -type d -exec chmod ugo+rx '{}' \;
	find $@ -type f -exec chmod ugo+r '{}' \;
	rm -Rf $@/Contents/Frameworks/BGHUDAppKit.framework/Resources/
	rm -Rf VLC-tmp.app

# common target to a VLC bundle used by both the dev and the release build
VLC-tmp.app: vlc
	$(AM_V_GEN)(cd src && $(MAKE) $(AM_MAKEFLAGS) install $(silentstd))
	(cd lib && $(MAKE) $(AM_MAKEFLAGS) install $(silentstd))
	rm -Rf "$(top_builddir)/tmp" "$@"
	mkdir -p "$(top_builddir)/tmp/extras/package/macosx"
	cd $(srcdir)/extras/package/macosx; cp -R Resources README.MacOSX.rtf $(abs_top_builddir)/tmp/extras/package/macosx/
	mkdir -p $(abs_top_builddir)/tmp/extras/package/macosx/vlc.xcodeproj/
	sed -e s,../../../contrib,$(CONTRIB_DIR),g $(srcdir)/extras/package/macosx/vlc.xcodeproj/project.pbxproj \
        > $(abs_top_builddir)/tmp/extras/package/macosx/vlc.xcodeproj/project.pbxproj
	REVISION=`(git --git-dir=$(srcdir)/.git describe --always || echo exported)` && \
	    sed "s/#REVISION#/$$REVISION/g" $(top_builddir)/extras/package/macosx/Info.plist \
        > $(top_builddir)/tmp/extras/package/macosx/Info.plist
	cp -R $(top_builddir)/extras/package/macosx/Resources $(top_builddir)/tmp/extras/package/macosx/
	cd "$(srcdir)"; cp AUTHORS COPYING THANKS $(abs_top_builddir)/tmp/
	mkdir -p $(top_builddir)/tmp/modules/audio_output
	mkdir -p $(top_builddir)/tmp/modules/gui/macosx
	cd "$(srcdir)/modules/gui/macosx/" && cp *.h *.m $(abs_top_builddir)/tmp/modules/gui/macosx/
	cd $(top_builddir)/tmp/extras/package/macosx && \
		xcodebuild -target vlc SYMROOT=../../../build DSTROOT=../../../build $(silentstd)
	cp -R -L $(top_builddir)/tmp/build/Default/VLC.bundle $@
	mkdir -p $@/Contents/Frameworks && cp -R -L $(CONTRIB_DIR)/Growl.framework $@/Contents/Frameworks/
	mkdir -p $@/Contents/MacOS/share/locale/
	cp -r $(srcdir)/share/lua $@/Contents/MacOS/share/
	$(INSTALL) -m 644 $(srcdir)/share/vlc512x512.png $@/Contents/MacOS/share/vlc512x512.png
	cat $(top_srcdir)/po/LINGUAS | while read i; do \
	  $(INSTALL) -d $@/Contents/MacOS/share/locale/$${i}/LC_MESSAGES ; \
	  $(INSTALL) $(srcdir)/po/$${i}.gmo $@/Contents/MacOS/share/locale/$${i}/LC_MESSAGES/vlc.mo; \
	  mkdir -p $@/Contents/Resources/$${i}.lproj/ ; \
	  $(LN_S) -f ../English.lproj/InfoPlist.strings ../English.lproj/MainMenu.nib \
		$@/Contents/Resources/$${i}.lproj/ ; \
	done
	printf "APPLVLC#" >| $@/Contents/PkgInfo

package-macosx: VLC-release.app
	mkdir -p "$(top_builddir)/vlc-$(VERSION)/Goodies/"
	cp -R "$(top_builddir)/VLC-release.app" "$(top_builddir)/vlc-$(VERSION)/VLC.app"
	cd $(srcdir); cp AUTHORS COPYING README THANKS NEWS $(abs_top_builddir)/vlc-$(VERSION)/Goodies/
	cp -R  $(srcdir)/extras/package/macosx/Delete_Preferences.app $(top_builddir)/vlc-$(VERSION)/Goodies/Delete\ VLC\ Preferences.app
	cp $(srcdir)/extras/package/macosx/README.MacOSX.rtf $(top_builddir)/vlc-$(VERSION)/Read\ Me.rtf
	mkdir -p $(top_builddir)/vlc-$(VERSION)/.background/
	cp $(srcdir)/extras/package/macosx/Resources/about_bg.png $(top_builddir)/vlc-$(VERSION)/.background/background.png
	$(LN_S) -f /Applications $(top_builddir)/vlc-$(VERSION)/
	rm -f "$(top_builddir)/vlc-$(VERSION)-rw.dmg"
	hdiutil create -verbose -srcfolder "$(top_builddir)/vlc-$(VERSION)" "$(top_builddir)/vlc-$(VERSION)-rw.dmg" -scrub
# Make sure the image is not writable
# Note: We can't directly create a read only dmg as we do the bless stuff
	rm -f "$(top_builddir)/vlc-$(VERSION).dmg"
	hdiutil convert "$(top_builddir)/vlc-$(VERSION)-rw.dmg" -format UDBZ -o "$(top_builddir)/vlc-$(VERSION).dmg"
	ls -l "$(top_builddir)/vlc-$(VERSION).dmg"

package-macosx-zip: VLC-release.app
	mkdir -p $(top_builddir)/vlc-$(VERSION)/Goodies/
	cp -R $(top_builddir)/VLC-release.app $(top_builddir)/vlc-$(VERSION)/VLC.app
	cd $(srcdir); cp -R AUTHORS COPYING README THANKS NEWS extras/package/macosx/Delete_Preferences.app/ \
		$(abs_top_builddir)/vlc-$(VERSION)/Goodies/
	cp $(srcdir)/extras/package/macosx/README.MacOSX.rtf $(top_builddir)/vlc-$(VERSION)/Read\ Me.rtf
	zip -r -y -9 $(top_builddir)/vlc-$(VERSION).zip $(top_builddir)/vlc-$(VERSION)

package-macosx-framework-zip:
	mkdir -p $(top_builddir)/vlckit-$(VERSION)/Goodies/
	cp -R $(srcdir)/projects/macosx/framework/build/Debug/VLCKit.framework $(top_builddir)/vlckit-$(VERSION)/
	cd $(srcdir); cp AUTHORS COPYING README THANKS NEWS $(abs_top_builddir)/vlckit-$(VERSION)/Goodies/
	zip -r -y -9 $(top_builddir)/vlckit-$(VERSION).zip $(top_builddir)/vlckit-$(VERSION)

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

.PHONY: package-macosx package-macosx-zip package-macosx-framework-zip package-translations
