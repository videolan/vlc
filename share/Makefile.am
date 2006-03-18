NULL =

EXTRA_DIST = $(DIST_rsrc) $(DIST_fonts) \
	$(skins2_default_vlt_FILES) $(skins2_playtree_vlt_FILES)

pkgdatadir = $(datadir)/vlc

dist_pkgdata_DATA = \
	$(DIST_pda) \
	$(DIST_icons16) \
	$(DIST_icons32) \
	$(DIST_icons48) \
	$(DIST_icons128) \
	$(NULL)

nobase_dist_pkgdata_DATA = \
	$(DIST_skins2) \
	$(DIST_http) \
	$(DIST_osdmenu_default) \
	$(DIST_osdmenu_dvd) \
	$(NULL)

nobase_pkgdata_DATA = skins2/default.vlt skins2/playtree.vlt

skins2/default.vlt: $(skins2_default_vlt_FILES)
skins2/playtree.vlt: $(skins2_playtree_vlt_FILES)

MOSTLYCLEANFILES = $(nobase_pkgdata_DATA)

DIST_fonts = \
	$(NULL)

DIST_rsrc = \
	newres.h \
	resource.h \
	vlc_wince_rc.rc \
	vlc_beos.rsrc \
	vlc_win32_rc.rc \
	$(NULL)

DIST_pda = \
        pda-forwardb16x16.xpm \
        pda-openb16x16.xpm \
        pda-pauseb16x16.xpm \
        pda-playb16x16.xpm \
        pda-playlistb16x16.xpm \
        pda-preferencesb16x16.xpm \
        pda-rewindb16x16.xpm \
        pda-stopb16x16.xpm \
        $(NULL)

DIST_icons16 = \
	vlc16x16.png \
	vlc16x16.xpm \
	$(NULL)

DIST_icons32 = \
	vlc32x32.png \
	vlc32x32.xpm \
	$(NULL)

DIST_icons48 = \
	vlc48x48.ico \
	vlc48x48.png \
	$(NULL)

DIST_icons128 = \
	vlc128x128.png \
	$(NULL)

DIST_skins2 = \
	skins2/fonts/FreeSans.ttf \
	skins2/fonts/FreeSansBold.ttf \
	skins2/skin.dtd \
	skins2/skin.catalog \
	skins2/winamp2.xml \
	$(NULL)

skins2_default_vlt_FILES = \
	skins2/default/add_item_onclick.png \
	skins2/default/add_item.png \
	skins2/default/body.png \
	skins2/default/close_mouseover.png \
	skins2/default/close_onclick.png \
	skins2/default/close.png \
	skins2/default/delete_item_onclick.png \
	skins2/default/delete_item.png \
	skins2/default/disabled.png \
	skins2/default/fast_disabled.png \
	skins2/default/fast_onclick.png \
	skins2/default/fast.png \
	skins2/default/FreeSansBold.ttf \
	skins2/default/next_disabled.png \
	skins2/default/next_onclick.png \
	skins2/default/next.png \
	skins2/default/pause_onclick.png \
	skins2/default/pause.png \
	skins2/default/playlist_body1.png \
	skins2/default/playlist_body2.png \
	skins2/default/playlist_body3.png \
	skins2/default/playlist_body4.png \
	skins2/default/playlist_body6.png \
	skins2/default/playlist_body7.png \
	skins2/default/playlist_body8.png \
	skins2/default/playlist_body9.png \
	skins2/default/playlist_button2.png \
	skins2/default/playlist_button_onclick2.png \
	skins2/default/playlist_button_onclick.png \
	skins2/default/playlist_button.png \
	skins2/default/playlist.png \
	skins2/default/play_onclick.png \
	skins2/default/play.png \
	skins2/default/preferences_onclick.png \
	skins2/default/preferences.png \
	skins2/default/previous_disabled.png \
	skins2/default/previous_onclick.png \
	skins2/default/previous.png \
	skins2/default/reduce_disabled.png \
	skins2/default/reduce_mouseover.png \
	skins2/default/reduce_onclick.png \
	skins2/default/reduce.png \
	skins2/default/resize.png \
	skins2/default/rev_disabled.png \
	skins2/default/rev_onclick.png \
	skins2/default/rev.png \
	skins2/default/slider2.png \
	skins2/default/slider_onclick.png \
	skins2/default/slider_playlist.png \
	skins2/default/slider.png \
	skins2/default/slider_volume_onclick.png \
	skins2/default/slider_volume.png \
	skins2/default/slow_disabled.png \
	skins2/default/slow_onclick.png \
	skins2/default/slow.png \
	skins2/default/sort_onclick.png \
	skins2/default/sort.png \
	skins2/default/stop_disabled.png \
	skins2/default/stop_onclick.png \
	skins2/default/stop.png \
	skins2/default/theme.xml \
	skins2/default/volume.png \
	skins2/default/vout2.png \
	skins2/default/vout_body1.png \
	skins2/default/vout_body.png \
	skins2/default/vout_bottom_left.png \
	skins2/default/vout_bottom.png \
	skins2/default/vout_bottom_right.png \
	skins2/default/vout_left.png \
	skins2/default/vout_onclick.png \
	skins2/default/vout.png \
	skins2/default/vout_right.png \
	skins2/default/vout_top_left.png \
	skins2/default/vout_top.png \
	skins2/default/vout_top_right.png

