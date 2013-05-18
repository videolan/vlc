if HAVE_WIN32
BUILT_SOURCES_distclean += \
	extras/package/win32/NSIS/vlc.win32.nsi extras/package/win32/NSIS/spad.nsi
endif

win32_destdir=$(abs_top_builddir)/vlc-$(VERSION)
win32_debugdir=$(abs_top_builddir)/symbols-$(VERSION)
win32_xpi_destdir=$(abs_top_builddir)/vlc-plugin-$(VERSION)

7Z_OPTS=-t7z -m0=lzma -mx=9 -mfb=64 -md=32m -ms=on


if HAVE_WIN32
include extras/package/npapi.am
endif

if HAVE_WIN64
WINVERSION=vlc-$(VERSION)-win64
else
WINVERSION=vlc-$(VERSION)-win32
endif

package-win-install:
	$(MAKE) install
	touch $@


package-win-common: package-win-install build-npapi
	mkdir -p "$(win32_destdir)"/

# Executables, major libs+manifests
	find $(prefix) -maxdepth 4 \( -name "*$(LIBEXT)" -o -name "*$(EXEEXT)" \) -exec cp {} "$(win32_destdir)/" \;
	cd $(top_srcdir)/extras/package/win32 && cp vlc$(EXEEXT).manifest libvlc$(LIBEXT).manifest "$(win32_destdir)/"

# Text files, clean them from mail addresses
	for file in AUTHORS THANKS ; \
		do sed 's/@/_AT_/' < "$(srcdir)/$$file" > "$(win32_destdir)/$${file}.txt"; \
	done
	for file in NEWS COPYING README; \
		do cp "$(srcdir)/$$file" "$(win32_destdir)/$${file}.txt"; \
	done

	cp $(srcdir)/share/icons/vlc.ico $(win32_destdir)
	cp -r $(prefix)/lib/vlc/plugins $(win32_destdir)
	-cp -r $(prefix)/share/locale $(win32_destdir)

