/*****************************************************************************
 * libvlc.h: main libvlc header
 *****************************************************************************
 * Copyright (C) 1998-2002 VideoLAN
 * $Id: libvlc.h,v 1.4 2002/06/11 09:44:22 gbazin Exp $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#define Nothing here, this is just to prevent update-po from being stupid

/*****************************************************************************
 * Configuration options for the main program. Each module will also separatly
 * define its own configuration options.
 * Look into configuration.h if you need to know more about the following
 * macros.
 *****************************************************************************/
#define INTF_TEXT N_("interface module")
#define INTF_LONGTEXT N_( \
    "This option allows you to select the interface used by vlc. " \
    "The default behavior is to automatically select the best module " \
    "available.")

#define VERBOSE_TEXT N_("be verbose")
#define VERBOSE_LONGTEXT N_( \
    "This options activates the output of information messages.")

#define QUIET_TEXT N_("be quiet")
#define QUIET_LONGTEXT N_( \
    "This options turns off all warning and information messages.")

#define COLOR_TEXT N_("color messages")
#define COLOR_LONGTEXT N_( \
    "When this option is turned on, the messages sent to the console will " \
    "be colorized. Your terminal needs Linux color support for this to work.")

#define INTF_PATH_TEXT N_("interface default search path")
#define INTF_PATH_LONGTEXT N_( \
    "This option allows you to set the default path that the interface will " \
    "open when looking for a file.")

#define AOUT_TEXT N_("audio output module")
#define AOUT_LONGTEXT N_( \
    "This option allows you to select the audio output method used by vlc. " \
    "The default behavior is to automatically select the best method " \
    "available.")

#define AUDIO_TEXT N_("enable audio")
#define AUDIO_LONGTEXT N_( \
    "You can completely disable the audio output. In this case the audio " \
    "decoding stage won't be done, and it will save some processing power.")

#define MONO_TEXT N_("force mono audio")
#define MONO_LONGTEXT N_("This will force a mono audio output")

#define VOLUME_TEXT N_("audio output volume")
#define VOLUME_LONGTEXT N_( \
    "You can set the default audio output volume here, in a range from 0 to " \
    "1024.")

#define FORMAT_TEXT N_("audio output format")
#define FORMAT_LONGTEXT N_( \
    "You can force the audio output format here.\n" \
    "0 -> 16 bits signed native endian (default)\n" \
    "1 ->  8 bits unsigned\n"                       \
    "2 -> 16 bits signed little endian\n"           \
    "3 -> 16 bits signed big endian\n"              \
    "4 ->  8 bits signed\n"                         \
    "5 -> 16 bits unsigned little endian\n"         \
    "6 -> 16 bits unsigned big endian\n"            \
    "7 -> mpeg2 audio (unsupported)\n"              \
    "8 -> ac3 pass-through")

#define RATE_TEXT N_("audio output frequency (Hz)")
#define RATE_LONGTEXT N_( \
    "You can force the audio output frequency here. Common values are " \
    "48000, 44100, 32000, 22050, 16000, 11025, 8000.")

#define DESYNC_TEXT N_("compensate desynchronization of audio (in ms)")
#define DESYNC_LONGTEXT N_( \
    "This option allows you to delay the audio output. This can be handy if " \
    "you notice a lag between the video and the audio.")

#define VOUT_TEXT N_("video output module")
#define VOUT_LONGTEXT N_( \
    "This option allows you to select the video output method used by vlc. " \
    "The default behavior is to automatically select the best " \
    "method available.")

#define VIDEO_TEXT N_("enable video")
#define VIDEO_LONGTEXT N_( \
    "You can completely disable the video output. In this case the video " \
    "decoding stage won't be done, which will save some processing power.")

#define DISPLAY_TEXT N_("display identifier")
#define DISPLAY_LONGTEXT N_( \
    "This is the local display port that will be used for X11 drawing. " \
    "For instance :0.1.")

#define WIDTH_TEXT N_("video width")
#define WIDTH_LONGTEXT N_( \
    "You can enforce the video width here. By default vlc will " \
    "adapt to the video characteristics.")

#define HEIGHT_TEXT N_("video height")
#define HEIGHT_LONGTEXT N_( \
    "You can enforce the video height here. By default vlc will " \
    "adapt to the video characteristics.")

