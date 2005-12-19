/*****************************************************************************
 * libvlc.h: main libvlc header
 *****************************************************************************
 * Copyright (C) 1998-2005 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Jean-Paul Saman <jpsaman #_at_# m2x.nl>
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
#include "vlc_keys.h"

static char *ppsz_language[] =
{ "auto", "en", "en_GB", "ca", "da", "de", "es",
  "fr", "it", "ja", "ko", "nl", "pt_BR", "ro", "ru", "tr", "zh_CN", "zh_TW" };
static char *ppsz_language_text[] =
{ N_("Auto"), N_("American English"), N_("British English"), N_("Catalan"), N_("Danish"), N_("German"), N_("Spanish"), N_("French"), N_("Italian"), N_("Japanese"), N_("Korean"), N_("Dutch"), N_("Brazilian Portuguese"), N_("Romanian"), N_("Russian"), N_("Turkish"), N_("Simplified Chinese"), N_("Chinese Traditional") };

static char *ppsz_snap_formats[] =
{ "png", "jpg" };

/*****************************************************************************
 * Configuration options for the main program. Each module will also separatly
 * define its own configuration options.
 * Look into configuration.h if you need to know more about the following
 * macros.
 *****************************************************************************/

#define INTF_CAT_LONGTEXT N_( \
    "These options allow you to configure the interfaces used by VLC.\n" \
    "You can select the main interface, additional " \
    "interface modules, and define various related options." )

#define INTF_TEXT N_("Interface module")
#define INTF_LONGTEXT N_( \
    "This option allows you to select the interface used by VLC.\n" \
    "The default behavior is to automatically select the best module " \
    "available.")

#define EXTRAINTF_TEXT N_("Extra interface modules")
#define EXTRAINTF_LONGTEXT N_( \
    "This option allows you to select additional interfaces used by VLC. " \
    "They will be launched in the background in addition to the default " \
    "interface. Use a comma separated list of interface modules. (common " \
    "values are logger, gestures, sap, rc, http or screensaver)")

#define CONTROL_TEXT N_("Control interfaces")
#define CONTROL_LONGTEXT N_( \
    "This option allows you to select control interfaces. " )

#define VERBOSE_TEXT N_("Verbosity (0,1,2)")
#define VERBOSE_LONGTEXT N_( \
    "This option sets the verbosity level (0=only errors and " \
    "standard messages, 1=warnings, 2=debug).")

#define QUIET_TEXT N_("Be quiet")
#define QUIET_LONGTEXT N_( \
    "This option turns off all warning and information messages.")

#define OPEN_TEXT N_("Default stream")
#define OPEN_LONGTEXT N_( \
    "This option allows you to always open a default stream on start-up." )

#define LANGUAGE_TEXT N_("Language")
#define LANGUAGE_LONGTEXT N_( "This option allows you to set the language " \
    "of the interface. The system language is auto-detected if \"auto\" is " \
    "specified here." )

#define COLOR_TEXT N_("Color messages")
#define COLOR_LONGTEXT N_( \
    "When this option is turned on, the messages sent to the console will " \
    "be colorized. Your terminal needs Linux color support for this to work.")

#define ADVANCED_TEXT N_("Show advanced options")
#define ADVANCED_LONGTEXT N_( \
    "When this option is turned on, the preferences and/or interfaces  will " \
    "show all the available options, including those that most users should " \
    "never touch.")

#define AOUT_CAT_LONGTEXT N_( \
    "These options allow you to modify the behavior of the audio " \
    "subsystem, and to add audio filters which can be used for " \
    "post processing or visual effects (spectrum analyzer, etc.).\n" \
    "Enable these filters here, and configure them in the \"audio filters\" " \
    "modules section.")

#define AOUT_TEXT N_("Audio output module")
#define AOUT_LONGTEXT N_( \
    "This option allows you to select the audio output method used by VLC. " \
    "The default behavior is to automatically select the best method " \
    "available.")

#define AUDIO_TEXT N_("Enable audio")
#define AUDIO_LONGTEXT N_( \
    "You can completely disable the audio output. In this case, the audio " \
    "decoding stage will not take place, thus saving some processing power.")

#define MONO_TEXT N_("Force mono audio")
#define MONO_LONGTEXT N_("This will force a mono audio output.")

#define VOLUME_TEXT N_("Default audio volume")
#define VOLUME_LONGTEXT N_( \
    "You can set the default audio output volume here, in a range from 0 to " \
    "1024.")

#define VOLUME_SAVE_TEXT N_("Audio output saved volume")
#define VOLUME_SAVE_LONGTEXT N_( \
    "This saves the audio output volume when you select mute.")

#define VOLUME_STEP_TEXT N_("Audio output volume step")
#define VOLUME_STEP_LONGTEXT N_( \
    "The step size of the volume is adjustable using this option, " \
    "in a range from 0 to 1024." )

#define AOUT_RATE_TEXT N_("Audio output frequency (Hz)")
#define AOUT_RATE_LONGTEXT N_( \
    "You can force the audio output frequency here. Common values are " \
    "-1 (default), 48000, 44100, 32000, 22050, 16000, 11025, 8000.")

#if !defined( SYS_DARWIN )
#define AOUT_RESAMP_TEXT N_("High quality audio resampling")
#define AOUT_RESAMP_LONGTEXT N_( \
    "This uses a high quality audio resampling algorithm. High quality "\
    "audio resampling can be processor intensive so you can " \
    "disable it and a cheaper resampling algorithm will be used instead.")
#endif

#define DESYNC_TEXT N_("Audio desynchronization compensation")
#define DESYNC_LONGTEXT N_( \
    "This option allows you to delay the audio output. You must give a " \
    "number of milliseconds. This can be handy if you notice a lag " \
    "between the video and the audio.")

#define MULTICHA_TEXT N_("Preferred audio output channels mode")
#define MULTICHA_LONGTEXT N_( \
    "This option allows you to set the audio output channels mode that will " \
    "be used by default when possible (ie. if your hardware supports it as " \
    "well as the audio stream being played).")

#define SPDIF_TEXT N_("Use the S/PDIF audio output when available")
#define SPDIF_LONGTEXT N_( \
    "This option allows you to use the S/PDIF audio output by default when " \
    "your hardware supports it as well as the audio stream being played.")

#define FORCE_DOLBY_TEXT N_("Force detection of Dolby Surround")
#define FORCE_DOLBY_LONGTEXT N_( \
    "Use this when you know your stream is (or is not) encoded with Dolby "\
    "Surround but fails to be detected as such. And even if the stream is "\
    "not actually encoded with Dolby Surround, turning on this option might "\
    "enhance your experience, especially when combined with the Headphone "\
    "Channel Mixer." )
static int pi_force_dolby_values[] = { 0, 1, 2 };
static char *ppsz_force_dolby_descriptions[] = { N_("Auto"), N_("On"), N_("Off") };


#define AUDIO_FILTER_TEXT N_("Audio filters")
#define AUDIO_FILTER_LONGTEXT N_( \
    "This allows you to add audio post processing filters, to modify " \
    "the sound" )

#define AUDIO_VISUAL_TEXT N_("Audio visualizations ")
#define AUDIO_VISUAL_LONGTEXT N_( \
    "This allows you to add visualization modules " \
    "(spectrum analyzer, etc.).")

#define VOUT_CAT_LONGTEXT N_( \
    "These options allow you to modify the behavior of the video output " \
    "subsystem. You can for example enable video filters (deinterlacing, " \
    "image adjusting, etc.). Enable these filters here and configure " \
    "them in the \"video filters\" modules section. You can also set many " \
    "miscellaneous video options." )

#define VOUT_TEXT N_("Video output module")
#define VOUT_LONGTEXT N_( \
    "This option allows you to select the video output method used by VLC. " \
    "The default behavior is to automatically select the best " \
    "method available.")

#define VIDEO_TEXT N_("Enable video")
#define VIDEO_LONGTEXT N_( \
    "You can completely disable the video output. In this case, the video " \
    "decoding stage will not take place, thus saving some processing power.")

#define WIDTH_TEXT N_("Video width")
#define WIDTH_LONGTEXT N_( \
    "You can enforce the video width here. By default (-1) VLC will " \
    "adapt to the video characteristics.")

#define HEIGHT_TEXT N_("Video height")
#define HEIGHT_LONGTEXT N_( \
    "You can enforce the video height here. By default (-1) VLC will " \
    "adapt to the video characteristics.")

#define VIDEOX_TEXT N_("Video x coordinate")
#define VIDEOX_LONGTEXT N_( \
    "You can enforce the position of the top left corner of the video window "\
    "here (x coordinate).")

#define VIDEOY_TEXT N_("Video y coordinate")
#define VIDEOY_LONGTEXT N_( \
    "You can enforce the position of the top left corner of the video window "\
    "here (y coordinate).")

#define VIDEO_TITLE_TEXT N_("Video title")
#define VIDEO_TITLE_LONGTEXT N_( \
    "You can specify a custom video window title here.")

#define ALIGN_TEXT N_("Video alignment")
#define ALIGN_LONGTEXT N_( \
    "You can enforce the video alignment in its window. By default (0) it " \
    "will be centered (0=center, 1=left, 2=right, 4=top, 8=bottom, you can " \
    "also use combinations of these values).")
static int pi_align_values[] = { 0, 1, 2, 4, 8, 5, 6, 9, 10 };
static char *ppsz_align_descriptions[] =
{ N_("Center"), N_("Left"), N_("Right"), N_("Top"), N_("Bottom"),
  N_("Top-Left"), N_("Top-Right"), N_("Bottom-Left"), N_("Bottom-Right") };

#define ZOOM_TEXT N_("Zoom video")
#define ZOOM_LONGTEXT N_( \
    "You can zoom the video by the specified factor.")

#define GRAYSCALE_TEXT N_("Grayscale video output")
#define GRAYSCALE_LONGTEXT N_( \
    "When enabled, the color information from the video won't be decoded " \
    "(this can also allow you to save some processing power).")

#define FULLSCREEN_TEXT N_("Fullscreen video output")
#define FULLSCREEN_LONGTEXT N_( \
    "If this option is enabled, VLC will always start a video in fullscreen " \
    "mode.")

#define OVERLAY_TEXT N_("Overlay video output")
#define OVERLAY_LONGTEXT N_( \
    "If enabled, VLC will try to take advantage of the overlay capabilities " \
    "of your graphics card (hardware acceleration).")

#define VIDEO_ON_TOP_TEXT N_("Always on top")
#define VIDEO_ON_TOP_LONGTEXT N_("Always place the video window on top of " \
    "other windows." )

#define SS_TEXT N_("Disable screensaver")
#define SS_LONGTEXT N_("Disable the screensaver during video playback." )

#define VIDEO_DECO_TEXT N_("Window decorations")
#define VIDEO_DECO_LONGTEXT N_( \
    "If this option is disabled, VLC will avoid creating window caption, " \
    "frames, etc... around the video.")

#define FILTER_TEXT N_("Video filter module")
#define FILTER_LONGTEXT N_( \
    "This will allow you to add a post-processing filter to enhance the " \
    "picture quality, for instance deinterlacing, or to clone or distort " \
    "the video window.")

#define SNAP_PATH_TEXT N_("Video snapshot directory")
#define SNAP_PATH_LONGTEXT N_( \
    "Allows you to specify the directory where the video snapshots will " \
    "be stored.")

#define SNAP_FORMAT_TEXT N_("Video snapshot format")
#define SNAP_FORMAT_LONGTEXT N_( \
    "Allows you to specify the image format in which the video snapshots will " \
    "be stored.")

#define CROP_TEXT N_("Video cropping")
#define CROP_LONGTEXT N_( \
    "This will force the cropping of the source video. " \
    "Accepted formats are x:y (4:3, 16:9, etc.) expressing the global image " \
    "aspect.")

#define ASPECT_RATIO_TEXT N_("Source aspect ratio")
#define ASPECT_RATIO_LONGTEXT N_( \
    "This will force the source aspect ratio. For instance, some DVDs claim " \
    "to be 16:9 while they are actually 4:3. This can also be used as a " \
    "hint for VLC when a movie does not have aspect ratio information. " \
    "Accepted formats are x:y (4:3, 16:9, etc.) expressing the global image " \
    "aspect, or a float value (1.25, 1.3333, etc.) expressing pixel " \
    "squareness.")

#define HDTV_FIX_TEXT N_("Fix HDTV height")
#define HDTV_FIX_LONGTEXT N_( \
    "This option allows proper handling of HDTV-1080 video format " \
    "even if broken encoder incorrectly set height to 1088 lines. " \
    "Disable this option only if your video has non-standard format " \
    "requiring all 1088 lines.")

#define MASPECT_RATIO_TEXT N_("Monitor pixel aspect ratio")
#define MASPECT_RATIO_LONGTEXT N_( \
    "This will force the monitor aspect ratio. Most monitors have square " \
    "pixels (1:1). If you have a 16:9 screen, you might need to change this " \
    "to 4:3 in order to keep proportions.")

#define SKIP_FRAMES_TEXT N_("Skip frames")
#define SKIP_FRAMES_LONGTEXT N_( \
    "This option enables framedropping on MPEG2 stream. Framedropping " \
    "occurs when your computer is not powerful enough" )

#define QUIET_SYNCHRO_TEXT N_("Quiet synchro")
#define QUIET_SYNCHRO_LONGTEXT N_( \
    "Enable this option to avoid flooding the message log with debug " \
    "output from the video output synchro.")

#define INPUT_CAT_LONGTEXT N_( \
    "These options allow you to modify the behavior of the input " \
    "subsystem, such as the DVD or VCD device, the network interface " \
    "settings or the subtitle channel.")

