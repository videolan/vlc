if HAVE_WIN32
BUILT_SOURCES_distclean += \
	extras/package/win32/vlc.win32.nsi extras/package/win32/spad.nsi
endif

win32_destdir=$(top_builddir)/vlc-$(VERSION)
win32_debugdir=$(top_builddir)/symbols-$(VERSION)
win32_xpi_destdir=$(win32_destdir)/vlc-plugin

if HAVE_WINCE
build-npapi:
	touch $@
else
if HAVE_WIN32
include extras/package/npapi.am
endif
endif

package-win-install:
	$(MAKE) install
	touch $@

#Win-common is for win32 and wince
package-win-common: package-win-install build-npapi
	mkdir -p "$(win32_debugdir)"
	mkdir -p "$(win32_destdir)"

# Executables, major libs+manifests
	find $(prefix) -maxdepth 4 \( -name "*$(LIBEXT)" -o -name "*$(EXEEXT)" \) -exec cp {} "$(win32_destdir)/" \;
	for file in $(top_srcdir)/extras/package/win32/vlc$(EXEEXT).manifest \
                $(top_srcdir)/extras/package/win32/libvlc$(LIBEXT).manifest; \
	    do cp $$file "$(win32_destdir)/" ; done;

# Text files and clean them
	for file in AUTHORS THANKS ; \
	  do sed 's/@/_AT_/' < "$(srcdir)/$$file" > "$(win32_destdir)/$${file}.txt" ; done;
	for file in NEWS COPYING README; \
	  do cp "$(srcdir)/$$file" "$(win32_destdir)/$${file}.txt"; done

# Necessary icon
	cp $(srcdir)/share/icons/vlc.ico $(win32_destdir)

# Locales
	-cp -r $(prefix)/share/locale $(win32_destdir)

# Plugins
	cp -r $(prefix)/lib/vlc/plugins $(win32_destdir)

