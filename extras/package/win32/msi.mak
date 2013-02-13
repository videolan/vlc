WIXPATH=`winepath -u 'C:\\Program Files\\Windows Installer XML v3.5\\bin'`
HEAT=wine "$(WIXPATH)/heat.exe"
CANDLE=wine "$(WIXPATH)/candle.exe"
LIGHT=wine "$(WIXPATH)/light.exe"
VLCDIR=`winepath -w '$(win32_destdir)'`
MSIDIR=$(abs_srcdir)/extras/package/win32/msi
W_MSIDIR=`winepath -w '$(MSIDIR)'`
MSIBUILDDIR=$(abs_top_builddir)/extras/package/win32/msi
W_MSIBUILDDIR=`winepath -w '$(MSIBUILDDIR)'`
MSIOUTFILE=vlc-$(VERSION).msi

package-msi: heat candle light package-win-strip

heat:
	$(HEAT) dir $(VLCDIR)/plugins -cg CompPluginsGroup -gg -scom -sreg -sfrag -srd -dr PLUGINSDIR -out $(W_MSIBUILDDIR)/Plugins.fragment.wxs
	$(HEAT) dir $(VLCDIR)/locale -cg CompLocaleGroup -gg -scom -sreg -sfrag -srd -dr LOCALEDIR -out $(W_MSIBUILDDIR)/Locale.fragment.wxs
	$(HEAT) dir $(VLCDIR)/lua -cg CompLuaGroup -gg -scom -sreg -sfrag -srd -dr LUADIR -out $(W_MSIBUILDDIR)/Lua.fragment.wxs
	$(HEAT) dir $(VLCDIR)/skins -cg CompSkinsGroup -gg -scom -sreg -sfrag -srd -dr SKINSDIR -out $(W_MSIBUILDDIR)/Skins.fragment.wxs

candle:
	$(am__cd) $(MSIBUILDDIR) && $(CANDLE) -ext WiXUtilExtension $(W_MSIDIR)\\product.wxs $(W_MSIDIR)\\axvlc.wxs $(W_MSIDIR)\\extensions.wxs $(W_MSIBUILDDIR)\\*.fragment.wxs

light:
	$(LIGHT) -sval -ext WixUIExtension -ext WixUtilExtension -cultures:en-us -b $(W_MSIDIR) -b $(VLCDIR)/plugins -b $(VLCDIR)/locale -b $(VLCDIR)/lua -b $(VLCDIR)/skins $(W_MSIBUILDDIR)\\product.wixobj $(W_MSIBUILDDIR)\\axvlc.wixobj $(W_MSIBUILDDIR)\\extensions.wixobj $(W_MSIBUILDDIR)\\*.fragment.wixobj -o $(MSIOUTFILE)

cleanmsi:
	-rm -f $(MSIBUILDDIR)/*.wixobj
	-rm -f $(MSIBUILDDIR)/*.wixpdb
	-rm -f $(MSIBUILDDIR)/*.fragment.wxs

distcleanmsi: cleanmsi
	-rm -f $(MSIOUTFILE)