#define CR_AVERAGE_TEXT N_("Clock reference average counter")
#define CR_AVERAGE_LONGTEXT N_( \
    "When using the PVR input (or a very irregular source), you should " \
    "set this to 10000.")

#define CLOCK_SYNCHRO_TEXT N_("Clock synchronisation")
#define CLOCK_SYNCHRO_LONGTEXT N_( \
    "Allows you to enable/disable the input clock synchronisation for " \
    "real-time sources.")

static int pi_clock_values[] = { -1, 0, 1 };
static char *ppsz_clock_descriptions[] =
{ N_("Default"), N_("Disable"), N_("Enable") };

#define SERVER_PORT_TEXT N_("UDP port")
#define SERVER_PORT_LONGTEXT N_( \
    "This is the port used for UDP streams. By default, we chose 1234.")

#define MTU_TEXT N_("MTU of the network interface")
#define MTU_LONGTEXT N_( \
    "This is the maximum packet size that can be transmitted " \
    "over network interface. On Ethernet it is usually 1500 bytes.")

#define TTL_TEXT N_("Hop limit (TTL)")
#define TTL_LONGTEXT N_( \
    "Specify the hop limit (TTL) of the multicast packets sent by " \
    "the stream output.")

#define MIFACE_TEXT N_("Multicast output interface")
#define MIFACE_LONGTEXT N_( \
    "Indicate here the multicast output interface. " \
    "This overrides the routing table.")

#define INPUT_PROGRAM_TEXT N_("Program to select")
#define INPUT_PROGRAM_LONGTEXT N_( \
    "Choose the program to select by giving its Service ID.\n" \
    "Only use this option if you want to read a multi-program stream " \
    "(like DVB streams for example)." )

#define INPUT_PROGRAMS_TEXT N_("Programs to select")
#define INPUT_PROGRAMS_LONGTEXT N_( \
    "Choose the programs to select by giving a comma-separated list of " \
    "SIDs.\n" \
    "Only use this option if you want to read a multi-program stream " \
    "(like DVB streams for example)." )

#define INPUT_AUDIOTRACK_TEXT N_("Audio track")
#define INPUT_AUDIOTRACK_LONGTEXT N_( \
    "Give the stream number of the audio track you want to use " \
    "(from 0 to n).")

#define INPUT_SUBTRACK_TEXT N_("Subtitles track")
#define INPUT_SUBTRACK_LONGTEXT N_( \
    "Give the stream number of the subtitle track you want to use " \
    "(from 0 to n).")

#define INPUT_AUDIOTRACK_LANG_TEXT N_("Audio language")
#define INPUT_AUDIOTRACK_LANG_LONGTEXT N_( \
    "Give the language of the audio track you want to use " \
    "(comma separted, two or tree letter country code).")

#define INPUT_SUBTRACK_LANG_TEXT N_("Subtitle language")
#define INPUT_SUBTRACK_LANG_LONGTEXT N_( \
    "Give the language of the subtitle track you want to use " \
    "(comma separted, two or tree letter country code).")

#define INPUT_AUDIOTRACK_ID_TEXT N_("Audio track ID")
#define INPUT_AUDIOTRACK_ID_LONGTEXT N_( \
    "Give the stream ID of the audio track you want to use.")

#define INPUT_SUBTRACK_ID_TEXT N_("Subtitles track ID")
#define INPUT_SUBTRACK_ID_LONGTEXT N_( \
    "Give the stream ID of the subtitle track you want to use.")

#define INPUT_REPEAT_TEXT N_("Input repetitions")
#define INPUT_REPEAT_LONGTEXT N_("Number of time the same input will be " \
                                 "repeated")

#define START_TIME_TEXT N_("Input start time (seconds)")
#define START_TIME_LONGTEXT N_("Input start time (seconds)")

#define STOP_TIME_TEXT N_("Input stop time (seconds)")
#define STOP_TIME_LONGTEXT N_("Input stop time (seconds)")

#define INPUT_LIST_TEXT N_("Input list")
#define INPUT_LIST_LONGTEXT N_("Allows you to specify a comma-separated list " \
    "of inputs that will be concatenated after the normal one.")

#define INPUT_SLAVE_TEXT N_("Input slave (experimental)")
#define INPUT_SLAVE_LONGTEXT N_("Allows you to play from several streams at " \
    "the same time. This feature is experimental, not all formats " \
    "are supported.")

#define BOOKMARKS_TEXT N_("Bookmarks list for a stream")
#define BOOKMARKS_LONGTEXT N_("You can specify a list of bookmarks for a stream in " \
    "the form \"{name=bookmark-name,time=optional-time-offset," \
    "bytes=optional-byte-offset},{...}\"")

#define SUB_CAT_LONGTEXT N_( \
    "These options allow you to modify the behavior of the subpictures " \
    "subsystem. You can for example enable subpictures filters (logo, etc.). " \
    "Enable these filters here and configure them in the " \
    "\"subpictures filters\" modules section. You can also set many " \
    "miscellaneous subpictures options." )

#define SUB_MARGIN_TEXT N_("Force subtitle position")
#define SUB_MARGIN_LONGTEXT N_( \
    "You can use this option to place the subtitles under the movie, " \
    "instead of over the movie. Try several positions.")

#define OSD_TEXT N_("On Screen Display")
#define OSD_LONGTEXT N_( \
    "VLC can display messages on the video. This is called OSD (On Screen " \
    "Display). You can disable this feature here.")

#define SUB_FILTER_TEXT N_("Subpictures filter module")
#define SUB_FILTER_LONGTEXT N_( \
    "This will allow you to add a subpictures filter for instance to overlay "\
    "a logo.")

#define SUB_AUTO_TEXT N_("Autodetect subtitle files")
#define SUB_AUTO_LONGTEXT \
    N_("Automatically detect a subtitle file, if no subtitle filename is " \
    "specified.")

#define SUB_FUZZY_TEXT N_("Subtitle autodetection fuzziness")
#define SUB_FUZZY_LONGTEXT \
    N_("This determines how fuzzy subtitle and movie filename matching " \
    "will be. Options are:\n" \
    "0 = no subtitles autodetected\n" \
    "1 = any subtitle file\n" \
    "2 = any subtitle file containing the movie name\n" \
    "3 = subtitle file matching the movie name with additional chars\n" \
    "4 = subtitle file matching the movie name exactly")

#define SUB_PATH_TEXT N_("Subtitle autodetection paths")
#define SUB_PATH_LONGTEXT \
    N_("Look for a subtitle file in those paths too, if your subtitle " \
    "file was not found in the current directory.")

#define SUB_FILE_TEXT N_("Use subtitle file")
#define SUB_FILE_LONGTEXT \
    N_("Load this subtitle file. To be used when autodetect cannot detect " \
    "your subtitle file.")

#define DVD_DEV_TEXT N_("DVD device")
#ifdef WIN32
#define DVD_DEV_LONGTEXT N_( \
    "This is the default DVD drive (or file) to use. Don't forget the colon " \
    "after the drive letter (eg. D:)")
#else
#define DVD_DEV_LONGTEXT N_( \
    "This is the default DVD device to use.")
#endif

#define VCD_DEV_TEXT N_("VCD device")
#ifdef HAVE_VCDX
#define VCD_DEV_LONGTEXT N_( \
    "This is the default VCD device to use. " \
    "If you don't specify anything, we'll scan for a suitable CD-ROM device." )
#else
#define VCD_DEV_LONGTEXT N_( \
    "This is the default VCD device to use." )
#endif

#define CDAUDIO_DEV_TEXT N_("Audio CD device")
#ifdef HAVE_CDDAX
#define CDAUDIO_DEV_LONGTEXT N_( \
    "This is the default Audio CD device to use. " \
    "If you don't specify anything, we'll scan for a suitable CD-ROM device." )
#else
#define CDAUDIO_DEV_LONGTEXT N_( \
    "This is the default Audio CD device to use." )
#endif

#define IPV6_TEXT N_("Force IPv6")
#define IPV6_LONGTEXT N_( \
    "If you check this box, IPv6 will be used by default for all UDP and " \
    "HTTP connections.")

#define IPV4_TEXT N_("Force IPv4")
#define IPV4_LONGTEXT N_( \
    "If you check this box, IPv4 will be used by default for all UDP and " \
    "HTTP connections.")

#define TIMEOUT_TEXT N_("TCP connection timeout in ms")
#define TIMEOUT_LONGTEXT N_( \
    "Allows you to modify the default TCP connection timeout. This " \
    "value should be set in millisecond units." )

#define SOCKS_SERVER_TEXT N_("SOCKS server")
#define SOCKS_SERVER_LONGTEXT N_( \
    "Allow you to specify a SOCKS server to use. It must be of the form " \
    "address:port . It will be used for all TCP connections" )

#define SOCKS_USER_TEXT N_("SOCKS user name")
#define SOCKS_USER_LONGTEXT N_("Allows you to modify the user name that will " \
    "be used for the connection to the SOCKS server.")

#define SOCKS_PASS_TEXT N_("SOCKS password")
#define SOCKS_PASS_LONGTEXT N_("Allows you to modify the password that will " \
    "be used for the connection to the SOCKS server.")

#define META_TITLE_TEXT N_("Title metadata")
#define META_TITLE_LONGTEXT N_( \
     "Allows you to specify a \"title\" metadata for an input.")

#define META_AUTHOR_TEXT N_("Author metadata")
#define META_AUTHOR_LONGTEXT N_( \
     "Allows you to specify an \"author\" metadata for an input.")

#define META_ARTIST_TEXT N_("Artist metadata")
#define META_ARTIST_LONGTEXT N_( \
     "Allows you to specify an \"artist\" metadata for an input.")

#define META_GENRE_TEXT N_("Genre metadata")
#define META_GENRE_LONGTEXT N_( \
     "Allows you to specify a \"genre\" metadata for an input.")

#define META_CPYR_TEXT N_("Copyright metadata")
#define META_CPYR_LONGTEXT N_( \
     "Allows you to specify a \"copyright\" metadata for an input.")

#define META_DESCR_TEXT N_("Description metadata")
#define META_DESCR_LONGTEXT N_( \
     "Allows you to specify a \"description\" metadata for an input.")

#define META_DATE_TEXT N_("Date metadata")
#define META_DATE_LONGTEXT N_( \
     "Allows you to specify a \"date\" metadata for an input.")

#define META_URL_TEXT N_("URL metadata")
#define META_URL_LONGTEXT N_( \
     "Allows you to specify a \"url\" metadata for an input.")

#define CODEC_CAT_LONGTEXT N_( \
    "This option can be used to alter the way VLC selects " \
    "its codecs (decompression methods). Only advanced users should " \
    "alter this option as it can break playback of all your streams." )

#define CODEC_TEXT N_("Preferred codecs list")
#define CODEC_LONGTEXT N_( \
    "This allows you to select a list of codecs that VLC will use in " \
    "priority. For instance, 'dummy,a52' will try the dummy and a52 codecs " \
    "before trying the other ones.")

#define ENCODER_TEXT N_("Preferred encoders list")
#define ENCODER_LONGTEXT N_( \
    "This allows you to select a list of encoders that VLC will use in " \
    "priority")

#define SOUT_CAT_LONGTEXT N_( \
    "These options allow you to set default global options for the " \
    "stream output subsystem." )

#define SOUT_TEXT N_("Default stream output chain")
#define SOUT_LONGTEXT N_( \
    "You can enter here a default stream output chain. Refer to "\
    "the documentation to learn how to build such chains." \
    "Warning: this chain will be enabled for all streams." )

#define SOUT_ALL_TEXT N_("Enable streaming of all ES")
#define SOUT_ALL_LONGTEXT N_( \
    "This allows you to stream all ES (video, audio and subtitles)")

#define SOUT_DISPLAY_TEXT N_("Display while streaming")
#define SOUT_DISPLAY_LONGTEXT N_( \
    "This allows you to play the stream while streaming it.")

#define SOUT_VIDEO_TEXT N_("Enable video stream output")
#define SOUT_VIDEO_LONGTEXT N_( \
    "This allows you to choose if the video stream should be redirected to " \
    "the stream output facility when this last one is enabled.")

#define SOUT_AUDIO_TEXT N_("Enable audio stream output")
#define SOUT_AUDIO_LONGTEXT N_( \
    "This allows you to choose if the audio stream should be redirected to " \
    "the stream output facility when this last one is enabled.")

#define SOUT_KEEP_TEXT N_("Keep stream output open" )
#define SOUT_KEEP_LONGTEXT N_( \
    "This allows you to keep an unique stream output instance across " \
    "multiple playlist item (automatically insert the gather stream output " \
    "if not specified)" )

#define PACKETIZER_TEXT N_("Preferred packetizer list")
#define PACKETIZER_LONGTEXT N_( \
    "This allows you to select the order in which VLC will choose its " \
    "packetizers."  )

#define MUX_TEXT N_("Mux module")
#define MUX_LONGTEXT N_( \
    "This is a legacy entry to let you configure mux modules")

#define ACCESS_OUTPUT_TEXT N_("Access output module")
#define ACCESS_OUTPUT_LONGTEXT N_( \
    "This is a legacy entry to let you configure access output modules")

#define ANN_SAPCTRL_TEXT N_("Control SAP flow")
#define ANN_SAPCTRL_LONGTEXT N_("If this option is enabled, the flow on " \
    "the SAP multicast address will be controlled. This is needed if you " \
    "want to make announcements on the MBone" )

#define ANN_SAPINTV_TEXT N_("SAP announcement interval")
#define ANN_SAPINTV_LONGTEXT N_("When the SAP flow control is disabled, " \
    "this lets you set the fixed interval between SAP announcements" )

