CLEANFILES = skins2/default.vlt

nobase_dist_data_DATA = applications/vlc.desktop

dist_vlcdata_DATA = \
	$(DIST_icons) \
	$(DIST_icons16) \
	$(DIST_icons32) \
	$(DIST_icons48) \
	$(DIST_icons128)
nobase_dist_vlcdata_SCRIPTS = \
	utils/gnome-vlc-default.sh \
	utils/audio-vlc-default.sh \
	utils/video-vlc-default.sh \
	$(NULL)

EXTRA_DIST = \
	$(skins2_default_vlt_FILES) \
	$(DIST_rsrc) \
	$(DIST_pda) \
	$(DIST_skins2) \
	$(DIST_http) \
	$(DIST_http_lua) \
	$(DIST_lua) \
	$(DIST_osdmenu_default) \
	$(DIST_mozilla) \
	$(DIST_maemo)

nobase_vlcdata_DATA =
if BUILD_SKINS
nobase_vlcdata_DATA += skins2/default.vlt
nobase_vlcdata_DATA += $(DIST_skins2)
endif
if BUILD_PDA
nobase_vlcdata_DATA += $(DIST_pda)
endif
if BUILD_HTTPD
nobase_vlcdata_DATA += $(DIST_http)
endif
if BUILD_OSDMENU
nobase_vlcdata_DATA += \
	$(DIST_osdmenu_default)
endif
if BUILD_LUA
nobase_vlcdata_DATA += $(DIST_lua)
if BUILD_HTTPD
nobase_vlcdata_DATA += $(DIST_http_lua)
endif
endif
if BUILD_MOZILLA
# TODO: move to the mozilla directory
nobase_vlcdata_DATA += $(DIST_mozilla)
endif
if BUILD_MAEMO
nobase_vlcdata_DATA += $(DIST_maemo) $(maemo_FILES)
endif

DIST_rsrc = \
	newres.h \
	resource.h \
	vlc_wince_rc.rc \
	vlc_beos.rsrc \
	vlc_win32_rc.rc.in \
	libvlc_win32_rc.rc.in

DIST_pda = \
	pda-forwardb16x16.xpm \
	pda-openb16x16.xpm \
	pda-pauseb16x16.xpm \
	pda-playb16x16.xpm \
	pda-playlistb16x16.xpm \
	pda-preferencesb16x16.xpm \
	pda-rewindb16x16.xpm \
	pda-stopb16x16.xpm

DIST_icons = vlc.ico

DIST_icons16 = vlc16x16.png vlc16x16.xpm

DIST_icons32 = vlc32x32.png vlc32x32.xpm vlc32x32-christmas.xpm

DIST_icons48 = vlc48x48.png vlc48x48-christmas.png

DIST_icons128 = vlc128x128.png vlc128x128-christmas.png

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
	mkdir -p skins2
	tar cvvzf skins2/default.vlt --exclude .svn -C $(srcdir)/skins2 default/

DIST_http = \
	http/.hosts \
	http/dialogs/.hosts \
	http/dialogs/browse \
	http/dialogs/footer \
	http/dialogs/input \
	http/dialogs/main \
	http/dialogs/mosaic \
	http/dialogs/playlist \
	http/dialogs/sout \
	http/dialogs/vlm \
	http/favicon.ico \
	http/images/delete.png \
	http/images/delete_small.png \
	http/images/eject.png \
	http/images/empty.png \
	http/images/fullscreen.png \
	http/images/help.png \
	http/images/info.png \
	http/images/loop.png \
	http/images/minus.png \
	http/images/next.png \
	http/images/pause.png \
	http/images/play.png \
	http/images/playlist.png \
	http/images/playlist_small.png \
	http/images/plus.png \
	http/images/prev.png \
	http/images/refresh.png \
	http/images/repeat.png \
	http/images/sd.png \
	http/images/shuffle.png \
	http/images/slider_bar.png \
	http/images/slider_left.png \
	http/images/slider_point.png \
	http/images/slider_right.png \
	http/images/snapshot.png \
	http/images/slow.png \
	http/images/sort.png \
	http/images/sout.png \
	http/images/speaker.png \
	http/images/speaker_mute.png \
	http/images/stop.png \
	http/images/vlc16x16.png \
	http/images/volume_down.png \
	http/images/volume_up.png \
	http/images/white.png \
	http/images/white_cross_small.png \
	http/index.html \
	http/js/functions.js \
	http/js/mosaic.js \
	http/js/vlm.js \
	http/mosaic.html \
	http/requests/browse.xml \
	http/requests/playlist.xml \
	http/requests/status.xml \
	http/requests/vlm.xml \
	http/requests/vlm_cmd.xml \
	http/requests/readme \
	http/style.css \
	http/iehacks.css \
	http/vlm.html \
	http/vlm_export.html \
	http/flash.html

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
	osdmenu/default/volume/volume_10.png