if BUILD_LUA
	mkdir -p $(win32_destdir)/lua/
	cp -r $(prefix)/lib/vlc/lua/* $(prefix)/share/vlc/lua/* $(win32_destdir)/lua/
endif

if BUILD_SKINS
	rm -fr $(win32_destdir)/skins
	cp -r $(prefix)/share/vlc/skins2 $(win32_destdir)/skins
endif

	cp "$(top_builddir)/npapi-vlc/activex/axvlc.dll.manifest" "$(win32_destdir)/"
	cp "$(top_builddir)/npapi-vlc/installed/lib/axvlc.dll" "$(win32_destdir)/"
	cp "$(top_builddir)/npapi-vlc/npapi/package/npvlc.dll.manifest" "$(win32_destdir)/"
	cp "$(top_builddir)/npapi-vlc/installed/lib/npvlc.dll" "$(win32_destdir)/"

# Compiler shared DLLs, when using compilers built with --enable-shared
# The shared DLLs may not necessarily be in the first LIBRARY_PATH, we
# should check them all.
	library_path_list=`$(CXX) -v /dev/null 2>&1 | grep ^LIBRARY_PATH|cut -d= -f2` ;\
	IFS=':' ;\
	for x in $$library_path_list ;\
	do \
		cp "$$x/libstdc++-6.dll" "$$x/libgcc_s_sjlj-1.dll" "$(win32_destdir)/" ; true ;\
	done

# SDK
	mkdir -p "$(win32_destdir)/sdk/lib/"
	cp -r $(prefix)/include "$(win32_destdir)/sdk"
	cp -r $(prefix)/lib/pkgconfig "$(win32_destdir)/sdk/lib"
	cd $(prefix)/lib && cp -rv libvlc.la libvlccore.la "$(win32_destdir)/sdk/lib/"
	cd $(prefix)/lib && cp -rv libvlc.dll.a "$(win32_destdir)/sdk/lib/libvlc.lib"
	cd $(prefix)/lib && cp -rv libvlccore.dll.a "$(win32_destdir)/sdk/lib/libvlccore.lib"
	$(DLLTOOL) -D libvlc.dll -l "$(win32_destdir)/sdk/lib/libvlc.lib" -d "$(top_builddir)/lib/.libs/libvlc.dll.def" "$(prefix)/bin/libvlc.dll"
	$(DLLTOOL) -D libvlccore.dll -l "$(win32_destdir)/sdk/lib/libvlccore.lib" -d "$(top_builddir)/src/.libs/libvlccore.dll.def" "$(prefix)/bin/libvlccore.dll"

	mkdir -p "$(win32_destdir)/sdk/activex/"
	cd $(top_builddir)/npapi-vlc && cp activex/README.TXT share/test/test.html $(win32_destdir)/sdk/activex/

# Convert to DOS line endings
	find $(win32_destdir) -type f \( -name "*xml" -or -name "*html" -or -name '*js' -or -name '*css' -or -name '*hosts' -or -iname '*txt' -or -name '*.cfg' -or -name '*.lua' \) -exec $(U2D) {} \;

# Remove cruft
	find $(win32_destdir)/plugins/ -type f \( -name '*.a' -or -name '*.la' \) -exec rm -rvf {} \;


package-win-strip: package-win-common
	mkdir -p "$(win32_debugdir)"/
	cd $(win32_destdir); find . -type f \( -name '*$(LIBEXT)' -or -name '*$(EXEEXT)' \) | while read i; \
	do if test -n "$$i" ; then \
	    $(OBJCOPY) --only-keep-debug "$$i" "$(win32_debugdir)/`basename $$i.dbg`"; \
	    $(OBJCOPY) --strip-all "$$i" ; \
	    $(OBJCOPY) --add-gnu-debuglink="$(win32_debugdir)/`basename $$i.dbg`" "$$i" ; \
	  fi ; \
	done


package-win32-webplugin-common: package-win-strip
	mkdir -p "$(win32_xpi_destdir)/"
	cp -r $(win32_destdir)/plugins/ "$(win32_xpi_destdir)/"
	find $(prefix) -maxdepth 4 -name "*$(LIBEXT)" -exec cp {} "$(win32_xpi_destdir)/" \;
	cp $(top_builddir)/npapi-vlc/npapi/package/npvlc.dll.manifest "$(win32_xpi_destdir)/plugins/"
	cp "$(top_srcdir)/extras/package/win32/libvlc.dll.manifest" "$(win32_xpi_destdir)/plugins/"
	rm -rf "$(win32_xpi_destdir)/plugins/gui/"


package-win32-xpi: package-win32-webplugin-common
	cp $(top_builddir)/npapi-vlc/npapi/package/install.rdf "$(win32_xpi_destdir)/"
	cd $(win32_xpi_destdir) && zip -r -9 "../vlc-$(VERSION).xpi" install.rdf plugins


package-win32-crx: package-win32-webplugin-common
	cp $(top_builddir)/npapi-vlc/npapi/package/manifest.json "$(win32_xpi_destdir)/"
	crxmake --pack-extension "$(win32_xpi_destdir)" \
		--extension-output "$(win32_destdir)/vlc-$(VERSION).crx" --ignore-file install.rdf


# nsis is a 32-bits installer, we need to build a 32bits DLL
$(win32_destdir)/NSIS/UAC.dll: extras/package/win32/NSIS/UAC/runas.cpp extras/package/win32/NSIS/UAC/uac.cpp
	mkdir -p "$(win32_destdir)/NSIS/"
if HAVE_WIN64
	i686-w64-mingw32-g++ $^ -shared -o $@ -lole32 -static-libstdc++ -static-libgcc
	i686-w64-mingw32-strip $@
else
	$(CXX) $^ -D_WIN32_IE=0x0601 -D__forceinline=inline -shared -o $@ -lole32 -static-libstdc++ -static-libgcc
	$(STRIP) $@
endif


package-win32-exe: package-win-strip $(win32_destdir)/NSIS/UAC.dll
# Script installer
	cp    $(top_builddir)/extras/package/win32/NSIS/vlc.win32.nsi "$(win32_destdir)/"
	cp    $(top_builddir)/extras/package/win32/NSIS/spad.nsi      "$(win32_destdir)/"
	cp -r $(srcdir)/extras/package/win32/NSIS/languages/    "$(win32_destdir)/"
	cp -r $(srcdir)/extras/package/win32/NSIS/helpers/      "$(win32_destdir)/"
	mkdir -p "$(win32_destdir)/NSIS/"
	cp "$(top_srcdir)/extras/package/win32/NSIS/UAC.nsh" "$(win32_destdir)/NSIS/"

# Create package
	if makensis -VERSION >/dev/null 2>&1; then \
	    MAKENSIS="makensis"; \
	elif [ -x "/cygdrive/c/Program Files/NSIS/makensis" ]; then \
	    MAKENSIS="/cygdrive/c/Program\ Files/NSIS/makensis"; \
	elif [ -x "$(PROGRAMFILES)/NSIS/makensis" ]; then \
	    MAKENSIS="$(PROGRAMFILES)/NSIS/makensis"; \
	elif wine --version >/dev/null 2>&1; then \
	    MAKENSIS="wine C:/Program\ Files/NSIS/makensis.exe"; \
	else \
	    echo 'Error: cannot locate makensis tool'; exit 1; \
	fi; \
	eval "$$MAKENSIS $(win32_destdir)/spad.nsi"; \
	eval "$$MAKENSIS $(win32_destdir)/vlc.win32.nsi"

package-win32-zip: package-win-strip
	rm -f -- $(WINVERSION).zip
	zip -r -9 $(WINVERSION).zip vlc-$(VERSION) --exclude \*.nsi \*NSIS\* \*languages\* \*sdk\* \*helpers\* spad\*

package-win32-debug-zip: package-win-common
	rm -f -- $(WINVERSION)-debug.zip
	zip -r -9 $(WINVERSION)-debug.zip vlc-$(VERSION)

package-win32-7zip: package-win-strip
	7z a $(7Z_OPTS) $(WINVERSION).7z vlc-$(VERSION)

package-win32-debug-7zip: package-win-common
	7z a $(7Z_OPTS) $(WINVERSION)-debug.7z vlc-$(VERSION)

package-win32-cleanup:
	rm -Rf $(win32_destdir) $(win32_debugdir) $(win32_xpi_destdir)

package-win32: package-win32-zip package-win32-7zip package-win32-exe package-win32-xpi

package-win32-debug: package-win32-debug-zip package-win32-debug-7zip


#######
# WinCE
#######
package-wince: package-win-strip
	rm -f -- vlc-$(VERSION)-wince.zip
	zip -r -9 vlc-$(VERSION)-wince.zip vlc-$(VERSION)

.PHONY: package-win-install package-win-common package-win-strip package-win32-webplugin-common package-win32-xpi package-win32-crx package-win32-exe package-win32-zip package-win32-debug-zip package-win32-7zip package-win32-debug-7zip package-win32-cleanup package-win32 package-win32-debug package-wince
