CLEANFILES = skins2/default.vlt
MOSTLYCLEANFILES =
EXTRA_DIST =

desktopdir = $(datadir)/applications
if !HAVE_WIN32
if !HAVE_DARWIN
desktop_DATA = vlc.desktop
appdatadir = $(datarootdir)/metainfo
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
appdata_in_files = vlc.appdata.xml.in
endif
endif

EXTRA_DIST += vlc.desktop.in vlc.desktop.mimetypes
CLEANFILES += $(desktop_DATA) $(appdata_DATA)

vlc.desktop: vlc.desktop.in $(top_builddir)/config.status
	$(AM_V_GEN)mimetypes="$$(sed 's/\s*#.*$$//g' $(srcdir)/vlc.desktop.mimetypes | egrep -v '^$$' | tr "\n" ';')"; \
	sed \
		-e 's,\@bindir\@,$(bindir),g' \
		 -e "s,\@MIMETYPES\@,$$mimetypes,g" < "$<" > tmp-$@
	$(AM_V_at)$(DESKTOP_FILE_VALIDATE) tmp-$@
	$(AM_V_at)mv -f tmp-$@ $@

vlc.appdata.xml: vlc.appdata.xml.in
	$(AM_V_GEN)$(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@ || cp $< $@

iconsdatadir = $(datadir)/icons/hicolor
icons16dir = $(iconsdatadir)/16x16/apps
icons32dir = $(iconsdatadir)/32x32/apps
icons48dir = $(iconsdatadir)/48x48/apps
icons128dir = $(iconsdatadir)/128x128/apps
icons256dir = $(iconsdatadir)/256x256/apps

if !HAVE_WIN32
dist_vlcdata_DATA = icons/vlc.ico
dist_icons16_DATA = icons/16x16/vlc.png icons/16x16/vlc.xpm
dist_icons32_DATA = icons/32x32/vlc.png icons/32x32/vlc.xpm \
			icons/32x32/vlc-xmas.xpm
dist_icons48_DATA = icons/48x48/vlc.png icons/48x48/vlc-xmas.png
dist_icons128_DATA = icons/128x128/vlc.png icons/128x128/vlc-xmas.png \
                     icons/128x128/vlc-kb.png
dist_icons256_DATA = icons/256x256/vlc.png

nobase_dist_vlcdata_SCRIPTS = \
	utils/gnome-vlc-default.sh \
	utils/audio-vlc-default.sh \
	utils/video-vlc-default.sh \
	$(NULL)
endif

EXTRA_DIST += \
	$(skins2_default_vlt_FILES) \
	$(DIST_icons) \
	$(DIST_http_lua) \
	$(DIST_solid)

nobase_vlcdata_DATA =
nobase_vlclib_DATA =
nobase_dist_vlcdata_DATA =
if BUILD_SKINS
nobase_vlcdata_DATA += skins2/default.vlt
nobase_dist_vlcdata_DATA += $(DIST_skins2)
endif
if KDE_SOLID
soliddata_DATA = $(DIST_solid)
endif

DIST_icons = \
	vlc512x512.png

DIST_skins2 = \
	skins2/fonts/FreeSans.ttf \
	skins2/fonts/FreeSansBold.ttf \
	skins2/skin.dtd \
	skins2/skin.catalog \
	skins2/winamp2.xml

skins2_default_vlt_FILES = \
	skins2/default/theme.xml \
	skins2/default/subX/about.png \
	skins2/default/subX/eq.png \
	skins2/default/subX/font.otf \
	skins2/default/subX/main.png \
	skins2/default/subX/playtreeglyphs.png \
	skins2/default/subX/pl.png \
	skins2/default/subX/sysbuttons.png \
	skins2/default/subX/vol_anim.png \
	skins2/default/subX/vol_slider.png

skins2/default.vlt: $(skins2_default_vlt_FILES)
	$(AM_V_at)mkdir -p skins2
	$(AM_V_at)rm -f -- skins2/default.vlt.tmp
	$(AM_V_GEN)GZIP=--no-name \
	tar cvvzf skins2/default.vlt.tmp \
		--owner=root --group=root --directory="$(srcdir)/skins2" \
		default/
	$(AM_V_at)mv -f -- skins2/default.vlt.tmp skins2/default.vlt

#
# LUA
#
luac_verbose = $(luac_verbose_$(V))
luac_verbose_ = $(luac_verbose_$(AM_DEFAULT_VERBOSITY))
luac_verbose_0 = @echo "  LUAC   $@";

.lua.luac:
	$(AM_V_at)for f in $(EXTRA_DIST); do \
		test "$(srcdir)" = . || f="$(srcdir)/$$f"; \
		if test $$f = $<; then \
			exit 0; \
		fi; \
	done; \
	echo "Attempt to byte-compile unknown file: $(<)!"; \
	exit 1
	$(AM_V_at)mkdir -p "$$(dirname '$@')"
	$(luac_verbose)$(LUAC) -o $@ $<

if BUILD_LUA
nobase_vlclib_DATA += \
	lua/extensions/VLSub.luac \
	lua/intf/cli.luac \
	lua/intf/dummy.luac \
	lua/intf/dumpmeta.luac \
	lua/intf/luac.luac \
	lua/intf/http.luac \
	lua/intf/modules/host.luac \
	lua/intf/modules/httprequests.luac \
	lua/intf/telnet.luac \
	lua/meta/art/02_frenchtv.luac \
	lua/meta/art/03_lastfm.luac \
	lua/meta/art/01_googleimage.luac \
	lua/meta/art/00_musicbrainz.luac \
	lua/meta/reader/filename.luac \
	lua/modules/common.luac \
	lua/modules/dkjson.luac \
	lua/modules/sandbox.luac \
	lua/modules/simplexml.luac \
	lua/playlist/anevia_streams.luac \
	lua/playlist/anevia_xml.luac \
	lua/playlist/appletrailers.luac \
	lua/playlist/bbc_co_uk.luac \
	lua/playlist/cue.luac \
	lua/playlist/dailymotion.luac \
	lua/playlist/jamendo.luac \
	lua/playlist/koreus.luac \
	lua/playlist/liveleak.luac \
	lua/playlist/newgrounds.luac \
	lua/playlist/rockbox_fm_presets.luac \
	lua/playlist/soundcloud.luac \
	lua/playlist/vimeo.luac \
	lua/playlist/vocaroo.luac \
	lua/playlist/youtube.luac \
	lua/playlist/twitch.luac \
	lua/sd/icecast.luac \
	lua/sd/jamendo.luac \
	$(NULL)

nobase_doc_DATA = \
	lua/README.txt \
	lua/extensions/README.txt \
	lua/http/requests/README.txt \
	lua/intf/README.txt \
	lua/intf/dumpmeta.lua \
	lua/meta/README.txt \
	lua/meta/art/README.txt \
	lua/meta/art/01_googleimage.lua \
	lua/meta/fetcher/README.txt \
	lua/meta/reader/README.txt \
	lua/meta/reader/filename.lua \
	lua/playlist/README.txt \
	lua/playlist/liveleak.lua \
	lua/playlist/youtube.lua \
	lua/sd/README.txt \
	lua/sd/icecast.lua \
	lua/sd/icast.lua \
	$(NULL)

nobase_dist_vlcdata_DATA += \
	lua/http/view.html \
	lua/http/mobile.html \
	lua/http/dialogs/create_stream.html \
	lua/http/dialogs/equalizer_window.html \
	lua/http/dialogs/mosaic_window.html \
	lua/http/dialogs/batch_window.html \
	lua/http/dialogs/error_window.html \
	lua/http/dialogs/browse_window.html \
	lua/http/dialogs/stream_window.html \
	lua/http/dialogs/offset_window.html \
	lua/http/dialogs/stream_config_window.html \
	lua/http/custom.lua \
	lua/http/vlm.html \
	lua/http/images/buttons.png \
	lua/http/images/speaker-32.png \
	lua/http/images/vlc-48.png \
	lua/http/images/Audio-48.png \
	lua/http/images/Folder-48.png \
	lua/http/images/Other-48.png \
	lua/http/images/Back-48.png \
	lua/http/images/Video-48.png \
	lua/http/images/vlc16x16.png \
	lua/http/mobile_equalizer.html \
	lua/http/vlm_export.html \
	lua/http/mobile_browse.html \
	lua/http/favicon.ico \
	lua/http/mobile_view.html \
	lua/http/requests/playlist.xml \
	lua/http/requests/playlist.json \
	lua/http/requests/README.txt \
	lua/http/requests/playlist_jstree.xml \
	lua/http/requests/browse.xml \
	lua/http/requests/browse.json \
	lua/http/requests/vlm_cmd.xml \
	lua/http/requests/status.xml \
	lua/http/requests/status.json \
	lua/http/requests/vlm.xml \
	lua/http/index.html \
	lua/http/css/ui-lightness/jquery-ui-1.8.13.custom.css \
	lua/http/css/ui-lightness/images/ui-bg_diagonals-thick_20_666666_40x40.png \
	lua/http/css/ui-lightness/images/ui-bg_flat_10_000000_40x100.png \
	lua/http/css/ui-lightness/images/ui-icons_ffffff_256x240.png \
	lua/http/css/ui-lightness/images/ui-bg_gloss-wave_35_f6a828_500x100.png \
	lua/http/css/ui-lightness/images/ui-bg_glass_65_ffffff_1x400.png \
	lua/http/css/ui-lightness/images/ui-icons_222222_256x240.png \
	lua/http/css/ui-lightness/images/ui-bg_glass_100_f6f6f6_1x400.png \
	lua/http/css/ui-lightness/images/ui-bg_highlight-soft_100_eeeeee_1x100.png \
	lua/http/css/ui-lightness/images/ui-bg_highlight-soft_75_ffe45c_1x100.png \
	lua/http/css/ui-lightness/images/ui-bg_diagonals-thick_18_b81900_40x40.png \
	lua/http/css/ui-lightness/images/ui-icons_ffd27a_256x240.png \
	lua/http/css/ui-lightness/images/ui-icons_228ef1_256x240.png \
	lua/http/css/ui-lightness/images/ui-icons_ef8c08_256x240.png \
	lua/http/css/ui-lightness/images/ui-bg_glass_100_fdf5ce_1x400.png \
	lua/http/css/main.css \
	lua/http/css/mobile.css \
	lua/http/js/ui.js \
	lua/http/js/common.js \
	lua/http/js/jquery.jstree.js \
	lua/http/js/controllers.js
endif
MOSTLYCLEANFILES += $(nobase_vlcdata_DATA)

EXTRA_DIST += \
	lua/README.txt \
	lua/extensions/README.txt \
	lua/extensions/VLSub.lua \
	lua/intf/README.txt \
	lua/intf/cli.lua \
	lua/intf/dummy.lua \
	lua/intf/dumpmeta.lua \
	lua/intf/http.lua \
	lua/intf/luac.lua \
	lua/intf/modules/host.lua \
	lua/intf/modules/httprequests.lua \
	lua/intf/telnet.lua \
	lua/meta/README.txt \
	lua/meta/art/README.txt \
	lua/meta/art/02_frenchtv.lua \
	lua/meta/art/03_lastfm.lua \
	lua/meta/art/01_googleimage.lua \
	lua/meta/art/00_musicbrainz.lua \
	lua/meta/fetcher/README.txt \
	lua/meta/reader/README.txt \
	lua/meta/reader/filename.lua \
	lua/modules/common.lua \
	lua/modules/dkjson.lua \
	lua/modules/sandbox.lua \
	lua/modules/simplexml.lua \
	lua/playlist/README.txt \
	lua/playlist/anevia_streams.lua \
	lua/playlist/anevia_xml.lua \
	lua/playlist/appletrailers.lua \
	lua/playlist/bbc_co_uk.lua \
	lua/playlist/break.lua \
	lua/playlist/cue.lua \
	lua/playlist/dailymotion.lua \
	lua/playlist/extreme.lua \
	lua/playlist/france2.lua \
	lua/playlist/jamendo.lua \
	lua/playlist/katsomo.lua \
	lua/playlist/koreus.lua \
	lua/playlist/lelombrik.lua \
	lua/playlist/liveleak.lua \
	lua/playlist/metacafe.lua \
	lua/playlist/mpora.lua \
	lua/playlist/newgrounds.lua \
	lua/playlist/pinkbike.lua \
	lua/playlist/rockbox_fm_presets.lua \
	lua/playlist/soundcloud.lua \
	lua/playlist/vimeo.lua \
	lua/playlist/vocaroo.lua \
	lua/playlist/youtube.lua \
	lua/playlist/twitch.lua \
	lua/playlist/zapiks.lua \
	lua/sd/README.txt \
	lua/sd/fmc.lua \
	lua/sd/icecast.lua \
	lua/sd/icast.lua \
	lua/sd/jamendo.lua
DIST_solid = \
	solid/vlc-openbd.desktop \
	solid/vlc-opencda.desktop \
	solid/vlc-opendvd.desktop \
	solid/vlc-openvcd.desktop