skins2_playtree_vlt_FILES = \
        skins2/playtree/add_item_onclick.png \
        skins2/playtree/add_item.png \
        skins2/playtree/body.png \
        skins2/playtree/closed.png \
        skins2/playtree/close_mouseover.png \
        skins2/playtree/close_onclick.png \
        skins2/playtree/close.png \
        skins2/playtree/delete_item_onclick.png \
        skins2/playtree/delete_item.png \
        skins2/playtree/disabled.png \
        skins2/playtree/fast_disabled.png \
        skins2/playtree/fast_onclick.png \
        skins2/playtree/fast.png \
        skins2/playtree/FreeSansBold.ttf \
        skins2/playtree/item.png \
        skins2/playtree/next_disabled.png \
        skins2/playtree/next_onclick.png \
        skins2/playtree/next.png \
        skins2/playtree/open.png \
        skins2/playtree/pause_onclick.png \
        skins2/playtree/pause.png \
        skins2/playtree/playlist_body1.png \
        skins2/playtree/playlist_body2.png \
        skins2/playtree/playlist_body3.png \
        skins2/playtree/playlist_body4.png \
        skins2/playtree/playlist_body6.png \
        skins2/playtree/playlist_body7.png \
        skins2/playtree/playlist_body8.png \
        skins2/playtree/playlist_body9.png \
        skins2/playtree/playlist_button2.png \
        skins2/playtree/playlist_button_onclick2.png \
        skins2/playtree/playlist_button_onclick.png \
        skins2/playtree/playlist_button.png \
        skins2/playtree/playlist.png \
        skins2/playtree/play_onclick.png \
        skins2/playtree/play.png \
        skins2/playtree/preferences_onclick.png \
        skins2/playtree/preferences.png \
        skins2/playtree/previous_disabled.png \
        skins2/playtree/previous_onclick.png \
        skins2/playtree/previous.png \
        skins2/playtree/reduce_disabled.png \
        skins2/playtree/reduce_mouseover.png \
        skins2/playtree/reduce_onclick.png \
        skins2/playtree/reduce.png \
        skins2/playtree/resize.png \
        skins2/playtree/rev_disabled.png \
        skins2/playtree/rev_onclick.png \
        skins2/playtree/rev.png \
        skins2/playtree/slider2.png \
        skins2/playtree/slider_onclick.png \
        skins2/playtree/slider_playlist.png \
        skins2/playtree/slider.png \
        skins2/playtree/slider_volume_onclick.png \
        skins2/playtree/slider_volume.png \
        skins2/playtree/slow_disabled.png \
        skins2/playtree/slow_onclick.png \
        skins2/playtree/slow.png \
        skins2/playtree/sort_onclick.png \
        skins2/playtree/sort.png \
        skins2/playtree/stop_disabled.png \
        skins2/playtree/stop_onclick.png \
        skins2/playtree/stop.png \
        skins2/playtree/theme.xml \
        skins2/playtree/volume.png \
        skins2/playtree/vout2.png \
        skins2/playtree/vout_body1.png \
        skins2/playtree/vout_body.png \
        skins2/playtree/vout_bottom_left.png \
        skins2/playtree/vout_bottom.png \
        skins2/playtree/vout_bottom_right.png \
        skins2/playtree/vout_left.png \
        skins2/playtree/vout_onclick.png \
        skins2/playtree/vout.png \
        skins2/playtree/vout_right.png \
        skins2/playtree/vout_top_left.png \
        skins2/playtree/vout_top.png \
        skins2/playtree/vout_top_right.png

