#!/bin/bash

    inkscape pixmap_source.svg -a 0:0:48:48       -e ../clear.png
    inkscape pixmap_source.svg -a 48:0:96:48      -e ../eject.png
    inkscape pixmap_source.svg -a 96:0:144:48     -e ../faster.png
    inkscape pixmap_source.svg -a 144:0:192:48    -e ../faster2.png
    inkscape pixmap_source.svg -a 192:0:240:48    -e ../go-next.png
    inkscape pixmap_source.svg -a 240:0:288:48    -e ../lock.png
    inkscape pixmap_source.svg -a 288:0:336:48    -e ../next.png
    inkscape pixmap_source.svg -a 336:0:384:48    -e ../pause.png
    inkscape pixmap_source.svg -a 384:0:432:48    -e ../play.png
    inkscape pixmap_source.svg -a 432:0:480:48    -e ../previous.png
    inkscape pixmap_source.svg -a 480:0:528:48    -e ../profile_new.png
    inkscape pixmap_source.svg -a 528:0:576:48    -e ../search_clear.png
    inkscape pixmap_source.svg -a 576:0:624:48    -e ../slower.png
    inkscape pixmap_source.svg -a 624:0:672:48    -e ../slower2.png
    inkscape pixmap_source.svg -a 672:0:720:48    -e ../space.png
    inkscape pixmap_source.svg -a 720:0:768:48    -e ../stop.png
    inkscape pixmap_source.svg -a 768:0:816:48    -e ../update.png
    inkscape pixmap_source.svg -a 816:0:864:48    -e ../valid.png

    inkscape pixmap_source.svg -a 0:96:48:144     -e ../addons/addon.png
    inkscape pixmap_source.svg -a 48:96:96:144    -e ../addons/addon_broken.png
    inkscape pixmap_source.svg -a 96:96:288:144   -e ../addons/score.png
    inkscape pixmap_source.svg -a 288:96:336:144  -e ../addons/addon_cyan.png
    inkscape pixmap_source.svg -a 336:96:384:144  -e ../addons/addon_green.png
    inkscape pixmap_source.svg -a 384:96:432:144  -e ../addons/addon_red.png
    inkscape pixmap_source.svg -a 432:96:480:144  -e ../addons/addon_blue.png
    inkscape pixmap_source.svg -a 480:96:528:144  -e ../addons/addon_magenta.png
    inkscape pixmap_source.svg -a 528:96:576:144  -e ../addons/addon_yellow.png

    inkscape pixmap_source.svg -a 0:192:48:240    -e ../menus/exit_16px.png
    inkscape pixmap_source.svg -a 48:192:96:240   -e ../menus/help_16px.png
    inkscape pixmap_source.svg -a 96:192:144:240  -e ../menus/info_16px.png
    inkscape pixmap_source.svg -a 144:192:192:240 -e ../menus/messages_16px.png
    inkscape pixmap_source.svg -a 192:192:240:240 -e ../menus/playlist_16px.png
    inkscape pixmap_source.svg -a 240:192:288:240 -e ../menus/preferences_16px.png
    inkscape pixmap_source.svg -a 288:192:336:240 -e ../menus/settings_16px.png
    inkscape pixmap_source.svg -a 336:192:384:240 -e ../menus/stream_16px.png

    inkscape pixmap_source.svg -a 0:288:48:336    -e ../playlist/add.png
    inkscape pixmap_source.svg -a 48:288:96:336   -e ../playlist/playlist.png
    inkscape pixmap_source.svg -a 96:288:144:336  -e ../playlist/remove.png
    inkscape pixmap_source.svg -a 144:288:192:336 -e ../playlist/repeat_all.png
    inkscape pixmap_source.svg -a 192:288:240:336 -e ../playlist/repeat_off.png
    inkscape pixmap_source.svg -a 240:288:288:336 -e ../playlist/repeat_one.png
    inkscape pixmap_source.svg -a 288:288:336:336 -e ../playlist/shuffle_on.png

    inkscape pixmap_source.svg -a 640:256:752:368 -e ../playlist/dropzone.png

    inkscape pixmap_source.svg -a 0:384:48:432    -e ../playlist/sidebar-icons/capture.png
    inkscape pixmap_source.svg -a 48:384:96:432   -e ../playlist/sidebar-icons/disc.png
    inkscape pixmap_source.svg -a 96:384:144:432  -e ../playlist/sidebar-icons/lan.png
    inkscape pixmap_source.svg -a 144:384:192:432 -e ../playlist/sidebar-icons/library.png
    inkscape pixmap_source.svg -a 192:384:240:432 -e ../playlist/sidebar-icons/movie.png
    inkscape pixmap_source.svg -a 240:384:288:432 -e ../playlist/sidebar-icons/mtp.png
    inkscape pixmap_source.svg -a 288:384:336:432 -e ../playlist/sidebar-icons/music.png
    inkscape pixmap_source.svg -a 336:384:384:432 -e ../playlist/sidebar-icons/network.png
    inkscape pixmap_source.svg -a 384:384:432:432 -e ../playlist/sidebar-icons/pictures.png
    inkscape pixmap_source.svg -a 432:384:480:432 -e ../playlist/sidebar-icons/playlist.png
    inkscape pixmap_source.svg -a 480:384:528:432 -e ../playlist/sidebar-icons/podcast.png
    inkscape pixmap_source.svg -a 528:384:576:432 -e ../playlist/sidebar-icons/screen.png

    inkscape pixmap_source.svg -a 0:480:48:528    -e ../playlist/sidebar-icons/sd/appletrailers.png
    inkscape pixmap_source.svg -a 48:480:96:528   -e ../playlist/sidebar-icons/sd/assembleenationale.png
    inkscape pixmap_source.svg -a 96:480:144:528  -e ../playlist/sidebar-icons/sd/fmc.png
    inkscape pixmap_source.svg -a 144:480:192:528 -e ../playlist/sidebar-icons/sd/frenchtv.png
    inkscape pixmap_source.svg -a 192:480:240:528 -e ../playlist/sidebar-icons/sd/icecast.png
    inkscape pixmap_source.svg -a 240:480:288:528 -e ../playlist/sidebar-icons/sd/jamendo.png
    inkscape pixmap_source.svg -a 288:480:336:528 -e ../playlist/sidebar-icons/sd/katsomo.png
    inkscape pixmap_source.svg -a 336:480:384:528 -e ../playlist/sidebar-icons/sd/metachannels.png

    inkscape pixmap_source.svg -a 0:576:48:624    -e ../prefs/advprefs_audio.png
    inkscape pixmap_source.svg -a 48:576:96:624   -e ../prefs/advprefs_codec.png
    inkscape pixmap_source.svg -a 96:576:144:624  -e ../prefs/advprefs_extended.png
    inkscape pixmap_source.svg -a 144:576:192:624 -e ../prefs/advprefs_intf.png
    inkscape pixmap_source.svg -a 192:576:240:624 -e ../prefs/advprefs_playlist.png
    inkscape pixmap_source.svg -a 240:576:288:624 -e ../prefs/advprefs_sout.png
    inkscape pixmap_source.svg -a 288:576:336:624 -e ../prefs/advprefs_video.png

    inkscape pixmap_source.svg -a 0:672:48:720      -e ../toolbar/arrows.png
    inkscape pixmap_source.svg -a 48:672:96:720     -e ../toolbar/aspect-ratio.png
    inkscape pixmap_source.svg -a 96:672:144:720    -e ../toolbar/atob_noa.png
    inkscape pixmap_source.svg -a 144:672:192:720   -e ../toolbar/atob_nob.png
    inkscape pixmap_source.svg -a 192:672:240:720   -e ../toolbar/atob.png
    inkscape pixmap_source.svg -a 240:672:288:720   -e ../toolbar/defullscreen.png
    inkscape pixmap_source.svg -a 288:672:336:720   -e ../toolbar/dvd_menu.png
    inkscape pixmap_source.svg -a 336:672:384:720   -e ../toolbar/dvd_next.png
    inkscape pixmap_source.svg -a 384:672:432:720   -e ../toolbar/dvd_prev.png
    inkscape pixmap_source.svg -a 432:672:480:720   -e ../toolbar/extended_16px.png
    inkscape pixmap_source.svg -a 480:672:528:720   -e ../toolbar/frame-by-frame.png
    inkscape pixmap_source.svg -a 528:672:576:720   -e ../toolbar/fullscreen.png
    inkscape pixmap_source.svg -a 576:672:624:720   -e ../toolbar/play_reverse.png
    inkscape pixmap_source.svg -a 624:672:672:720   -e ../toolbar/record_16px.png
    inkscape pixmap_source.svg -a 672:672:720:720   -e ../toolbar/renderer.png
    inkscape pixmap_source.svg -a 720:672:768:720   -e ../toolbar/skip_back.png
    inkscape pixmap_source.svg -a 768:672:816:720   -e ../toolbar/skip_for.png
    inkscape pixmap_source.svg -a 816:672:864:720   -e ../toolbar/snapshot.png
    inkscape pixmap_source.svg -a 864:672:912:720   -e ../toolbar/tv.png
    inkscape pixmap_source.svg -a 912:672:960:720   -e ../toolbar/tvtelx.png
    inkscape pixmap_source.svg -a 960:672:1008:720  -e ../toolbar/visu.png
    inkscape pixmap_source.svg -a 1008:672:1056:720 -e ../toolbar/volume-high.png
    inkscape pixmap_source.svg -a 1056:672:1104:720 -e ../toolbar/volume-low.png
    inkscape pixmap_source.svg -a 1104:672:1152:720 -e ../toolbar/volume-medium.png
    inkscape pixmap_source.svg -a 1152:672:1200:720 -e ../toolbar/volume-muted.png

    inkscape pixmap_source.svg -a 576:576:661:602   -e ../toolbar/volume-slider-inside.png
    inkscape pixmap_source.svg -a 672:576:757:602   -e ../toolbar/volume-slider-outside.png

    inkscape pixmap_source.svg -a 0:768:48:816     -e ../types/capture-card_16px.png
    inkscape pixmap_source.svg -a 48:768:96:816    -e ../types/disc_16px.png
    inkscape pixmap_source.svg -a 96:768:144:816   -e ../types/file-asym_16px.png
    inkscape pixmap_source.svg -a 144:768:192:816  -e ../types/file-wide_16px.png
    inkscape pixmap_source.svg -a 192:768:240:816  -e ../types/folder-blue_16px.png
    inkscape pixmap_source.svg -a 240:768:288:816  -e ../types/folder-grey_16px.png
    inkscape pixmap_source.svg -a 288:768:336:816  -e ../types/harddisk_16px.png
    inkscape pixmap_source.svg -a 336:768:384:816  -e ../types/network_16px.png
    inkscape pixmap_source.svg -a 384:768:432:816  -e ../types/tape_16px.png
    inkscape pixmap_source.svg -a 432:768:480:816  -e ../types/type_directory.png
    inkscape pixmap_source.svg -a 480:768:528:816  -e ../types/type_file.png
    inkscape pixmap_source.svg -a 528:768:576:816  -e ../types/type_node.png
    inkscape pixmap_source.svg -a 576:768:624:816  -e ../types/type_playlist.png
    inkscape pixmap_source.svg -a 624:768:672:816  -e ../types/type_stream.png
#    inkscape pixmap_source.svg -a 672:768:720:816  -e ../types/type_unknown.xpm
    inkscape pixmap_source.svg -a 672:768:720:816  -e ../types/type_unknown.png

    inkscape pixmap_source.svg -a 0:864:48:912     -e ../util/wait1.png
    inkscape pixmap_source.svg -a 48:864:96:912    -e ../util/wait2.png
    inkscape pixmap_source.svg -a 96:864:144:912   -e ../util/wait3.png
    inkscape pixmap_source.svg -a 144:864:192:912  -e ../util/wait4.png

    inkscape pixmap_source.svg -a 0:960:48:1008     -e ../win7/win7thumbnail_next.png
    inkscape pixmap_source.svg -a 48:960:96:1008    -e ../win7/win7thumbnail_pause.png
    inkscape pixmap_source.svg -a 96:960:144:1008   -e ../win7/win7thumbnail_play.png
    inkscape pixmap_source.svg -a 144:960:192:1008  -e ../win7/win7thumbnail_prev.png
