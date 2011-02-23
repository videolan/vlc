NULL =

doc_DATA = \
	bugreport-howto.txt \
	fortunes.txt \
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
	ChangeLog-2009 \
	ChangeLog-2010 \
	ChangeLog-2011 \
	$(NULL)

man1_MANS = \
	   vlc.1 \
	   vlc-wrapper.1 \
	   vlc-config.1

EXTRA_DIST = \
	$(CHANGELOGS) \
        $(man1_MANS) \
        bugreport-howto.txt \
	fortunes.txt \
	intf-vcd.txt \
	release-howto.txt \
	Doxyfile.in \
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
	demo.sh \
	$(NULL)

DISTCLEANFILES = Doxyfile
MAINTAINERCLEANFILES = $(CHANGELOGS)

Doxyfile: Doxyfile.in $(top_builddir)/config.status
	cd $(top_builddir) && $(SHELL) ./config.status --file=$(subdir)/$@

doc: Doxyfile
	doxygen

$(CHANGELOGS): Makefile.am
	if test -d "$(top_srcdir)/.git"; then \
		y="$@"; y="$${y##ChangeLog-}" ; \
		git --git-dir="$(top_srcdir)/.git" log \
			--since="$$y-01-01" \
			--until="$$y-12-31 23:00:00 -0100" \
			> "$@" ; \
	fi

# This one needs to be rebuilt all the time :)
.PHONY: ChangeLog-2011 changelogs doc

changelogs: $(CHANGELOGS)

#distclean-hook:
#	test "$(srcdir)" = "$(builddir)" || rm -f -- $(CHANGELOGS)