DIST_http = \
	http/js/functions.js \
	http/js/mosaic.js \
	http/js/vlm.js \
	http/old/vlm/edit.html \
	http/old/vlm/index.html \
	http/old/vlm/show.html \
	http/old/vlm/new.html \
	http/old/webcam.html \
	http/old/favicon.ico \
	http/old/admin/dboxfiles.html \
	http/old/admin/.access \
	http/old/admin/index.html \
	http/old/admin/browse.html \
	http/old/style.css \
	http/old/index.html \
	http/old/cone_minus.png \
	http/old/cone_plus.png \
	http/old/.hosts \
	http/old/info.html \
	http/vlm_export.html \
	http/favicon.ico \
	http/dialogs/vlm \
	http/dialogs/main \
	http/dialogs/sout \
	http/dialogs/input \
	http/dialogs/browse \
	http/dialogs/footer \
	http/dialogs/mosaic \
	http/dialogs/.hosts \
	http/dialogs/playlist \
	http/requests/vlm_cmd.xml \
	http/requests/browse.xml \
	http/requests/playlist.xml \
	http/requests/vlm.xml \
	http/requests/status.xml \
	http/style.css \
	http/index.html \
	http/vlm.html \
	http/images/plus.png \
	http/images/eject.png \
	http/images/speaker_mute.png \
	http/images/delete_small.png \
	http/images/sort.png \
	http/images/sout.png \
	http/images/shuffle.png \
	http/images/vlc16x16.png \
	http/images/volume_up.png \
	http/images/refresh.png \
	http/images/slider_left.png \
	http/images/slider_point.png \
	http/images/minus.png \
	http/images/pause.png \
	http/images/fullscreen.png \
	http/images/repeat.png \
	http/images/slider_bar.png \
	http/images/empty.png \
	http/images/slider_right.png \
	http/images/prev.png \
	http/images/slow.png \
	http/images/playlist.png \
	http/images/next.png \
	http/images/volume_down.png \
	http/images/play.png \
	http/images/delete.png \
	http/images/speaker.png \
	http/images/info.png \
	http/images/loop.png \
	http/images/stop.png \
	http/images/help.png \
	http/images/playlist_small.png \
	http/images/white.png \
	http/images/sd.png \
	http/.hosts \
	http/mosaic.html \
	$(NULL)

DIST_osdmenu_default = \
	osdmenu/default.cfg \
	osdmenu/default/unselected.png \
	osdmenu/default/selection/bw.png \
	osdmenu/default/selection/esc.png \
	osdmenu/default/selection/fw.png \
	osdmenu/default/selection/volume.png \
	osdmenu/default/selection/next.png \
	osdmenu/default/selection/play_pause.png \
	osdmenu/default/selection/previous.png \
	osdmenu/default/selection/stop.png \
	osdmenu/default/selected/bw.png \
	osdmenu/default/selected/next.png \
	osdmenu/default/selected/play_pause.png \
	osdmenu/default/selected/fw.png \
	osdmenu/default/selected/stop.png \
	osdmenu/default/selected/previous.png \
	osdmenu/default/selected/esc.png \
	osdmenu/default/selected/volume.png \
	osdmenu/default/volume/volume_00.png \
	osdmenu/default/volume/volume_01.png \
	osdmenu/default/volume/volume_02.png \
	osdmenu/default/volume/volume_03.png \
	osdmenu/default/volume/volume_04.png \
	osdmenu/default/volume/volume_05.png \
	osdmenu/default/volume/volume_06.png \
	osdmenu/default/volume/volume_07.png \
	osdmenu/default/volume/volume_08.png \
	osdmenu/default/volume/volume_09.png \
	osdmenu/default/volume/volume_10.png \
	$(NULL)

DIST_osdmenu_dvd = \
	osdmenu/dvd.cfg \
	osdmenu/dvd/selection/bw.png \
	osdmenu/dvd/selection/esc.png \
	osdmenu/dvd/selection/fw.png \
	osdmenu/dvd/selection/mute.png \
	osdmenu/dvd/selection/next.png \
	osdmenu/dvd/selection/pause.png \
	osdmenu/dvd/selection/play.png \
	osdmenu/dvd/selection/previous.png \
	osdmenu/dvd/selection/slow.png \
	osdmenu/dvd/selection/stop.png \
	osdmenu/dvd/selected/bw.png \
	osdmenu/dvd/selected/next.png \
	osdmenu/dvd/selected/play.png \
	osdmenu/dvd/selected/fw.png \
	osdmenu/dvd/selected/stop.png \
	osdmenu/dvd/selected/pause.png \
	osdmenu/dvd/selected/previous.png \
	osdmenu/dvd/selected/esc.png \
	osdmenu/dvd/selected/mute.png \
	osdmenu/dvd/selected/slow.png \
	osdmenu/dvd/selected/volume.png \
	osdmenu/dvd/volume/volume00.png \
	osdmenu/dvd/volume/volume01.png \
	osdmenu/dvd/volume/volume02.png \
	osdmenu/dvd/volume/volume03.png \
	osdmenu/dvd/volume/volume04.png \
	osdmenu/dvd/volume/volume05.png \
	osdmenu/dvd/unselect/barroff.png \
	$(NULL)

skins2/%.vlt:
	mkdir -p skins2
	tar cvvzf $@ --exclude .svn -C $(srcdir)/skins2 $*/

