if HAVE_DARWIN
if BUILD_MACOSX_VLC_APP
# Create the MacOS X app
noinst_DATA = VLC.app
endif
endif

# VLC-release.app for packaging and giving it to your friends
# use package-macosx to get a nice dmg
VLC-release.app: vlc
	( cd src && $(MAKE) $(AM_MAKEFLAGS) install $(silentstd) )
	( cd lib && $(MAKE) $(AM_MAKEFLAGS) install $(silentstd) )
	rm -Rf "$(top_builddir)/tmp"
	mkdir -p "$(top_builddir)/tmp/extras/package/macosx"
	rm -Rf $(top_builddir)/VLC-release.app
	for i in vlc.xcodeproj Resources README.MacOSX.rtf ; do \
	  cp -R $(srcdir)/extras/package/macosx/$$i $(top_builddir)/tmp/extras/package/macosx/; \
	done
	REVISION=`(git --git-dir=$(srcdir)/.git describe --always || echo exported)` && \
	cat $(top_builddir)/extras/package/macosx/Info.plist | \
	sed "s/#REVISION#/$$REVISION/g" > $(top_builddir)/tmp/extras/package/macosx/Info.plist
	cp -R $(top_builddir)/extras/package/macosx/Resources $(top_builddir)/tmp/extras/package/macosx/
	for i in AUTHORS COPYING THANKS; do \
	  cp "$(srcdir)/$$i" $(top_builddir)/tmp; \
	done
	mkdir -p $(top_builddir)/tmp/extras/contrib/Sparkle
	cp -R $(CONTRIB_DIR)/Sparkle/Sparkle.framework $(top_builddir)/tmp/extras/contrib/Sparkle
	mkdir -p $(top_builddir)/tmp/extras/contrib/BGHUDAppKit
	cp -R $(CONTRIB_DIR)/BGHUDAppKit/BGHUDAppKit.framework $(top_builddir)/tmp/extras/contrib/BGHUDAppKit
	mkdir -p $(top_builddir)/tmp/extras/contrib/Growl
	cp -R $(CONTRIB_DIR)/Growl/Growl.framework $(top_builddir)/tmp/extras/contrib/Growl
	mkdir -p $(top_builddir)/tmp/modules/audio_output
	mkdir -p $(top_builddir)/tmp/modules/gui/macosx
	for i in \
	    AppleRemote.h \
	    AppleRemote.m \
	    about.h \
	    about.m \
	    applescript.h \
	    applescript.m \
	    controls.h \
	    controls.m \
	    intf.h \
	    intf.m \
	    macosx.m \
	    misc.h \
	    misc.m \
	    open.h \
	    open.m \
	    output.h \
	    output.m \
	    playlist.h \
	    playlist.m \
	    playlistinfo.h \
	    playlistinfo.m \
	    prefs_widgets.h \
	    prefs_widgets.m \
	    prefs.h \
	    prefs.m \
	    simple_prefs.h \
	    simple_prefs.m \
	    wizard.h \
	    wizard.m \
	    bookmarks.h \
	    bookmarks.m \
	    coredialogs.h \
	    coredialogs.m \
	    fspanel.h \
	    fspanel.m; do \
	  cp "$(srcdir)/modules/gui/macosx/$$i" \
             $(top_builddir)/tmp/modules/gui/macosx; \
	done
	$(AM_V_GEN)cd $(top_builddir)/tmp/extras/package/macosx && \
	xcodebuild -target vlc SYMROOT=../../../build DSTROOT=../../../build $(silentstd) && \
	cd ../../../../ && \
	cp -R $(top_builddir)/tmp/build/Default/VLC.bundle $(top_builddir)/VLC-release.app; \
	rm -Rf $(top_builddir)/tmp
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS
	PRODUCT="VLC-release.app" ACTION="release-makefile" src_dir=$(srcdir) build_dir=$(top_builddir) sh $(srcdir)/projects/macosx/framework/Pre-Compile.sh
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua
	for i in $(srcdir)/share/lua/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/`basename $${i}` ; \
	done ; \
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/playlist
	for i in $(srcdir)/share/lua/playlist/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/playlist/`basename $${i}` ; \
	done ; \
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/meta
	for i in $(srcdir)/share/lua/meta/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/meta/`basename $${i}` ; \
	done ; \
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/intf
	for i in $(srcdir)/share/lua/intf/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/intf/`basename $${i}` ; \
	done ; \
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/intf/modules
	for i in $(srcdir)/share/lua/intf/modules/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/intf/modules/`basename $${i}` ; \
	done ; \
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/http/dialogs
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/http/js
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/http/images
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/http/requests
	$(INSTALL) -m 644 $(srcdir)/share/lua/http/.hosts $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/http/.hosts
	for i in $(srcdir)/share/lua/http/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/http/`basename $${i}` ; \
	done
	for i in $(srcdir)/share/lua/http/dialogs/* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/http/dialogs/`basename $${i}` ; \
	done
	for i in $(srcdir)/share/lua/http/js/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/http/js/`basename $${i}` ; \
	done
	for i in $(srcdir)/share/lua/http/images/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/http/images/`basename $${i}` ; \
	done
	for i in $(srcdir)/share/lua/http/requests/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/http/requests/`basename $${i}` ; \
	done
	$(INSTALL) -m 644 $(srcdir)/share/lua/http/requests/README.txt $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/http/requests/README.txt
	$(INSTALL) -m 644 $(srcdir)/share/vlc512x512.png $(top_builddir)/VLC-release.app/Contents/MacOS/share/vlc512x512.png
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/locale
	cat $(top_srcdir)/po/LINGUAS | while read i; do \
	  $(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/locale/$${i}/LC_MESSAGES ; \
	  $(INSTALL) $(srcdir)/po/$${i}.gmo $(top_builddir)/VLC-release.app/Contents/MacOS/share/locale/$${i}/LC_MESSAGES/vlc.mo || true ; \
	  mkdir -p $(top_builddir)/VLC-release.app/Contents/Resources/$${i}.lproj ; \
	  $(LN_S) -f ../English.lproj/InfoPlist.strings \
	      $(top_builddir)/VLC-release.app/Contents/Resources/$${i}.lproj ; \
	  $(LN_S) -f ../English.lproj/MainMenu.xib \
	      $(top_builddir)/VLC-release.app/Contents/Resources/$${i}.lproj ; \
	done
	printf "APPLVLC#" >| $(top_builddir)/VLC-release.app/Contents/PkgInfo
	rm -Rf $(top_builddir)/VLC-release.app/Contents/Frameworks/BGHUDAppKit.framework/Resources/
	find $(top_builddir)/VLC-release.app -type d -exec chmod ugo+rx '{}' \;
	find $(top_builddir)/VLC-release.app -type f -exec chmod ugo+r '{}' \;

# This is just for development purposes. 
# The resulting VLC.app will only run in this tree.
VLC.app: vlc $(top_builddir)/src/.libs/libvlccore.dylib $(top_builddir)/lib/.libs/libvlc.dylib
	$(AM_V_GEN)(cd src && make install $(silentstd))
	$(AM_V_GEN)(cd lib && make install $(silentstd))
	rm -Rf $(top_builddir)/tmp
	mkdir -p "$(top_builddir)/tmp/extras/package/macosx"
	rm -Rf $(top_builddir)/VLC.app
	for i in vlc.xcodeproj Resources README.MacOSX.rtf; do \
	  cp -R $(srcdir)/extras/package/macosx/$$i $(top_builddir)/tmp/extras/package/macosx/; \
	done
	REVISION=`(git --git-dir=$(srcdir)/.git describe --always || echo exported)` && \
	cat $(top_builddir)/extras/package/macosx/Info.plist | \
	sed "s/#REVISION#/$$REVISION/g" > $(top_builddir)/tmp/extras/package/macosx/Info.plist
	cp -R $(top_builddir)/extras/package/macosx/Resources $(top_builddir)/tmp/extras/package/macosx/
	for i in AUTHORS COPYING THANKS; do \
	  cp "$(srcdir)/$$i" $(top_builddir)/tmp; \
	done
	mkdir -p $(top_builddir)/tmp/extras/contrib/Sparkle
	cp -R $(CONTRIB_DIR)/Sparkle/Sparkle.framework $(top_builddir)/tmp/extras/contrib/Sparkle
	mkdir -p $(top_builddir)/tmp/extras/contrib/BGHUDAppKit
	cp -R $(CONTRIB_DIR)/BGHUDAppKit/BGHUDAppKit.framework $(top_builddir)/tmp/extras/contrib/BGHUDAppKit
	mkdir -p $(top_builddir)/tmp/extras/contrib/Growl
	cp -R $(CONTRIB_DIR)/Growl/Growl.framework $(top_builddir)/tmp/extras/contrib/Growl
	mkdir -p $(top_builddir)/tmp/modules/audio_output
	mkdir -p $(top_builddir)/tmp/modules/gui/macosx
	for i in \
	    AppleRemote.h \
	    AppleRemote.m \
	    about.h \
	    about.m \
	    applescript.h \
	    applescript.m \
	    controls.h \
	    controls.m \
	    intf.h \
	    intf.m \
	    macosx.m \
	    misc.h \
	    misc.m \
	    open.h \
	    open.m \
	    output.h \
	    output.m \
	    playlist.h \
	    playlist.m \
	    playlistinfo.h \
	    playlistinfo.m \
	    prefs_widgets.h \
	    prefs_widgets.m \
	    prefs.h \
	    prefs.m \
	    simple_prefs.h \
	    simple_prefs.m \
	    wizard.h \
	    wizard.m \
	    bookmarks.h \
	    bookmarks.m \
	    coredialogs.h \
	    coredialogs.m \
	    fspanel.h \
	    fspanel.m; do \
	  cp "$(srcdir)/modules/gui/macosx/$$i" \
             $(top_builddir)/tmp/modules/gui/macosx; \
	done
	$(AM_V_GEN)cd $(top_builddir)/tmp/extras/package/macosx && \
	xcodebuild -target vlc SYMROOT=../../../build DSTROOT=../../../build $(silentstd) && \
	cd ../../../../ && \
	cp -R -L $(top_builddir)/tmp/build/Default/VLC.bundle $(top_builddir)/VLC.app
	$(INSTALL) -d $(top_builddir)/VLC.app/Contents/MacOS
	touch $(top_builddir)/VLC.app/Contents/MacOS/VLC
	chmod +x $(top_builddir)/VLC.app/Contents/MacOS/VLC
	$(INSTALL) $(top_builddir)/bin/.libs/vlc $(top_builddir)/VLC.app/Contents/MacOS/VLC
	$(LN_S) -f ../../../modules $(top_builddir)/VLC.app/Contents/MacOS/plugins
	install -d $(top_builddir)/VLC.app/Contents/MacOS/share
	for i in `ls $(srcdir)/share`; do \
	   $(LN_S) -f `pwd`/$(srcdir)/share/$$i $(top_builddir)/VLC.app/Contents/MacOS/share/; \
	done
	$(INSTALL) -d $(top_builddir)/VLC.app/Contents/MacOS/share/locale
	cat $(top_srcdir)/po/LINGUAS | while read i; do \
	  mkdir -p $(top_builddir)/VLC.app/Contents/MacOS/share/locale/$${i}/LC_MESSAGES ; \
	  $(LN_S) -f `pwd`/$(srcdir)/po/$${i}.gmo $(top_builddir)/VLC.app/Contents/MacOS/share/locale/$${i}/LC_MESSAGES/vlc.mo || true ; \
	  mkdir -p $(top_builddir)/VLC.app/Contents/Resources/$${i}.lproj ; \
	  $(LN_S) -f ../English.lproj/InfoPlist.strings \
	      $(top_builddir)/VLC.app/Contents/Resources/$${i}.lproj ; \
	  $(LN_S) -f ../English.lproj/MainMenu.xib \
	      $(top_builddir)/VLC.app/Contents/Resources/$${i}.lproj ; \
	done
	printf "APPLVLC#" >| $(top_builddir)/VLC.app/Contents/PkgInfo

package-macosx: VLC-release.app ChangeLog
	mkdir -p "$(top_builddir)/vlc-$(VERSION)/"
	@if test -e "$(top_builddir)/VLC-release.app/"; then \
	  cp -R "$(top_builddir)/VLC-release.app" "$(top_builddir)/vlc-$(VERSION)/VLC.app"; \
	else \
	  cp -R "$(top_builddir)/VLC.app" "$(top_builddir)/vlc-$(VERSION)/VLC.app"; \
	fi
	mkdir -p $(top_builddir)/vlc-$(VERSION)/Goodies/
	for i in AUTHORS COPYING README THANKS NEWS; do \
	  cp $(srcdir)/$$i $(top_builddir)/vlc-$(VERSION)/Goodies/; \
	done
	cp $(top_builddir)/ChangeLog $(top_builddir)/vlc-$(VERSION)/Goodies/
	cp -R  $(srcdir)/extras/package/macosx/Delete_Preferences.app $(top_builddir)/vlc-$(VERSION)/Goodies/Delete\ VLC\ Preferences.app
	cp $(srcdir)/extras/package/macosx/README.MacOSX.rtf $(top_builddir)/vlc-$(VERSION)/Read\ Me.rtf
	mkdir -p $(top_builddir)/vlc-$(VERSION)/.background/
	cp $(srcdir)/extras/package/macosx/Resources/about_bg.png $(top_builddir)/vlc-$(VERSION)/.background/background.png
	$(LN_S) /Applications $(top_builddir)/vlc-$(VERSION)/
	rm -f "$(top_builddir)/vlc-$(VERSION)-rw.dmg"
	hdiutil create -verbose -srcfolder "$(top_builddir)/vlc-$(VERSION)" "$(top_builddir)/vlc-$(VERSION)-rw.dmg" -scrub
# Make sure the image is not writable
# Note: We can't directly create a read only dmg as we do the bless stuff
	hdiutil convert "$(top_builddir)/vlc-$(VERSION)-rw.dmg" -format UDBZ -o "$(top_builddir)/vlc-$(VERSION).dmg"
	ls -l "$(top_builddir)/vlc-$(VERSION).dmg"

package-macosx-zip: VLC-release.app
	mkdir -p $(top_builddir)/vlc-$(VERSION)
	cp -R $(top_builddir)/VLC-release.app $(top_builddir)/vlc-$(VERSION)/VLC.app
	mkdir -p $(top_builddir)/vlc-$(VERSION)/Goodies
	for i in AUTHORS COPYING ChangeLog README THANKS NEWS; do \
	  cp $(srcdir)/$$i $(top_builddir)/vlc-$(VERSION)/Goodies; \
	done
	cp -R  $(srcdir)/extras/package/macosx/Delete_Preferences.app \
	     $(top_builddir)/vlc-$(VERSION)/Goodies
	cp $(srcdir)/extras/package/macosx/README.MacOSX.rtf \
	   $(top_builddir)/vlc-$(VERSION)/Read\ Me.rtf
	zip -r -y -9 $(top_builddir)/vlc-$(VERSION).zip $(top_builddir)/vlc-$(VERSION)

package-macosx-framework-zip:
	mkdir -p $(top_builddir)/vlckit-$(VERSION)
	cp -R $(srcdir)/projects/macosx/framework/build/Debug/VLCKit.framework $(top_builddir)/vlckit-$(VERSION)/
	mkdir -p $(top_builddir)/vlc-$(VERSION)/Goodies
	for i in AUTHORS COPYING ChangeLog README THANKS NEWS; do \
	  cp $(srcdir)/$$i $(top_builddir)/vlckit-$(VERSION)/Goodies; \
	done
	zip -r -y -9 $(top_builddir)/vlckit-$(VERSION).zip $(top_builddir)/vlckit-$(VERSION)

package-translations:
	mkdir -p "$(srcdir)/vlc-translations-$(VERSION)"
	cat $(top_srcdir)/po/LINGUAS | while read i; do \
	  cp "$(srcdir)/po/$${i}.po" \
	    "$(srcdir)/vlc-translations-$(VERSION)/$${i}.po" \
	    || true ; \
	done
	cp "$(srcdir)/doc/translations.txt" \
	  "$(srcdir)/vlc-translations-$(VERSION)/README.txt"

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
