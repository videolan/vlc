/*****************************************************************************
 * libvlc.h: main libvlc header
 *****************************************************************************
 * Copyright (C) 1998-2002 VideoLAN
 * $Id: libvlc.h,v 1.35 2003/01/11 20:25:36 gitan Exp $
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

#define VERBOSE_TEXT N_("verbosity (0,1,2)")
#define VERBOSE_LONGTEXT N_( \
    "This options sets the verbosity level (0=only errors and " \
    "standard messages, 1=warnings, 2=debug).")

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

#define PLUGIN_PATH_TEXT N_("plugin search path")
#define PLUGIN_PATH_LONGTEXT N_( \
    "This option allows you to specify an additional path for vlc to look " \
    "for its plugins.")

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

#define AOUT_RATE_TEXT N_("audio output frequency (Hz)")
#define AOUT_RATE_LONGTEXT N_( \
    "You can force the audio output frequency here. Common values are " \
    "48000, 44100, 32000, 22050, 16000, 11025, 8000.")

#define DESYNC_TEXT N_("compensate desynchronization of audio (in ms)")
#define DESYNC_LONGTEXT N_( \
    "This option allows you to delay the audio output. This can be handy if " \
    "you notice a lag between the video and the audio.")

#define HEADPHONE_TEXT N_("headphone virtual spatialization effect")
#define HEADPHONE_LONGTEXT N_( \
    "This effect gives you the feeling that you stands in a real room " \
    "with a complete 5.1 speaker set when using only a headphone, " \
    "providing a more realistic sound experience. It should also be " \
    "more comfortable and less tiring when listening to music for " \
    "long periods of time.\nIt works with any source format from mono " \
    "to 5.1.")

#define HEADPHONE_DIM_TEXT N_("characteristic dimension")
#define HEADPHONE_DIM_LONGTEXT N_( \
     "Headphone virtual spatialization effect parameter: "\
     "distance between front left speaker and listener in meters.")

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
    "of your graphic card.")

#define SPUMARGIN_TEXT N_("force SPU position")
#define SPUMARGIN_LONGTEXT N_( \
    "You can use this option to place the subtitles under the movie, " \
    "instead of over the movie. Try several positions.")

#define FILTER_TEXT N_("video filter module")
#define FILTER_LONGTEXT N_( \
    "This will allow you to add a post-processing filter to enhance the " \
    "picture quality, for instance deinterlacing, or to clone or distort " \
    "the video window.")

#define ASPECT_RATIO_TEXT N_("source aspect ratio")
#define ASPECT_RATIO_LONGTEXT N_( \
    "This will force the source aspect ratio. For instance, some DVDs claim " \
    "to be 16:9 while they are actually 4:3. This can also be used as a " \
    "hint for VLC when a movie does not have aspect ratio information. " \
    "Accepted formats are x:y (4:3, 16:9, etc.) expressing the global image " \
    "aspect, or a float value (1.25, 1.3333, etc.) expressing pixel " \
    "squareness.")

#if 0
#define PIXEL_RATIO_TEXT N_("destination aspect ratio")
#define PIXEL_RATIO_LONGTEXT N_( \
    "This will force the destination pixel size. By default VLC assumes " \
    "your pixels are square, unless your hardware has a way to tell it " \
    "otherwise. This may be used when you output VLC's signal to another " \
    "device such as a TV set. Accepted format is a float value (1, 1.25, " \
    "1.3333, etc.) expressing pixel squareness.")
#endif

#define SERVER_PORT_TEXT N_("server port")
#define SERVER_PORT_LONGTEXT N_( \
    "This is the port used for UDP streams. By default, we chose 1234.")

#define MTU_TEXT N_("MTU of the network interface")
#define MTU_LONGTEXT N_( \
    "This is the typical size of UDP packets that we expect. On Ethernet " \
    "it is usually 1500.")

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

#define IFACE_ADDR_TEXT N_("network interface address")
#define IFACE_ADDR_LONGTEXT N_( \
    "If you have several interfaces on your machine and use the " \
    "multicast solution, you will probably have to indicate the IP address " \
    "of your multicasting interface here.")

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
#ifdef WIN32
#define DVD_DEV_LONGTEXT N_( \
    "This is the default DVD drive (or file) to use. Don't forget the colon " \
    "after the drive letter (eg D:)")
#else
#define DVD_DEV_LONGTEXT N_( \
    "This is the default DVD device to use.")
#endif

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

#define CODEC_TEXT N_("choose prefered codec list")
#define CODEC_LONGTEXT N_( \
    "This allows you to select the order in which vlc will choose its " \
    "codecs. For instance, 'a52old,a52,any' will try the old a52 codec " \
    "before the new one. Please be aware that vlc does not make any " \
    "difference between audio or video codecs, so you should always specify " \
    "'any' at the end of the list to make sure there is a fallback for the " \
    "types you didn't specify.")

#define SOUT_TEXT N_("choose a stream output")
#define SOUT_LONGTEXT N_( \
    "Empty if no stream output.")

#define SOUT_VIDEO_TEXT N_("enable video stream output")
#define SOUT_VIDEO_LONGTEXT N_( \
    "This allows you to choose if the video stream should be redirected to " \
    "the stream output facility when this last one is enabled.")

#define SOUT_AUDIO_TEXT N_("enable audio stream output")
#define SOUT_AUDIO_LONGTEXT N_( \
    "This allows you to choose if the video stream should be redirected to " \
    "the stream output facility when this last one is enabled.")

#define PACKETIZER_TEXT N_("choose prefered packetizer list")
#define PACKETIZER_LONGTEXT N_( \
    "This allows you to select the order in which vlc will choose its " \
    "packetizers."  )

#define MUX_TEXT N_("mux module")
#define MUX_LONGTEXT N_( \
    "This is a legacy entry to let you configure mux modules")

#define ACCESS_OUTPUT_TEXT N_("access output module")
#define ACCESS_OUTPUT_LONGTEXT N_( \
    "This is a legacy entry to let you configure access output modules")


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
    "advantage of them.")

#define ALTIVEC_TEXT N_("enable CPU AltiVec support")
#define ALTIVEC_LONGTEXT N_( \
    "If your processor supports the AltiVec instructions set, vlc can take "\
    "advantage of them.")

#define RANDOM_TEXT N_("play files randomly forever")
#define RANDOM_LONGTEXT N_( \
    "When selected, vlc will randomly play files in the playlist until " \
    "interrupted.")

#define LAUNCH_TEXT N_("launch playlist on startup")
#define LAUNCH_LONGTEXT N_( \
    "If you want vlc to start playing on startup, then enable this option.")

#define ENQUEUE_TEXT N_("enqueue items in playlist")
#define ENQUEUE_LONGTEXT N_( \
    "If you want vlc to add items to the playlist as you open them, then " \
    "enable this option.")

#define LOOP_TEXT N_("loop playlist on end")
#define LOOP_LONGTEXT N_( \
    "If you want vlc to keep playing the playlist indefinitely then enable " \
    "this option.")

#define MEMCPY_TEXT N_("memory copy module")
#define MEMCPY_LONGTEXT N_( \
    "You can select which memory copy module you want to use. By default" \
    "vlc will select the fastest one supported by your hardware.")

#define ACCESS_TEXT N_("access module")
#define ACCESS_LONGTEXT N_( \
    "This is a legacy entry to let you configure access modules")

#define DEMUX_TEXT N_("demux module")
#define DEMUX_LONGTEXT N_( \
    "This is a legacy entry to let you configure demux modules")

#define FAST_MUTEX_TEXT N_("fast mutex on NT/2K/XP (developers only)")
#define FAST_MUTEX_LONGTEXT N_( \
    "On Windows NT/2K/XP we use a slow mutex implementation but which " \
    "allows us to correctely implement condition variables. " \
    "You can also use the faster Win9x implementation but you might " \
    "experience problems with it.")

#define WIN9X_CV_TEXT N_("Condition variables implementation for Win9x " \
    "(developers only)")
#define WIN9X_CV_LONGTEXT N_( \
    "On Windows 9x/Me we use a fast but not correct condition variables " \
    "implementation (more precisely there is a possibility for a race " \
    "condition to happen). " \
    "However it is possible to use slower alternatives which should be more " \
    "robust. " \
    "Currently you can choose between implementation 0 (which is the " \
    "default and the fastest), 1 and 2.")

#define PLAYLIST_USAGE N_("\nPlaylist items:" \
    "\n  *.mpg, *.vob                   plain MPEG-1/2 files" \
    "\n  [dvd:][device][@raw_device][@[title][,[chapter][,angle]]]" \
    "\n                                 DVD device" \
    "\n  [vcd:][device][@[title][,[chapter]]" \
    "\n                                 VCD device" \
    "\n  udpstream:[@[<bind address>][:<bind port>]]" \
    "\n                                 UDP stream sent by VLS" \
    "\n  vlc:pause                      pause execution of " \
    "playlist items" \
    "\n  vlc:quit                       quit VLC" \
    "\n")

#define SAP_TEXT N_( "Session Announcement Protocol support" )
#define SAP_LONGTEXT N_( "Session Announcement Protocol support" )


/*
 * Quick usage guide for the configuration options:
 *
 * add_category_hint( N_(text), N_(longtext) );
 * add_subcategory_hint( N_(text), N_(longtext) );
 * add_usage_hint( N_(text) );
 * add_string( option_name, value, p_callback, N_(text), N_(longtext) );
 * add_file( option_name, psz_value, p_callback, N_(text), N_(longtext) );
 * add_module( option_name, psz_value, i_capability, p_callback,
 *             N_(text), N_(longtext) );
 * add_integer( option_name, i_value, p_callback, N_(text), N_(longtext) );
 * add_bool( option_name, b_value, p_callback, N_(text), N_(longtext) );
 */

