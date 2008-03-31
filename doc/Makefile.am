NULL =

docdir = $(datadir)/doc/@PACKAGE@

doc_DATA = \
	bugreport-howto.txt \
	fortunes.txt \
	intf-cdda.txt \
	intf-vcd.txt \
	$(NULL)

CHANGELOGS = \
	ChangeLog-1999 \
	ChangeLog-2000 \
	ChangeLog-2001 \
	ChangeLog-2002 \
	ChangeLog-2003 \
	ChangeLog-2004 \
	ChangeLog-2005 \
	ChangeLog-2006 \
	ChangeLog-2007 \
	ChangeLog-2008 \
	$(NULL)

EXTRA_DIST = \
	$(CHANGELOGS) \
	vlc.1 \
	vlc-config.1 \
	bugreport-howto.txt \
	fortunes.txt \
	intf-cdda.txt \
	intf-vcd.txt \
	release-howto.txt \
	arm-crosscompile-howto.sgml \
	Configure.help \
	lirc/example.lircrc \
	skins/skins-howto.txt \
	skins/events-howto.txt \
	skins/curve_maker/Bezier.bas \
	skins/curve_maker/Bezier.frm \
	skins/curve_maker/Bezier.frx \
	skins/curve_maker/bezier.vbp \
	skins/curve_maker/Bezier.vbw \
	strings.txt \
	translations.txt \
	win32/Cross-Compile-Howto.txt \
	demo.sh \
	$(NULL)

$(CHANGELOGS): Makefile.am
	y="$$(echo "$@" | sed -e 's,ChangeLog-,,')" ; \
	git-log \
		--since="$$y-01-01" \
		--until="$$y-12-31 23:00:00 -0100" \
		> "$@"

# This one needs to be rebuilt all the time :)
.PHONY: ChangeLog-2008
