if HAVE_WIN32
BUILT_SOURCES_distclean += \
	extras/package/win32/msi/config.wxi
endif

WIXPATH=`wine winepath -u 'C:\\Program Files (x86)\\Windows Installer XML v3.5\\bin'`
HEAT=wine "$(WIXPATH)/heat.exe"
CANDLE=wine "$(WIXPATH)/candle.exe"
LIGHT=wine "$(WIXPATH)/light.exe"
VLCDIR=`wine winepath -s \`wine winepath -w '$(win32_destdir)'\``
MSIDIR=$(abs_srcdir)/extras/package/win32/msi
W_MSIDIR=`wine winepath -w '$(MSIDIR)'`
MSIBUILDDIR=$(abs_top_builddir)/extras/package/win32/msi
W_MSIBUILDDIR=`wine winepath -w '$(MSIBUILDDIR)'`
MSIOUTFILE=vlc-$(VERSION).msi
WINE_C=`wine winepath c:`

package-msi: heat candle light

heat: package-win-strip
	$(HEAT) dir $(VLCDIR)/plugins -cg CompPluginsGroup -gg -scom -sreg -sfrag -dr APPLICATIONFOLDER -out $(W_MSIBUILDDIR)/Plugins.fragment.wxs
	$(HEAT) dir $(VLCDIR)/locale -cg CompLocaleGroup -gg -scom -sreg -sfrag -dr APPLICATIONFOLDER -out $(W_MSIBUILDDIR)/Locale.fragment.wxs
	$(HEAT) dir $(VLCDIR)/lua -cg CompLuaGroup -gg -scom -sreg -sfrag -dr APPLICATIONFOLDER -out $(W_MSIBUILDDIR)/Lua.fragment.wxs
	$(HEAT) dir $(VLCDIR)/skins -cg CompSkinsGroup -gg -scom -sreg -sfrag -dr APPLICATIONFOLDER -out $(W_MSIBUILDDIR)/Skins.fragment.wxs

candle:
	$(am__cd) $(MSIBUILDDIR) && $(CANDLE) -arch $(WINDOWS_ARCH) -ext WiXUtilExtension $(W_MSIDIR)\\product.wxs $(W_MSIDIR)\\axvlc.wxs $(W_MSIDIR)\\extensions.wxs $(W_MSIBUILDDIR)\\*.fragment.wxs

light:
	test ! -d "$(WINE_C)/v" -o ! -f "$(WINE_C)/v"
	ln -Tsf "$(win32_destdir)" "$(WINE_C)"/v
	$(LIGHT) -sval -ext WixUIExtension -ext WixUtilExtension -cultures:en-us -b $(W_MSIDIR) -b C:/v/plugins -b C:/v/locale -b C:/v/lua -b C:/v/skins $(W_MSIBUILDDIR)\\product.wixobj $(W_MSIBUILDDIR)\\axvlc.wixobj $(W_MSIBUILDDIR)\\extensions.wixobj $(W_MSIBUILDDIR)\\*.fragment.wixobj -o $(MSIOUTFILE)
	chmod 644 $(MSIOUTFILE)

cleanmsi:
	-rm -f $(MSIBUILDDIR)/*.wixobj
	-rm -f $(MSIBUILDDIR)/*.wixpdb
	-rm -f $(MSIBUILDDIR)/*.fragment.wxs

distcleanmsi: cleanmsi
	-rm -f $(MSIOUTFILE)

.PHONY: heat candle light cleanmsi distcleanmsi package-msi