#define CPU_CAT_LONGTEXT N_( \
    "These options allow you to enable special CPU optimizations.\n" \
    "You should always leave all these enabled." )

#define FPU_TEXT N_("Enable FPU support")
#define FPU_LONGTEXT N_( \
    "If your processor has a floating point calculation unit, VLC can take " \
    "advantage of it.")

#define MMX_TEXT N_("Enable CPU MMX support")
#define MMX_LONGTEXT N_( \
    "If your processor supports the MMX instructions set, VLC can take " \
    "advantage of them.")

#define THREE_DN_TEXT N_("Enable CPU 3D Now! support")
#define THREE_DN_LONGTEXT N_( \
    "If your processor supports the 3D Now! instructions set, VLC can take " \
    "advantage of them.")

#define MMXEXT_TEXT N_("Enable CPU MMX EXT support")
#define MMXEXT_LONGTEXT N_( \
    "If your processor supports the MMX EXT instructions set, VLC can take " \
    "advantage of them.")

#define SSE_TEXT N_("Enable CPU SSE support")
#define SSE_LONGTEXT N_( \
    "If your processor supports the SSE instructions set, VLC can take " \
    "advantage of them.")

#define SSE2_TEXT N_("Enable CPU SSE2 support")
#define SSE2_LONGTEXT N_( \
    "If your processor supports the SSE2 instructions set, VLC can take " \
    "advantage of them.")

#define ALTIVEC_TEXT N_("Enable CPU AltiVec support")
#define ALTIVEC_LONGTEXT N_( \
    "If your processor supports the AltiVec instructions set, VLC can take " \
    "advantage of them.")

#define PLAYLIST_CAT_LONGTEXT N_( \
     "These options define the behavior of the playlist. Some " \
     "of them can be overridden in the playlist dialog box." )

#define SD_TEXT N_( "Services discovery modules")
#define SD_LONGTEXT N_( \
     "Specifies the services discovery modules to load, separated by " \
     "semi-colons. Typical values are sap, hal, ..." )

#define RANDOM_TEXT N_("Play files randomly forever")
#define RANDOM_LONGTEXT N_( \
    "When selected, VLC will randomly play files in the playlist until " \
    "interrupted.")

#define LOOP_TEXT N_("Repeat all")
#define LOOP_LONGTEXT N_( \
    "If you want VLC to keep playing the playlist indefinitely then enable " \
    "this option.")

#define REPEAT_TEXT N_("Repeat current item")
#define REPEAT_LONGTEXT N_( \
    "When this is active, VLC will keep playing the current playlist item " \
    "over and over again.")

#define PAS_TEXT N_("Play and stop")
#define PAS_LONGTEXT N_( \
    "Stop the playlist after each played playlist item. " )

#define MISC_CAT_LONGTEXT N_( \
    "These options allow you to select default modules. Leave these " \
    "alone unless you really know what you are doing." )

#define MEMCPY_TEXT N_("Memory copy module")
#define MEMCPY_LONGTEXT N_( \
    "You can select which memory copy module you want to use. By default " \
    "VLC will select the fastest one supported by your hardware.")

#define ACCESS_TEXT N_("Access module")
#define ACCESS_LONGTEXT N_( \
    "This allows you to force an access module. You can use it if " \
    "the correct access is not automatically detected. You should not "\
    "set this as a global option unless you really know what you are doing." )

#define ACCESS_FILTER_TEXT N_("Access filter module")
#define ACCESS_FILTER_LONGTEXT N_( \
    "This is a legacy entry to let you configure access filter modules.")

#define DEMUX_TEXT N_("Demux module")
#define DEMUX_LONGTEXT N_( \
    "This is a legacy entry to let you configure demux modules.")

#define RT_PRIORITY_TEXT N_("Allow real-time priority")
#define RT_PRIORITY_LONGTEXT N_( \
    "Running VLC in real-time priority will allow for much more precise " \
    "scheduling and yield better, especially when streaming content. " \
    "It can however lock up your whole machine, or make it very very " \
    "slow. You should only activate this if you know what you're " \
    "doing.")

#define RT_OFFSET_TEXT N_("Adjust VLC priority")
#define RT_OFFSET_LONGTEXT N_( \
    "This option adds an offset (positive or negative) to VLC default " \
    "priorities. You can use it to tune VLC priority against other " \
    "programs, or against other VLC instances.")

#define MINIMIZE_THREADS_TEXT N_("Minimize number of threads")
#define MINIMIZE_THREADS_LONGTEXT N_( \
     "This option minimizes the number of threads needed to run VLC")

#define PLUGIN_PATH_TEXT N_("Modules search path")
#define PLUGIN_PATH_LONGTEXT N_( \
    "This option allows you to specify an additional path for VLC to look " \
    "for its modules.")

#define VLM_CONF_TEXT N_("VLM configuration file")
#define VLM_CONF_LONGTEXT N_( \
    "This option allows you to specify a VLM configuration file that will " \
    "be read when VLM is launched.")

#define PLUGINS_CACHE_TEXT N_("Use a plugins cache")
#define PLUGINS_CACHE_LONGTEXT N_( \
    "This option allows you to use a plugins cache which will greatly " \
    "improve the start time of VLC.")

#define DAEMON_TEXT N_("Run as daemon process")
#define DAEMON_LONGTEXT N_( \
     "Runs VLC as a background daemon process.")

#define ONEINSTANCE_TEXT N_("Allow only one running instance")
#define ONEINSTANCE_LONGTEXT N_( \
    "Allowing only one running instance of VLC can sometimes be useful, " \
    "for instance if you associated VLC with some media types and you " \
    "don't want a new instance of VLC to be opened each time you " \
    "double-click on a file in the explorer. This option will allow you " \
    "to play the file with the already running instance or enqueue it.")

#define PLAYLISTENQUEUE_TEXT N_( \
    "Enqueue items to playlist when in one instance mode")
#define PLAYLISTENQUEUE_LONGTEXT N_( \
    "When using the one instance only option, enqueue items to playlist " \
    "and keep playing current item.")

#define HPRIORITY_TEXT N_("Increase the priority of the process")
#define HPRIORITY_LONGTEXT N_( \
    "Increasing the priority of the process will very likely improve your " \
    "playing experience as it allows VLC not to be disturbed by other " \
    "applications that could otherwise take too much processor time.\n" \
    "However be advised that in certain circumstances (bugs) VLC could take " \
    "all the processor time and render the whole system unresponsive which " \
    "might require a reboot of your machine.")

#define FAST_MUTEX_TEXT N_("Fast mutex on NT/2K/XP (developers only)")
#define FAST_MUTEX_LONGTEXT N_( \
    "On Windows NT/2K/XP we use a slow mutex implementation but which " \
    "allows us to correctly implement condition variables. " \
    "You can also use the faster Win9x implementation but you might " \
    "experience problems with it.")

#define WIN9X_CV_TEXT N_("Condition variables implementation for Win9x " \
    "(developers only)")
#define WIN9X_CV_LONGTEXT N_( \
    "On Windows 9x/Me you can use a fast but incorrect condition variables " \
    "implementation (more precisely there is a possibility for a race " \
    "condition to happen). " \
    "However it is possible to use slower alternatives which are more " \
    "robust. " \
    "Currently you can choose between implementation 0 (which is the " \
    "fastest but slightly incorrect), 1 (default) and 2.")

#define HOTKEY_CAT_LONGTEXT N_( "These settings are the global VLC key " \
    "bindings, known as \"hotkeys\"." )

#define FULLSCREEN_KEY_TEXT N_("Fullscreen")
#define FULLSCREEN_KEY_LONGTEXT N_("Select the hotkey to use to swap fullscreen state.")
#define PLAY_PAUSE_KEY_TEXT N_("Play/Pause")
#define PLAY_PAUSE_KEY_LONGTEXT N_("Select the hotkey to use to swap paused state.")
#define PAUSE_KEY_TEXT N_("Pause only")
#define PAUSE_KEY_LONGTEXT N_("Select the hotkey to use to pause.")
#define PLAY_KEY_TEXT N_("Play only")
#define PLAY_KEY_LONGTEXT N_("Select the hotkey to use to play.")
#define FASTER_KEY_TEXT N_("Faster")
#define FASTER_KEY_LONGTEXT N_("Select the hotkey to use for fast forward playback.")
#define SLOWER_KEY_TEXT N_("Slower")
#define SLOWER_KEY_LONGTEXT N_("Select the hotkey to use for slow motion playback.")
#define NEXT_KEY_TEXT N_("Next")
#define NEXT_KEY_LONGTEXT N_("Select the hotkey to use to skip to the next item in the playlist.")
#define PREV_KEY_TEXT N_("Previous")
#define PREV_KEY_LONGTEXT N_("Select the hotkey to use to skip to the previous item in the playlist.")
#define STOP_KEY_TEXT N_("Stop")
#define STOP_KEY_LONGTEXT N_("Select the hotkey to stop the playback.")
#define POSITION_KEY_TEXT N_("Position")
#define POSITION_KEY_LONGTEXT N_("Select the hotkey to display the position.")

#define JBEXTRASHORT_KEY_TEXT N_("Very short backwards jump")
#define JBEXTRASHORT_KEY_LONGTEXT \
    N_("Select the hotkey to make a very short backwards jump.")
#define JBSHORT_KEY_TEXT N_("Short backwards jump")
#define JBSHORT_KEY_LONGTEXT \
    N_("Select the hotkey to make a short backwards jump.")

#define JBMEDIUM_KEY_TEXT N_("Medium backwards jump")
#define JBMEDIUM_KEY_LONGTEXT \
    N_("Select the hotkey to make a medium backwards jump.")
#define JBLONG_KEY_TEXT N_("Long backwards jump")
#define JBLONG_KEY_LONGTEXT \
    N_("Select the hotkey to make a long backwards jump.")

#define JFEXTRASHORT_KEY_TEXT N_("Very short forward jump")
#define JFEXTRASHORT_KEY_LONGTEXT \
    N_("Select the hotkey to make a very short forward jump.")
#define JFSHORT_KEY_TEXT N_("Short forward jump")
#define JFSHORT_KEY_LONGTEXT \
    N_("Select the hotkey to make a short forward jump.")

#define JFMEDIUM_KEY_TEXT N_("Medium forward jump")
#define JFMEDIUM_KEY_LONGTEXT \
    N_("Select the hotkey to make a medium forward jump.")
#define JFLONG_KEY_TEXT N_("Long forward jump")
#define JFLONG_KEY_LONGTEXT \
    N_("Select the hotkey to make a long forward jump.")

#define JIEXTRASHORT_TEXT N_("Very short jump size")
#define JIEXTRASHORT_LONGTEXT N_("Very short jump \"size\", in seconds")
#define JISHORT_TEXT N_("Short jump size")
#define JISHORT_LONGTEXT N_("Short jump \"size\", in seconds")
#define JIMEDIUM_TEXT N_("Medium jump size")
#define JIMEDIUM_LONGTEXT N_("Medium jump \"size\", in seconds")
#define JILONG_TEXT N_("Long jump size")
#define JILONG_LONGTEXT N_("Long jump \"size\", in seconds")