vlc_module_begin();
    /* Interface options */
    add_category_hint( N_("Interface"), NULL);
    add_module_with_short( "intf", 'I', "interface", NULL, NULL,
                           INTF_TEXT, INTF_LONGTEXT );
    add_integer_with_short( "verbose", 'v', -1, NULL,
                            VERBOSE_TEXT, VERBOSE_LONGTEXT );
    add_bool_with_short( "quiet", 'q', 0, NULL, QUIET_TEXT, QUIET_LONGTEXT );
    add_bool( "color", 0, NULL, COLOR_TEXT, COLOR_LONGTEXT );
    add_string( "search-path", NULL, NULL, INTF_PATH_TEXT, INTF_PATH_LONGTEXT );
    add_string( "plugin-path", NULL, NULL,
                PLUGIN_PATH_TEXT, PLUGIN_PATH_LONGTEXT );

    /* Audio options */
    add_category_hint( N_("Audio"), NULL);
    add_module_with_short( "aout", 'A', "audio output", NULL, NULL,
                           AOUT_TEXT, AOUT_LONGTEXT );
    add_bool( "audio", 1, NULL, AUDIO_TEXT, AUDIO_LONGTEXT );
    add_integer_with_range( "volume", 256, 0, 1024, NULL, VOLUME_TEXT, VOLUME_LONGTEXT );
    add_integer( "aout-rate", -1, NULL, AOUT_RATE_TEXT, AOUT_RATE_LONGTEXT );
    add_integer( "desync", 0, NULL, DESYNC_TEXT, DESYNC_LONGTEXT );
    add_bool( "headphone", 0, NULL, HEADPHONE_TEXT, HEADPHONE_LONGTEXT );
    add_integer( "headphone-dim", 5, NULL, HEADPHONE_DIM_TEXT,
                 HEADPHONE_DIM_LONGTEXT );

    /* Video options */
    add_category_hint( N_("Video"), NULL );
    add_module_with_short( "vout", 'V', "video output", NULL, NULL,
                           VOUT_TEXT, VOUT_LONGTEXT );
    add_bool( "video", 1, NULL, VIDEO_TEXT, VIDEO_LONGTEXT );
    add_integer( "width", -1, NULL, WIDTH_TEXT, WIDTH_LONGTEXT );
    add_integer( "height", -1, NULL, HEIGHT_TEXT, HEIGHT_LONGTEXT );
    add_float( "zoom", 1, NULL, ZOOM_TEXT, ZOOM_LONGTEXT );
    add_bool( "grayscale", 0, NULL, GRAYSCALE_TEXT, GRAYSCALE_LONGTEXT );
    add_bool( "fullscreen", 0, NULL, FULLSCREEN_TEXT, FULLSCREEN_LONGTEXT );
    add_bool( "overlay", 1, NULL, OVERLAY_TEXT, OVERLAY_LONGTEXT );
    add_integer( "spumargin", -1, NULL, SPUMARGIN_TEXT, SPUMARGIN_LONGTEXT );
    add_module( "filter", "video filter", NULL, NULL,
                FILTER_TEXT, FILTER_LONGTEXT );
    add_string( "aspect-ratio", "", NULL,
                ASPECT_RATIO_TEXT, ASPECT_RATIO_TEXT );
