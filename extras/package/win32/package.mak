if HAVE_WIN32
BUILT_SOURCES_distclean += \
	extras/package/win32/NSIS/vlc.win32.nsi extras/package/win32/NSIS/spad.nsi
endif

win32_destdir=$(top_builddir)/vlc-$(VERSION)
win32_debugdir=$(abs_top_builddir)/symbols-$(VERSION)

7ZIP_OPTS=-t7z -m0=lzma -mx=9 -mfb=64 -md=32m -ms=on


if HAVE_WIN32
include extras/package/npapi.am

build-npapi: package-win-install
endif

if HAVE_WIN64
WINVERSION=vlc-$(VERSION)-win64
else
WINVERSION=vlc-$(VERSION)-win32
endif

package-win-install:
	$(MAKE) install
	cp '$(DESTDIR)$(libdir)/libvlc.dll.a' '$(DESTDIR)$(libdir)/libvlc.lib'
	cp '$(DESTDIR)$(libdir)/libvlccore.dll.a' '$(DESTDIR)$(libdir)/libvlccore.lib'
if ENABLE_PDB
	cp lib/.libs/libvlc.pdb '$(DESTDIR)$(bindir)'
	cp src/.libs/libvlccore.pdb '$(DESTDIR)$(bindir)'
	find '$(DESTDIR)$(libdir)/vlc/plugins' -name "*.dll" -exec sh -c "echo {} | sed -e 's@$(DESTDIR)$(libdir)/vlc/plugins/\(.*\)/\(.*\).dll@modules/.libs/\2.pdb $(DESTDIR)$(libdir)/vlc/plugins/\1@' | xargs -t cp " \;
endif
	touch $@

package-win-sdk: package-win-install
	mkdir -p "$(win32_destdir)/sdk/lib/"
	cp -r $(prefix)/include "$(win32_destdir)/sdk"
	cp -r $(prefix)/lib/pkgconfig "$(win32_destdir)/sdk/lib"
	$(DLLTOOL) -D libvlc.dll -l "$(win32_destdir)/sdk/lib/libvlc.lib" -d "$(top_builddir)/lib/.libs/libvlc.dll.def"
	cp -rv "$(win32_destdir)/sdk/lib/libvlc.lib" "$(win32_destdir)/sdk/lib/libvlc.dll.a"
	$(DLLTOOL) -D libvlccore.dll -l "$(win32_destdir)/sdk/lib/libvlccore.lib" -d "$(top_builddir)/src/.libs/libvlccore.dll.def"
	cp -rv "$(win32_destdir)/sdk/lib/libvlccore.lib" "$(win32_destdir)/sdk/lib/libvlccore.dll.a"

package-win-common: package-win-install package-win-sdk
# Executables, major libs
	find $(prefix) -maxdepth 4 \( -name "*$(LIBEXT)" -o -name "*$(EXEEXT)" \) -exec cp {} "$(win32_destdir)/" \;
if ENABLE_PDB
	for dir in bin lib src; do \
	  cp $$dir/.libs/*.pdb $(win32_destdir); \
	done
endif

# Text files, clean them from mail addresses
	for file in AUTHORS THANKS ; \
		do sed 's/@/_AT_/' < "$(srcdir)/$$file" > "$(win32_destdir)/$${file}.txt"; \
	done
	for file in NEWS COPYING README; \
		do cp "$(srcdir)/$$file" "$(win32_destdir)/$${file}.txt"; \
	done

	cp $(srcdir)/share/icons/vlc.ico $(win32_destdir)
	for plugindir in $(prefix)/lib/vlc/plugins/*/; do \
		plugin_destdir="$(win32_destdir)/plugins/`basename $$plugindir`"; \
		mkdir -p "$$plugin_destdir"; \
		find "$$plugindir" -type f \( -not -name '*.la' -and -not -name '*.a' \) -exec cp -v "{}" "$$plugin_destdir" \; ;\
	done