#define QUIT_KEY_TEXT N_("Quit")
#define QUIT_KEY_LONGTEXT N_("Select the hotkey to quit the application.")
#define NAV_UP_KEY_TEXT N_("Navigate up")
#define NAV_UP_KEY_LONGTEXT N_("Select the key to move the selector up in DVD menus.")
#define NAV_DOWN_KEY_TEXT N_("Navigate down")
#define NAV_DOWN_KEY_LONGTEXT N_("Select the key to move the selector down in DVD menus.")
#define NAV_LEFT_KEY_TEXT N_("Navigate left")
#define NAV_LEFT_KEY_LONGTEXT N_("Select the key to move the selector left in DVD menus.")
#define NAV_RIGHT_KEY_TEXT N_("Navigate right")
#define NAV_RIGHT_KEY_LONGTEXT N_("Select the key to move the selector right in DVD menus.")
#define NAV_ACTIVATE_KEY_TEXT N_("Activate")
#define NAV_ACTIVATE_KEY_LONGTEXT N_("Select the key to activate selected item in DVD menus.")
#define DISC_MENU_TEXT N_("Go to the DVD menu")
#define DISC_MENU_LONGTEXT N_("Select the key to take you to the DVD menu")
#define TITLE_PREV_TEXT N_("Select previous DVD title")
#define TITLE_PREV_LONGTEXT N_("Select the key to choose the previous title from the DVD")
#define TITLE_NEXT_TEXT N_("Select next DVD title")
#define TITLE_NEXT_LONGTEXT N_("Select the key to choose the next title from the DVD")
#define CHAPTER_PREV_TEXT N_("Select prev DVD chapter")
#define CHAPTER_PREV_LONGTEXT N_("Select the key to choose the previous chapter from the DVD")
#define CHAPTER_NEXT_TEXT N_("Select next DVD chapter")
#define CHAPTER_NEXT_LONGTEXT N_("Select the key to choose the next chapter from the DVD")
#define VOL_UP_KEY_TEXT N_("Volume up")
#define VOL_UP_KEY_LONGTEXT N_("Select the key to increase audio volume.")
#define VOL_DOWN_KEY_TEXT N_("Volume down")
#define VOL_DOWN_KEY_LONGTEXT N_("Select the key to decrease audio volume.")
#define VOL_MUTE_KEY_TEXT N_("Mute")
#define VOL_MUTE_KEY_LONGTEXT N_("Select the key to turn off audio volume.")
#define SUBDELAY_UP_KEY_TEXT N_("Subtitle delay up")
#define SUBDELAY_UP_KEY_LONGTEXT N_("Select the key to increase the subtitle delay.")
#define SUBDELAY_DOWN_KEY_TEXT N_("Subtitle delay down")
#define SUBDELAY_DOWN_KEY_LONGTEXT N_("Select the key to decrease the subtitle delay.")
#define AUDIODELAY_UP_KEY_TEXT N_("Audio delay up")
#define AUDIODELAY_UP_KEY_LONGTEXT N_("Select the key to increase the audio delay.")
#define AUDIODELAY_DOWN_KEY_TEXT N_("Audio delay down")
#define AUDIODELAY_DOWN_KEY_LONGTEXT N_("Select the key to decrease the audio delay.")
#define PLAY_BOOKMARK1_KEY_TEXT N_("Play playlist bookmark 1")
#define PLAY_BOOKMARK2_KEY_TEXT N_("Play playlist bookmark 2")
#define PLAY_BOOKMARK3_KEY_TEXT N_("Play playlist bookmark 3")
#define PLAY_BOOKMARK4_KEY_TEXT N_("Play playlist bookmark 4")
#define PLAY_BOOKMARK5_KEY_TEXT N_("Play playlist bookmark 5")
#define PLAY_BOOKMARK6_KEY_TEXT N_("Play playlist bookmark 6")
#define PLAY_BOOKMARK7_KEY_TEXT N_("Play playlist bookmark 7")
#define PLAY_BOOKMARK8_KEY_TEXT N_("Play playlist bookmark 8")
#define PLAY_BOOKMARK9_KEY_TEXT N_("Play playlist bookmark 9")
#define PLAY_BOOKMARK10_KEY_TEXT N_("Play playlist bookmark 10")
#define PLAY_BOOKMARK_KEY_LONGTEXT N_("Select the key to play this bookmark.")
#define SET_BOOKMARK1_KEY_TEXT N_("Set playlist bookmark 1")
#define SET_BOOKMARK2_KEY_TEXT N_("Set playlist bookmark 2")
#define SET_BOOKMARK3_KEY_TEXT N_("Set playlist bookmark 3")
#define SET_BOOKMARK4_KEY_TEXT N_("Set playlist bookmark 4")
#define SET_BOOKMARK5_KEY_TEXT N_("Set playlist bookmark 5")
#define SET_BOOKMARK6_KEY_TEXT N_("Set playlist bookmark 6")
#define SET_BOOKMARK7_KEY_TEXT N_("Set playlist bookmark 7")
#define SET_BOOKMARK8_KEY_TEXT N_("Set playlist bookmark 8")
#define SET_BOOKMARK9_KEY_TEXT N_("Set playlist bookmark 9")
#define SET_BOOKMARK10_KEY_TEXT N_("Set playlist bookmark 10")
#define SET_BOOKMARK_KEY_LONGTEXT N_("Select the key to set this playlist bookmark.")

#define HISTORY_BACK_TEXT N_("Go back in browsing history")
#define HISTORY_BACK_LONGTEXT N_("Select the key to go back (to the previous media item) in the browsing history.")
#define HISTORY_FORWARD_TEXT N_("Go forward in browsing history")
#define HISTORY_FORWARD_LONGTEXT N_("Select the key to go forward (to the next media item) in the browsing history.")

#define AUDIO_TRACK_KEY_TEXT N_("Cycle audio track")
#define AUDIO_TRACK_KEY_LONGTEXT N_("Cycle through the available audio tracks(languages)")
#define SUBTITLE_TRACK_KEY_TEXT N_("Cycle subtitle track")
#define SUBTITLE_TRACK_KEY_LONGTEXT N_("Cycle through the available subtitle tracks")
#define ASPECT_RATIO_KEY_TEXT N_("Cycle source aspect ratio")
#define ASPECT_RATIO_KEY_LONGTEXT N_("Cycle through a predefined list of source aspect ratios")
#define CROP_KEY_TEXT N_("Cycle video crop")
#define CROP_KEY_LONGTEXT N_("Cycle through a predefined list of crop formats")
#define DEINTERLACE_KEY_TEXT N_("Cycle deinterlace modes")
#define DEINTERLACE_KEY_LONGTEXT N_("Cycle through all the deinterlace modes")
#define INTF_SHOW_KEY_TEXT N_("Show interface")
#define INTF_SHOW_KEY_LONGTEXT N_("Raise the interface above all other windows")
#define INTF_HIDE_KEY_TEXT N_("Hide interface")
#define INTF_HIDE_KEY_LONGTEXT N_("Lower the interface below all other windows")
#define SNAP_KEY_TEXT N_("Take video snapshot")
#define SNAP_KEY_LONGTEXT N_("Takes a video snapshot and writes it to disk.")

#define RECORD_KEY_TEXT N_("Record")
#define RECORD_KEY_LONGTEXT N_("Record access filter start/stop.")


#define VLC_USAGE N_( \
    "Usage: %s [options] [playlistitems] ..." \
    "\nYou can specify multiple playlistitems on the commandline. They will be enqueued in the playlist." \
    "\nThe first item specified will be played first." \
    "\n" \
    "\nOptions-styles:" \
    "\n  --option  A global option that is set for the duration of the program." \
    "\n   -option  A single letter version of a global --option." \
    "\n   :option  An option that only applies to the playlistitem directly before it" \
    "\n            and that overrides previous settings." \
    "\n" \
    "\nPlaylistitem MRL syntax:" \
    "\n  [[access][/demux]://]URL[@[title][:chapter][-[title][:chapter]]] [:option=value ...]" \
    "\n" \
    "\n  Many of the global --options can also be used as MRL specific :options." \
    "\n  Multiple :option=value pairs can be specified." \
    "\n" \
    "\nURL syntax:" \
    "\n  [file://]filename              Plain media file" \
    "\n  http://ip:port/file            HTTP URL" \
    "\n  ftp://ip:port/file             FTP URL" \
    "\n  mms://ip:port/file             MMS URL" \
    "\n  screen://                      Screen capture" \
    "\n  [dvd://][device][@raw_device]  DVD device" \
    "\n  [vcd://][device]               VCD device" \
    "\n  [cdda://][device]              Audio CD device" \
    "\n  udp:[[<source address>]@[<bind address>][:<bind port>]]" \
    "\n                                 UDP stream sent by a streaming server"\
    "\n  vlc:pause:<seconds>            Special item to pause the playlist for a certain time" \
    "\n  vlc:quit                       Special item to quit VLC" \
    "\n")


/*
 * Quick usage guide for the configuration options:
 *
 * add_category_hint( N_(text), N_(longtext), b_advanced_option );
 * add_subcategory_hint( N_(text), N_(longtext), b_advanced_option );
 * add_usage_hint( N_(text), b_advanced_option );
 * add_string( option_name, value, p_callback, N_(text), N_(longtext),
               b_advanced_option );
 * add_file( option_name, psz_value, p_callback, N_(text), N_(longtext) );
 * add_module( option_name, psz_value, i_capability, p_callback,
 *             N_(text), N_(longtext) );
 * add_integer( option_name, i_value, p_callback, N_(text), N_(longtext),
                b_advanced_option );
 * add_bool( option_name, b_value, p_callback, N_(text), N_(longtext),
             b_advanced_option );
 */

vlc_module_begin();
/* Audio options */
    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_GENERAL );
    add_category_hint( N_("Audio"), AOUT_CAT_LONGTEXT , VLC_FALSE );

    add_bool( "audio", 1, NULL, AUDIO_TEXT, AUDIO_LONGTEXT, VLC_FALSE );
    add_integer_with_range( "volume", AOUT_VOLUME_DEFAULT, AOUT_VOLUME_MIN,
                            AOUT_VOLUME_MAX, NULL, VOLUME_TEXT,
                            VOLUME_LONGTEXT, VLC_FALSE );
    add_integer_with_range( "volume-step", AOUT_VOLUME_STEP, AOUT_VOLUME_MIN,
                            AOUT_VOLUME_MAX, NULL, VOLUME_STEP_TEXT,
                            VOLUME_STEP_LONGTEXT, VLC_TRUE );
    add_integer( "aout-rate", -1, NULL, AOUT_RATE_TEXT,
                 AOUT_RATE_LONGTEXT, VLC_TRUE );
#if !defined( SYS_DARWIN )
    add_bool( "hq-resampling", 1, NULL, AOUT_RESAMP_TEXT,
              AOUT_RESAMP_LONGTEXT, VLC_TRUE );
#endif
    add_bool( "spdif", 0, NULL, SPDIF_TEXT, SPDIF_LONGTEXT, VLC_FALSE );
    add_integer( "force-dolby-surround", 0, NULL, FORCE_DOLBY_TEXT,
                 FORCE_DOLBY_LONGTEXT, VLC_FALSE );
        change_integer_list( pi_force_dolby_values, ppsz_force_dolby_descriptions, 0 );
    add_integer( "audio-desync", 0, NULL, DESYNC_TEXT,
                 DESYNC_LONGTEXT, VLC_TRUE );
    set_subcategory( SUBCAT_AUDIO_AOUT );
    add_module( "aout", "audio output", NULL, NULL, AOUT_TEXT, AOUT_LONGTEXT,
                VLC_TRUE );
    set_subcategory( SUBCAT_AUDIO_AFILTER );
    add_module_list_cat( "audio-filter", SUBCAT_AUDIO_AFILTER, 0,
                         NULL, AUDIO_FILTER_TEXT,
                         AUDIO_FILTER_LONGTEXT, VLC_FALSE );
    set_subcategory( SUBCAT_AUDIO_VISUAL );
    add_module( "audio-visual", "visualization",NULL, NULL,AUDIO_VISUAL_TEXT,
                AUDIO_VISUAL_LONGTEXT, VLC_FALSE );

/* Video options */
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_GENERAL );
    add_category_hint( N_("Video"), VOUT_CAT_LONGTEXT , VLC_FALSE );

    add_bool( "video", 1, NULL, VIDEO_TEXT, VIDEO_LONGTEXT, VLC_TRUE );
    add_bool( "grayscale", 0, NULL, GRAYSCALE_TEXT,
              GRAYSCALE_LONGTEXT, VLC_FALSE );
    add_bool( "fullscreen", 0, NULL, FULLSCREEN_TEXT,
              FULLSCREEN_LONGTEXT, VLC_FALSE );
        change_short('f');
    add_bool( "skip-frames", 1, NULL, SKIP_FRAMES_TEXT,
              SKIP_FRAMES_LONGTEXT, VLC_TRUE );
    add_bool( "quiet-synchro", 0, NULL, QUIET_SYNCHRO_TEXT,
              QUIET_SYNCHRO_LONGTEXT, VLC_TRUE );
#ifndef SYS_DARWIN
    add_bool( "overlay", 1, NULL, OVERLAY_TEXT, OVERLAY_LONGTEXT, VLC_TRUE );
#endif
    add_bool( "video-on-top", 0, NULL, VIDEO_ON_TOP_TEXT,
              VIDEO_ON_TOP_LONGTEXT, VLC_FALSE );
    add_bool( "disable-screensaver", VLC_TRUE, NULL, SS_TEXT, SS_LONGTEXT,
              VLC_TRUE );

    set_section( N_("Snapshot") , NULL );
    add_directory( "snapshot-path", NULL, NULL, SNAP_PATH_TEXT,
                   SNAP_PATH_LONGTEXT, VLC_FALSE );
    add_string( "snapshot-format", "png", NULL, SNAP_FORMAT_TEXT,
                   SNAP_FORMAT_LONGTEXT, VLC_FALSE );
        change_string_list( ppsz_snap_formats, NULL, 0 );

    set_section( N_("Window properties" ), NULL );
    add_integer( "width", -1, NULL, WIDTH_TEXT, WIDTH_LONGTEXT, VLC_TRUE );
    add_integer( "height", -1, NULL, HEIGHT_TEXT, HEIGHT_LONGTEXT, VLC_TRUE );
    add_integer( "video-x", -1, NULL, VIDEOX_TEXT, VIDEOX_LONGTEXT, VLC_TRUE );
    add_integer( "video-y", -1, NULL, VIDEOY_TEXT, VIDEOY_LONGTEXT, VLC_TRUE );
    add_string( "crop", NULL, NULL, CROP_TEXT, CROP_LONGTEXT, VLC_FALSE );
    add_string( "aspect-ratio", NULL, NULL,
                ASPECT_RATIO_TEXT, ASPECT_RATIO_LONGTEXT, VLC_FALSE );
    add_string( "monitor-par", NULL, NULL,
                MASPECT_RATIO_TEXT, MASPECT_RATIO_LONGTEXT, VLC_TRUE );
    add_bool( "hdtv-fix", 1, NULL, HDTV_FIX_TEXT, HDTV_FIX_LONGTEXT, VLC_TRUE );
    add_bool( "video-deco", 1, NULL, VIDEO_DECO_TEXT,
              VIDEO_DECO_LONGTEXT, VLC_TRUE );
    add_string( "video-title", NULL, NULL, VIDEO_TITLE_TEXT,
                 VIDEO_TITLE_LONGTEXT, VLC_TRUE );
    add_integer( "align", 0, NULL, ALIGN_TEXT, ALIGN_LONGTEXT, VLC_TRUE );
        change_integer_list( pi_align_values, ppsz_align_descriptions, 0 );
    add_float( "zoom", 1, NULL, ZOOM_TEXT, ZOOM_LONGTEXT, VLC_TRUE );


    set_subcategory( SUBCAT_VIDEO_VOUT );
    add_module( "vout", "video output", NULL, NULL, VOUT_TEXT, VOUT_LONGTEXT,
                VLC_TRUE );
        change_short('V');

    set_subcategory( SUBCAT_VIDEO_VFILTER );
    add_module_list_cat( "vout-filter", SUBCAT_VIDEO_VFILTER, NULL, NULL,
                FILTER_TEXT, FILTER_LONGTEXT, VLC_FALSE );
       add_deprecated( "filter", VLC_FALSE ); /*deprecated since 0.8.2 */