DIST_lua= \
	lua/README.txt \
	lua/meta/README.txt \
	lua/meta/01_musicbrainz.lua \
	lua/meta/10_googleimage.lua \
	lua/playlist/README.txt \
	lua/playlist/anevia_streams.lua \
	lua/playlist/appletrailers.lua \
	lua/playlist/break.lua \
	lua/playlist/dailymotion.lua \
	lua/playlist/france2.lua \
	lua/playlist/joox.lua \
	lua/playlist/lelombrik.lua \
	lua/playlist/youtube.lua \
	lua/playlist/youtube_homepage.lua \
	lua/playlist/megavideo.lua \
	lua/playlist/metacafe.lua \
	lua/playlist/googlevideo.lua \
	lua/playlist/mpora.lua \
	lua/playlist/vimeo.lua \
	lua/intf/README.txt \
	lua/intf/http.lua \
	lua/intf/rc.lua \
	lua/intf/hotkeys.lua \
	lua/intf/modules/common.lua \
	lua/intf/modules/host.lua \
	lua/intf/modules/sandbox.lua \
	lua/intf/telnet.lua \
	lua/intf/dummy.lua

DIST_http_lua = \
	lua/http/.hosts \
	lua/http/custom.lua \
	lua/http/dialogs/.hosts \
	lua/http/dialogs/browse \
	lua/http/dialogs/footer \
	lua/http/dialogs/input \
	lua/http/dialogs/main \
	lua/http/dialogs/mosaic \
	lua/http/dialogs/playlist \
	lua/http/dialogs/sout \
	lua/http/dialogs/vlm \
	lua/http/favicon.ico \
	lua/http/images/delete.png \
	lua/http/images/delete_small.png \
	lua/http/images/eject.png \
	lua/http/images/empty.png \
	lua/http/images/fullscreen.png \
	lua/http/images/help.png \
	lua/http/images/info.png \
	lua/http/images/loop.png \
	lua/http/images/minus.png \
	lua/http/images/next.png \
	lua/http/images/pause.png \
	lua/http/images/play.png \
	lua/http/images/playlist.png \
	lua/http/images/playlist_small.png \
	lua/http/images/plus.png \
	lua/http/images/prev.png \
	lua/http/images/refresh.png \
	lua/http/images/repeat.png \
	lua/http/images/reset.png \
	lua/http/images/sd.png \
	lua/http/images/shuffle.png \
	lua/http/images/slider_bar.png \
	lua/http/images/slider_left.png \
	lua/http/images/slider_point.png \
	lua/http/images/slider_right.png \
	lua/http/images/snapshot.png \
	lua/http/images/slow.png \
	lua/http/images/sort.png \
	lua/http/images/sout.png \
	lua/http/images/speaker.png \
	lua/http/images/speaker_mute.png \
	lua/http/images/stop.png \
	lua/http/images/vlc16x16.png \
	lua/http/images/volume_down.png \
	lua/http/images/volume_up.png \
	lua/http/images/white.png \
	lua/http/images/white_cross_small.png \
	lua/http/index.html \
	lua/http/js/functions.js \
	lua/http/js/mosaic.js \
	lua/http/js/vlm.js \
	lua/http/mosaic.html \
	lua/http/requests/browse.xml \
	lua/http/requests/playlist.xml \
	lua/http/requests/status.xml \
	lua/http/requests/vlm.xml \
	lua/http/requests/vlm_cmd.xml \
	lua/http/requests/readme \
	lua/http/style.css \
	lua/http/iehacks.css \
	lua/http/vlm.html \
	lua/http/vlm_export.html \
	lua/http/flash.html

DIST_mozilla = \
	mozilla/fullscreen.xpm \
	mozilla/pause.xpm \
	mozilla/play.xpm \
	mozilla/skip_next.xpm \
	mozilla/skip_previous.xpm \
	mozilla/stop.xpm \
	mozilla/time_icon.xpm \
	mozilla/time_line.xpm \
	mozilla/volume_knob.xpm \
	mozilla/volume_max.xpm \
	mozilla/volume_mute.xpm \
	mozilla/volume_slider_bar.xpm

DIST_maemo = \
	maemo/vlc_intf.rc \
	maemo/vlc_left_tab_active.png \
	maemo/vlc_left_tab_passive.png \
	maemo/playlist.png

maemo_FILES = \
	../modules/gui/qt4/pixmaps/play.png \
	../modules/gui/qt4/pixmaps/pause.png \
	../modules/gui/qt4/pixmaps/stop.png \
	../modules/gui/qt4/pixmaps/previous.png \
	../modules/gui/qt4/pixmaps/next.png \
	vlc32x32.png