#define ZOOM_TEXT N_("zoom video")
#define ZOOM_LONGTEXT N_( \
    "You can zoom the video by the specified factor.")

#define GRAYSCALE_TEXT N_("grayscale video output")
#define GRAYSCALE_LONGTEXT N_( \
    "When enabled, the color information from the video won't be decoded " \
    "(this can also allow you to save some processing power).")

#define FULLSCREEN_TEXT N_("fullscreen video output")
#define FULLSCREEN_LONGTEXT N_( \
    "If this option is enabled, vlc will always start a video in fullscreen " \
    "mode.")

#define OVERLAY_TEXT N_("overlay video output")
#define OVERLAY_LONGTEXT N_( \
    "If enabled, vlc will try to take advantage of the overlay capabilities " \
    "of you graphics card.")

#define SPUMARGIN_TEXT N_("force SPU position")
#define SPUMARGIN_LONGTEXT N_( \
    "You can use this option to place the subtitles under the movie, " \
    "instead of over the movie. Try several positions.")

#define FILTER_TEXT N_("video filter module")
#define FILTER_LONGTEXT N_( \
    "This will allow you to add a post-processing filter to enhance the " \
    "picture quality, for instance deinterlacing, or to clone or distort " \
    "the video window.")

#define SERVER_PORT_TEXT N_("server port")
#define SERVER_PORT_LONGTEXT N_( \
    "This is the port used for UDP streams. By default, we chose 1234.")

#define NETCHANNEL_TEXT N_("enable network channel mode")
#define NETCHANNEL_LONGTEXT N_( \
    "Activate this option if you want to use the VideoLAN Channel Server.")

#define CHAN_SERV_TEXT N_("channel server address")
#define CHAN_SERV_LONGTEXT N_( \
    "Indicate here the address of the VideoLAN Channel Server.")

#define CHAN_PORT_TEXT N_("channel server port")
#define CHAN_PORT_LONGTEXT N_( \
    "Indicate here the port on which the VideoLAN Channel Server runs.")

#define IFACE_TEXT N_("network interface")
#define IFACE_LONGTEXT N_( \
    "If you have several interfaces on your Linux machine and use the " \
    "VLAN solution, you may indicate here which interface to use.")

#define INPUT_PROGRAM_TEXT N_("choose program (SID)")
#define INPUT_PROGRAM_LONGTEXT N_( \
    "Choose the program to select by giving its Service ID.")

#define INPUT_AUDIO_TEXT N_("choose audio")
#define INPUT_AUDIO_LONGTEXT N_( \
    "Give the default type of audio you want to use in a DVD.")

#define INPUT_CHAN_TEXT N_("choose channel")
#define INPUT_CHAN_LONGTEXT N_( \
    "Give the stream number of the audio channel you want to use in a DVD " \
    "(from 1 to n).")

#define INPUT_SUBT_TEXT N_("choose subtitles")
#define INPUT_SUBT_LONGTEXT N_( \
    "Give the stream number of the subtitle channel you want to use in a " \
    "DVD (from 1 to n).")

#define DVD_DEV_TEXT N_("DVD device")
#define DVD_DEV_LONGTEXT N_( \
    "This is the default DVD device to use.")

#define VCD_DEV_TEXT N_("VCD device")
#define VCD_DEV_LONGTEXT N_( \
    "This is the default VCD device to use.")

#define IPV6_TEXT N_("force IPv6")
#define IPV6_LONGTEXT N_( \
    "If you check this box, IPv6 will be used by default for all UDP and " \
    "HTTP connections.")

#define IPV4_TEXT N_("force IPv4")
#define IPV4_LONGTEXT N_( \
    "If you check this box, IPv4 will be used by default for all UDP and " \
    "HTTP connections.")

#define ADEC_MPEG_TEXT N_("choose MPEG audio decoder")
#define ADEC_MPEG_LONGTEXT N_( \
    "This allows you to select the MPEG audio decoder you want to use. " \
    "Common choices are builtin and mad.")

#define ADEC_AC3_TEXT N_("choose AC3 audio decoder")
#define ADEC_AC3_LONGTEXT N_( \
    "This allows you to select the AC3/A52 audio decoder you want to use. " \
    "Common choices are builtin and a52.")