#if 0
    add_string( "pixel-ratio", "1", NULL, PIXEL_RATIO_TEXT, PIXEL_RATIO_TEXT );
#endif

/* Subpictures options */
    set_subcategory( SUBCAT_VIDEO_SUBPIC );
    set_section( N_("On Screen Display") , NULL );
    add_category_hint( N_("Subpictures"), SUB_CAT_LONGTEXT , VLC_FALSE );
    add_bool( "osd", 1, NULL, OSD_TEXT, OSD_LONGTEXT, VLC_FALSE );

    set_section( N_("Subtitles") , NULL );
    add_file( "sub-file", NULL, NULL, SUB_FILE_TEXT,
              SUB_FILE_LONGTEXT, VLC_FALSE );
    add_bool( "sub-autodetect-file", VLC_TRUE, NULL,
                 SUB_AUTO_TEXT, SUB_AUTO_LONGTEXT, VLC_FALSE );
    add_integer( "sub-autodetect-fuzzy", 3, NULL,
                 SUB_FUZZY_TEXT, SUB_FUZZY_LONGTEXT, VLC_TRUE );
#ifdef WIN32
#   define SUB_PATH ".\\subtitles"
#else
#   define SUB_PATH "./Subtitles, ./subtitles"
#endif
    add_string( "sub-autodetect-path", SUB_PATH, NULL,
                 SUB_PATH_TEXT, SUB_PATH_LONGTEXT, VLC_TRUE );
    add_integer( "sub-margin", 0, NULL, SUB_MARGIN_TEXT,
                 SUB_MARGIN_LONGTEXT, VLC_TRUE );
        add_deprecated( "spu-margin", VLC_FALSE ); /*Deprecated since 0.8.2 */
    set_section( N_( "Overlays" ) , NULL );
    add_module_list_cat( "sub-filter", SUBCAT_VIDEO_SUBPIC, NULL, NULL,
                SUB_FILTER_TEXT, SUB_FILTER_LONGTEXT, VLC_FALSE );

/* Input options */
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_GENERAL );

    set_section( N_( "Track settings" ), NULL );
    add_integer( "program", 0, NULL,
                 INPUT_PROGRAM_TEXT, INPUT_PROGRAM_LONGTEXT, VLC_TRUE );
    add_string( "programs", "", NULL,
                INPUT_PROGRAMS_TEXT, INPUT_PROGRAMS_LONGTEXT, VLC_TRUE );
    add_integer( "audio-track", -1, NULL,
                 INPUT_AUDIOTRACK_TEXT, INPUT_AUDIOTRACK_LONGTEXT, VLC_TRUE );
       add_deprecated( "audio-channel", VLC_FALSE ); /*deprecated since 0.8.2 */
    add_integer( "sub-track", -1, NULL,
                 INPUT_SUBTRACK_TEXT, INPUT_SUBTRACK_LONGTEXT, VLC_TRUE );
       add_deprecated("spu-channel",VLC_FALSE); /*deprecated since 0.8.2*/
    add_string( "audio-language", "", NULL,
                 INPUT_AUDIOTRACK_LANG_TEXT, INPUT_AUDIOTRACK_LANG_LONGTEXT,
                  VLC_FALSE );
    add_string( "sub-language", "", NULL,
                 INPUT_SUBTRACK_LANG_TEXT, INPUT_SUBTRACK_LANG_LONGTEXT,
                  VLC_FALSE );
    add_integer( "audio-track-id", -1, NULL, INPUT_AUDIOTRACK_ID_TEXT,
                 INPUT_AUDIOTRACK_ID_LONGTEXT, VLC_TRUE );
    add_integer( "sub-track-id", -1, NULL,
                 INPUT_SUBTRACK_ID_TEXT, INPUT_SUBTRACK_ID_LONGTEXT, VLC_TRUE );

    set_section( N_( "Playback control" ) , NULL);
    add_integer( "input-repeat", 0, NULL,
                 INPUT_REPEAT_TEXT, INPUT_REPEAT_LONGTEXT, VLC_FALSE );
    add_integer( "start-time", 0, NULL,
                 START_TIME_TEXT, START_TIME_LONGTEXT, VLC_TRUE );
    add_integer( "stop-time", 0, NULL,
                 STOP_TIME_TEXT, STOP_TIME_LONGTEXT, VLC_TRUE );
    add_string( "input-list", NULL, NULL,
                 INPUT_LIST_TEXT, INPUT_LIST_LONGTEXT, VLC_TRUE );
    add_string( "input-slave", NULL, NULL,
                 INPUT_SLAVE_TEXT, INPUT_SLAVE_LONGTEXT, VLC_TRUE );

    add_string( "bookmarks", NULL, NULL,
                 BOOKMARKS_TEXT, BOOKMARKS_LONGTEXT, VLC_TRUE );

    set_section( N_( "Default devices") , NULL )

    add_file( "dvd", NULL, NULL, DVD_DEV_TEXT, DVD_DEV_LONGTEXT,
              VLC_FALSE );
    add_file( "vcd", VCD_DEVICE, NULL, VCD_DEV_TEXT, VCD_DEV_LONGTEXT,
              VLC_FALSE );
    add_file( "cd-audio", CDAUDIO_DEVICE, NULL, CDAUDIO_DEV_TEXT,
              CDAUDIO_DEV_LONGTEXT, VLC_FALSE );

    set_section( N_( "Network settings" ), NULL );

    add_integer( "server-port", 1234, NULL,
                 SERVER_PORT_TEXT, SERVER_PORT_LONGTEXT, VLC_FALSE );
    add_integer( "mtu", 1500, NULL, MTU_TEXT, MTU_LONGTEXT, VLC_TRUE );
    add_bool( "ipv6", 0, NULL, IPV6_TEXT, IPV6_LONGTEXT, VLC_FALSE );
        change_short('6');
    add_bool( "ipv4", 0, NULL, IPV4_TEXT, IPV4_LONGTEXT, VLC_FALSE );
        change_short('4');
    add_integer( "ipv4-timeout", 5 * 1000, NULL, TIMEOUT_TEXT,
                 TIMEOUT_LONGTEXT, VLC_TRUE );

    set_section( N_( "Socks proxy") , NULL )
    add_string( "socks", NULL, NULL,
                 SOCKS_SERVER_TEXT, SOCKS_SERVER_LONGTEXT, VLC_TRUE );
    add_string( "socks-user", NULL, NULL,
                 SOCKS_USER_TEXT, SOCKS_USER_LONGTEXT, VLC_TRUE );
    add_string( "socks-pwd", NULL, NULL,
                 SOCKS_PASS_TEXT, SOCKS_PASS_LONGTEXT, VLC_TRUE );


    set_section( N_("Metadata" ) , NULL )
    add_string( "meta-title", NULL, NULL, META_TITLE_TEXT,
                META_TITLE_LONGTEXT, VLC_TRUE );
    add_string( "meta-author", NULL, NULL, META_AUTHOR_TEXT,
                META_AUTHOR_LONGTEXT, VLC_TRUE );
    add_string( "meta-artist", NULL, NULL, META_ARTIST_TEXT,
                META_ARTIST_LONGTEXT, VLC_TRUE );
    add_string( "meta-genre", NULL, NULL, META_GENRE_TEXT,
                META_GENRE_LONGTEXT, VLC_TRUE );
    add_string( "meta-copyright", NULL, NULL, META_CPYR_TEXT,
                META_CPYR_LONGTEXT, VLC_TRUE );
    add_string( "meta-description", NULL, NULL, META_DESCR_TEXT,
                META_DESCR_LONGTEXT, VLC_TRUE );
    add_string( "meta-date", NULL, NULL, META_DATE_TEXT,
                META_DATE_LONGTEXT, VLC_TRUE );
    add_string( "meta-url", NULL, NULL, META_URL_TEXT,
                META_URL_LONGTEXT, VLC_TRUE );

    set_section( N_( "Advanced" ), NULL );

    add_integer( "cr-average", 40, NULL, CR_AVERAGE_TEXT,
                 CR_AVERAGE_LONGTEXT, VLC_TRUE );
    add_integer( "clock-synchro", -1, NULL, CLOCK_SYNCHRO_TEXT,
                 CLOCK_SYNCHRO_LONGTEXT, VLC_TRUE );
        change_integer_list( pi_clock_values, ppsz_clock_descriptions, 0 );

/* Decoder options */
    add_category_hint( N_("Decoders"), CODEC_CAT_LONGTEXT , VLC_TRUE );
    add_string( "codec", NULL, NULL, CODEC_TEXT,
                CODEC_LONGTEXT, VLC_TRUE );
    add_string( "encoder",  NULL, NULL, ENCODER_TEXT,
                ENCODER_LONGTEXT, VLC_TRUE );

    set_subcategory( SUBCAT_INPUT_ACCESS );
    add_category_hint( N_("Input"), INPUT_CAT_LONGTEXT , VLC_FALSE );
    add_module( "access", "access2", NULL, NULL, ACCESS_TEXT,
                ACCESS_LONGTEXT, VLC_TRUE );

    set_subcategory( SUBCAT_INPUT_ACCESS_FILTER );
    add_module_list_cat( "access-filter", SUBCAT_INPUT_ACCESS_FILTER, NULL, NULL,
                ACCESS_FILTER_TEXT, ACCESS_FILTER_LONGTEXT, VLC_FALSE );


    set_subcategory( SUBCAT_INPUT_DEMUX );
    add_module( "demux", "demux2", NULL, NULL, DEMUX_TEXT,
                DEMUX_LONGTEXT, VLC_TRUE );
    set_subcategory( SUBCAT_INPUT_VCODEC );
    set_subcategory( SUBCAT_INPUT_ACODEC );
    set_subcategory( SUBCAT_INPUT_SCODEC );


/* Stream output options */
    set_category( CAT_SOUT );
    set_subcategory( SUBCAT_SOUT_GENERAL );
    add_category_hint( N_("Stream output"), SOUT_CAT_LONGTEXT , VLC_TRUE );

    add_string( "sout", NULL, NULL, SOUT_TEXT, SOUT_LONGTEXT, VLC_TRUE );
    add_bool( "sout-display", VLC_FALSE, NULL, SOUT_DISPLAY_TEXT,
                                SOUT_DISPLAY_LONGTEXT, VLC_TRUE );
    add_bool( "sout-keep", VLC_FALSE, NULL, SOUT_KEEP_TEXT,
                                SOUT_KEEP_LONGTEXT, VLC_TRUE );
    add_bool( "sout-all", 0, NULL, SOUT_ALL_TEXT,
                                SOUT_ALL_LONGTEXT, VLC_TRUE );
    add_bool( "sout-audio", 1, NULL, SOUT_AUDIO_TEXT,
                                SOUT_AUDIO_LONGTEXT, VLC_TRUE );
    add_bool( "sout-video", 1, NULL, SOUT_VIDEO_TEXT,
                                SOUT_VIDEO_LONGTEXT, VLC_TRUE );

    set_subcategory( SUBCAT_SOUT_STREAM );
    set_subcategory( SUBCAT_SOUT_MUX );
    add_module( "mux", "sout mux", NULL, NULL, MUX_TEXT,
                                MUX_LONGTEXT, VLC_TRUE );
    set_subcategory( SUBCAT_SOUT_ACO );
    add_module( "access_output", "sout access", NULL, NULL,
                ACCESS_OUTPUT_TEXT, ACCESS_OUTPUT_LONGTEXT, VLC_TRUE );
    add_integer( "ttl", 1, NULL, TTL_TEXT, TTL_LONGTEXT, VLC_TRUE );
    add_string( "miface-addr", NULL, NULL, MIFACE_TEXT, MIFACE_LONGTEXT, VLC_TRUE );

    set_subcategory( SUBCAT_SOUT_PACKETIZER );
    add_module( "packetizer","packetizer", NULL, NULL,
                PACKETIZER_TEXT, PACKETIZER_LONGTEXT, VLC_TRUE );

    set_subcategory( SUBCAT_SOUT_SAP );
    add_bool( "sap-flow-control", VLC_FALSE, NULL, ANN_SAPCTRL_TEXT,
                               ANN_SAPCTRL_LONGTEXT, VLC_TRUE );
    add_integer( "sap-interval", 5, NULL, ANN_SAPINTV_TEXT,
                               ANN_SAPINTV_LONGTEXT, VLC_TRUE );
    set_subcategory( SUBCAT_SOUT_VOD );

/* CPU options */
    set_category( CAT_ADVANCED );
    set_subcategory( SUBCAT_ADVANCED_CPU );
    add_category_hint( N_("CPU"), CPU_CAT_LONGTEXT, VLC_TRUE );
    add_bool( "fpu", 1, NULL, FPU_TEXT, FPU_LONGTEXT, VLC_TRUE );
#if defined( __i386__ ) || defined( __x86_64__ )
    add_bool( "mmx", 1, NULL, MMX_TEXT, MMX_LONGTEXT, VLC_TRUE );
    add_bool( "3dn", 1, NULL, THREE_DN_TEXT, THREE_DN_LONGTEXT, VLC_TRUE );
    add_bool( "mmxext", 1, NULL, MMXEXT_TEXT, MMXEXT_LONGTEXT, VLC_TRUE );
    add_bool( "sse", 1, NULL, SSE_TEXT, SSE_LONGTEXT, VLC_TRUE );
    add_bool( "sse2", 1, NULL, SSE2_TEXT, SSE2_LONGTEXT, VLC_TRUE );