#if 0
    add_string( "pixel-ratio", "1", NULL, PIXEL_RATIO_TEXT, PIXEL_RATIO_TEXT );
#endif

    /* Input options */
    add_category_hint( N_("Input"), NULL );
    add_integer( "server-port", 1234, NULL,
                 SERVER_PORT_TEXT, SERVER_PORT_LONGTEXT );
    add_bool( "network-channel", 0, NULL,
              NETCHANNEL_TEXT, NETCHANNEL_LONGTEXT );
    add_string( "channel-server", "localhost", NULL,
                CHAN_SERV_TEXT, CHAN_SERV_LONGTEXT );
    add_integer( "channel-port", 6010, NULL,
                 CHAN_PORT_TEXT, CHAN_PORT_LONGTEXT );
    add_integer( "mtu", 1500, NULL, MTU_TEXT, MTU_LONGTEXT );
#ifdef SYS_DARWIN
    add_string( "iface", "en0", NULL, IFACE_TEXT, IFACE_LONGTEXT );
#else
    add_string( "iface", "eth0", NULL, IFACE_TEXT, IFACE_LONGTEXT );
#endif
    add_string( "iface-addr", "", NULL, IFACE_ADDR_TEXT, IFACE_ADDR_LONGTEXT );

    add_integer( "program", 0, NULL,
                 INPUT_PROGRAM_TEXT, INPUT_PROGRAM_LONGTEXT );
    add_integer( "audio-type", -1, NULL,
                 INPUT_AUDIO_TEXT, INPUT_AUDIO_LONGTEXT );
    add_integer( "audio-channel", -1, NULL,
                 INPUT_CHAN_TEXT, INPUT_CHAN_LONGTEXT );
    add_integer( "spu-channel", -1, NULL,
                 INPUT_SUBT_TEXT, INPUT_SUBT_LONGTEXT );

    add_string( "dvd", DVD_DEVICE, NULL, DVD_DEV_TEXT, DVD_DEV_LONGTEXT );
    add_string( "vcd", VCD_DEVICE, NULL, VCD_DEV_TEXT, VCD_DEV_LONGTEXT );

    add_bool_with_short( "ipv6", '6', 0, NULL, IPV6_TEXT, IPV6_LONGTEXT );
    add_bool_with_short( "ipv4", '4', 0, NULL, IPV4_TEXT, IPV4_LONGTEXT );

    /* Decoder options */
    add_category_hint( N_("Decoders"), NULL );
    add_module( "codec", "decoder", NULL, NULL, CODEC_TEXT, CODEC_LONGTEXT );

    /* Stream output options */
    add_category_hint( N_("Stream output"), NULL );
    add_module( "packetizer", "packetizer", NULL, NULL,
                PACKETIZER_TEXT, PACKETIZER_LONGTEXT );
    add_module( "mux", "sout mux", NULL, NULL, MUX_TEXT, MUX_LONGTEXT );
    add_module( "access_output", "sout access", NULL, NULL,
                ACCESS_OUTPUT_TEXT, ACCESS_OUTPUT_LONGTEXT );
    add_string( "sout", NULL, NULL, SOUT_TEXT, SOUT_LONGTEXT );
    add_bool( "sout-video", 1, NULL, SOUT_VIDEO_TEXT, SOUT_VIDEO_LONGTEXT );
    add_bool( "sout-audio", 1, NULL, SOUT_AUDIO_TEXT, SOUT_AUDIO_LONGTEXT );

    /* CPU options */
    add_category_hint( N_("CPU"), NULL );