#define MMX_TEXT N_("enable CPU MMX support")
#define MMX_LONGTEXT N_( \
    "If your processor supports the MMX instructions set, vlc can take " \
    "advantage of them.")

#define THREE_DN_TEXT N_("enable CPU 3D Now! support")
#define THREE_DN_LONGTEXT N_( \
    "If your processor supports the 3D Now! instructions set, vlc can take "\
    "advantage of them.")

#define MMXEXT_TEXT N_("enable CPU MMX EXT support")
#define MMXEXT_LONGTEXT N_( \
    "If your processor supports the MMX EXT instructions set, vlc can take "\
    "advantage of them.")

#define SSE_TEXT N_("enable CPU SSE support")
#define SSE_LONGTEXT N_( \
    "If your processor supports the SSE instructions set, vlc can take " \
    "can take advantage of them.")

#define ALTIVEC_TEXT N_("enable CPU AltiVec support")
#define ALTIVEC_LONGTEXT N_( \
    "If your processor supports the AltiVec instructions set, vlc can take "\
    "advantage of them.")

#define PL_LAUNCH_TEXT N_("launch playlist on startup")
#define PL_LAUNCH_LONGTEXT N_( \
    "If you want vlc to start playing on startup, then enable this option.")

#define PL_ENQUEUE_TEXT N_("enqueue items in playlist")
#define PL_ENQUEUE_LONGTEXT N_( \
    "If you want vlc to add items to the playlist as you open them, then " \
    "enable this option.")

#define PL_LOOP_TEXT N_("loop playlist on end")
#define PL_LOOP_LONGTEXT N_( \
    "If you want vlc to keep playing the playlist indefinitely then enable " \
    "this option.")

#define MEMCPY_TEXT N_("memory copy module")
#define MEMCPY_LONGTEXT N_( \
    "You can select wich memory copy module you want to use. By default vlc " \
    "will select the fastest one supported by your hardware.")

#define ACCESS_TEXT N_("access module")
#define ACCESS_LONGTEXT N_( \
    "This is a legacy entry to let you configure access modules")

#define DEMUX_TEXT N_("demux module")
#define DEMUX_LONGTEXT N_( \
    "This is a legacy entry to let you configure demux modules")

#define FAST_PTHREAD_TEXT N_("fast pthread on NT/2K/XP (developpers only)")
#define FAST_PTHREAD_LONGTEXT N_( \
    "On Windows NT/2K/XP we use a slow but correct pthread implementation, " \
    "you can also use this faster implementation but you might experience " \
    "problems with it.")

#define PLAYLIST_USAGE N_("\nPlaylist items:" \
    "\n  *.mpg, *.vob                   plain MPEG-1/2 files" \
    "\n  [dvd:][device][@raw_device][@[title][,[chapter][,angle]]]" \
    "\n                                 DVD device" \
    "\n  [vcd:][device][@[title][,[chapter]]" \
    "\n                                 VCD device" \
    "\n  udpstream:[@[<bind address>][:<bind port>]]" \
    "\n                                 UDP stream sent by VLS" \
    "\n  vlc:loop                       loop execution of the " \
    "playlist" \
    "\n  vlc:pause                      pause execution of " \
    "playlist items" \
    "\n  vlc:quit                       quit VLC" \
    "\n")

/*
 * Quick usage guide for the configuration options:
 *
 * MODULE_CONFIG_START
 * MODULE_CONFIG_STOP
 * ADD_CATEGORY_HINT( N_(text), N_(longtext) )
 * ADD_SUBCATEGORY_HINT( N_(text), N_(longtext) )
 * ADD_USAGE_HINT( N_(text) )
 * ADD_STRING( option_name, value, p_callback, N_(text), N_(longtext) )
 * ADD_FILE( option_name, psz_value, p_callback, N_(text), N_(longtext) )
 * ADD_MODULE( option_name, psz_value, i_capability, p_callback,
 *             N_(text), N_(longtext) )
 * ADD_INTEGER( option_name, i_value, p_callback, N_(text), N_(longtext) )
 * ADD_BOOL( option_name, b_value, p_callback, N_(text), N_(longtext) )
 */

MODULE_CONFIG_START