#endif
#if defined( __powerpc__ ) || defined( SYS_DARWIN )
    add_bool( "altivec", 1, NULL, ALTIVEC_TEXT, ALTIVEC_LONGTEXT, VLC_TRUE );
#endif

/* Misc options */
    set_subcategory( SUBCAT_ADVANCED_MISC );
    set_section( N_("Special modules"), NULL );
    add_category_hint( N_("Miscellaneous"), MISC_CAT_LONGTEXT, VLC_TRUE );
    add_module( "memcpy", "memcpy", NULL, NULL, MEMCPY_TEXT,
                MEMCPY_LONGTEXT, VLC_TRUE );
        change_short('A');

    set_section( N_("Plugins" ), NULL );
    add_bool( "plugins-cache", VLC_TRUE, NULL, PLUGINS_CACHE_TEXT,
              PLUGINS_CACHE_LONGTEXT, VLC_TRUE );
    add_directory( "plugin-path", NULL, NULL, PLUGIN_PATH_TEXT,
                   PLUGIN_PATH_LONGTEXT, VLC_TRUE );

    set_section( N_("Performance options"), NULL );
    add_bool( "minimize-threads", 0, NULL, MINIMIZE_THREADS_TEXT,
              MINIMIZE_THREADS_LONGTEXT, VLC_TRUE );

#if !defined(SYS_DARWIN) && !defined(SYS_BEOS) && defined(PTHREAD_COND_T_IN_PTHREAD_H)
    add_bool( "rt-priority", VLC_FALSE, NULL, RT_PRIORITY_TEXT,
              RT_PRIORITY_LONGTEXT, VLC_TRUE );
#endif

#if !defined(SYS_BEOS) && defined(PTHREAD_COND_T_IN_PTHREAD_H)
    add_integer( "rt-offset", 0, NULL, RT_OFFSET_TEXT,
                 RT_OFFSET_LONGTEXT, VLC_TRUE );
#endif

#if defined(WIN32)
    add_bool( "one-instance", 0, NULL, ONEINSTANCE_TEXT,
              ONEINSTANCE_LONGTEXT, VLC_TRUE );
    add_bool( "playlist-enqueue", 0, NULL, PLAYLISTENQUEUE_TEXT,
              PLAYLISTENQUEUE_LONGTEXT, VLC_TRUE );
    add_bool( "high-priority", 0, NULL, HPRIORITY_TEXT,
              HPRIORITY_LONGTEXT, VLC_FALSE );
    add_bool( "fast-mutex", 0, NULL, FAST_MUTEX_TEXT,
              FAST_MUTEX_LONGTEXT, VLC_TRUE );
    add_integer( "win9x-cv-method", 1, NULL, WIN9X_CV_TEXT,
                  WIN9X_CV_LONGTEXT, VLC_TRUE );
#endif

    set_section( N_("Miscellaneous" ), NULL );
    add_string( "vlm-conf", NULL, NULL, VLM_CONF_TEXT,
                    VLM_CONF_LONGTEXT, VLC_TRUE );

#if !defined(WIN32)
    add_bool( "daemon", 0, NULL, DAEMON_TEXT, DAEMON_LONGTEXT, VLC_TRUE );
        change_short('d');
#endif

/* Playlist options */
    set_category( CAT_PLAYLIST );
    set_subcategory( SUBCAT_PLAYLIST_GENERAL );
    add_category_hint( N_("Playlist"), PLAYLIST_CAT_LONGTEXT , VLC_FALSE );
    add_bool( "random", 0, NULL, RANDOM_TEXT, RANDOM_LONGTEXT, VLC_FALSE );
        change_short('Z');
    add_bool( "loop", 0, NULL, LOOP_TEXT, LOOP_LONGTEXT, VLC_FALSE );
        change_short('L');
    add_bool( "repeat", 0, NULL, REPEAT_TEXT, REPEAT_LONGTEXT, VLC_FALSE );
        change_short('R');
    add_bool( "play-and-stop", 0, NULL, PAS_TEXT, PAS_LONGTEXT, VLC_FALSE );

    add_string( "open", "", NULL, OPEN_TEXT, OPEN_LONGTEXT, VLC_FALSE );

    set_subcategory( SUBCAT_PLAYLIST_SD );
    add_module_list_cat( "services-discovery", SUBCAT_PLAYLIST_SD, NULL,
                          NULL, SD_TEXT, SD_LONGTEXT, VLC_FALSE );
        change_short('S');

/* Interface options */
    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_GENERAL );
    add_category_hint( N_("Interface"), INTF_CAT_LONGTEXT , VLC_FALSE );
    set_section ( N_("Interface module" ), NULL );
    add_module_cat( "intf", SUBCAT_INTERFACE_GENERAL, NULL, NULL, INTF_TEXT,
                INTF_LONGTEXT, VLC_FALSE );
        change_short('I');

    set_section ( N_("Extra interface modules" ),  NULL );
    add_module_list_cat( "extraintf", SUBCAT_INTERFACE_GENERAL,
                         NULL, NULL, EXTRAINTF_TEXT,
                         EXTRAINTF_LONGTEXT, VLC_FALSE );

    set_section ( N_("Miscellaneous"), NULL );
    add_integer( "verbose", 0, NULL, VERBOSE_TEXT, VERBOSE_LONGTEXT,
                 VLC_FALSE );
        change_short('v');
    add_bool( "quiet", 0, NULL, QUIET_TEXT, QUIET_LONGTEXT, VLC_TRUE );
        change_short('q');
    add_string( "language", "auto", NULL, LANGUAGE_TEXT, LANGUAGE_LONGTEXT,
                VLC_FALSE );
        change_string_list( ppsz_language, ppsz_language_text, 0 );
    add_bool( "color", 0, NULL, COLOR_TEXT, COLOR_LONGTEXT, VLC_TRUE );
    add_bool( "advanced", 0, NULL, ADVANCED_TEXT, ADVANCED_LONGTEXT,
                    VLC_FALSE );

    set_subcategory( SUBCAT_INTERFACE_CONTROL );
    add_module_list_cat( "control", SUBCAT_INTERFACE_CONTROL, NULL, NULL,
                         CONTROL_TEXT, CONTROL_LONGTEXT, VLC_FALSE );

/* Hotkey options*/
    set_subcategory( SUBCAT_INTERFACE_HOTKEYS );
    add_category_hint( N_("Hot keys"), HOTKEY_CAT_LONGTEXT , VLC_FALSE );

#if defined(SYS_DARWIN)
/* Don't use the following combo's */

/*  copy                          KEY_MODIFIER_COMMAND|'c'
 *  cut                           KEY_MODIFIER_COMMAND|'x'
 *  paste                         KEY_MODIFIER_COMMAND|'v'
 *  select all                    KEY_MODIFIER_COMMAND|'a'
 *  preferences                   KEY_MODIFIER_COMMAND|','
 *  hide vlc                      KEY_MODIFIER_COMMAND|'h'
 *  hide other                    KEY_MODIFIER_COMMAND|KEY_MODIFIER_ALT|'h'
 *  open file                     KEY_MODIFIER_COMMAND|KEY_MODIFIER_SHIFT|'o'
 *  open                          KEY_MODIFIER_COMMAND|'o'
 *  open disk                     KEY_MODIFIER_COMMAND|'d'
 *  open network                  KEY_MODIFIER_COMMAND|'n'
 *  save playlist                 KEY_MODIFIER_COMMAND|'s'
 *  playlist random               KEY_MODIFIER_COMMAND|'z'
 *  playlist repeat all           KEY_MODIFIER_COMMAND|'l'
 *  playlist repeat               KEY_MODIFIER_COMMAND|'r'
 *  video half size               KEY_MODIFIER_COMMAND|'0'
 *  video normal size             KEY_MODIFIER_COMMAND|'1'
 *  video double size             KEY_MODIFIER_COMMAND|'2'
 *  video fit to screen           KEY_MODIFIER_COMMAND|'3'
 *  minimize window               KEY_MODIFIER_COMMAND|'m'
 *  close window                  KEY_MODIFIER_COMMAND|'w'
 *  show controller               KEY_MODIFIER_COMMAND|KEY_MODIFIER_SHIFT|'c'
 *  show playlist                 KEY_MODIFIER_COMMAND|KEY_MODIFIER_SHIFT|'p'
 *  show info                     KEY_MODIFIER_COMMAND|'i'
 *  help                          KEY_MODIFIER_COMMAND|'?'
 */
#   define KEY_FULLSCREEN         KEY_MODIFIER_COMMAND|'f'
#   define KEY_PLAY_PAUSE         KEY_MODIFIER_COMMAND|'p'
#   define KEY_PAUSE              KEY_UNSET
#   define KEY_PLAY               KEY_UNSET
#   define KEY_FASTER             KEY_MODIFIER_COMMAND|'='
#   define KEY_SLOWER             KEY_MODIFIER_COMMAND|'-'
#   define KEY_NEXT               KEY_MODIFIER_COMMAND|KEY_RIGHT
#   define KEY_PREV               KEY_MODIFIER_COMMAND|KEY_LEFT
#   define KEY_STOP               KEY_MODIFIER_COMMAND|'.'
#   define KEY_POSITION           't'
#   define KEY_JUMP_MEXTRASHORT   KEY_MODIFIER_COMMAND|KEY_MODIFIER_CTRL|KEY_LEFT
#   define KEY_JUMP_PEXTRASHORT   KEY_MODIFIER_COMMAND|KEY_MODIFIER_CTRL|KEY_RIGHT
#   define KEY_JUMP_MSHORT        KEY_MODIFIER_COMMAND|KEY_MODIFIER_ALT|KEY_LEFT
#   define KEY_JUMP_PSHORT        KEY_MODIFIER_COMMAND|KEY_MODIFIER_ALT|KEY_RIGHT
#   define KEY_JUMP_MMEDIUM       KEY_MODIFIER_COMMAND|KEY_MODIFIER_SHIFT|KEY_LEFT
#   define KEY_JUMP_PMEDIUM       KEY_MODIFIER_COMMAND|KEY_MODIFIER_SHIFT|KEY_RIGHT
#   define KEY_JUMP_MLONG         KEY_MODIFIER_COMMAND|KEY_MODIFIER_SHIFT|KEY_MODIFIER_ALT|KEY_LEFT
#   define KEY_JUMP_PLONG         KEY_MODIFIER_COMMAND|KEY_MODIFIER_SHIFT|KEY_MODIFIER_ALT|KEY_RIGHT
#   define KEY_NAV_ACTIVATE       KEY_ENTER
#   define KEY_NAV_UP             KEY_UP
#   define KEY_NAV_DOWN           KEY_DOWN
#   define KEY_NAV_LEFT           KEY_LEFT
#   define KEY_NAV_RIGHT          KEY_RIGHT
#   define KEY_QUIT               KEY_MODIFIER_COMMAND|'q'
#   define KEY_VOL_UP             KEY_MODIFIER_COMMAND|KEY_UP
#   define KEY_VOL_DOWN           KEY_MODIFIER_COMMAND|KEY_DOWN
#   define KEY_VOL_MUTE           KEY_MODIFIER_COMMAND|KEY_MODIFIER_ALT|KEY_DOWN
#   define KEY_SUBDELAY_UP        'j'
#   define KEY_SUBDELAY_DOWN      'h'
#   define KEY_AUDIODELAY_UP      'g'
#   define KEY_AUDIODELAY_DOWN    'f'
#   define KEY_AUDIO_TRACK        'l'
#   define KEY_SUBTITLE_TRACK     's'
#   define KEY_ASPECT_RATIO       'a'
#   define KEY_CROP               'c'
#   define KEY_DEINTERLACE        'd'
#   define KEY_INTF_SHOW          'i'
#   define KEY_INTF_HIDE          'I'
#   define KEY_DISC_MENU          KEY_MODIFIER_CTRL|'m'
#   define KEY_TITLE_PREV         KEY_MODIFIER_CTRL|'p'
#   define KEY_TITLE_NEXT         KEY_MODIFIER_CTRL|'n'
#   define KEY_CHAPTER_PREV       KEY_MODIFIER_CTRL|'u'
#   define KEY_CHAPTER_NEXT       KEY_MODIFIER_CTRL|'d'
#   define KEY_SNAPSHOT           KEY_MODIFIER_COMMAND|KEY_MODIFIER_ALT|'s'

#   define KEY_SET_BOOKMARK1      KEY_MODIFIER_COMMAND|KEY_F1
#   define KEY_SET_BOOKMARK2      KEY_MODIFIER_COMMAND|KEY_F2
#   define KEY_SET_BOOKMARK3      KEY_MODIFIER_COMMAND|KEY_F3
#   define KEY_SET_BOOKMARK4      KEY_MODIFIER_COMMAND|KEY_F4
#   define KEY_SET_BOOKMARK5      KEY_MODIFIER_COMMAND|KEY_F5
#   define KEY_SET_BOOKMARK6      KEY_MODIFIER_COMMAND|KEY_F6
#   define KEY_SET_BOOKMARK7      KEY_MODIFIER_COMMAND|KEY_F7
#   define KEY_SET_BOOKMARK8      KEY_MODIFIER_COMMAND|KEY_F8
#   define KEY_SET_BOOKMARK9      KEY_UNSET
#   define KEY_SET_BOOKMARK10     KEY_UNSET
#   define KEY_PLAY_BOOKMARK1     KEY_F1
#   define KEY_PLAY_BOOKMARK2     KEY_F2
#   define KEY_PLAY_BOOKMARK3     KEY_F3
#   define KEY_PLAY_BOOKMARK4     KEY_F4
#   define KEY_PLAY_BOOKMARK5     KEY_F5
#   define KEY_PLAY_BOOKMARK6     KEY_F6
#   define KEY_PLAY_BOOKMARK7     KEY_F7
#   define KEY_PLAY_BOOKMARK8     KEY_F8
#   define KEY_PLAY_BOOKMARK9     KEY_UNSET
#   define KEY_PLAY_BOOKMARK10    KEY_UNSET
#   define KEY_HISTORY_BACK       KEY_MODIFIER_COMMAND|'['
#   define KEY_HISTORY_FORWARD    KEY_MODIFIER_COMMAND|']'
#   define KEY_RECORD             KEY_MODIFIER_COMMAND|KEY_MODIFIER_SHIFT|'r'