#if defined( __i386__ )
    add_bool( "mmx", 1, NULL, MMX_TEXT, MMX_LONGTEXT );
    add_bool( "3dn", 1, NULL, THREE_DN_TEXT, THREE_DN_LONGTEXT );
    add_bool( "mmxext", 1, NULL, MMXEXT_TEXT, MMXEXT_LONGTEXT );
    add_bool( "sse", 1, NULL, SSE_TEXT, SSE_LONGTEXT );
#endif
#if defined( __powerpc__ ) || defined( SYS_DARWIN )
    add_bool( "altivec", 1, NULL, ALTIVEC_TEXT, ALTIVEC_LONGTEXT );
#endif

    /* Playlist options */
    add_category_hint( N_("Playlist"), NULL );
    add_bool_with_short( "random", 'Z', 0, NULL, RANDOM_TEXT, RANDOM_LONGTEXT );
    add_bool( "playlist", 0, NULL, LAUNCH_TEXT, LAUNCH_LONGTEXT );
    add_bool( "enqueue", 0, NULL, ENQUEUE_TEXT, ENQUEUE_LONGTEXT );
    add_bool( "loop", 0, NULL, LOOP_TEXT, LOOP_LONGTEXT );

    /* Misc options */
    add_category_hint( N_("Miscellaneous"), NULL );
    add_bool( "sap", 0, NULL, SAP_TEXT, SAP_LONGTEXT );
    add_module( "memcpy", "memcpy", NULL, NULL, MEMCPY_TEXT, MEMCPY_LONGTEXT );
    add_module( "access", "access", NULL, NULL, ACCESS_TEXT, ACCESS_LONGTEXT );
    add_module( "demux", "demux", NULL, NULL, DEMUX_TEXT, DEMUX_LONGTEXT );