/* Interface options */
ADD_CATEGORY_HINT( N_("Interface"), NULL)
ADD_MODULE_WITH_SHORT ( "intf", 'I', MODULE_CAPABILITY_INTF, NULL, NULL, INTF_TEXT, INTF_LONGTEXT )
ADD_BOOL_WITH_SHORT ( "verbose", 'v', 0, NULL, VERBOSE_TEXT, VERBOSE_LONGTEXT )
ADD_BOOL_WITH_SHORT ( "quiet", 'q', 0, NULL, QUIET_TEXT, QUIET_LONGTEXT )
ADD_BOOL            ( "color", 0, NULL, COLOR_TEXT, COLOR_LONGTEXT )
ADD_STRING  ( "search-path", NULL, NULL, INTF_PATH_TEXT, INTF_PATH_LONGTEXT )

/* Audio options */
ADD_CATEGORY_HINT( N_("Audio"), NULL)
ADD_MODULE_WITH_SHORT ( "aout", 'A', MODULE_CAPABILITY_AOUT, NULL, NULL, AOUT_TEXT, AOUT_LONGTEXT )
ADD_BOOL    ( "audio", 1, NULL, AUDIO_TEXT, AUDIO_LONGTEXT )
ADD_BOOL    ( "mono", 0, NULL, MONO_TEXT, MONO_LONGTEXT )
ADD_INTEGER ( "volume", VOLUME_DEFAULT, NULL, VOLUME_TEXT, VOLUME_LONGTEXT )
ADD_INTEGER ( "rate", 44100, NULL, RATE_TEXT, RATE_LONGTEXT )
ADD_INTEGER ( "desync", 0, NULL, DESYNC_TEXT, DESYNC_LONGTEXT )
ADD_INTEGER ( "audio-format", 0, NULL, FORMAT_TEXT, FORMAT_LONGTEXT )

/* Video options */
ADD_CATEGORY_HINT( N_("Video"), NULL )
ADD_MODULE_WITH_SHORT ( "vout", 'V', MODULE_CAPABILITY_VOUT, NULL, NULL, VOUT_TEXT, VOUT_LONGTEXT )
ADD_BOOL    ( "video", 1, NULL, VIDEO_TEXT, VIDEO_LONGTEXT )
ADD_INTEGER ( "width", -1, NULL, WIDTH_TEXT, WIDTH_LONGTEXT )
ADD_INTEGER ( "height", -1, NULL, HEIGHT_TEXT, HEIGHT_LONGTEXT )
ADD_FLOAT   ( "zoom", 1, NULL, ZOOM_TEXT, ZOOM_LONGTEXT )
ADD_BOOL    ( "grayscale", 0, NULL, GRAYSCALE_TEXT, GRAYSCALE_LONGTEXT )
ADD_BOOL    ( "fullscreen", 0, NULL, FULLSCREEN_TEXT, FULLSCREEN_LONGTEXT )
ADD_BOOL    ( "overlay", 1, NULL, OVERLAY_TEXT, OVERLAY_LONGTEXT )
ADD_INTEGER ( "spumargin", -1, NULL, SPUMARGIN_TEXT, SPUMARGIN_LONGTEXT )
ADD_MODULE  ( "filter", MODULE_CAPABILITY_VOUT_FILTER, NULL, NULL, FILTER_TEXT, FILTER_LONGTEXT )

/* Input options */
ADD_CATEGORY_HINT( N_("Input"), NULL )
ADD_INTEGER ( "server-port", 1234, NULL, SERVER_PORT_TEXT, SERVER_PORT_LONGTEXT )
ADD_BOOL    ( "network-channel", 0, NULL, NETCHANNEL_TEXT, NETCHANNEL_LONGTEXT )
ADD_STRING  ( "channel-server", "localhost", NULL, CHAN_SERV_TEXT, CHAN_SERV_LONGTEXT )
ADD_INTEGER ( "channel-port", 6010, NULL, CHAN_PORT_TEXT, CHAN_PORT_LONGTEXT )
ADD_STRING  ( "iface", "eth0", NULL, IFACE_TEXT, IFACE_LONGTEXT )

ADD_INTEGER ( "program", 0, NULL, INPUT_PROGRAM_TEXT, INPUT_PROGRAM_LONGTEXT )
ADD_INTEGER ( "audio-type", -1, NULL, INPUT_AUDIO_TEXT, INPUT_AUDIO_LONGTEXT )
ADD_INTEGER ( "audio-channel", -1, NULL, INPUT_CHAN_TEXT, INPUT_CHAN_LONGTEXT )
ADD_INTEGER ( "spu-channel", -1, NULL, INPUT_SUBT_TEXT, INPUT_SUBT_LONGTEXT )