#else
#   define KEY_FULLSCREEN         'f'
#   define KEY_PLAY_PAUSE         KEY_SPACE
#   define KEY_PAUSE              KEY_UNSET
#   define KEY_PLAY               KEY_UNSET
#   define KEY_FASTER             '+'
#   define KEY_SLOWER             '-'
#   define KEY_NEXT               'n'
#   define KEY_PREV               'p'
#   define KEY_STOP               's'
#   define KEY_POSITION           't'
#   define KEY_JUMP_MEXTRASHORT   KEY_MODIFIER_SHIFT|KEY_LEFT
#   define KEY_JUMP_PEXTRASHORT   KEY_MODIFIER_SHIFT|KEY_RIGHT
#   define KEY_JUMP_MSHORT        KEY_MODIFIER_ALT|KEY_LEFT
#   define KEY_JUMP_PSHORT        KEY_MODIFIER_ALT|KEY_RIGHT
#   define KEY_JUMP_MMEDIUM       KEY_MODIFIER_CTRL|KEY_LEFT
#   define KEY_JUMP_PMEDIUM       KEY_MODIFIER_CTRL|KEY_RIGHT
#   define KEY_JUMP_MLONG         KEY_MODIFIER_CTRL|KEY_MODIFIER_ALT|KEY_LEFT
#   define KEY_JUMP_PLONG         KEY_MODIFIER_CTRL|KEY_MODIFIER_ALT|KEY_RIGHT
#   define KEY_NAV_ACTIVATE       KEY_ENTER
#   define KEY_NAV_UP             KEY_UP
#   define KEY_NAV_DOWN           KEY_DOWN
#   define KEY_NAV_LEFT           KEY_LEFT
#   define KEY_NAV_RIGHT          KEY_RIGHT
#   define KEY_QUIT               KEY_MODIFIER_CTRL|'q'
#   define KEY_VOL_UP             KEY_MODIFIER_CTRL|KEY_UP
#   define KEY_VOL_DOWN           KEY_MODIFIER_CTRL|KEY_DOWN
#   define KEY_VOL_MUTE           'm'
#   define KEY_SUBDELAY_UP        KEY_MODIFIER_CTRL|'h'
#   define KEY_SUBDELAY_DOWN      KEY_MODIFIER_CTRL|'j'
#   define KEY_AUDIODELAY_UP      KEY_MODIFIER_CTRL|'k'
#   define KEY_AUDIODELAY_DOWN    KEY_MODIFIER_CTRL|'l'

#   define KEY_AUDIO_TRACK        'l'
#   define KEY_SUBTITLE_TRACK     'k'
#   define KEY_ASPECT_RATIO       'a'
#   define KEY_CROP               'c'
#   define KEY_DEINTERLACE        'd'
#   define KEY_INTF_SHOW          'i'
#   define KEY_INTF_HIDE          'I'
#   define KEY_DISC_MENU          KEY_MODIFIER_CTRL|'m'
#   define KEY_TITLE_PREV         KEY_MODIFIER_CTRL|'p'
#   define KEY_TITLE_NEXT         KEY_MODIFIER_CTRL|'n'
#   define KEY_CHAPTER_PREV       KEY_MODIFIER_CTRL|'u'
#   define KEY_CHAPTER_NEXT       KEY_MODIFIER_CTRL|'d'
#   define KEY_SNAPSHOT           KEY_MODIFIER_CTRL|KEY_MODIFIER_ALT|'s'

#   define KEY_SET_BOOKMARK1      KEY_MODIFIER_CTRL|KEY_F1
#   define KEY_SET_BOOKMARK2      KEY_MODIFIER_CTRL|KEY_F2
#   define KEY_SET_BOOKMARK3      KEY_MODIFIER_CTRL|KEY_F3
#   define KEY_SET_BOOKMARK4      KEY_MODIFIER_CTRL|KEY_F4
#   define KEY_SET_BOOKMARK5      KEY_MODIFIER_CTRL|KEY_F5
#   define KEY_SET_BOOKMARK6      KEY_MODIFIER_CTRL|KEY_F6
#   define KEY_SET_BOOKMARK7      KEY_MODIFIER_CTRL|KEY_F7
#   define KEY_SET_BOOKMARK8      KEY_MODIFIER_CTRL|KEY_F8
#   define KEY_SET_BOOKMARK9      KEY_MODIFIER_CTRL|KEY_F9
#   define KEY_SET_BOOKMARK10     KEY_MODIFIER_CTRL|KEY_F10
#   define KEY_PLAY_BOOKMARK1     KEY_F1
#   define KEY_PLAY_BOOKMARK2     KEY_F2
#   define KEY_PLAY_BOOKMARK3     KEY_F3
#   define KEY_PLAY_BOOKMARK4     KEY_F4
#   define KEY_PLAY_BOOKMARK5     KEY_F5
#   define KEY_PLAY_BOOKMARK6     KEY_F6
#   define KEY_PLAY_BOOKMARK7     KEY_F7
#   define KEY_PLAY_BOOKMARK8     KEY_F8
#   define KEY_PLAY_BOOKMARK9     KEY_F9
#   define KEY_PLAY_BOOKMARK10    KEY_F10
#   define KEY_HISTORY_BACK       KEY_MODIFIER_CTRL|'v'
#   define KEY_HISTORY_FORWARD    KEY_MODIFIER_CTRL|'b'
#   define KEY_RECORD             KEY_MODIFIER_CTRL|'r'
#endif

    add_key( "key-fullscreen", KEY_FULLSCREEN, NULL, FULLSCREEN_KEY_TEXT,
             FULLSCREEN_KEY_LONGTEXT, VLC_FALSE );
    add_key( "key-play-pause", KEY_PLAY_PAUSE, NULL, PLAY_PAUSE_KEY_TEXT,
             PLAY_PAUSE_KEY_LONGTEXT, VLC_FALSE );
    add_key( "key-pause", KEY_PAUSE, NULL, PAUSE_KEY_TEXT,
             PAUSE_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-play", KEY_PLAY, NULL, PLAY_KEY_TEXT,
             PLAY_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-faster", KEY_FASTER, NULL, FASTER_KEY_TEXT,
             FASTER_KEY_LONGTEXT, VLC_FALSE );
    add_key( "key-slower", KEY_SLOWER, NULL, SLOWER_KEY_TEXT,
             SLOWER_KEY_LONGTEXT, VLC_FALSE );
    add_key( "key-next", KEY_NEXT, NULL, NEXT_KEY_TEXT,
             NEXT_KEY_LONGTEXT, VLC_FALSE );
    add_key( "key-prev", KEY_PREV, NULL, PREV_KEY_TEXT,
             PREV_KEY_LONGTEXT, VLC_FALSE );
    add_key( "key-stop", KEY_STOP, NULL, STOP_KEY_TEXT,
             STOP_KEY_LONGTEXT, VLC_FALSE );
    add_key( "key-position", KEY_POSITION, NULL, POSITION_KEY_TEXT,
             POSITION_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-jump-extrashort", KEY_JUMP_MEXTRASHORT, NULL,
             JBEXTRASHORT_KEY_TEXT, JBEXTRASHORT_KEY_LONGTEXT, VLC_FALSE );
    add_key( "key-jump+extrashort", KEY_JUMP_PEXTRASHORT, NULL,
             JFEXTRASHORT_KEY_TEXT, JFEXTRASHORT_KEY_LONGTEXT, VLC_FALSE );
    add_key( "key-jump-short", KEY_JUMP_MSHORT, NULL, JBSHORT_KEY_TEXT,
             JBSHORT_KEY_LONGTEXT, VLC_FALSE );
    add_key( "key-jump+short", KEY_JUMP_PSHORT, NULL, JFSHORT_KEY_TEXT,
             JFSHORT_KEY_LONGTEXT, VLC_FALSE );
    add_key( "key-jump-medium", KEY_JUMP_MMEDIUM, NULL, JBMEDIUM_KEY_TEXT,
             JBMEDIUM_KEY_LONGTEXT, VLC_FALSE );
    add_key( "key-jump+medium", KEY_JUMP_PMEDIUM, NULL, JFMEDIUM_KEY_TEXT,
             JFMEDIUM_KEY_LONGTEXT, VLC_FALSE );
    add_key( "key-jump-long", KEY_JUMP_MLONG, NULL, JBLONG_KEY_TEXT,
             JBLONG_KEY_LONGTEXT, VLC_FALSE );
    add_key( "key-jump+long", KEY_JUMP_PLONG, NULL, JFLONG_KEY_TEXT,
             JFLONG_KEY_LONGTEXT, VLC_FALSE );
    add_key( "key-nav-activate", KEY_NAV_ACTIVATE, NULL, NAV_ACTIVATE_KEY_TEXT,
             NAV_ACTIVATE_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-nav-up", KEY_NAV_UP, NULL, NAV_UP_KEY_TEXT,
             NAV_UP_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-nav-down", KEY_NAV_DOWN, NULL, NAV_DOWN_KEY_TEXT,
             NAV_DOWN_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-nav-left", KEY_NAV_LEFT, NULL, NAV_LEFT_KEY_TEXT,
             NAV_LEFT_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-nav-right", KEY_NAV_RIGHT, NULL, NAV_RIGHT_KEY_TEXT,
             NAV_RIGHT_KEY_LONGTEXT, VLC_TRUE );

    add_key( "key-disc-menu", KEY_DISC_MENU, NULL, DISC_MENU_TEXT,
             DISC_MENU_LONGTEXT, VLC_TRUE );
    add_key( "key-title-prev", KEY_TITLE_PREV, NULL, TITLE_PREV_TEXT,
             TITLE_PREV_LONGTEXT, VLC_TRUE );
    add_key( "key-title-next", KEY_TITLE_NEXT, NULL, TITLE_NEXT_TEXT,
             TITLE_NEXT_LONGTEXT, VLC_TRUE );
    add_key( "key-chapter-prev", KEY_CHAPTER_PREV, NULL, CHAPTER_PREV_TEXT,
             CHAPTER_PREV_LONGTEXT, VLC_TRUE );
    add_key( "key-chapter-next", KEY_CHAPTER_NEXT, NULL, CHAPTER_NEXT_TEXT,
             CHAPTER_NEXT_LONGTEXT, VLC_TRUE );
    add_key( "key-quit", KEY_QUIT, NULL, QUIT_KEY_TEXT,
             QUIT_KEY_LONGTEXT, VLC_FALSE );
    add_key( "key-vol-up", KEY_VOL_UP, NULL, VOL_UP_KEY_TEXT,
             VOL_UP_KEY_LONGTEXT, VLC_FALSE );
    add_key( "key-vol-down", KEY_VOL_DOWN, NULL, VOL_DOWN_KEY_TEXT,
             VOL_DOWN_KEY_LONGTEXT, VLC_FALSE );
    add_key( "key-vol-mute", KEY_VOL_MUTE, NULL, VOL_MUTE_KEY_TEXT,
             VOL_MUTE_KEY_LONGTEXT, VLC_FALSE );
    add_key( "key-subdelay-up", KEY_SUBDELAY_UP, NULL,
             SUBDELAY_UP_KEY_TEXT, SUBDELAY_UP_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-subdelay-down", KEY_SUBDELAY_DOWN, NULL,
             SUBDELAY_DOWN_KEY_TEXT, SUBDELAY_DOWN_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-audiodelay-up", KEY_AUDIODELAY_UP, NULL,
             AUDIODELAY_UP_KEY_TEXT, AUDIODELAY_UP_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-audiodelay-down", KEY_AUDIODELAY_DOWN, NULL,
             AUDIODELAY_DOWN_KEY_TEXT, AUDIODELAY_DOWN_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-audio-track", KEY_AUDIO_TRACK, NULL, AUDIO_TRACK_KEY_TEXT,
             AUDIO_TRACK_KEY_LONGTEXT, VLC_FALSE );
    add_key( "key-subtitle-track", KEY_SUBTITLE_TRACK, NULL,
             SUBTITLE_TRACK_KEY_TEXT, SUBTITLE_TRACK_KEY_LONGTEXT, VLC_FALSE );
    add_key( "key-aspect-ratio", KEY_ASPECT_RATIO, NULL,
             ASPECT_RATIO_KEY_TEXT, ASPECT_RATIO_KEY_LONGTEXT, VLC_FALSE );
    add_key( "key-crop", KEY_CROP, NULL,
             CROP_KEY_TEXT, CROP_KEY_LONGTEXT, VLC_FALSE );
    add_key( "key-deinterlace", KEY_DEINTERLACE, NULL,
             DEINTERLACE_KEY_TEXT, DEINTERLACE_KEY_LONGTEXT, VLC_FALSE );
    add_key( "key-intf-show", KEY_INTF_SHOW, NULL,
             INTF_SHOW_KEY_TEXT, INTF_SHOW_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-intf-hide", KEY_INTF_HIDE, NULL,
             INTF_HIDE_KEY_TEXT, INTF_HIDE_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-snapshot", KEY_SNAPSHOT, NULL,
        SNAP_KEY_TEXT, SNAP_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-history-back", KEY_HISTORY_BACK, NULL, HISTORY_BACK_TEXT,
             HISTORY_BACK_LONGTEXT, VLC_TRUE );
    add_key( "key-history-forward", KEY_HISTORY_FORWARD, NULL,
             HISTORY_FORWARD_TEXT, HISTORY_FORWARD_LONGTEXT, VLC_TRUE );
    add_key( "key-record", KEY_RECORD, NULL,
             RECORD_KEY_TEXT, RECORD_KEY_LONGTEXT, VLC_TRUE );
    add_integer( "extrashort-jump-size", 3, NULL, JIEXTRASHORT_TEXT,
                                    JIEXTRASHORT_LONGTEXT, VLC_FALSE );
    add_integer( "short-jump-size", 10, NULL, JISHORT_TEXT,
                                    JISHORT_LONGTEXT, VLC_FALSE );
    add_integer( "medium-jump-size", 60, NULL, JIMEDIUM_TEXT,
                                    JIMEDIUM_LONGTEXT, VLC_FALSE );
    add_integer( "long-jump-size", 300, NULL, JILONG_TEXT,
                                    JILONG_LONGTEXT, VLC_FALSE );

    /* HACK so these don't get displayed */
    set_category( -1 );
    set_subcategory( -1 );
    add_key( "key-set-bookmark1", KEY_SET_BOOKMARK1, NULL,
             SET_BOOKMARK1_KEY_TEXT, SET_BOOKMARK_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-set-bookmark2", KEY_SET_BOOKMARK2, NULL,
             SET_BOOKMARK2_KEY_TEXT, SET_BOOKMARK_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-set-bookmark3", KEY_SET_BOOKMARK3, NULL,
             SET_BOOKMARK3_KEY_TEXT, SET_BOOKMARK_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-set-bookmark4", KEY_SET_BOOKMARK4, NULL,
             SET_BOOKMARK4_KEY_TEXT, SET_BOOKMARK_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-set-bookmark5", KEY_SET_BOOKMARK5, NULL,
             SET_BOOKMARK5_KEY_TEXT, SET_BOOKMARK_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-set-bookmark6", KEY_SET_BOOKMARK6, NULL,
             SET_BOOKMARK6_KEY_TEXT, SET_BOOKMARK_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-set-bookmark7", KEY_SET_BOOKMARK7, NULL,
             SET_BOOKMARK7_KEY_TEXT, SET_BOOKMARK_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-set-bookmark8", KEY_SET_BOOKMARK8, NULL,
             SET_BOOKMARK8_KEY_TEXT, SET_BOOKMARK_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-set-bookmark9", KEY_SET_BOOKMARK9, NULL,
             SET_BOOKMARK9_KEY_TEXT, SET_BOOKMARK_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-set-bookmark10", KEY_SET_BOOKMARK10, NULL,
             SET_BOOKMARK10_KEY_TEXT, SET_BOOKMARK_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-play-bookmark1", KEY_PLAY_BOOKMARK1, NULL,
             PLAY_BOOKMARK1_KEY_TEXT, PLAY_BOOKMARK_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-play-bookmark2", KEY_PLAY_BOOKMARK2, NULL,
             PLAY_BOOKMARK2_KEY_TEXT, PLAY_BOOKMARK_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-play-bookmark3", KEY_PLAY_BOOKMARK3, NULL,
             PLAY_BOOKMARK3_KEY_TEXT, PLAY_BOOKMARK_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-play-bookmark4", KEY_PLAY_BOOKMARK4, NULL,
             PLAY_BOOKMARK4_KEY_TEXT, PLAY_BOOKMARK_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-play-bookmark5", KEY_PLAY_BOOKMARK5, NULL,
             PLAY_BOOKMARK5_KEY_TEXT, PLAY_BOOKMARK_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-play-bookmark6", KEY_PLAY_BOOKMARK6, NULL,
             PLAY_BOOKMARK6_KEY_TEXT, PLAY_BOOKMARK_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-play-bookmark7", KEY_PLAY_BOOKMARK7, NULL,
             PLAY_BOOKMARK7_KEY_TEXT, PLAY_BOOKMARK_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-play-bookmark8", KEY_PLAY_BOOKMARK8, NULL,
             PLAY_BOOKMARK8_KEY_TEXT, PLAY_BOOKMARK_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-play-bookmark9", KEY_PLAY_BOOKMARK9, NULL,
             PLAY_BOOKMARK9_KEY_TEXT, PLAY_BOOKMARK_KEY_LONGTEXT, VLC_TRUE );
    add_key( "key-play-bookmark10", KEY_PLAY_BOOKMARK10, NULL,
             PLAY_BOOKMARK10_KEY_TEXT, PLAY_BOOKMARK_KEY_LONGTEXT, VLC_TRUE );

    /* Usage (mainly useful for cmd line stuff) */
    /* add_usage_hint( PLAYLIST_USAGE ); */

    set_description( N_("main program") );
    set_capability( "main", 100 );