#if defined(WIN32)
    add_bool( "fast-mutex", 0, NULL, FAST_MUTEX_TEXT, FAST_MUTEX_LONGTEXT );
    add_integer( "win9x-cv-method", 0, NULL, WIN9X_CV_TEXT, WIN9X_CV_LONGTEXT );
#endif

    /* Usage (mainly useful for cmd line stuff) */
    add_usage_hint( PLAYLIST_USAGE );

    set_description( N_("main program") );
    set_capability( "main", 100 );
vlc_module_end();

static module_config_t p_help_config[] =
{
    { CONFIG_ITEM_BOOL, NULL, "help", 'h', N_("print help"),
      NULL, NULL, 0, 0.0, 0, 0, 0.0, 0.0, NULL, NULL, NULL, VLC_FALSE },
    { CONFIG_ITEM_BOOL, NULL, "longhelp", 'H', N_("print detailed help"),
      NULL, NULL, 0, 0.0, 0, 0, 0.0, 0.0, NULL, NULL, NULL, VLC_FALSE },
    { CONFIG_ITEM_BOOL, NULL, "list", 'l',
                              N_("print a list of available modules"),
      NULL, NULL, 0, 0.0, 0, 0, 0.0, 0.0, NULL, NULL, NULL, VLC_FALSE },
    { CONFIG_ITEM_STRING, NULL, "module", 'p', N_("print help on module"),
      NULL, NULL, 0, 0.0, 0, 0, 0.0, 0.0, NULL, NULL, NULL, VLC_FALSE },
    { CONFIG_ITEM_BOOL, NULL, "version", '\0',
                              N_("print version information"),
      NULL, NULL, 0, 0.0, 0, 0, 0.0, 0.0, NULL, NULL, NULL, VLC_FALSE },
    { CONFIG_HINT_END, NULL, NULL, '\0', NULL,
      NULL, NULL, 0, 0.0, 0, 0, 0.0, 0.0, NULL, NULL, NULL, VLC_FALSE }
};

/*****************************************************************************
 * End configuration.
 *****************************************************************************/