if BUILD_LUA
	mkdir -p $(win32_destdir)/lua
	cp -r $(prefix)/lib/vlc/lua/* $(win32_destdir)/lua
	cp -r $(prefix)/share/vlc/lua/* $(win32_destdir)/lua
endif

if BUILD_SKINS
	cp -r $(prefix)/share/vlc/skins2 $(win32_destdir)/skins
endif
if BUILD_OSDMENU
	cp -r $(prefix)/share/vlc/osdmenu "$(win32_destdir)/osdmenu"
	for file in $(win32_destdir)/osdmenu/*.cfg; do \
		sed 's%share/osdmenu%osdmenu%g' "$$file" > "$$file.tmp" || exit $$? ; \
		sed 's%/%\\%g' "$$file.tmp" > "$$file" || exit$$? ; \
		rm -f -- "$$file.tmp"; \
	done
endif

if !HAVE_WINCE
if !HAVE_WIN64
	cp "$(top_builddir)/npapi-vlc/activex/axvlc.dll.manifest" "$(win32_destdir)/"
	cp "$(top_builddir)/npapi-vlc/installed/lib/axvlc.dll" "$(win32_destdir)/"
endif
	cp "$(top_builddir)/npapi-vlc/npapi/npvlc.dll.manifest" "$(win32_destdir)/"
	cp "$(top_builddir)/npapi-vlc/installed/lib/npvlc.dll" "$(win32_destdir)/"
endif

# Compiler shared DLLs, when using compilers built with --enable-shared
# If gcc_s_sjlj/stdc++-6 DLLs exist, our C++ modules were linked to them
	gcc_lib_dir=`$(CXX) -v /dev/null 2>&1 | grep ^LIBRARY_PATH|cut -d= -f2|cut -d: -f1` ; \
	cp "$${gcc_lib_dir}/libstdc++-6.dll" "$${gcc_lib_dir}/libgcc_s_sjlj-1.dll" "$(win32_destdir)/" ; true

# SDK
	mkdir -p "$(win32_destdir)/sdk/lib"
	cp -r $(prefix)/include "$(win32_destdir)/sdk"
	cp -r $(prefix)/lib/pkgconfig "$(win32_destdir)/sdk/lib"
	for file in libvlc.dll.a libvlc.la libvlccore.dll.a libvlccore.la; do \
		cp -rv $(prefix)/lib/$$file "$(win32_destdir)/sdk/lib"; \
	done
if !HAVE_WINCE
	$(DLLTOOL) -D libvlc.dll -l "$(win32_destdir)/sdk/lib/libvlc.lib" -d "$(top_builddir)/lib/.libs/libvlc.dll.def" "$(prefix)/bin/libvlc.dll"
	$(DLLTOOL) -D libvlccore.dll -l "$(win32_destdir)/sdk/lib/libvlccore.lib" -d "$(top_builddir)/src/.libs/libvlccore.dll.def" "$(prefix)/bin/libvlccore.dll"

if !HAVE_WIN64
	mkdir -p "$(win32_destdir)/sdk/activex"
	cp $(top_builddir)/npapi-vlc/activex/README.TXT $(win32_destdir)/sdk/activex/README.TXT
	cp $(top_builddir)/npapi-vlc/share/test.html $(win32_destdir)/sdk/activex/
endif
endif

	find $(win32_destdir) -type f \( -name "*xml" -or -name "*html" -or -name '*js' -or -name '*css' -or -name '*hosts' -or -iname '*txt' -or -name '*.cfg' -or -name '*.lua' \) -exec $(U2D) {} \;

#Enable DEP and ASLR for all the binaries
	find $(win32_destdir) -type f \( -name '*$(LIBEXT)' -print -o -name '*$(EXEEXT)' -print \) -exec $(top_srcdir)/extras/package/win32/peflags.pl {} \;
	find $(win32_destdir)/plugins/ -type f \( -name '*.a' -or -name '*.la' \) -exec rm -rvf {} \;

package-win-strip: package-win-common
	find $(win32_destdir) -type f \( -name '*$(LIBEXT)' -or -name '*$(EXEEXT)' \) | while read i; \
	do if test -n "$$i" ; then \
	    $(OBJCOPY) --only-keep-debug "$$i" "$$i.dbg"; \
	    $(OBJCOPY) --strip-all "$$i" ; \
	    $(OBJCOPY) --add-gnu-debuglink="$$i.dbg" "$$i" ; \
	    mv "$$i.dbg" "$(win32_debugdir)"; \
	  fi ; \
	done

package-win32-webplugin-common: package-win-strip
	mkdir -p "$(win32_xpi_destdir)/plugins"
	find $(prefix) -maxdepth 4 -name "*$(LIBEXT)" -exec cp {} "$(win32_xpi_destdir)/" \;
	cp $(top_builddir)/npapi-vlc/npapi/npvlc.dll.manifest "$(win32_xpi_destdir)/plugins"
	cp "$(top_srcdir)/extras/package/win32/libvlc.dll.manifest" "$(win32_xpi_destdir)/plugins"
	cp -r $(win32_destdir)/plugins/ "$(win32_xpi_destdir)/plugins"
	rm -rf "$(win32_xpi_destdir)/plugins/plugins/*qt*"
	rm -rf "$(win32_xpi_destdir)/plugins/plugins/*skins*"

package-win32-xpi: package-win32-webplugin-common
	cp $(top_builddir)/npapi-vlc/npapi/install.rdf "$(win32_xpi_destdir)"
	cd $(win32_xpi_destdir) && zip -r "../vlc-$(VERSION).xpi" install.rdf plugins

package-win32-crx: package-win32-webplugin-common
	cp $(top_builddir)/npapi-vlc/npapi/manifest.json "$(win32_xpi_destdir)"
	crxmake --pack-extension "$(win32_xpi_destdir)" \
		--extension-output "$(win32_destdir)/vlc-$(VERSION).crx" --ignore-file install.rdf

package-win32-exe: package-win-strip
# Script installer
	cp "$(top_builddir)/extras/package/win32/vlc.win32.nsi" "$(win32_destdir)/"
	cp "$(top_builddir)/extras/package/win32/spad.nsi" "$(win32_destdir)/"
	mkdir -p "$(win32_destdir)/languages"
	cp $(srcdir)/extras/package/win32/languages/*.nsh "$(win32_destdir)/languages/"
# Copy the UAC NSIS plugin
	mkdir -p "$(win32_destdir)/NSIS"
	cp "$(top_srcdir)/extras/package/win32/UAC.nsh" "$(win32_destdir)/NSIS"
	cp "$(top_srcdir)/extras/package/win32/UAC.dll" "$(win32_destdir)/NSIS"

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
	rm -f -- vlc-$(VERSION)-win32.zip
	zip -r -9 vlc-$(VERSION)-win32.zip vlc-$(VERSION)

package-win32-debug-zip: package-win-common
	rm -f -- vlc-$(VERSION)-win32-debug.zip
	zip -r -9 vlc-$(VERSION)-win32-debug.zip vlc-$(VERSION)

package-win32-7zip: package-win-strip
	7z a -t7z -m0=lzma -mx=9 -mfb=64 -md=32m -ms=on vlc-$(VERSION)-win32.7z vlc-$(VERSION)

package-win32-debug-7zip: package-win-common
	7z a -t7z -m0=lzma -mx=9 -mfb=64 -md=32m -ms=on vlc-$(VERSION)-win32-debug.7z vlc-$(VERSION)

package-win32-cleanup:
	rm -Rf $(win32_destdir) $(win32_debugdir)

package-win32: package-win32-zip package-win32-7zip package-win32-exe package-win32-xpi

package-win32-debug: package-win32-debug-zip package-win32-debug-7zip


#######
# WinCE
#######
package-wince: package-win-strip
	rm -f -- vlc-$(VERSION)-wince.zip
	zip -r -9 vlc-$(VERSION)-wince.zip vlc-$(VERSION)

.PHONY: package-win-install package-win-common package-win-strip package-win32-webplugin-common package-win32-xpi package-win32-crx package-win32-exe package-win32-zip package-win32-debug-zip package-win32-7zip package-win32-debug-7zip package-win32-cleanup package-win32 package-win32-debug package-wince