vlc_module_end();

static module_config_t p_help_config[] =
{
    { CONFIG_ITEM_BOOL, NULL, "help", 'h',
      N_("print help for VLC (can be combined with --advanced)") },
    { CONFIG_ITEM_BOOL, NULL, "longhelp", 'H',
      N_("print help for VLC and all it's modules (can be combined with --advanced)") },
    { CONFIG_ITEM_BOOL, NULL, "advanced", '\0',
      N_("print help for the advanced options") },
    { CONFIG_ITEM_BOOL, NULL, "help-verbose", '\0',
      N_("ask for extra verbosity when displaying help") },
    { CONFIG_ITEM_BOOL, NULL, "list", 'l',
      N_("print a list of available modules") },
    { CONFIG_ITEM_STRING, NULL, "module", 'p',
      N_("print help on a specific module (can be combined with --advanced)") },
    { CONFIG_ITEM_BOOL, NULL, "save-config", '\0',
      N_("save the current command line options in the config") },
    { CONFIG_ITEM_BOOL, NULL, "reset-config", '\0',
      N_("reset the current config to the default values") },
    { CONFIG_ITEM_STRING, NULL, "config", '\0',
      N_("use alternate config file") },
    { CONFIG_ITEM_BOOL, NULL, "reset-plugins-cache", '\0',
      N_("resets the current plugins cache") },
    { CONFIG_ITEM_BOOL, NULL, "version", '\0',
      N_("print version information") },
    { CONFIG_HINT_END, NULL, NULL, '\0', NULL }
};

/*****************************************************************************
 * End configuration.
 *****************************************************************************/

/*****************************************************************************
 * Initializer for the vlc_t structure storing the action / key associations
 *****************************************************************************/
static struct hotkey p_hotkeys[] =
{
    { "key-quit", ACTIONID_QUIT, 0, 0, 0, 0 },
    { "key-play-pause", ACTIONID_PLAY_PAUSE, 0, 0, 0, 0 },
    { "key-play", ACTIONID_PLAY, 0, 0, 0, 0 },
    { "key-pause", ACTIONID_PAUSE, 0, 0, 0, 0 },
    { "key-stop", ACTIONID_STOP, 0, 0, 0, 0 },
    { "key-position", ACTIONID_POSITION, 0, 0, 0, 0 },
    { "key-jump-extrashort", ACTIONID_JUMP_BACKWARD_EXTRASHORT, 0, 1000000, 0, 0 },
    { "key-jump+extrashort", ACTIONID_JUMP_FORWARD_EXTRASHORT, 0, 1000000, 0, 0 },
    { "key-jump-short", ACTIONID_JUMP_BACKWARD_SHORT, 0, 1000000, 0, 0 },
    { "key-jump+short", ACTIONID_JUMP_FORWARD_SHORT, 0, 1000000, 0, 0 },
    { "key-jump-medium", ACTIONID_JUMP_BACKWARD_MEDIUM, 0, 1000000, 0, 0 },
    { "key-jump+medium", ACTIONID_JUMP_FORWARD_MEDIUM, 0, 1000000, 0, 0 },
    { "key-jump-long", ACTIONID_JUMP_BACKWARD_LONG, 0, 1000000, 0, 0 },
    { "key-jump+long", ACTIONID_JUMP_FORWARD_LONG, 0, 1000000, 0, 0 },
    { "key-prev", ACTIONID_PREV, 0, 0, 0, 0 },
    { "key-next", ACTIONID_NEXT, 0, 0, 0, 0 },
    { "key-faster", ACTIONID_FASTER, 0, 0, 0, 0 },
    { "key-slower", ACTIONID_SLOWER, 0, 0, 0, 0 },
    { "key-fullscreen", ACTIONID_FULLSCREEN, 0, 0, 0, 0 },
    { "key-vol-up", ACTIONID_VOL_UP, 0, 0, 0, 0 },
    { "key-vol-down", ACTIONID_VOL_DOWN, 0, 0, 0, 0 },
    { "key-vol-mute", ACTIONID_VOL_MUTE, 0, 0, 0, 0 },
    { "key-subdelay-down", ACTIONID_SUBDELAY_DOWN, 0, 0, 0, 0 },
    { "key-subdelay-up", ACTIONID_SUBDELAY_UP, 0, 0, 0, 0 },
    { "key-audiodelay-down", ACTIONID_AUDIODELAY_DOWN, 0, 0, 0, 0 },
    { "key-audiodelay-up", ACTIONID_AUDIODELAY_UP, 0, 0, 0, 0 },
    { "key-audio-track", ACTIONID_AUDIO_TRACK, 0, 0, 0, 0 },
    { "key-subtitle-track", ACTIONID_SUBTITLE_TRACK, 0, 0, 0, 0 },
    { "key-aspect-ratio", ACTIONID_ASPECT_RATIO, 0, 0, 0, 0 },
    { "key-crop", ACTIONID_CROP, 0, 0, 0, 0 },
    { "key-deinterlace", ACTIONID_DEINTERLACE, 0, 0, 0, 0 },
    { "key-intf-show", ACTIONID_INTF_SHOW, 0, 0, 0, 0 },
    { "key-intf-hide", ACTIONID_INTF_HIDE, 0, 0, 0, 0 },
    { "key-snapshot", ACTIONID_SNAPSHOT, 0, 0, 0, 0 },
    { "key-nav-activate", ACTIONID_NAV_ACTIVATE, 0, 0, 0, 0 },
    { "key-nav-up", ACTIONID_NAV_UP, 0, 0, 0, 0 },
    { "key-nav-down", ACTIONID_NAV_DOWN, 0, 0, 0, 0 },
    { "key-nav-left", ACTIONID_NAV_LEFT, 0, 0, 0, 0 },
    { "key-nav-right", ACTIONID_NAV_RIGHT, 0, 0, 0, 0 },
    { "key-disc-menu", ACTIONID_DISC_MENU, 0, 0, 0, 0 },
    { "key-title-prev", ACTIONID_TITLE_PREV, 0, 0, 0, 0 },
    { "key-title-next", ACTIONID_TITLE_NEXT, 0, 0, 0, 0 },
    { "key-chapter-prev", ACTIONID_CHAPTER_PREV, 0, 0, 0, 0 },
    { "key-chapter-next", ACTIONID_CHAPTER_NEXT, 0, 0, 0, 0 },
    { "key-set-bookmark1", ACTIONID_SET_BOOKMARK1, 0, 0, 0, 0 },
    { "key-set-bookmark2", ACTIONID_SET_BOOKMARK2, 0, 0, 0, 0 },
    { "key-set-bookmark3", ACTIONID_SET_BOOKMARK3, 0, 0, 0, 0 },
    { "key-set-bookmark4", ACTIONID_SET_BOOKMARK4, 0, 0, 0, 0 },
    { "key-set-bookmark5", ACTIONID_SET_BOOKMARK5, 0, 0, 0, 0 },
    { "key-set-bookmark6", ACTIONID_SET_BOOKMARK6, 0, 0, 0, 0 },
    { "key-set-bookmark7", ACTIONID_SET_BOOKMARK7, 0, 0, 0, 0 },
    { "key-set-bookmark8", ACTIONID_SET_BOOKMARK8, 0, 0, 0, 0 },
    { "key-set-bookmark9", ACTIONID_SET_BOOKMARK9, 0, 0, 0, 0 },
    { "key-set-bookmark10", ACTIONID_SET_BOOKMARK10, 0, 0, 0, 0 },
    { "key-play-bookmark1", ACTIONID_PLAY_BOOKMARK1, 0, 0, 0, 0 },
    { "key-play-bookmark2", ACTIONID_PLAY_BOOKMARK2, 0, 0, 0, 0 },
    { "key-play-bookmark3", ACTIONID_PLAY_BOOKMARK3, 0, 0, 0, 0 },
    { "key-play-bookmark4", ACTIONID_PLAY_BOOKMARK4, 0, 0, 0, 0 },
    { "key-play-bookmark5", ACTIONID_PLAY_BOOKMARK5, 0, 0, 0, 0 },
    { "key-play-bookmark6", ACTIONID_PLAY_BOOKMARK6, 0, 0, 0, 0 },
    { "key-play-bookmark7", ACTIONID_PLAY_BOOKMARK7, 0, 0, 0, 0 },
    { "key-play-bookmark8", ACTIONID_PLAY_BOOKMARK8, 0, 0, 0, 0 },
    { "key-play-bookmark9", ACTIONID_PLAY_BOOKMARK9, 0, 0, 0, 0 },
    { "key-play-bookmark10", ACTIONID_PLAY_BOOKMARK10, 0, 0, 0, 0 },
    { "key-history-back", ACTIONID_HISTORY_BACK, 0, 0, 0, 0 },
    { "key-history-forward", ACTIONID_HISTORY_FORWARD, 0, 0, 0, 0 },
    { "key-record", ACTIONID_RECORD, 0, 0, 0, 0 },
    { NULL, 0, 0, 0, 0, 0 }
};