ADD_STRING  ( "dvd", DVD_DEVICE, NULL, DVD_DEV_TEXT, DVD_DEV_LONGTEXT )
ADD_STRING  ( "vcd", VCD_DEVICE, NULL, VCD_DEV_TEXT, VCD_DEV_LONGTEXT )

ADD_BOOL_WITH_SHORT ( "ipv6", '6', 0, NULL, IPV6_TEXT, IPV6_LONGTEXT )
ADD_BOOL_WITH_SHORT ( "ipv4", '4', 0, NULL, IPV4_TEXT, IPV4_LONGTEXT )

/* Decoder options */
ADD_CATEGORY_HINT( N_("Decoders"), NULL )
ADD_MODULE  ( "mpeg-adec", MODULE_CAPABILITY_DECODER, NULL, NULL, ADEC_MPEG_TEXT, ADEC_MPEG_LONGTEXT )
ADD_MODULE  ( "ac3-adec", MODULE_CAPABILITY_DECODER, NULL, NULL, ADEC_AC3_TEXT, ADEC_AC3_LONGTEXT )

/* CPU options */
ADD_CATEGORY_HINT( N_("CPU"), NULL )
ADD_BOOL ( "mmx", 1, NULL, MMX_TEXT, MMX_LONGTEXT )
ADD_BOOL ( "3dn", 1, NULL, THREE_DN_TEXT, THREE_DN_LONGTEXT )
ADD_BOOL ( "mmxext", 1, NULL, MMXEXT_TEXT, MMXEXT_LONGTEXT )
ADD_BOOL ( "sse", 1, NULL, SSE_TEXT, SSE_LONGTEXT )
ADD_BOOL ( "altivec", 1, NULL, ALTIVEC_TEXT, ALTIVEC_LONGTEXT )

/* Playlist options */
ADD_CATEGORY_HINT( N_("Playlist"), NULL )
ADD_BOOL ( "playlist", 0, NULL, PL_LAUNCH_TEXT, PL_LAUNCH_LONGTEXT )
ADD_BOOL ( "enqueue", 0, NULL, PL_ENQUEUE_TEXT, PL_ENQUEUE_LONGTEXT )
ADD_BOOL ( "loop", 0, NULL, PL_LOOP_TEXT, PL_LOOP_LONGTEXT )

/* Misc options */
ADD_CATEGORY_HINT( N_("Miscellaneous"), NULL )
ADD_MODULE  ( "memcpy", MODULE_CAPABILITY_MEMCPY, NULL, NULL, MEMCPY_TEXT, MEMCPY_LONGTEXT )
ADD_MODULE  ( "access", MODULE_CAPABILITY_ACCESS, NULL, NULL, ACCESS_TEXT, ACCESS_LONGTEXT )
ADD_MODULE  ( "demux", MODULE_CAPABILITY_DEMUX, NULL, NULL, DEMUX_TEXT, DEMUX_LONGTEXT )

#if defined(WIN32)
ADD_BOOL ( "fast-pthread", 0, NULL, FAST_PTHREAD_TEXT, FAST_PTHREAD_LONGTEXT )
#endif

/* Usage (mainly useful for cmd line stuff) */
ADD_USAGE_HINT( PLAYLIST_USAGE )

MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( N_("main program") )
    ADD_CAPABILITY( MAIN, 100/*whatever*/ )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

static module_config_t p_help_config[] =
{
    { CONFIG_ITEM_BOOL, "help", 'h', N_("print help") },
    { CONFIG_ITEM_BOOL, "longhelp", 'H', N_("print detailed help") },
    { CONFIG_ITEM_BOOL, "list", 'l', N_("print a list of available modules") },
    { CONFIG_ITEM_STRING, "module", 'p', N_("print help on module") },
    { CONFIG_ITEM_BOOL, "version", '\0', N_("print version information") },
    { CONFIG_ITEM_BOOL, "build", '\0', N_("print build information") },
    { CONFIG_HINT_END, NULL, '\0' }
};

/*****************************************************************************
 * End configuration.
 *****************************************************************************/