if ENABLE_PDB
	for plugindir in $(prefix)/lib/vlc/plugins/*/; do \
		plugin_destdir="$(win32_destdir)/plugins/`basename $$plugindir`"; \
		for plugin in $$(find "$$plugindir" -type f \( -not -name '*.la' -and -not -name '*.a' \)); do cp modules/.libs/$$(basename "$$plugin" | sed s/dll/pdb/) "$$plugin_destdir"; done; \
	done
endif
	-cp -r $(prefix)/share/locale $(win32_destdir)

# BD-J JAR
	-cp $(CONTRIB_DIR)/share/java/*.jar $(win32_destdir)/plugins/access/

if BUILD_LUA
	mkdir -p $(win32_destdir)/lua/
	cp -r $(pkglibexecdir)/lua/* $(win32_destdir)/lua/
	cp -r $(prefix)/share/vlc/lua/* $(win32_destdir)/lua/
endif

if BUILD_SKINS
	rm -fr $(win32_destdir)/skins
	cp -r $(prefix)/share/vlc/skins2 $(win32_destdir)/skins
endif

# HRTF
	cp -r $(srcdir)/share/hrtfs $(win32_destdir)/

# Convert to DOS line endings
	find $(win32_destdir) -type f \( -name "*xml" -or -name "*html" -or -name '*js' -or -name '*css' -or -name '*hosts' -or -iname '*txt' -or -name '*.cfg' -or -name '*.lua' \) -exec $(U2D) -q {} \;

package-win-activex: build-npapi
	cp "$(top_builddir)/npapi-vlc/installed/lib/axvlc.dll" "$(win32_destdir)/"
	mkdir -p "$(win32_destdir)/sdk/activex/"
	cp $(top_builddir)/npapi-vlc/activex/README.TXT $(top_builddir)/npapi-vlc/share/test/test.html $(win32_destdir)/sdk/activex/

package-win-strip: package-win-common package-win-activex
	mkdir -p "$(win32_debugdir)"/
	find $(win32_destdir) -type f \( -name '*$(LIBEXT)' -or -name '*$(EXEEXT)' \) | while read i; \
	do if test -n "$$i" ; then \
	    $(OBJCOPY) --only-keep-debug "$$i" "$(win32_debugdir)/`basename $$i.dbg`"; \
	    $(OBJCOPY) --strip-all "$$i" ; \
	    $(OBJCOPY) --add-gnu-debuglink="$(win32_debugdir)/`basename $$i.dbg`" "$$i" ; \
	  fi ; \
	done

$(win32_destdir)/NSIS/nsProcess.dll: extras/package/win32/NSIS/nsProcess/nsProcess.c extras/package/win32/NSIS/nsProcess/pluginapi.c
	mkdir -p "$(win32_destdir)/NSIS/"
if HAVE_WIN64
	i686-w64-mingw32-gcc $^ -shared -o $@ -lole32 -static-libgcc -D_UNICODE=1 -DUNICODE=1
	i686-w64-mingw32-strip $@
else
	$(CC) $^ -D_WIN32_IE=0x0601 -shared -o $@ -lole32 -static-libgcc -D_UNICODE=1 -DUNICODE=1
	$(STRIP) $@
endif

if HAVE_MAKENSIS
package-win32-exe: package-win-strip $(win32_destdir)/NSIS/nsProcess.dll extras/package/win32/NSIS/vlc.win32.nsi
# Script installer
	cp    $(top_builddir)/extras/package/win32/NSIS/vlc.win32.nsi "$(win32_destdir)/"
	cp    $(top_builddir)/extras/package/win32/NSIS/spad.nsi      "$(win32_destdir)/"
	cp -r $(srcdir)/extras/package/win32/NSIS/languages    "$(win32_destdir)/"
	cp -r $(srcdir)/extras/package/win32/NSIS/helpers      "$(win32_destdir)/"
	cp "$(top_srcdir)/extras/package/win32/NSIS/nsProcess.nsh" "$(win32_destdir)/NSIS/"
	cp "$(top_srcdir)/extras/package/win32/NSIS/vlc_branding.bmp" "$(win32_destdir)/NSIS/"

# Create package
	$(MAKENSIS) "$(win32_destdir)/spad.nsi"
	$(MAKENSIS) "$(win32_destdir)/vlc.win32.nsi"
else
package-win32-exe:
	@echo "makensis require to build NSIS installer not found or too old"; exit 1;
endif

package-win32-zip: package-win-strip
	rm -f -- $(WINVERSION).zip
	zip -r -9 $(WINVERSION).zip vlc-$(VERSION) --exclude \*.nsi \*NSIS\* \*languages\* \*sdk\* \*helpers\* spad\*

package-win32-debug-zip: package-win-common
	rm -f -- $(WINVERSION)-debug.zip
	zip -r -9 $(WINVERSION)-debug.zip vlc-$(VERSION)

package-win32-7zip: package-win-strip
	$(SEVENZIP) a $(7ZIP_OPTS) $(WINVERSION).7z vlc-$(VERSION)

package-win32-debug-7zip: package-win-common
	$(SEVENZIP) a $(7ZIP_OPTS) $(WINVERSION)-debug.7z vlc-$(VERSION)

package-win32-cleanup:
	rm -Rf $(win32_destdir) $(win32_debugdir)

package-win32: package-win32-zip package-win32-7zip package-win32-exe

package-win32-debug: package-win32-debug-zip package-win32-debug-7zip

package-win32-release: package-win-strip $(win32_destdir)/NSIS/nsProcess.dll package-win-sdk
	cp    $(top_builddir)/extras/package/win32/NSIS/vlc.win32.nsi "$(win32_destdir)/"
	cp    $(top_builddir)/extras/package/win32/NSIS/spad.nsi      "$(win32_destdir)/"
	cp -r $(srcdir)/extras/package/win32/NSIS/languages    		  "$(win32_destdir)/"
	cp -r $(srcdir)/extras/package/win32/NSIS/helpers      		  "$(win32_destdir)/"
	cp "$(top_srcdir)/extras/package/win32/NSIS/nsProcess.nsh" "$(win32_destdir)/NSIS/"
	cp "$(top_srcdir)/extras/package/win32/NSIS/vlc_branding.bmp" "$(win32_destdir)/NSIS/"

	mkdir -p "$(win32_destdir)/msi/"
	cp    $(top_builddir)/extras/package/win32/msi/config.wxi	  "$(win32_destdir)/msi/"
	cp    $(top_srcdir)/extras/package/win32/msi/axvlc.wxs		  "$(win32_destdir)/msi/"
	cp    $(top_srcdir)/extras/package/win32/msi/bannrbmp.bmp	  "$(win32_destdir)/msi/"
	cp    $(top_srcdir)/extras/package/win32/msi/extensions.wxs	  "$(win32_destdir)/msi/"
	cp    $(top_srcdir)/extras/package/win32/msi/LICENSE.rtf	  "$(win32_destdir)/msi/"
	cp    $(top_srcdir)/extras/package/win32/msi/product.wxs	  "$(win32_destdir)/msi/"

	$(SEVENZIP) a $(7ZIP_OPTS) $(WINVERSION)-release.7z $(win32_debugdir) "$(win32_destdir)/"

#######
# WinCE
#######
package-wince: package-win-strip
	rm -f -- vlc-$(VERSION)-wince.zip
	zip -r -9 vlc-$(VERSION)-wince.zip vlc-$(VERSION)

.PHONY: package-win-install package-win-common package-win-strip package-win32-exe package-win32-zip package-win32-debug-zip package-win32-7zip package-win32-debug-7zip package-win32-cleanup package-win32 package-win32-debug package-wince

EXTRA_DIST += \
	extras/package/win32/vlc.exe.manifest \
	extras/package/win32/libvlc.dll.manifest \
	extras/package/win32/configure.sh \
	extras/package/win32/NSIS/vlc.win32.nsi.in \
	extras/package/win32/NSIS/spad.nsi.in \
	extras/package/win32/NSIS/vlc_branding.bmp \
	extras/package/win32/NSIS/languages/BengaliExtra.nsh \
	extras/package/win32/NSIS/languages/BasqueExtra.nsh \
	extras/package/win32/NSIS/languages/PortugueseBRExtra.nsh \
	extras/package/win32/NSIS/languages/BulgarianExtra.nsh \
	extras/package/win32/NSIS/languages/CatalanExtra.nsh \
	extras/package/win32/NSIS/languages/DanishExtra.nsh \
	extras/package/win32/NSIS/languages/DutchExtra.nsh \
	extras/package/win32/NSIS/languages/EnglishExtra.nsh \
	extras/package/win32/NSIS/languages/EstonianExtra.nsh \
	extras/package/win32/NSIS/languages/FinnishExtra.nsh \
	extras/package/win32/NSIS/languages/FrenchExtra.nsh \
	extras/package/win32/NSIS/languages/GalicianExtra.nsh \
	extras/package/win32/NSIS/languages/GermanExtra.nsh \
	extras/package/win32/NSIS/languages/HebrewExtra.nsh \
	extras/package/win32/NSIS/languages/HungarianExtra.nsh \
	extras/package/win32/NSIS/languages/ItalianExtra.nsh \
	extras/package/win32/NSIS/languages/JapaneseExtra.nsh \
	extras/package/win32/NSIS/languages/LithuanianExtra.nsh \
	extras/package/win32/NSIS/languages/OccitanExtra.nsh \
	extras/package/win32/NSIS/languages/PolishExtra.nsh \
	extras/package/win32/NSIS/languages/PunjabiExtra.nsh \
	extras/package/win32/NSIS/languages/RussianExtra.nsh \
	extras/package/win32/NSIS/languages/RomanianExtra.nsh \
	extras/package/win32/NSIS/languages/SimpChineseExtra.nsh \
	extras/package/win32/NSIS/languages/SlovakExtra.nsh \
	extras/package/win32/NSIS/languages/SlovenianExtra.nsh \
	extras/package/win32/NSIS/languages/SoraniExtra.nsh \
	extras/package/win32/NSIS/languages/SpanishExtra.nsh \
	extras/package/win32/NSIS/languages/SwedishExtra.nsh \
	extras/package/win32/NSIS/languages/AfrikaansExtra.nsh \
	extras/package/win32/NSIS/languages/AlbanianExtra.nsh \
	extras/package/win32/NSIS/languages/CroatianExtra.nsh \
	extras/package/win32/NSIS/languages/IcelandicExtra.nsh \
	extras/package/win32/NSIS/languages/LatvianExtra.nsh \
	extras/package/win32/NSIS/languages/IndonesianExtra.nsh


