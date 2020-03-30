/*****************************************************************************
 * libvlc-module.c: Options for the core (libvlc itself) module
 *****************************************************************************
 * Copyright (C) 1998-2009 VLC authors and VideoLAN
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Jean-Paul Saman <jpsaman #_at_# m2x.nl>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

// Pretend we are a builtin module
#define MODULE_NAME core

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <limits.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_cpu.h>
#include "libvlc.h"
#include "modules/modules.h"

//#define Nothing here, this is just to prevent update-po from being stupid
#include "vlc_actions.h"
#include "vlc_meta.h"
#include <vlc_aout.h>
#include <vlc_vout.h>
#include <vlc_player.h>

#include "clock/clock.h"

static const char *const ppsz_snap_formats[] =
{ "png", "jpg", "tiff" };

/*****************************************************************************
 * Configuration options for the core module. Each module will also separatly
 * define its own configuration options.
 * Look into configuration.h if you need to know more about the following
 * macros.
 *****************************************************************************/

/*****************************************************************************
 * Intf
 ****************************************************************************/

// DEPRECATED
#define INTF_CAT_LONGTEXT N_( \
    "These options allow you to configure the interfaces used by VLC. " \
    "You can select the main interface, additional " \
    "interface modules, and define various related options." )

#define INTF_TEXT N_("Interface module")
#define INTF_LONGTEXT N_( \
    "This is the main interface used by VLC. " \
    "The default behavior is to automatically select the best module " \
    "available.")

#define EXTRAINTF_TEXT N_("Extra interface modules")
#define EXTRAINTF_LONGTEXT N_( \
    "You can select \"additional interfaces\" for VLC. " \
    "They will be launched in the background in addition to the default " \
    "interface. Use a colon separated list of interface modules. (common " \
    "values are \"rc\" (remote control), \"http\", \"gestures\" ...)")

#define CONTROL_TEXT N_("Control interfaces")
#define CONTROL_LONGTEXT N_( \
    "You can select control interfaces for VLC.")

#define VERBOSE_TEXT N_("Verbosity (0,1,2)")
#define VERBOSE_LONGTEXT N_( \
    "This is the verbosity level (0=only errors and " \
    "standard messages, 1=warnings, 2=debug).")

#define OPEN_TEXT N_("Default stream")
#define OPEN_LONGTEXT N_( \
    "This stream will always be opened at VLC startup." )

#define COLOR_TEXT N_("Color messages")
#define COLOR_LONGTEXT N_( \
    "This enables colorization of the messages sent to the console. " \
    "Your terminal needs Linux color support for this to work.")

#define INTERACTION_TEXT N_("Interface interaction")
#define INTERACTION_LONGTEXT N_( \
    "When this is enabled, the interface will show a dialog box each time " \
    "some user input is required." )


/*****************************************************************************
 * Audio
 ****************************************************************************/

// DEPRECATED
#define AOUT_CAT_LONGTEXT N_( \
    "These options allow you to modify the behavior of the audio " \
    "subsystem, and to add audio filters which can be used for " \
    "post processing or visual effects (spectrum analyzer, etc.). " \
    "Enable these filters here, and configure them in the \"audio filters\" " \
    "modules section.")

#define AOUT_TEXT N_("Audio output module")
#define AOUT_LONGTEXT N_( \
    "This is the audio output method used by VLC. " \
    "The default behavior is to automatically select the best method " \
    "available.")

#define ROLE_TEXT N_("Media role")
#define ROLE_LONGTEXT N_("Media (player) role for operating system policy.")

#define AUDIO_BITEXACT_TEXT N_("Enable bit-exact mode (pure mode)")
#define AUDIO_BITEXACT_LONGTEXT N_( \
    "This will disable all audio filters, even audio converters. " \
    "This may result on audio not working if the output can't adapt to the " \
    "input format.")

#define AUDIO_TEXT N_("Enable audio")
#define AUDIO_LONGTEXT N_( \
    "You can completely disable the audio output. The audio " \
    "decoding stage will not take place, thus saving some processing power.")
static const char *ppsz_roles[] = {
     "video", "music", "communication", "game",
     "notification", "animation", "production",
     "accessibility", "test",
};
static const char *ppsz_roles_text[] = {
    N_("Video"), N_("Music"), N_("Communication"), N_("Game"),
    N_("Notification"),  N_("Animation"), N_("Production"),
    N_("Accessibility"), N_("Test"),
};

#define GAIN_TEXT N_("Audio gain")
#define GAIN_LONGTEXT N_( \
    "This linear gain will be applied to outputted audio.")

#define VOLUME_STEP_TEXT N_("Audio output volume step")
#define VOLUME_STEP_LONGTEXT N_( \
    "The step size of the volume is adjustable using this option.")
#define AOUT_VOLUME_STEP 12.8

#define VOLUME_SAVE_TEXT N_( "Remember the audio volume" )
#define VOLUME_SAVE_LONGTEXT N_( \
    "The volume can be recorded and automatically restored next time " \
    "VLC is used." )

#define DESYNC_TEXT N_("Audio desynchronization compensation")
#define DESYNC_LONGTEXT N_( \
    "This delays the audio output. The delay must be given in milliseconds. " \
    "This can be handy if you notice a lag between the video and the audio.")

#define AUDIO_RESAMPLER_TEXT N_("Audio resampler")
#define AUDIO_RESAMPLER_LONGTEXT N_( \
    "This selects which plugin to use for audio resampling." )

#define MULTICHA_LONGTEXT N_( \
    "Sets the audio output channels mode that will be used by default " \
    "if your hardware and the audio stream are compatible.")

#if defined(__ANDROID__) || defined(__APPLE__) || defined(_WIN32)
#define SPDIF_TEXT N_("Force S/PDIF support")
#define SPDIF_LONGTEXT N_( \
    "This option should be used when the audio output can't negotiate S/PDIF support.")
#endif

#define FORCE_DOLBY_TEXT N_("Force detection of Dolby Surround")
#define FORCE_DOLBY_LONGTEXT N_( \
    "Use this when you know your stream is (or is not) encoded with Dolby "\
    "Surround but fails to be detected as such. Even if the stream is "\
    "not actually encoded with Dolby Surround, turning on this option might "\
    "enhance your experience, especially when combined with the Headphone "\
    "Channel Mixer." )
static const int pi_force_dolby_values[] = { 0, 1, 2 };
static const char *const ppsz_force_dolby_descriptions[] = {
    N_("Auto"), N_("On"), N_("Off") };

#define STEREO_MODE_TEXT N_("Stereo audio output mode")
static const int pi_stereo_mode_values[] = { AOUT_VAR_CHAN_UNSET,
    AOUT_VAR_CHAN_STEREO, AOUT_VAR_CHAN_RSTEREO,
    AOUT_VAR_CHAN_LEFT, AOUT_VAR_CHAN_RIGHT, AOUT_VAR_CHAN_DOLBYS,
    AOUT_VAR_CHAN_HEADPHONES,
};
static const char *const ppsz_stereo_mode_texts[] = { N_("Unset"),
    N_("Stereo"), N_("Reverse stereo"),
    N_("Left"), N_("Right"), N_("Dolby Surround"),
    N_("Headphones"),
};

#define AUDIO_FILTER_TEXT N_("Audio filters")
#define AUDIO_FILTER_LONGTEXT N_( \
    "This adds audio post processing filters, to modify " \
    "the sound rendering." )

#define AUDIO_VISUAL_TEXT N_("Audio visualizations")
#define AUDIO_VISUAL_LONGTEXT N_( \
    "This adds visualization modules (spectrum analyzer, etc.).")


#define AUDIO_REPLAY_GAIN_MODE_TEXT N_( \
    "Replay gain mode" )
#define AUDIO_REPLAY_GAIN_MODE_LONGTEXT N_( \
    "Select the replay gain mode" )
#define AUDIO_REPLAY_GAIN_PREAMP_TEXT N_( \
    "Replay preamp" )
#define AUDIO_REPLAY_GAIN_PREAMP_LONGTEXT N_( \
    "This allows you to change the default target level (89 dB) " \
    "for stream with replay gain information" )
#define AUDIO_REPLAY_GAIN_DEFAULT_TEXT N_( \
    "Default replay gain" )
#define AUDIO_REPLAY_GAIN_DEFAULT_LONGTEXT N_( \
    "This is the gain used for stream without replay gain information" )
#define AUDIO_REPLAY_GAIN_PEAK_PROTECTION_TEXT N_( \
    "Peak protection" )
#define AUDIO_REPLAY_GAIN_PEAK_PROTECTION_LONGTEXT N_( \
    "Protect against sound clipping" )

#define AUDIO_TIME_STRETCH_TEXT N_( \
    "Enable time stretching audio" )
#define AUDIO_TIME_STRETCH_LONGTEXT N_( \
    "This allows playing audio at lower or higher speed without " \
    "affecting the audio pitch" )


static const char *const ppsz_replay_gain_mode[] = {
    "none", "track", "album" };
static const char *const ppsz_replay_gain_mode_text[] = {
    N_("None"), N_("Track"), N_("Album") };

/*****************************************************************************
 * Video
 ****************************************************************************/

// DEPRECATED
#define VOUT_CAT_LONGTEXT N_( \
    "These options allow you to modify the behavior of the video output " \
    "subsystem. You can for example enable video filters (deinterlacing, " \
    "image adjusting, etc.). Enable these filters here and configure " \
    "them in the \"video filters\" modules section. You can also set many " \
    "miscellaneous video options." )

#define VOUT_TEXT N_("Video output module")
#define VOUT_LONGTEXT N_( \
    "This is the the video output method used by VLC. " \
    "The default behavior is to automatically select the best method available.")

#define VIDEO_TEXT N_("Enable video")
#define VIDEO_LONGTEXT N_( \
    "You can completely disable the video output. The video " \
    "decoding stage will not take place, thus saving some processing power.")

#define WIDTH_TEXT N_("Video width")
#define WIDTH_LONGTEXT N_( \
    "You can enforce the video width. By default (-1) VLC will " \
    "adapt to the video characteristics.")

#define HEIGHT_TEXT N_("Video height")
#define HEIGHT_LONGTEXT N_( \
    "You can enforce the video height. By default (-1) VLC will " \
    "adapt to the video characteristics.")

#define VIDEOX_TEXT N_("Video X coordinate")
#define VIDEOX_LONGTEXT N_( \
    "You can enforce the position of the top left corner of the video window "\
    "(X coordinate).")

#define VIDEOY_TEXT N_("Video Y coordinate")
#define VIDEOY_LONGTEXT N_( \
    "You can enforce the position of the top left corner of the video window "\
    "(Y coordinate).")

#define VIDEO_TITLE_TEXT N_("Video title")
#define VIDEO_TITLE_LONGTEXT N_( \
    "Custom title for the video window (in case the video is not embedded in "\
    "the interface).")

#define ALIGN_TEXT N_("Video alignment")
#define ALIGN_LONGTEXT N_( \
    "Enforce the alignment of the video in its window. By default (0) it " \
    "will be centered (0=center, 1=left, 2=right, 4=top, 8=bottom, you can " \
    "also use combinations of these values, like 6=4+2 meaning top-right).")
static const int pi_align_values[] = { 0, VOUT_ALIGN_LEFT, VOUT_ALIGN_RIGHT,
                                       VOUT_ALIGN_TOP, VOUT_ALIGN_BOTTOM,
                                       VOUT_ALIGN_TOP|VOUT_ALIGN_LEFT,
                                       VOUT_ALIGN_TOP|VOUT_ALIGN_RIGHT,
                                       VOUT_ALIGN_BOTTOM|VOUT_ALIGN_LEFT,
                                       VOUT_ALIGN_BOTTOM|VOUT_ALIGN_RIGHT };
static const char *const ppsz_align_descriptions[] =
{ N_("Center"), N_("Left"), N_("Right"), N_("Top"), N_("Bottom"),
  N_("Top-Left"), N_("Top-Right"), N_("Bottom-Left"), N_("Bottom-Right") };

#define ZOOM_TEXT N_("Zoom video")
#define ZOOM_LONGTEXT N_( \
    "You can zoom the video by the specified factor.")

#define GRAYSCALE_TEXT N_("Grayscale video output")
#define GRAYSCALE_LONGTEXT N_( \
    "Output video in grayscale. As the color information aren't decoded, " \
    "this can save some processing power." )

#define EMBEDDED_TEXT N_("Embedded video")
#define EMBEDDED_LONGTEXT N_( \
    "Embed the video output in the main interface." )

#define FULLSCREEN_TEXT N_("Fullscreen video output")
#define FULLSCREEN_LONGTEXT N_( \
    "Start video in fullscreen mode" )

#define VIDEO_ON_TOP_TEXT N_("Always on top")
#define VIDEO_ON_TOP_LONGTEXT N_( \
    "Always place the video window on top of other windows." )

#define WALLPAPER_TEXT N_("Enable wallpaper mode")
#define WALLPAPER_LONGTEXT N_( \
    "The wallpaper mode allows you to display the video as the desktop " \
    "background." )

#define VIDEO_TITLE_SHOW_TEXT N_("Show media title on video")
#define VIDEO_TITLE_SHOW_LONGTEXT N_( \
    "Display the title of the video on top of the movie.")

#define VIDEO_TITLE_TIMEOUT_TEXT N_("Show video title for x milliseconds")
#define VIDEO_TITLE_TIMEOUT_LONGTEXT N_( \
    "Show the video title for n milliseconds, default is 5000 ms (5 sec.)")

#define VIDEO_TITLE_POSITION_TEXT N_("Position of video title")
#define VIDEO_TITLE_POSITION_LONGTEXT N_( \
    "Place on video where to display the title (default bottom center).")

#define MOUSE_HIDE_TIMEOUT_TEXT N_("Hide cursor and fullscreen " \
                                   "controller after x milliseconds")
#define MOUSE_HIDE_TIMEOUT_LONGTEXT N_( \
    "Hide mouse cursor and fullscreen controller after " \
    "n milliseconds.")

#define DEINTERLACE_TEXT N_("Deinterlace")
#define DEINTERLACE_LONGTEXT N_(\
    "Deinterlace")
static const int pi_deinterlace[] = {
    0, -1, 1
};
static const char * const  ppsz_deinterlace_text[] = {
    "Off", "Automatic", "On"
};

#define DEINTERLACE_MODE_TEXT N_("Deinterlace mode")
#define DEINTERLACE_MODE_LONGTEXT N_( \
    "Deinterlace method to use for video processing.")
static const char * const ppsz_deinterlace_mode[] = {
    "auto", "discard", "blend", "mean", "bob",
    "linear", "x", "yadif", "yadif2x", "phosphor",
    "ivtc"
};
static const char * const ppsz_deinterlace_mode_text[] = {
    N_("Auto"), N_("Discard"), N_("Blend"), N_("Mean"), N_("Bob"),
    N_("Linear"), "X", "Yadif", "Yadif (2x)", N_("Phosphor"),
    N_("Film NTSC (IVTC)")
};

static const int pi_pos_values[] = { 0, 1, 2, 4, 8, 5, 6, 9, 10 };
static const char *const ppsz_pos_descriptions[] =
{ N_("Center"), N_("Left"), N_("Right"), N_("Top"), N_("Bottom"),
  N_("Top-Left"), N_("Top-Right"), N_("Bottom-Left"), N_("Bottom-Right") };

static const int pi_sub_align_values[] = { -1, 0, 1, 2, 4, 8, 5, 6, 9, 10 };
static const char *const ppsz_sub_align_descriptions[] =
{ N_("Unset"), N_("Center"), N_("Left"), N_("Right"), N_("Top"), N_("Bottom"),
  N_("Top-Left"), N_("Top-Right"), N_("Bottom-Left"), N_("Bottom-Right") };

#define SS_TEXT N_("Disable screensaver")
#define SS_LONGTEXT N_("Disable the screensaver during video playback." )

static const int screensaver_values[] = { 0, 2, 1, };
static const char *const screensaver_texts[] = {
    N_("Never"), N_("When fullscreen"), N_("Always"),
};

#define VIDEO_DECO_TEXT N_("Window decorations")
#define VIDEO_DECO_LONGTEXT N_( \
    "VLC can avoid creating window caption, frames, etc... around the video" \
    ", giving a \"minimal\" window.")

#define VIDEO_FILTER_TEXT N_("Video filter module")
#define VIDEO_FILTER_LONGTEXT N_( \
    "This adds post-processing filters to enhance the " \
    "picture quality, for instance deinterlacing, or distort " \
    "the video.")

#define SNAP_PATH_TEXT N_("Video snapshot directory (or filename)")
#define SNAP_PATH_LONGTEXT N_( \
    "Directory where the video snapshots will be stored.")

#define SNAP_PREFIX_TEXT N_("Video snapshot file prefix")
#define SNAP_PREFIX_LONGTEXT N_( \
    "Video snapshot file prefix" )

#define SNAP_FORMAT_TEXT N_("Video snapshot format")
#define SNAP_FORMAT_LONGTEXT N_( \
    "Image format which will be used to store the video snapshots" )

#define SNAP_PREVIEW_TEXT N_("Display video snapshot preview")
#define SNAP_PREVIEW_LONGTEXT N_( \
    "Display the snapshot preview in the screen's top-left corner.")

#define SNAP_SEQUENTIAL_TEXT N_("Use sequential numbers instead of timestamps")
#define SNAP_SEQUENTIAL_LONGTEXT N_( \
    "Use sequential numbers instead of timestamps for snapshot numbering")

#define SNAP_WIDTH_TEXT N_("Video snapshot width")
#define SNAP_WIDTH_LONGTEXT N_( \
    "You can enforce the width of the video snapshot. By default " \
    "it will keep the original width (-1). Using 0 will scale the width " \
    "to keep the aspect ratio." )

#define SNAP_HEIGHT_TEXT N_("Video snapshot height")
#define SNAP_HEIGHT_LONGTEXT N_( \
    "You can enforce the height of the video snapshot. By default " \
    "it will keep the original height (-1). Using 0 will scale the height " \
    "to keep the aspect ratio." )

#define CROP_TEXT N_("Video cropping")
#define CROP_LONGTEXT N_( \
    "This forces the cropping of the source video. " \
    "Accepted formats are x:y (4:3, 16:9, etc.) expressing the global image " \
    "aspect.")

#define ASPECT_RATIO_TEXT N_("Source aspect ratio")
#define ASPECT_RATIO_LONGTEXT N_( \
    "This forces the source aspect ratio. For instance, some DVDs claim " \
    "to be 16:9 while they are actually 4:3. This can also be used as a " \
    "hint for VLC when a movie does not have aspect ratio information. " \
    "Accepted formats are x:y (4:3, 16:9, etc.) expressing the global image " \
    "aspect, or a float value (1.25, 1.3333, etc.) expressing pixel " \
    "squareness.")

#define AUTOSCALE_TEXT N_("Video Auto Scaling")
#define AUTOSCALE_LONGTEXT N_( \
    "Let the video scale to fit a given window or fullscreen.")

#define SCALEFACTOR_TEXT N_("Video scaling factor")
#define SCALEFACTOR_LONGTEXT N_( \
    "Scaling factor used when Auto Scaling is disabled.\n" \
    "Default value is 1.0 (original video size).")

#define CUSTOM_CROP_RATIOS_TEXT N_("Custom crop ratios list")
#define CUSTOM_CROP_RATIOS_LONGTEXT N_( \
    "Comma separated list of crop ratios which will be added in the " \
    "interface's crop ratios list.")

#define CUSTOM_ASPECT_RATIOS_TEXT N_("Custom aspect ratios list")
#define CUSTOM_ASPECT_RATIOS_LONGTEXT N_( \
    "Comma separated list of aspect ratios which will be added in the " \
    "interface's aspect ratio list.")

#define HDTV_FIX_TEXT N_("Fix HDTV height")
#define HDTV_FIX_LONGTEXT N_( \
    "This allows proper handling of HDTV-1080 video format " \
    "even if broken encoder incorrectly sets height to 1088 lines. " \
    "You should only disable this option if your video has a " \
    "non-standard format requiring all 1088 lines.")

#define MASPECT_RATIO_TEXT N_("Monitor pixel aspect ratio")
#define MASPECT_RATIO_LONGTEXT N_( \
    "This forces the monitor aspect ratio. Most monitors have square " \
    "pixels (1:1). If you have a 16:9 screen, you might need to change this " \
    "to 4:3 in order to keep proportions.")

#define SKIP_FRAMES_TEXT N_("Skip frames")
#define SKIP_FRAMES_LONGTEXT N_( \
    "Enables framedropping on MPEG2 stream. Framedropping " \
    "occurs when your computer is not powerful enough" )

#define DROP_LATE_FRAMES_TEXT N_("Drop late frames")
#define DROP_LATE_FRAMES_LONGTEXT N_( \
    "This drops frames that are late (arrive to the video output after " \
    "their intended display date)." )

#define QUIET_SYNCHRO_TEXT N_("Quiet synchro")
#define QUIET_SYNCHRO_LONGTEXT N_( \
    "This avoids flooding the message log with debug output from the " \
    "video output synchronization mechanism.")

#define KEYBOARD_EVENTS_TEXT N_("Key press events")
#define KEYBOARD_EVENTS_LONGTEXT N_( \
    "This enables VLC hotkeys from the (non-embedded) video window." )

#define MOUSE_EVENTS_TEXT N_("Mouse events")
#define MOUSE_EVENTS_LONGTEXT N_( \
    "This enables handling of mouse clicks on the video." )

/*****************************************************************************
 * Input
 ****************************************************************************/

// Deprecated
#define INPUT_CAT_LONGTEXT N_( \
    "These options allow you to modify the behavior of the input " \
    "subsystem, such as the DVD or VCD device, the network interface " \
    "settings or the subtitle channel.")

#define CACHING_TEXT N_("File caching (ms)")
#define CACHING_LONGTEXT N_( \
    "Caching value for local files, in milliseconds." )

#define CAPTURE_CACHING_TEXT N_("Live capture caching (ms)")
#define CAPTURE_CACHING_LONGTEXT N_( \
    "Caching value for cameras and microphones, in milliseconds." )

#define DISC_CACHING_TEXT N_("Disc caching (ms)")
#define DISC_CACHING_LONGTEXT N_( \
    "Caching value for optical media, in milliseconds." )

#define NETWORK_CACHING_TEXT N_("Network caching (ms)")
#define NETWORK_CACHING_LONGTEXT N_( \
    "Caching value for network resources, in milliseconds." )

#define CR_AVERAGE_TEXT N_("Clock reference average counter")
#define CR_AVERAGE_LONGTEXT N_( \
    "When using the PVR input (or a very irregular source), you should " \
    "set this to 10000.")

#define CLOCK_SYNCHRO_TEXT N_("Clock synchronisation")
#define CLOCK_SYNCHRO_LONGTEXT N_( \
    "It is possible to disable the input clock synchronisation for " \
    "real-time sources. Use this if you experience jerky playback of " \
    "network streams.")

#define CLOCK_JITTER_TEXT N_("Clock jitter")
#define CLOCK_JITTER_LONGTEXT N_( \
    "This defines the maximum input delay jitter that the synchronization " \
    "algorithms should try to compensate (in milliseconds)." )

#define CLOCK_MASTER_TEXT N_("Clock master source")

static const int pi_clock_master_values[] = {
    VLC_CLOCK_MASTER_AUDIO,
    VLC_CLOCK_MASTER_MONOTONIC,
};
static const char *const ppsz_clock_master_descriptions[] = {
    N_("Audio"),
    N_("Monotonic")
};

#define NETSYNC_TEXT N_("Network synchronisation" )
#define NETSYNC_LONGTEXT N_( "This allows you to remotely " \
        "synchronise clocks for server and client. The detailed settings " \
        "are available in Advanced / Network Sync." )

static const int pi_clock_values[] = { -1, 0, 1 };
static const char *const ppsz_clock_descriptions[] =
{ N_("Default"), N_("Disable"), N_("Enable") };

#define MTU_TEXT N_("MTU of the network interface")
#define MTU_LONGTEXT N_( \
    "This is the maximum application-layer packet size that can be " \
    "transmitted over the network (in bytes).")
/* Should be less than 1500 - 8[ppp] - 40[ip6] - 8[udp] in any case. */
#define MTU_DEFAULT 1400

#define TTL_TEXT N_("Hop limit (TTL)")
#define TTL_LONGTEXT N_( \
    "This is the hop limit (also known as \"Time-To-Live\" or TTL) of " \
    "the multicast packets sent by the stream output (-1 = use operating " \
    "system built-in default).")

#define MIFACE_TEXT N_("Multicast output interface")
#define MIFACE_LONGTEXT N_( \
    "Default multicast interface. This overrides the routing table.")

#define DSCP_TEXT N_("DiffServ Code Point")
#define DSCP_LONGTEXT N_("Differentiated Services Code Point " \
    "for outgoing UDP streams (or IPv4 Type Of Service, " \
    "or IPv6 Traffic Class). This is used for network Quality of Service.")

#define INPUT_PROGRAM_TEXT N_("Program")
#define INPUT_PROGRAM_LONGTEXT N_( \
    "Choose the program to select by giving its Service ID. " \
    "Only use this option if you want to read a multi-program stream " \
    "(like DVB streams for example)." )

#define INPUT_PROGRAMS_TEXT N_("Programs")
#define INPUT_PROGRAMS_LONGTEXT N_( \
    "Choose the programs to select by giving a comma-separated list of " \
    "Service IDs (SIDs). " \
    "Only use this option if you want to read a multi-program stream " \
    "(like DVB streams for example)." )

/// \todo Document how to find it
#define INPUT_VIDEOTRACK_TEXT N_("Video track")
#define INPUT_VIDEOTRACK_LONGTEXT N_( \
    "Stream number of the video track to use " \
    "(from 0 to n).")

#define INPUT_AUDIOTRACK_TEXT N_("Audio track")
#define INPUT_AUDIOTRACK_LONGTEXT N_( \
    "Stream number of the audio track to use " \
    "(from 0 to n).")

#define INPUT_SUBTRACK_TEXT N_("Subtitle track")
#define INPUT_SUBTRACK_LONGTEXT N_( \
    "Stream number of the subtitle track to use " \
    "(from 0 to n).")

#define INPUT_AUDIOTRACK_LANG_TEXT N_("Audio language")
#define INPUT_AUDIOTRACK_LANG_LONGTEXT N_( \
    "Language of the audio track you want to use " \
    "(comma separated, two or three letter country code, you may use 'none' to avoid a fallback to another language).")

#define INPUT_SUBTRACK_LANG_TEXT N_("Subtitle language")
#define INPUT_SUBTRACK_LANG_LONGTEXT N_( \
    "Language of the subtitle track you want to use " \
    "(comma separated, two or three letters country code, you may use 'any' as a fallback).")

#define INPUT_MENUTRACK_LANG_TEXT N_("Menu language")
#define INPUT_MENUTRACK_LANG_LONGTEXT N_( \
    "Language of the menus you want to use with DVD/BluRay " \
    "(comma separated, two or three letters country code, you may use 'any' as a fallback).")

#define INPUT_VIDEOTRACK_ID_TEXT N_("Video track ID")
#define INPUT_VIDEOTRACK_ID_LONGTEXT N_( \
    "Stream ID of the video track to use.")

#define INPUT_AUDIOTRACK_ID_TEXT N_("Audio track ID")
#define INPUT_AUDIOTRACK_ID_LONGTEXT N_( \
    "Stream ID of the audio track to use.")

#define INPUT_SUBTRACK_ID_TEXT N_("Subtitle track ID")
#define INPUT_SUBTRACK_ID_LONGTEXT N_( \
    "Stream ID of the subtitle track to use.")

#define INPUT_CAPTIONS_TEXT N_(N_("Preferred Closed Captions decoder"))
static const int pi_captions[] = { 608, 708 };
static const char *const ppsz_captions[] = { "EIA/CEA 608", "CEA 708" };

#define INPUT_PREFERREDRESOLUTION_TEXT N_("Preferred video resolution")
#define INPUT_PREFERREDRESOLUTION_LONGTEXT N_( \
    "When several video formats are available, select one whose " \
    "resolution is closest to (but not higher than) this setting, " \
    "in number of lines. Use this option if you don't have enough CPU " \
    "power or network bandwidth to play higher resolutions.")
static const int pi_prefres[] = { -1, 1080, 720, 576, 360, 240 };
static const char *const ppsz_prefres[] = {
    N_("Best available"), N_("Full HD (1080p)"), N_("HD (720p)"),
    N_("Standard Definition (576 or 480 lines)"),
    N_("Low Definition (360 lines)"),
    N_("Very Low Definition (240 lines)"),
};

#define INPUT_LOWDELAY_TEXT N_("Low delay mode")
#define INPUT_LOWDELAY_LONGTEXT N_(\
    "Try to minimize delay along decoding chain."\
    "Might break with non compliant streams.")

#define INPUT_REPEAT_TEXT N_("Input repetitions")
#define INPUT_REPEAT_LONGTEXT N_( \
    "Number of time the same input will be repeated")

#define START_TIME_TEXT N_("Start time")
#define START_TIME_LONGTEXT N_( \
    "The stream will start at this position (in seconds)." )

#define STOP_TIME_TEXT N_("Stop time")
#define STOP_TIME_LONGTEXT N_( \
    "The stream will stop at this position (in seconds)." )

#define RUN_TIME_TEXT N_("Run time")
#define RUN_TIME_LONGTEXT N_( \
    "The stream will run this duration (in seconds)." )

#define INPUT_FAST_SEEK_TEXT N_("Fast seek")
#define INPUT_FAST_SEEK_LONGTEXT N_( \
    "Favor speed over precision while seeking" )

#define INPUT_RATE_TEXT N_("Playback speed")
#define INPUT_RATE_LONGTEXT N_( \
    "This defines the playback speed (nominal speed is 1.0)." )

#define INPUT_LIST_TEXT N_("Input list")
#define INPUT_LIST_LONGTEXT N_( \
    "You can give a comma-separated list " \
    "of inputs that will be concatenated together after the normal one.")

#define INPUT_SLAVE_TEXT N_("Input slave (experimental)")
#define INPUT_SLAVE_LONGTEXT N_( \
    "This allows you to play from several inputs at " \
    "the same time. This feature is experimental, not all formats " \
    "are supported. Use a '#' separated list of inputs.")

#define BOOKMARKS_TEXT N_("Bookmarks list for a stream")
#define BOOKMARKS_LONGTEXT N_( \
    "You can manually give a list of bookmarks for a stream in " \
    "the form \"{name=bookmark-name,time=optional-time-offset," \
    "bytes=optional-byte-offset},{...}\"")

#define RESTORE_PLAYBACK_POS_TEXT N_("Continue playback?")

#define RESTORE_PLAYBACK_STATE_TEXT N_("Resume last playback states")
#define RESTORE_PLAYBACK_STATE_LONGTEXT N_("This will resume the last playback " \
    "state, such as the selected tracks, rate, aspect-ratio, ..." )

#define INPUT_RECORD_PATH_TEXT N_("Record directory")
#define INPUT_RECORD_PATH_LONGTEXT N_( \
    "Directory where the records will be stored" )

#define INPUT_RECORD_NATIVE_TEXT N_("Prefer native stream recording")
#define INPUT_RECORD_NATIVE_LONGTEXT N_( \
    "When possible, the input stream will be recorded instead of using " \
    "the stream output module" )

#define INPUT_TIMESHIFT_PATH_TEXT N_("Timeshift directory")
#define INPUT_TIMESHIFT_PATH_LONGTEXT N_( \
    "Directory used to store the timeshift temporary files." )

#define INPUT_TIMESHIFT_GRANULARITY_TEXT N_("Timeshift granularity")
#define INPUT_TIMESHIFT_GRANULARITY_LONGTEXT N_( \
    "This is the maximum size in bytes of the temporary files " \
    "that will be used to store the timeshifted streams." )

#define INPUT_TITLE_FORMAT_TEXT N_( "Change title according to current media" )
#define INPUT_TITLE_FORMAT_LONGTEXT N_( "This option allows you to set the title according to what's being played<br>"  \
    "$a: Artist<br>$b: Album<br>$c: Copyright<br>$t: Title<br>$g: Genre<br>"  \
    "$n: Track num<br>$p: Now playing<br>$A: Date<br>$D: Duration<br>"  \
    "$Z: \"Now playing\" (Fall back on Title - Artist)" )

#define INPUT_LUA_TEXT N_( "Disable all lua plugins" )

// DEPRECATED
#define SUB_CAT_LONGTEXT N_( \
    "These options allow you to modify the behavior of the subpictures " \
    "subsystem. You can for example enable subpictures sources (logo, etc.). " \
    "Enable these filters here and configure them in the " \
    "\"subsources filters\" modules section. You can also set many " \
    "miscellaneous subpictures options." )

#define SUB_MARGIN_TEXT N_("Force subtitle position")
#define SUB_MARGIN_LONGTEXT N_( \
    "You can use this option to place the subtitles under the movie, " \
    "instead of over the movie. Try several positions.")

#define SUB_TEXT_SCALE_TEXT N_("Subtitles text scaling factor")
#define SUB_TEXT_SCALE_LONGTEXT N_("Changes the subtitles size where possible")

#define SPU_TEXT N_("Enable sub-pictures")
#define SPU_LONGTEXT N_( \
    "You can completely disable the sub-picture processing.")

#define SECONDARY_SUB_POSITION_TEXT N_("Position of secondary subtitles")
#define SECONDARY_SUB_POSITION_LONGTEXT N_( \
    "Place on video where to display secondary subtitles (default bottom center).")

#define SECONDARY_SUB_MARGIN_TEXT N_("Force secondary subtitle position")
#define SECONDARY_SUB_MARGIN_LONGTEXT N_( \
    "You can use this option to vertically adjust the position secondary " \
    "subtitles are displayed.")

#define OSD_TEXT N_("On Screen Display")
#define OSD_LONGTEXT N_( \
    "VLC can display messages on the video. This is called OSD (On Screen " \
    "Display).")

#define TEXTRENDERER_TEXT N_("Text rendering module")
#define TEXTRENDERER_LONGTEXT N_( \
    "VLC normally uses Freetype for rendering, but this allows you to use svg for instance.")

#define SUB_SOURCE_TEXT N_("Subpictures source module")
#define SUB_SOURCE_LONGTEXT N_( \
    "This adds so-called \"subpicture sources\". These filters overlay " \
    "some images or text over the video (like a logo, arbitrary text, ...)." )

#define SUB_FILTER_TEXT N_("Subpictures filter module")
#define SUB_FILTER_LONGTEXT N_( \
    "This adds so-called \"subpicture filters\". These filter subpictures " \
    "created by subtitle decoders or other subpictures sources." )

#define SUB_AUTO_TEXT N_("Autodetect subtitle files")
#define SUB_AUTO_LONGTEXT N_( \
    "Automatically detect a subtitle file, if no subtitle filename is " \
    "specified (based on the filename of the movie).")

#define SUB_FUZZY_TEXT N_("Subtitle autodetection fuzziness")
#define SUB_FUZZY_LONGTEXT N_( \
    "This determines how fuzzy subtitle and movie filename matching " \
    "will be. Options are:\n" \
    "0 = no subtitles autodetected\n" \
    "1 = any subtitle file\n" \
    "2 = any subtitle file containing the movie name\n" \
    "3 = subtitle file matching the movie name with additional chars\n" \
    "4 = subtitle file matching the movie name exactly")

#define SUB_PATH_TEXT N_("Subtitle autodetection paths")
#define SUB_PATH_LONGTEXT N_( \
    "Look for a subtitle file in those paths too, if your subtitle " \
    "file was not found in the current directory.")

#define SUB_FPS_TEXT  N_("Subtitle Frames per Second")
#define SUB_FPS_LONGTEXT \
    N_("Override the normal frames per second settings. ")

#define SUB_DELAY_TEXT N_("Subtitle delay")
#define SUB_DELAY_LONGTEXT \
    N_("Apply a delay to all subtitles (in 1/10s, eg 100 means 10s).")

#define SUB_FILE_TEXT N_("Use subtitle file")
#define SUB_FILE_LONGTEXT N_( \
    "Load this subtitle file. To be used when autodetect cannot detect " \
    "your subtitle file.")

/* DVD and VCD devices */
#define DVD_DEV_TEXT N_("DVD device")
#define VCD_DEV_TEXT N_("VCD device")
#define CDAUDIO_DEV_TEXT N_("Audio CD device")

#if defined( _WIN32 ) || defined( __OS2__ )
# define DVD_DEV_LONGTEXT N_( \
    "This is the default DVD drive (or file) to use. Don't forget the colon " \
    "after the drive letter (e.g. D:)")
# define VCD_DEV_LONGTEXT N_( \
    "This is the default VCD drive (or file) to use. Don't forget the colon " \
    "after the drive letter (e.g. D:)")
# define CDAUDIO_DEV_LONGTEXT N_( \
    "This is the default Audio CD drive (or file) to use. Don't forget the " \
    "colon after the drive letter (e.g. D:)")
# define DVD_DEVICE     NULL
# define VCD_DEVICE     "D:"

#else
# define DVD_DEV_LONGTEXT N_( \
    "This is the default DVD device to use.")
# define VCD_DEV_LONGTEXT N_( \
    "This is the default VCD device to use." )
# define CDAUDIO_DEV_LONGTEXT N_( \
    "This is the default Audio CD device to use." )

# if defined(__OpenBSD__)
#  define DVD_DEVICE     "/dev/cd0c"
#  define VCD_DEVICE     "/dev/cd0c"
# elif defined(__linux__)
#  define DVD_DEVICE     "/dev/sr0"
#  define VCD_DEVICE     "/dev/sr0"
# else
#  define DVD_DEVICE     "/dev/dvd"
#  define VCD_DEVICE     "/dev/cdrom"
# endif
#endif

#define TIMEOUT_TEXT N_("TCP connection timeout")
#define TIMEOUT_LONGTEXT N_( \
    "Default TCP connection timeout (in milliseconds)." )

#define HTTP_HOST_TEXT N_( "HTTP server address" )
#define HOST_LONGTEXT N_( \
    "By default, the server will listen on any local IP address. " \
    "Specify an IP address (e.g. ::1 or 127.0.0.1) or a host name " \
    "(e.g. localhost) to restrict them to a specific network interface." )

#define RTSP_HOST_TEXT N_( "RTSP server address" )
#define RTSP_HOST_LONGTEXT N_( \
    "This defines the address the RTSP server will listen on, along " \
    "with the base path of the RTSP VOD media. Syntax is address/path. " \
    "By default, the server will listen on any local IP address. " \
    "Specify an IP address (e.g. ::1 or 127.0.0.1) or a host name " \
    "(e.g. localhost) to restrict them to a specific network interface." )

#define HTTP_PORT_TEXT N_( "HTTP server port" )
#define HTTP_PORT_LONGTEXT N_( \
    "The HTTP server will listen on this TCP port. " \
    "The standard HTTP port number is 80. " \
    "However allocation of port numbers below 1025 is usually restricted " \
    "by the operating system." )

#define HTTPS_PORT_TEXT N_( "HTTPS server port" )
#define HTTPS_PORT_LONGTEXT N_( \
    "The HTTPS server will listen on this TCP port. " \
    "The standard HTTPS port number is 443. " \
    "However allocation of port numbers below 1025 is usually restricted " \
    "by the operating system." )

#define RTSP_PORT_TEXT N_( "RTSP server port" )
#define RTSP_PORT_LONGTEXT N_( \
    "The RTSP server will listen on this TCP port. " \
    "The standard RTSP port number is 554. " \
    "However allocation of port numbers below 1025 is usually restricted " \
    "by the operating system." )

#define HTTP_CERT_TEXT N_("HTTP/TLS server certificate")
#define CERT_LONGTEXT N_( \
   "This X.509 certicate file (PEM format) is used for server-side TLS. " \
   "On OS X, the string is used as a label to search the certificate in the keychain." )

#define HTTP_KEY_TEXT N_("HTTP/TLS server private key")
#define KEY_LONGTEXT N_( \
   "This private key file (PEM format) is used for server-side TLS.")

#define PROXY_TEXT N_("HTTP proxy")
#define PROXY_LONGTEXT N_( \
    "HTTP proxy to be used It must be of the form " \
    "http://[user@]myproxy.mydomain:myport/ ; " \
    "if empty, the http_proxy environment variable will be tried." )

#define PROXY_PASS_TEXT N_("HTTP proxy password")
#define PROXY_PASS_LONGTEXT N_( \
    "If your HTTP proxy requires a password, set it here." )

#define SOCKS_SERVER_TEXT N_("SOCKS server")
#define SOCKS_SERVER_LONGTEXT N_( \
    "SOCKS proxy server to use. This must be of the form " \
    "address:port. It will be used for all TCP connections" )

#define SOCKS_USER_TEXT N_("SOCKS user name")
#define SOCKS_USER_LONGTEXT N_( \
    "User name to be used for connection to the SOCKS proxy." )

#define SOCKS_PASS_TEXT N_("SOCKS password")
#define SOCKS_PASS_LONGTEXT N_( \
    "Password to be used for connection to the SOCKS proxy." )

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

// DEPRECATED
#define CODEC_CAT_LONGTEXT N_( \
    "This option can be used to alter the way VLC selects " \
    "its codecs (decompression methods). Only advanced users should " \
    "alter this option as it can break playback of all your streams." )

#define CODEC_TEXT N_("Preferred decoders list")
#define CODEC_LONGTEXT N_( \
    "List of codecs that VLC will use in " \
    "priority. For instance, 'dummy,a52' will try the dummy and a52 codecs " \
    "before trying the other ones. Only advanced users should " \
    "alter this option as it can break playback of all your streams." )

#define HW_DEC_TEXT N_("Enable hardware decoders")
#define HW_DEC_LONGTEXT N_( \
    "VLC will fallback automatically to software decoders in case of " \
    "hardware decoder failure." )

#define ENCODER_TEXT N_("Preferred encoders list")
#define ENCODER_LONGTEXT N_( \
    "This allows you to select a list of encoders that VLC will use in " \
    "priority.")

#define DEC_DEV_TEXT N_("Preferred decoder hardware device")
#define DEC_DEV_LONGTEXT N_("This allows hardware decoding when available.")

/*****************************************************************************
 * Sout
 ****************************************************************************/

// DEPRECATED
#define SOUT_CAT_LONGTEXT N_( \
    "These options allow you to set default global options for the " \
    "stream output subsystem." )

#define SOUT_TEXT N_("Default stream output chain")
#define SOUT_LONGTEXT N_( \
    "You can enter here a default stream output chain. Refer to "\
    "the documentation to learn how to build such chains. " \
    "Warning: this chain will be enabled for all streams." )

#define SOUT_ALL_TEXT N_("Enable streaming of all ES")
#define SOUT_ALL_LONGTEXT N_( \
    "Stream all elementary streams (video, audio and subtitles)")

#define SOUT_DISPLAY_TEXT N_("Display while streaming")
#define SOUT_DISPLAY_LONGTEXT N_( \
    "Play locally the stream while streaming it.")

#define SOUT_VIDEO_TEXT N_("Enable video stream output")
#define SOUT_VIDEO_LONGTEXT N_( \
    "Choose whether the video stream should be redirected to " \
    "the stream output facility when this last one is enabled.")

#define SOUT_AUDIO_TEXT N_("Enable audio stream output")
#define SOUT_AUDIO_LONGTEXT N_( \
    "Choose whether the audio stream should be redirected to " \
    "the stream output facility when this last one is enabled.")

#define SOUT_SPU_TEXT N_("Enable SPU stream output")
#define SOUT_SPU_LONGTEXT N_( \
    "Choose whether the SPU streams should be redirected to " \
    "the stream output facility when this last one is enabled.")

#define SOUT_KEEP_TEXT N_("Keep stream output open" )
#define SOUT_KEEP_LONGTEXT N_( \
    "This allows you to keep an unique stream output instance across " \
    "multiple playlist item (automatically insert the gather stream output " \
    "if not specified)" )

#define SOUT_MUX_CACHING_TEXT N_("Stream output muxer caching (ms)")
#define SOUT_MUX_CACHING_LONGTEXT N_( \
    "This allow you to configure the initial caching amount for stream output " \
    "muxer. This value should be set in milliseconds." )

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

#define ANN_SAPCTRL_LONGTEXT N_( \
    "If this option is enabled, the flow on " \
    "the SAP multicast address will be controlled. This is needed if you " \
    "want to make announcements on the MBone." )

#define ANN_SAPINTV_TEXT N_("SAP announcement interval")
#define ANN_SAPINTV_LONGTEXT N_( \
    "When the SAP flow control is disabled, " \
    "this lets you set the fixed interval between SAP announcements." )

/*****************************************************************************
 * Advanced
 ****************************************************************************/

// DEPRECATED
#define MISC_CAT_LONGTEXT N_( \
    "These options allow you to select default modules. Leave these " \
    "alone unless you really know what you are doing." )

#define ACCESS_TEXT N_("Access module")
#define ACCESS_LONGTEXT N_( \
    "This allows you to force an access module. You can use it if " \
    "the correct access is not automatically detected. You should not "\
    "set this as a global option unless you really know what you are doing." )

#define STREAM_FILTER_TEXT N_("Stream filter module")
#define STREAM_FILTER_LONGTEXT N_( \
    "Stream filters are used to modify the stream that is being read." )

#define DEMUX_FILTER_TEXT N_("Demux filter module")
#define DEMUX_FILTER_LONGTEXT N_( \
    "Demux filters are used to modify/control the stream that is being read." )

#define DEMUX_TEXT N_("Demux module")
#define DEMUX_LONGTEXT N_( \
    "Demultiplexers are used to separate the \"elementary\" streams " \
    "(like audio and video streams). You can use it if " \
    "the correct demuxer is not automatically detected. You should not "\
    "set this as a global option unless you really know what you are doing." )

#define VOD_SERVER_TEXT N_("VoD server module")
#define VOD_SERVER_LONGTEXT N_( \
    "You can select which VoD server module you want to use. Set this " \
    "to 'vod_rtsp' to switch back to the old, legacy module." )

#define USE_STREAM_IMMEDIATE_LONGTEXT N_( \
     "This option is useful if you want to lower the latency when " \
     "reading a stream")

#define VLM_CONF_TEXT N_("VLM configuration file")
#define VLM_CONF_LONGTEXT N_( \
    "Read a VLM configuration file as soon as VLM is started." )

#define PLUGINS_CACHE_TEXT N_("Use a plugins cache")
#define PLUGINS_CACHE_LONGTEXT N_( \
    "Use a plugins cache which will greatly improve the startup time of VLC.")

#define PLUGINS_SCAN_TEXT N_("Scan for new plugins")
#define PLUGINS_SCAN_LONGTEXT N_( \
    "Scan plugin directories for new plugins at startup. " \
    "This increases the startup time of VLC.")

#define KEYSTORE_TEXT N_("Preferred keystore list")
#define KEYSTORE_LONGTEXT N_( \
    "List of keystores that VLC will use in priority." )

#define STATS_TEXT N_("Locally collect statistics")
#define STATS_LONGTEXT N_( \
     "Collect miscellaneous local statistics about the playing media.")

#define ONEINSTANCE_TEXT N_("Allow only one running instance")
#define ONEINSTANCE_LONGTEXT N_( \
    "Allowing only one running instance of VLC can sometimes be useful, " \
    "for example if you associated VLC with some media types and you " \
    "don't want a new instance of VLC to be opened each time you " \
    "open a file in your file manager. This option will allow you " \
    "to play the file with the already running instance or enqueue it.")

#define STARTEDFROMFILE_TEXT N_("VLC is started from file association")
#define STARTEDFROMFILE_LONGTEXT N_( \
    "Tell VLC that it is being launched due to a file association in the OS" )

#define ONEINSTANCEWHENSTARTEDFROMFILE_TEXT N_( \
    "Use only one instance when started from file manager")

#define HPRIORITY_TEXT N_("Increase the priority of the process")
#define HPRIORITY_LONGTEXT N_( \
    "Increasing the priority of the process will very likely improve your " \
    "playing experience as it allows VLC not to be disturbed by other " \
    "applications that could otherwise take too much processor time. " \
    "However be advised that in certain circumstances (bugs) VLC could take " \
    "all the processor time and render the whole system unresponsive which " \
    "might require a reboot of your machine.")

#define CLOCK_SOURCE_TEXT N_("Clock source")
#ifdef _WIN32
static const char *const clock_sources[] = {
    "", "interrupt", "tick",
#if !VLC_WINSTORE_APP
    "multimedia",
#endif
    "perf", "wall",
};

static const char *const clock_sources_text[] = {
    N_("Auto"), "Interrupt time", "Windows time",
#if !VLC_WINSTORE_APP
    "Multimedia timers",
#endif
    "Performance counters", "System time (DANGEROUS!)",
};
#endif

#define PLAYLISTENQUEUE_TEXT N_( \
    "Enqueue items into playlist in one instance mode")
#define PLAYLISTENQUEUE_LONGTEXT N_( \
    "When using the one instance only option, enqueue items to playlist " \
    "and keep playing current item.")

#define DBUS_TEXT N_("Expose media player via D-Bus")
#define DBUS_LONGTEXT N_("Allow other applications to control VLC " \
    "using the D-Bus MPRIS protocol.")

/*****************************************************************************
 * Playlist
 ****************************************************************************/

// DEPRECATED
#define PLAYLIST_CAT_LONGTEXT N_( \
     "These options define the behavior of the playlist. Some " \
     "of them can be overridden in the playlist dialog box." )

#define PREPARSE_TEXT N_( "Automatically preparse items")
#define PREPARSE_LONGTEXT N_( \
    "Automatically preparse items added to the playlist " \
    "(to retrieve some metadata)." )

#define PREPARSE_TIMEOUT_TEXT N_( "Preparsing timeout" )
#define PREPARSE_TIMEOUT_LONGTEXT N_( \
    "Maximum time allowed to preparse an item, in milliseconds" )

#define PREPARSE_THREADS_TEXT N_( "Preparsing threads" )
#define PREPARSE_THREADS_LONGTEXT N_( \
    "Maximum number of threads used to preparse items" )

#define FETCH_ART_THREADS_TEXT N_( "Fetch-art threads" )
#define FETCH_ART_THREADS_LONGTEXT N_( \
    "Maximum number of threads used to fetch art" )

#define METADATA_NETWORK_TEXT N_( "Allow metadata network access" )

static const char *const psz_recursive_list[] = {
    "none", "collapse", "expand" };
static const char *const psz_recursive_list_text[] = {
    N_("None"), N_("Collapse"), N_("Expand") };

#define RECURSIVE_TEXT N_("Subdirectory behavior")
#define RECURSIVE_LONGTEXT N_( \
        "Select whether subdirectories must be expanded.\n" \
        "none: subdirectories do not appear in the playlist.\n" \
        "collapse: subdirectories appear but are expanded on first play.\n" \
        "expand: all subdirectories are expanded.\n" )

#define IGNORE_TEXT N_("Ignored extensions")
#define IGNORE_LONGTEXT N_( \
        "Files with these extensions will not be added to playlist when " \
        "opening a directory.\n" \
        "This is useful if you add directories that contain playlist files " \
        "for instance. Use a comma-separated list of extensions." )

#define SHOW_HIDDENFILES_TEXT N_("Show hidden files")
#define SHOW_HIDDENFILES_LONGTEXT N_( \
        "Ignore files starting with '.'" )

#define SD_TEXT N_( "Services discovery modules")
#define SD_LONGTEXT N_( \
     "Specifies the services discovery modules to preload, separated by " \
     "colons. Typical value is \"sap\"." )

#define RANDOM_TEXT N_("Play files randomly forever")
#define RANDOM_LONGTEXT N_( \
    "VLC will randomly play files in the playlist until interrupted.")

#define LOOP_TEXT N_("Repeat all")
#define LOOP_LONGTEXT N_( \
    "VLC will keep playing the playlist indefinitely." )

#define REPEAT_TEXT N_("Repeat current item")
#define REPEAT_LONGTEXT N_( \
    "VLC will keep playing the current playlist item." )

#define PAS_TEXT N_("Play and stop")
#define PAS_LONGTEXT N_( \
    "Stop the playlist after each played playlist item." )

#define PAE_TEXT N_("Play and exit")
#define PAE_LONGTEXT N_( \
    "Exit if there are no more items in the playlist." )

#define PAP_TEXT N_("Play and pause")
#define PAP_LONGTEXT N_( \
    "Pause each item in the playlist on the last frame." )

#define SP_TEXT N_("Start paused")
#define SP_LONGTEXT N_( \
    "Pause each item in the playlist on the first frame." )

#define AUTOSTART_TEXT N_( "Auto start" )
#define AUTOSTART_LONGTEXT N_( "Automatically start playing the playlist " \
                "content once it's loaded." )

#define CORK_TEXT N_("Pause on audio communication")
#define CORK_LONGTEXT N_( \
    "If pending audio communication is detected, playback will be paused " \
    "automatically." )

#define ML_TEXT N_("Use media library")
#define ML_LONGTEXT N_( \
    "The media library is automatically saved and reloaded each time you " \
    "start VLC." )

#define PLTREE_TEXT N_("Display playlist tree")
#define PLTREE_LONGTEXT N_( \
    "The playlist can use a tree to categorize some items, like the " \
    "contents of a directory." )


/*****************************************************************************
 * Hotkeys
 ****************************************************************************/

// DEPRECATED
#define HOTKEY_CAT_LONGTEXT N_( "These settings are the global VLC key " \
    "bindings, known as \"hotkeys\"." )

static const int mouse_wheel_values[] = { -1, 0, 2, 3, };
static const char *const mouse_wheel_texts[] = {
    N_("Ignore"), N_("Volume control"),
    N_("Position control"), N_("Position control reversed"),
};

#define MOUSE_Y_WHEEL_MODE_TEXT N_("Mouse wheel vertical axis control")
#define MOUSE_Y_WHEEL_MODE_LONGTEXT N_( \
   "The mouse wheel vertical (up/down) axis can control volume, " \
   "position or be ignored.")
#define MOUSE_X_WHEEL_MODE_TEXT N_("Mouse wheel horizontal axis control")
#define MOUSE_X_WHEEL_MODE_LONGTEXT N_( \
   "The mouse wheel horizontal (left/right) axis can control volume, " \
   "position or be ignored.")
#define TOGGLE_FULLSCREEN_KEY_TEXT N_("Fullscreen")
#define TOGGLE_FULLSCREEN_KEY_LONGTEXT N_("Select the hotkey to use to swap fullscreen state.")
#define LEAVE_FULLSCREEN_KEY_TEXT N_("Exit fullscreen")
#define LEAVE_FULLSCREEN_KEY_LONGTEXT N_("Select the hotkey to use to exit fullscreen state.")
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
#define RATE_NORMAL_KEY_TEXT N_("Normal rate")
#define RATE_NORMAL_KEY_LONGTEXT N_("Select the hotkey to set the playback rate back to normal.")
#define RATE_FASTER_FINE_KEY_TEXT N_("Faster (fine)")
#define RATE_FASTER_FINE_KEY_LONGTEXT N_("Select the hotkey to use for fast forward playback.")
#define RATE_SLOWER_FINE_KEY_TEXT N_("Slower (fine)")
#define RATE_SLOWER_FINE_KEY_LONGTEXT N_("Select the hotkey to use for slow motion playback.")
#define NEXT_KEY_TEXT N_("Next")
#define NEXT_KEY_LONGTEXT N_("Select the hotkey to use to skip to the next item in the playlist.")
#define PREV_KEY_TEXT N_("Previous")
#define PREV_KEY_LONGTEXT N_("Select the hotkey to use to skip to the previous item in the playlist.")
#define STOP_KEY_TEXT N_("Stop")
#define STOP_KEY_LONGTEXT N_("Select the hotkey to stop playback.")
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
#define FRAME_NEXT_KEY_TEXT N_("Next frame")
#define FRAME_NEXT_KEY_LONGTEXT \
    N_("Select the hotkey to got to the next video frame.")

#define JIEXTRASHORT_TEXT N_("Very short jump length")
#define JIEXTRASHORT_LONGTEXT N_("Very short jump length, in seconds.")
#define JISHORT_TEXT N_("Short jump length")
#define JISHORT_LONGTEXT N_("Short jump length, in seconds.")
#define JIMEDIUM_TEXT N_("Medium jump length")
#define JIMEDIUM_LONGTEXT N_("Medium jump length, in seconds.")
#define JILONG_TEXT N_("Long jump length")
#define JILONG_LONGTEXT N_("Long jump length, in seconds.")

#define QUIT_KEY_TEXT N_("Quit")
#define QUIT_KEY_LONGTEXT N_("Select the hotkey to quit the application.")
#define NAV_UP_KEY_TEXT N_("Navigate up")
#define NAV_UP_KEY_LONGTEXT N_("Select the key to move the selector up in DVD menus / Move viewpoint to up (pitch).")
#define NAV_DOWN_KEY_TEXT N_("Navigate down")
#define NAV_DOWN_KEY_LONGTEXT N_("Select the key to move the selector down in DVD menus / Move viewpoint to down (pitch).")
#define NAV_LEFT_KEY_TEXT N_("Navigate left")
#define NAV_LEFT_KEY_LONGTEXT N_("Select the key to move the selector left in DVD menus / Move viewpoint to left (yaw).")
#define NAV_RIGHT_KEY_TEXT N_("Navigate right")
#define NAV_RIGHT_KEY_LONGTEXT N_("Select the key to move the selector right in DVD menus / Move viewpoint to right (yaw).")
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
#define VOL_MUTE_KEY_LONGTEXT N_("Select the key to mute audio.")
#define SUBDELAY_UP_KEY_TEXT N_("Subtitle delay up")
#define SUBDELAY_UP_KEY_LONGTEXT N_("Select the key to increase the subtitle delay.")
#define SUBDELAY_DOWN_KEY_TEXT N_("Subtitle delay down")
#define SUBDELAY_DOWN_KEY_LONGTEXT N_("Select the key to decrease the subtitle delay.")
#define SUBTEXT_SCALE_KEY_TEXT     N_("Reset subtitles text scale")
#define SUBTEXT_SCALEDOWN_KEY_TEXT N_("Scale up subtitles text")
#define SUBTEXT_SCALEUP_KEY_TEXT   N_("Scale down subtitles text")
#define SUBTEXT_SCALE_KEY_LONGTEXT N_("Select the key to change subtitles text scaling")
#define SUBSYNC_MARKAUDIO_KEY_TEXT N_("Subtitle sync / bookmark audio timestamp")
#define SUBSYNC_MARKAUDIO_KEY_LONGTEXT N_("Select the key to bookmark audio timestamp when syncing subtitles.")
#define SUBSYNC_MARKSUB_KEY_TEXT N_("Subtitle sync / bookmark subtitle timestamp")
#define SUBSYNC_MARKSUB_KEY_LONGTEXT N_("Select the key to bookmark subtitle timestamp when syncing subtitles.")
#define SUBSYNC_APPLY_KEY_TEXT N_("Subtitle sync / synchronize audio & subtitle timestamps")
#define SUBSYNC_APPLY_KEY_LONGTEXT N_("Select the key to synchronize bookmarked audio & subtitle timestamps.")
#define SUBSYNC_RESET_KEY_TEXT N_("Subtitle sync / reset audio & subtitle synchronization")
#define SUBSYNC_RESET_KEY_LONGTEXT N_("Select the key to reset synchronization of audio & subtitle timestamps.")
#define SUBPOS_UP_KEY_TEXT N_("Subtitle position up")
#define SUBPOS_UP_KEY_LONGTEXT N_("Select the key to move subtitles higher.")
#define SUBPOS_DOWN_KEY_TEXT N_("Subtitle position down")
#define SUBPOS_DOWN_KEY_LONGTEXT N_("Select the key to move subtitles lower.")
#define AUDIODELAY_UP_KEY_TEXT N_("Audio delay up")
#define AUDIODELAY_UP_KEY_LONGTEXT N_("Select the key to increase the audio delay.")
#define AUDIODELAY_DOWN_KEY_TEXT N_("Audio delay down")
#define AUDIODELAY_DOWN_KEY_LONGTEXT N_("Select the key to decrease the audio delay.")

#define ZOOM_QUARTER_KEY_TEXT N_("1:4 Quarter")
#define ZOOM_HALF_KEY_TEXT N_("1:2 Half")
#define ZOOM_ORIGINAL_KEY_TEXT N_("1:1 Original")
#define ZOOM_DOUBLE_KEY_TEXT N_("2:1 Double")

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
#define PLAY_CLEAR_KEY_TEXT N_("Clear the playlist")
#define PLAY_CLEAR_KEY_LONGTEXT N_("Select the key to clear the current playlist.")

#define BOOKMARK1_TEXT N_("Playlist bookmark 1")
#define BOOKMARK2_TEXT N_("Playlist bookmark 2")
#define BOOKMARK3_TEXT N_("Playlist bookmark 3")
#define BOOKMARK4_TEXT N_("Playlist bookmark 4")
#define BOOKMARK5_TEXT N_("Playlist bookmark 5")
#define BOOKMARK6_TEXT N_("Playlist bookmark 6")
#define BOOKMARK7_TEXT N_("Playlist bookmark 7")
#define BOOKMARK8_TEXT N_("Playlist bookmark 8")
#define BOOKMARK9_TEXT N_("Playlist bookmark 9")
#define BOOKMARK10_TEXT N_("Playlist bookmark 10")
#define BOOKMARK_LONGTEXT N_( \
      "This allows you to define playlist bookmarks.")

#define AUDIO_TRACK_KEY_TEXT N_("Cycle audio track")
#define AUDIO_TRACK_KEY_LONGTEXT N_("Cycle through the available audio tracks(languages).")
#define SUBTITLE_REVERSE_TRACK_KEY_TEXT N_("Cycle subtitle track in reverse order")
#define SUBTITLE_REVERSE_TRACK_KEY_LONGTEXT N_("Cycle through the available subtitle tracks in reverse order.")
#define SUBTITLE_TRACK_KEY_TEXT N_("Cycle subtitle track")
#define SUBTITLE_TRACK_KEY_LONGTEXT N_("Cycle through the available subtitle tracks.")
#define SUBTITLE_TOGGLE_KEY_TEXT N_("Toggle subtitles")
#define SUBTITLE_TOGGLE_KEY_LONGTEXT N_("Toggle subtitle track visibility.")
#define SUBTITLE_CONTROL_SECONDARY_KEY_TEXT N_("Toggle secondary subtitle control")
#define SUBTITLE_CONTROL_SECONDARY_KEY_LONGTEXT N_("Use original subtitle controls to manage secondary subtitles.")
#define PROGRAM_SID_NEXT_KEY_TEXT N_("Cycle next program Service ID")
#define PROGRAM_SID_NEXT_KEY_LONGTEXT N_("Cycle through the available next program Service IDs (SIDs).")
#define PROGRAM_SID_PREV_KEY_TEXT N_("Cycle previous program Service ID")
#define PROGRAM_SID_PREV_KEY_LONGTEXT N_("Cycle through the available previous program Service IDs (SIDs).")
#define ASPECT_RATIO_KEY_TEXT N_("Cycle source aspect ratio")
#define ASPECT_RATIO_KEY_LONGTEXT N_("Cycle through a predefined list of source aspect ratios.")
#define CROP_KEY_TEXT N_("Cycle video crop")
#define CROP_KEY_LONGTEXT N_("Cycle through a predefined list of crop formats.")
#define TOGGLE_AUTOSCALE_KEY_TEXT N_("Toggle autoscaling")
#define TOGGLE_AUTOSCALE_KEY_LONGTEXT N_("Activate or deactivate autoscaling.")
#define SCALE_UP_KEY_TEXT N_("Increase scale factor")
#define SCALE_UP_KEY_LONGTEXT SCALE_UP_KEY_TEXT
#define SCALE_DOWN_KEY_TEXT N_("Decrease scale factor")
#define SCALE_DOWN_KEY_LONGTEXT SCALE_DOWN_KEY_TEXT
#define DEINTERLACE_KEY_TEXT N_("Toggle deinterlacing")
#define DEINTERLACE_KEY_LONGTEXT N_("Activate or deactivate deinterlacing.")
#define DEINTERLACE_MODE_KEY_TEXT N_("Cycle deinterlace modes")
#define DEINTERLACE_MODE_KEY_LONGTEXT N_("Cycle through available deinterlace modes.")
#define INTF_TOGGLE_FSC_KEY_TEXT N_("Show controller in fullscreen")
#define INTF_BOSS_KEY_TEXT N_("Boss key")
#define INTF_BOSS_KEY_LONGTEXT N_("Hide the interface and pause playback.")
#define INTF_POPUP_MENU_KEY_TEXT N_("Context menu")
#define INTF_POPUP_MENU_KEY_LONGTEXT N_("Show the contextual popup menu.")
#define SNAP_KEY_TEXT N_("Take video snapshot")
#define SNAP_KEY_LONGTEXT N_("Takes a video snapshot and writes it to disk.")

#define RECORD_KEY_TEXT N_("Record")
#define RECORD_KEY_LONGTEXT N_("Record access filter start/stop.")

#define LOOP_KEY_TEXT N_("Normal/Loop/Repeat")
#define LOOP_KEY_LONGTEXT N_("Toggle Normal/Loop/Repeat playlist modes")

#define RANDOM_KEY_TEXT N_("Random")
#define RANDOM_KEY_LONGTEXT N_("Toggle random playlist playback")

#define ZOOM_KEY_TEXT N_("Zoom")
#define ZOOM_KEY_LONGTEXT N_("Zoom")

#define UNZOOM_KEY_TEXT N_("Un-Zoom")
#define UNZOOM_KEY_LONGTEXT N_("Un-Zoom")

#define CROP_TOP_KEY_TEXT N_("Crop one pixel from the top of the video")
#define CROP_TOP_KEY_LONGTEXT N_("Crop one pixel from the top of the video")
#define UNCROP_TOP_KEY_TEXT N_("Uncrop one pixel from the top of the video")
#define UNCROP_TOP_KEY_LONGTEXT N_("Uncrop one pixel from the top of the video")

#define CROP_LEFT_KEY_TEXT N_("Crop one pixel from the left of the video")
#define CROP_LEFT_KEY_LONGTEXT N_("Crop one pixel from the left of the video")
#define UNCROP_LEFT_KEY_TEXT N_("Uncrop one pixel from the left of the video")
#define UNCROP_LEFT_KEY_LONGTEXT N_("Uncrop one pixel from the left of the video")

#define CROP_BOTTOM_KEY_TEXT N_("Crop one pixel from the bottom of the video")
#define CROP_BOTTOM_KEY_LONGTEXT N_("Crop one pixel from the bottom of the video")
#define UNCROP_BOTTOM_KEY_TEXT N_("Uncrop one pixel from the bottom of the video")
#define UNCROP_BOTTOM_KEY_LONGTEXT N_("Uncrop one pixel from the bottom of the video")

#define CROP_RIGHT_KEY_TEXT N_("Crop one pixel from the right of the video")
#define CROP_RIGHT_KEY_LONGTEXT N_("Crop one pixel from the right of the video")
#define UNCROP_RIGHT_KEY_TEXT N_("Uncrop one pixel from the right of the video")
#define UNCROP_RIGHT_KEY_LONGTEXT N_("Uncrop one pixel from the right of the video")

/* 360° Viewpoint */
#define VIEWPOINT_FOV_IN_KEY_TEXT N_("Shrink the viewpoint field of view (360°)")
#define VIEWPOINT_FOV_OUT_KEY_TEXT N_("Expand the viewpoint field of view (360°)")
#define VIEWPOINT_ROLL_CLOCK_KEY_TEXT N_("Roll the viewpoint clockwise (360°)")
#define VIEWPOINT_ROLL_ANTICLOCK_KEY_TEXT N_("Roll the viewpoint anti-clockwise (360°)")

#define WALLPAPER_KEY_TEXT N_("Toggle wallpaper mode in video output")
#define WALLPAPER_KEY_LONGTEXT N_( \
    "Toggle wallpaper mode in video output." )

#define AUDIO_DEVICE_CYCLE_KEY_TEXT N_("Cycle through audio devices")
#define AUDIO_DEVICE_CYCLE_KEY_LONGTEXT N_("Cycle through available audio devices")

/*
 * Quick usage guide for the configuration options:
 *
 * add_category_hint(N_(text), N_(longtext))
 * add_usage_hint( N_(text), b_advanced_option )
 * add_string( option_name, value, N_(text), N_(longtext),
               b_advanced_option )
 * add_loadfile( option_name, psz_value, N_(text), N_(longtext) )
 * add_savefile( option_name, psz_value, N_(text), N_(longtext) )
 * add_module( option_name, psz_value, i_capability,
 *             N_(text), N_(longtext) )
 * add_integer( option_name, i_value, N_(text), N_(longtext),
 *              b_advanced_option )
 * add_bool( option_name, b_value, N_(text), N_(longtext),
 *           b_advanced_option )
 */

vlc_module_begin ()
/* Audio options */
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_GENERAL )
    add_category_hint(N_("Audio"), AOUT_CAT_LONGTEXT)

    add_bool( "audio", 1, AUDIO_TEXT, AUDIO_LONGTEXT, false )
        change_safe ()
    add_float( "gain", 1., GAIN_TEXT, GAIN_LONGTEXT, true )
        change_float_range( 0., 8. )
    add_obsolete_integer( "volume" ) /* since 2.1.0 */
    add_float( "volume-step", AOUT_VOLUME_STEP, VOLUME_STEP_TEXT,
                 VOLUME_STEP_LONGTEXT, true )
        change_float_range( 1., AOUT_VOLUME_DEFAULT )
    add_bool( "volume-save", true, VOLUME_SAVE_TEXT, VOLUME_SAVE_TEXT, true )
    add_obsolete_integer( "aout-rate" ) /* since 2.0.0 */
    add_obsolete_bool( "hq-resampling" ) /* since 1.1.8 */
#if defined(__ANDROID__) || defined(__APPLE__) || defined(_WIN32)
    add_bool( "spdif", false, SPDIF_TEXT, SPDIF_LONGTEXT, true )
#else
    add_obsolete_bool("spdif") /* since 4.0.0 */
#endif
    add_integer( "force-dolby-surround", 0, FORCE_DOLBY_TEXT,
                 FORCE_DOLBY_LONGTEXT, false )
        change_integer_list( pi_force_dolby_values, ppsz_force_dolby_descriptions )
    add_integer( "stereo-mode", 0, STEREO_MODE_TEXT, STEREO_MODE_TEXT, true )
        change_integer_list( pi_stereo_mode_values, ppsz_stereo_mode_texts )
    add_integer( "audio-desync", 0, DESYNC_TEXT,
                 DESYNC_LONGTEXT, true )
        change_safe ()

    /* FIXME TODO create a subcat replay gain ? */
    add_string( "audio-replay-gain-mode", ppsz_replay_gain_mode[0], AUDIO_REPLAY_GAIN_MODE_TEXT,
                AUDIO_REPLAY_GAIN_MODE_LONGTEXT, false )
        change_string_list( ppsz_replay_gain_mode, ppsz_replay_gain_mode_text )
    add_float( "audio-replay-gain-preamp", 0.0,
               AUDIO_REPLAY_GAIN_PREAMP_TEXT, AUDIO_REPLAY_GAIN_PREAMP_LONGTEXT, false )
    add_float( "audio-replay-gain-default", -7.0,
               AUDIO_REPLAY_GAIN_DEFAULT_TEXT, AUDIO_REPLAY_GAIN_DEFAULT_LONGTEXT, false )
    add_bool( "audio-replay-gain-peak-protection", true,
              AUDIO_REPLAY_GAIN_PEAK_PROTECTION_TEXT, AUDIO_REPLAY_GAIN_PEAK_PROTECTION_LONGTEXT, true )

    add_bool( "audio-time-stretch", true,
              AUDIO_TIME_STRETCH_TEXT, AUDIO_TIME_STRETCH_LONGTEXT, false )

    set_subcategory( SUBCAT_AUDIO_AOUT )
    add_module("aout", "audio output", NULL, AOUT_TEXT, AOUT_LONGTEXT)
        change_short('A')
    add_string( "role", "video", ROLE_TEXT, ROLE_LONGTEXT, true )
        change_string_list( ppsz_roles, ppsz_roles_text )

    set_subcategory( SUBCAT_AUDIO_AFILTER )
        add_bool( "audio-bitexact", false, AUDIO_BITEXACT_TEXT,
                   AUDIO_BITEXACT_LONGTEXT, false )
    add_module_list("audio-filter", "audio filter", NULL,
                    AUDIO_FILTER_TEXT, AUDIO_FILTER_LONGTEXT)
    set_subcategory( SUBCAT_AUDIO_VISUAL )
    add_module("audio-visual", "visualization", "none",
               AUDIO_VISUAL_TEXT, AUDIO_VISUAL_LONGTEXT)

    set_subcategory( SUBCAT_AUDIO_RESAMPLER )
    add_module("audio-resampler", "audio resampler", NULL,
               AUDIO_RESAMPLER_TEXT, AUDIO_RESAMPLER_LONGTEXT)


/* Video options */
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_GENERAL )
    add_category_hint(N_("Video"), VOUT_CAT_LONGTEXT)

    add_bool( "video", 1, VIDEO_TEXT, VIDEO_LONGTEXT, true )
        change_safe ()
    add_bool( "grayscale", 0, GRAYSCALE_TEXT,
              GRAYSCALE_LONGTEXT, true )
    add_bool( "fullscreen", false, FULLSCREEN_TEXT, FULLSCREEN_LONGTEXT, false )
        change_short('f')
        change_safe ()
    add_bool( "embedded-video", 1, EMBEDDED_TEXT, EMBEDDED_LONGTEXT,
              true )
    add_bool( "xlib", true, "", "", true )
        change_private ()
    add_bool( "drop-late-frames", 1, DROP_LATE_FRAMES_TEXT,
              DROP_LATE_FRAMES_LONGTEXT, true )
    /* Used in vout_synchro */
    add_bool( "skip-frames", 1, SKIP_FRAMES_TEXT,
              SKIP_FRAMES_LONGTEXT, true )
    add_bool( "quiet-synchro", 0, QUIET_SYNCHRO_TEXT,
              QUIET_SYNCHRO_LONGTEXT, true )
    add_bool( "keyboard-events", true, KEYBOARD_EVENTS_TEXT,
              KEYBOARD_EVENTS_LONGTEXT, true )
    add_bool( "mouse-events", true, MOUSE_EVENTS_TEXT,
              MOUSE_EVENTS_LONGTEXT, true )
    add_obsolete_integer( "vout-event" ) /* deprecated since 1.1.0 */
    add_obsolete_integer( "x11-event" ) /* renamed since 1.0.0 */
    add_bool( "video-on-top", 0, VIDEO_ON_TOP_TEXT,
              VIDEO_ON_TOP_LONGTEXT, false )
    add_bool( "video-wallpaper", false, WALLPAPER_TEXT,
              WALLPAPER_LONGTEXT, false )
    add_integer("disable-screensaver", 1, SS_TEXT, SS_LONGTEXT, true)
        change_integer_list(screensaver_values, screensaver_texts)

    add_bool( "video-title-show", 1, VIDEO_TITLE_SHOW_TEXT,
              VIDEO_TITLE_SHOW_LONGTEXT, false )
        change_safe()
    add_integer( "video-title-timeout", 5000, VIDEO_TITLE_TIMEOUT_TEXT,
                 VIDEO_TITLE_TIMEOUT_LONGTEXT, false )
        change_safe()
    add_integer( "video-title-position", 8, VIDEO_TITLE_POSITION_TEXT,
                 VIDEO_TITLE_POSITION_LONGTEXT, false )
        change_safe()
        change_integer_list( pi_pos_values, ppsz_pos_descriptions )
    // autohide after 1 second
    add_integer( "mouse-hide-timeout", 1000, MOUSE_HIDE_TIMEOUT_TEXT,
                 MOUSE_HIDE_TIMEOUT_LONGTEXT, false )
    set_section( N_("Snapshot") , NULL )
    add_directory("snapshot-path", NULL, SNAP_PATH_TEXT, SNAP_PATH_LONGTEXT)
    add_string( "snapshot-prefix", "vlcsnap-", SNAP_PREFIX_TEXT,
                   SNAP_PREFIX_LONGTEXT, false )
    add_string( "snapshot-format", "png", SNAP_FORMAT_TEXT,
                   SNAP_FORMAT_LONGTEXT, false )
        change_string_list( ppsz_snap_formats, ppsz_snap_formats )
    add_bool( "snapshot-preview", true, SNAP_PREVIEW_TEXT,
              SNAP_PREVIEW_LONGTEXT, false )
    add_bool( "snapshot-sequential", false, SNAP_SEQUENTIAL_TEXT,
              SNAP_SEQUENTIAL_LONGTEXT, false )
    add_integer( "snapshot-width", -1, SNAP_WIDTH_TEXT,
                 SNAP_WIDTH_LONGTEXT, true )
    add_integer( "snapshot-height", -1, SNAP_HEIGHT_TEXT,
                 SNAP_HEIGHT_LONGTEXT, true )

    set_section( N_("Window properties" ), NULL )
    add_integer( "width", -1, WIDTH_TEXT, WIDTH_LONGTEXT, true )
        change_safe ()
    add_integer( "height", -1, HEIGHT_TEXT, HEIGHT_LONGTEXT, true )
        change_safe ()
#if defined(__APPLE__) || defined(_WIN32)
    add_integer( "video-x", 0, VIDEOX_TEXT, VIDEOX_LONGTEXT, true )
        change_safe ()
    add_integer( "video-y", 0, VIDEOY_TEXT, VIDEOY_LONGTEXT, true )
        change_safe ()
#endif
    add_string( "crop", NULL, CROP_TEXT, CROP_LONGTEXT, false )
        change_safe ()
    add_string( "custom-crop-ratios", NULL, CUSTOM_CROP_RATIOS_TEXT,
                CUSTOM_CROP_RATIOS_LONGTEXT, false )
    add_string( "aspect-ratio", NULL,
                ASPECT_RATIO_TEXT, ASPECT_RATIO_LONGTEXT, false )
        change_safe ()
    add_bool( "autoscale", true, AUTOSCALE_TEXT, AUTOSCALE_LONGTEXT, false )
        change_safe ()
    add_obsolete_float( "scale" ) /* since 3.0.0 */
    add_string( "monitor-par", NULL,
                MASPECT_RATIO_TEXT, MASPECT_RATIO_LONGTEXT, true )
    add_string( "custom-aspect-ratios", NULL, CUSTOM_ASPECT_RATIOS_TEXT,
                CUSTOM_ASPECT_RATIOS_LONGTEXT, false )
    add_bool( "hdtv-fix", 1, HDTV_FIX_TEXT, HDTV_FIX_LONGTEXT, true )
    add_bool( "video-deco", 1, VIDEO_DECO_TEXT,
              VIDEO_DECO_LONGTEXT, true )
    add_string( "video-title", NULL, VIDEO_TITLE_TEXT,
                 VIDEO_TITLE_LONGTEXT, true )
    add_integer( "align", 0, ALIGN_TEXT, ALIGN_LONGTEXT, true )
        change_integer_list( pi_align_values, ppsz_align_descriptions )
    add_float( "zoom", 1., ZOOM_TEXT, ZOOM_LONGTEXT, true )
        change_safe()
    add_integer( "deinterlace", -1,
                 DEINTERLACE_TEXT, DEINTERLACE_LONGTEXT, false )
        change_integer_list( pi_deinterlace, ppsz_deinterlace_text )
        change_safe()
    add_string( "deinterlace-mode", "auto",
                DEINTERLACE_MODE_TEXT, DEINTERLACE_MODE_LONGTEXT, false )
        change_string_list( ppsz_deinterlace_mode, ppsz_deinterlace_mode_text )
        change_safe()

    set_subcategory( SUBCAT_VIDEO_VOUT )
    add_module("vout", "vout display", NULL, VOUT_TEXT, VOUT_LONGTEXT)
        change_short('V')

    set_subcategory( SUBCAT_VIDEO_VFILTER )
    add_module_list("video-filter", "video filter", NULL,
                    VIDEO_FILTER_TEXT, VIDEO_FILTER_LONGTEXT)

#if 0
    add_string( "pixel-ratio", "1", PIXEL_RATIO_TEXT, PIXEL_RATIO_TEXT )
#endif

/* Subpictures options */
    set_subcategory( SUBCAT_VIDEO_SUBPIC )
    set_section( N_("On Screen Display") , NULL )
    add_category_hint(N_("Subpictures"), SUB_CAT_LONGTEXT)

    add_bool( "spu", 1, SPU_TEXT, SPU_LONGTEXT, false )
        change_safe ()
    add_bool( "osd", 1, OSD_TEXT, OSD_LONGTEXT, false )
    add_module("text-renderer", "text renderer", NULL,
               TEXTRENDERER_TEXT, TEXTRENDERER_LONGTEXT)

    set_section( N_("Subtitles") , NULL )
    add_float( "sub-fps", 0.0, SUB_FPS_TEXT, SUB_FPS_LONGTEXT, false )
    add_integer( "sub-delay", 0, SUB_DELAY_TEXT, SUB_DELAY_LONGTEXT, false )
    add_loadfile("sub-file", NULL, SUB_FILE_TEXT, SUB_FILE_LONGTEXT)
        change_safe()
    add_bool( "sub-autodetect-file", true,
                 SUB_AUTO_TEXT, SUB_AUTO_LONGTEXT, false )
    add_integer( "sub-autodetect-fuzzy", 3,
                 SUB_FUZZY_TEXT, SUB_FUZZY_LONGTEXT, true )
#if defined( _WIN32 ) || defined( __OS2__ )
#   define SUB_PATH ".\\subtitles, .\\subs"
#else
#   define SUB_PATH "./Subtitles, ./subtitles, ./Subs, ./subs"
#endif
    add_string( "sub-autodetect-path", SUB_PATH,
                 SUB_PATH_TEXT, SUB_PATH_LONGTEXT, true )
    add_integer( "sub-margin", 0, SUB_MARGIN_TEXT,
                 SUB_MARGIN_LONGTEXT, true )
    add_integer_with_range( "sub-text-scale", 100, 10, 500,
               SUB_TEXT_SCALE_TEXT, SUB_TEXT_SCALE_LONGTEXT, false )
    set_section( N_( "Overlays" ) , NULL )
    add_module_list("sub-source", "sub source", NULL,
                    SUB_SOURCE_TEXT, SUB_SOURCE_LONGTEXT)
    add_module_list("sub-filter", "sub filter", NULL,
                    SUB_FILTER_TEXT, SUB_FILTER_LONGTEXT)

    set_section( N_( "Multiple Subtitles" ) , NULL )
    add_integer( "secondary-sub-alignment", -1, SECONDARY_SUB_POSITION_TEXT,
                 SECONDARY_SUB_POSITION_LONGTEXT, false )
        change_integer_list( pi_sub_align_values, ppsz_sub_align_descriptions )
    /* Push the secondary subtitles up a bit so they won't overlap with
       the primary subtitles using the default settings.*/
    add_integer( "secondary-sub-margin", 100, SECONDARY_SUB_MARGIN_TEXT,
                 SECONDARY_SUB_MARGIN_LONGTEXT, true )

/* Input options */
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_GENERAL )

    set_section( N_( "Track settings" ), NULL )
    add_integer( "program", 0,
                 INPUT_PROGRAM_TEXT, INPUT_PROGRAM_LONGTEXT, true )
        change_safe ()
    add_string( "programs", "",
                INPUT_PROGRAMS_TEXT, INPUT_PROGRAMS_LONGTEXT, true )
        change_safe ()
    add_integer( "video-track", -1,
                 INPUT_VIDEOTRACK_TEXT, INPUT_VIDEOTRACK_LONGTEXT, true )
        change_safe ()
    add_integer( "audio-track", -1,
                 INPUT_AUDIOTRACK_TEXT, INPUT_AUDIOTRACK_LONGTEXT, true )
        change_safe ()
    add_integer( "sub-track", -1,
                 INPUT_SUBTRACK_TEXT, INPUT_SUBTRACK_LONGTEXT, true )
        change_safe ()
    add_string( "audio-language", "",
                 INPUT_AUDIOTRACK_LANG_TEXT, INPUT_AUDIOTRACK_LANG_LONGTEXT,
                  false )
        change_safe ()
    add_string( "sub-language", "",
                 INPUT_SUBTRACK_LANG_TEXT, INPUT_SUBTRACK_LANG_LONGTEXT,
                  false )
        change_safe ()
    add_string( "menu-language", "",
                 INPUT_MENUTRACK_LANG_TEXT, INPUT_MENUTRACK_LANG_LONGTEXT,
                  false )
        change_safe ()
    add_string( "video-track-id", NULL, INPUT_VIDEOTRACK_ID_TEXT,
                 INPUT_VIDEOTRACK_ID_LONGTEXT, true )
        change_safe ()
    add_string( "audio-track-id", NULL, INPUT_AUDIOTRACK_ID_TEXT,
                 INPUT_AUDIOTRACK_ID_LONGTEXT, true )
        change_safe ()
    add_string( "sub-track-id", NULL,
                 INPUT_SUBTRACK_ID_TEXT, INPUT_SUBTRACK_ID_LONGTEXT, true )
        change_safe ()
    add_integer( "captions", 608,
                 INPUT_CAPTIONS_TEXT, INPUT_CAPTIONS_TEXT, true )
        change_integer_list( pi_captions, ppsz_captions )
        change_safe ()
    add_integer( "preferred-resolution", -1, INPUT_PREFERREDRESOLUTION_TEXT,
                 INPUT_PREFERREDRESOLUTION_LONGTEXT, false )
        change_safe ()
        change_integer_list( pi_prefres, ppsz_prefres )
    add_bool( "low-delay", 0, INPUT_LOWDELAY_TEXT,
              INPUT_LOWDELAY_LONGTEXT, true )
        change_safe ()

    set_section( N_( "Playback control" ) , NULL)
    add_integer( "input-repeat", 0,
                 INPUT_REPEAT_TEXT, INPUT_REPEAT_LONGTEXT, false )
        change_integer_range( 0, 65535 )
        change_safe ()
    add_float( "start-time", 0,
               START_TIME_TEXT, START_TIME_LONGTEXT, true )
        change_safe ()
    add_float( "stop-time", 0,
               STOP_TIME_TEXT, STOP_TIME_LONGTEXT, true )
        change_safe ()
    add_float( "run-time", 0,
               RUN_TIME_TEXT, RUN_TIME_LONGTEXT, true )
        change_safe ()
    add_bool( "input-fast-seek", false,
              INPUT_FAST_SEEK_TEXT, INPUT_FAST_SEEK_LONGTEXT, false )
        change_safe ()
    add_float( "rate", 1.,
               INPUT_RATE_TEXT, INPUT_RATE_LONGTEXT, false )

    add_string( "input-list", NULL,
                 INPUT_LIST_TEXT, INPUT_LIST_LONGTEXT, true )
    add_string( "input-slave", NULL,
                 INPUT_SLAVE_TEXT, INPUT_SLAVE_LONGTEXT, true )

    add_string( "bookmarks", NULL,
                 BOOKMARKS_TEXT, BOOKMARKS_LONGTEXT, true )
        change_safe ()

    add_integer( "restore-playback-pos", VLC_PLAYER_RESTORE_PLAYBACK_POS_ASK,
                 RESTORE_PLAYBACK_POS_TEXT, RESTORE_PLAYBACK_POS_TEXT, false )
    add_bool( "restore-playback-states", false,
                 RESTORE_PLAYBACK_STATE_TEXT, RESTORE_PLAYBACK_STATE_LONGTEXT, false )

    set_section( N_( "Default devices") , NULL )

    add_loadfile("dvd", DVD_DEVICE, DVD_DEV_TEXT, DVD_DEV_LONGTEXT)
    add_loadfile("vcd", VCD_DEVICE, VCD_DEV_TEXT, VCD_DEV_LONGTEXT)

    set_section( N_( "Network settings" ), NULL )

    add_integer( "mtu", MTU_DEFAULT, MTU_TEXT, MTU_LONGTEXT, true )
    add_obsolete_bool( "ipv6" ) /* since 2.0.0 */
    add_obsolete_bool( "ipv4" ) /* since 2.0.0 */
    add_integer( "ipv4-timeout", 5 * 1000, TIMEOUT_TEXT,
                 TIMEOUT_LONGTEXT, true )
        change_integer_range( 0, INT_MAX )

    add_string( "http-host", NULL, HTTP_HOST_TEXT, HOST_LONGTEXT, true )
    add_integer( "http-port", 8080, HTTP_PORT_TEXT, HTTP_PORT_LONGTEXT, true )
        change_integer_range( 1, 65535 )
    add_integer( "https-port", 8443, HTTPS_PORT_TEXT, HTTPS_PORT_LONGTEXT, true )
        change_integer_range( 1, 65535 )
    add_string( "rtsp-host", NULL, RTSP_HOST_TEXT, RTSP_HOST_LONGTEXT, true )
    add_integer( "rtsp-port", 554, RTSP_PORT_TEXT, RTSP_PORT_LONGTEXT, true )
        change_integer_range( 1, 65535 )
    add_loadfile("http-cert", NULL, HTTP_CERT_TEXT, CERT_LONGTEXT)
    add_obsolete_string( "sout-http-cert" ) /* since 2.0.0 */
    add_loadfile("http-key", NULL, HTTP_KEY_TEXT, KEY_LONGTEXT)
    add_obsolete_string( "sout-http-key" ) /* since 2.0.0 */
    add_obsolete_string( "http-ca" ) /* since 3.0.0 */
    add_obsolete_string( "sout-http-ca" ) /* since 2.0.0 */
    add_obsolete_string( "http-crl" ) /* since 3.0.0 */
    add_obsolete_string( "sout-http-crl" ) /* since 2.0.0 */

#ifdef _WIN32
    add_string( "http-proxy", NULL, PROXY_TEXT, PROXY_LONGTEXT,
                false )
    add_password("http-proxy-pwd", NULL, PROXY_PASS_TEXT, PROXY_PASS_LONGTEXT)
#else
    add_obsolete_string( "http-proxy" )
    add_obsolete_string( "http-proxy-pwd" )

#endif
    add_obsolete_bool( "http-use-IE-proxy" )

    set_section( N_( "Socks proxy") , NULL )
    add_string( "socks", NULL,
                 SOCKS_SERVER_TEXT, SOCKS_SERVER_LONGTEXT, true )
    add_string( "socks-user", NULL,
                 SOCKS_USER_TEXT, SOCKS_USER_LONGTEXT, true )
    add_string( "socks-pwd", NULL,
                 SOCKS_PASS_TEXT, SOCKS_PASS_LONGTEXT, true )


    set_section( N_("Metadata" ) , NULL )
    add_string( "meta-title", NULL, META_TITLE_TEXT,
                META_TITLE_LONGTEXT, true )
        change_safe()
    add_string( "meta-author", NULL, META_AUTHOR_TEXT,
                META_AUTHOR_LONGTEXT, true )
        change_safe()
    add_string( "meta-artist", NULL, META_ARTIST_TEXT,
                META_ARTIST_LONGTEXT, true )
        change_safe()
    add_string( "meta-genre", NULL, META_GENRE_TEXT,
                META_GENRE_LONGTEXT, true )
        change_safe()
    add_string( "meta-copyright", NULL, META_CPYR_TEXT,
                META_CPYR_LONGTEXT, true )
        change_safe()
    add_string( "meta-description", NULL, META_DESCR_TEXT,
                META_DESCR_LONGTEXT, true )
        change_safe()
    add_string( "meta-date", NULL, META_DATE_TEXT,
                META_DATE_LONGTEXT, true )
        change_safe()
    add_string( "meta-url", NULL, META_URL_TEXT,
                META_URL_LONGTEXT, true )
        change_safe()

    set_section( N_( "Advanced" ), NULL )

    add_integer( "file-caching", 1000,
                 CACHING_TEXT, CACHING_LONGTEXT, true )
        change_integer_range( 0, 60000 )
        change_safe()
    add_obsolete_integer( "vdr-caching" ) /* 2.0.0 */
    add_integer( "live-caching", MS_FROM_VLC_TICK(DEFAULT_PTS_DELAY),
                 CAPTURE_CACHING_TEXT, CAPTURE_CACHING_LONGTEXT, true )
        change_integer_range( 0, 60000 )
        change_safe()
    add_obsolete_integer( "alsa-caching" ) /* 2.0.0 */
    add_obsolete_integer( "dshow-caching" ) /* 2.0.0 */
    add_obsolete_integer( "dv-caching" ) /* 2.0.0 */
    add_obsolete_integer( "dvb-caching" ) /* 2.0.0 */
    add_obsolete_integer( "eyetv-caching" ) /* 2.0.0 */
    add_obsolete_integer( "jack-input-caching" ) /* 2.0.0 */
    add_obsolete_integer( "linsys-hdsdi-caching" ) /* 2.0.0 */
    add_obsolete_integer( "linsys-sdi-caching" ) /* 2.0.0 */
    add_obsolete_integer( "oss-caching" ) /* 2.0.0 */
    add_obsolete_integer( "screen-caching" ) /* 2.0.0 */
    add_obsolete_integer( "v4l2-caching" ) /* 2.0.0 */
    add_integer( "disc-caching", MS_FROM_VLC_TICK(DEFAULT_PTS_DELAY),
                 DISC_CACHING_TEXT, DISC_CACHING_LONGTEXT, true )
        change_integer_range( 0, 60000 )
        change_safe()
    add_obsolete_integer( "bd-caching" ) /* 2.0.0 */
    add_obsolete_integer( "bluray-caching" ) /* 2.0.0 */
    add_obsolete_integer( "cdda-caching" ) /* 2.0.0 */
    add_obsolete_integer( "dvdnav-caching" ) /* 2.0.0 */
    add_obsolete_integer( "dvdread-caching" ) /* 2.0.0 */
    add_obsolete_integer( "vcd-caching" ) /* 2.0.0 */
    add_integer( "network-caching", 1000,
                 NETWORK_CACHING_TEXT, NETWORK_CACHING_LONGTEXT, true )
        change_integer_range( 0, 60000 )
        change_safe()
    add_obsolete_integer( "ftp-caching" ) /* 2.0.0 */
    add_obsolete_integer( "http-caching" ) /* 2.0.0 */
    add_obsolete_integer( "mms-caching" ) /* 2.0.0 */
    add_obsolete_integer( "realrtsp-caching" ) /* 2.0.0 */
    add_obsolete_integer( "rtp-caching" ) /* 2.0.0 */
    add_obsolete_integer( "rtsp-caching" ) /* 2.0.0 */
    add_obsolete_integer( "sftp-caching" ) /* 2.0.0 */
    add_obsolete_integer( "smb-caching" ) /* 2.0.0 */
    add_obsolete_integer( "tcp-caching" ) /* 2.0.0 */
    add_obsolete_integer( "udp-caching" ) /* 2.0.0 */

    add_integer( "cr-average", 40, CR_AVERAGE_TEXT,
                 CR_AVERAGE_LONGTEXT, true )
    add_integer( "clock-synchro", -1, CLOCK_SYNCHRO_TEXT,
                 CLOCK_SYNCHRO_LONGTEXT, true )
        change_integer_list( pi_clock_values, ppsz_clock_descriptions )
    add_integer( "clock-jitter", 5000, CLOCK_JITTER_TEXT,
              CLOCK_JITTER_LONGTEXT, true )
        change_safe()
    add_integer( "clock-master", VLC_CLOCK_MASTER_DEFAULT,
                 CLOCK_MASTER_TEXT, NULL, true )
        change_integer_list( pi_clock_master_values, ppsz_clock_master_descriptions )

    add_bool( "network-synchronisation", false, NETSYNC_TEXT,
              NETSYNC_LONGTEXT, true )

    add_directory("input-record-path", NULL,
                  INPUT_RECORD_PATH_TEXT, INPUT_RECORD_PATH_LONGTEXT)
    add_bool( "input-record-native", true, INPUT_RECORD_NATIVE_TEXT,
              INPUT_RECORD_NATIVE_LONGTEXT, true )

    add_directory("input-timeshift-path", NULL,
                  INPUT_TIMESHIFT_PATH_TEXT, INPUT_TIMESHIFT_PATH_LONGTEXT)
    add_integer( "input-timeshift-granularity", -1, INPUT_TIMESHIFT_GRANULARITY_TEXT,
                 INPUT_TIMESHIFT_GRANULARITY_LONGTEXT, true )

    add_string( "input-title-format", "$Z", INPUT_TITLE_FORMAT_TEXT, INPUT_TITLE_FORMAT_LONGTEXT, false );

    add_bool( "lua", true, INPUT_LUA_TEXT, INPUT_LUA_TEXT, true );

/* Decoder options */
    set_subcategory( SUBCAT_INPUT_VCODEC )
    add_category_hint(N_("Decoders"), CODEC_CAT_LONGTEXT)
    add_string( "codec", NULL, CODEC_TEXT,
                CODEC_LONGTEXT, true )
    add_bool( "hw-dec", true, HW_DEC_TEXT, HW_DEC_LONGTEXT, true )
    add_string( "encoder",  NULL, ENCODER_TEXT,
                ENCODER_LONGTEXT, true )
    add_module("dec-dev", "decoder device", "any", DEC_DEV_TEXT, DEC_DEV_LONGTEXT)

    set_subcategory( SUBCAT_INPUT_ACCESS )
    add_category_hint(N_("Input"), INPUT_CAT_LONGTEXT)
    add_module("access", "access", NULL, ACCESS_TEXT, ACCESS_LONGTEXT)

    set_subcategory( SUBCAT_INPUT_DEMUX )
    add_module("demux", "demux", "any", DEMUX_TEXT, DEMUX_LONGTEXT)
    set_subcategory( SUBCAT_INPUT_ACODEC )
    set_subcategory( SUBCAT_INPUT_SCODEC )
    add_obsolete_bool( "prefer-system-codecs" )

    set_subcategory( SUBCAT_INPUT_STREAM_FILTER )
    add_module_list("stream-filter", "stream_filter", NULL,
                    STREAM_FILTER_TEXT, STREAM_FILTER_LONGTEXT)

    add_string( "demux-filter", NULL, DEMUX_FILTER_TEXT, DEMUX_FILTER_LONGTEXT, true )

/* Stream output options */
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_GENERAL )
    add_category_hint(N_("Stream output"), SOUT_CAT_LONGTEXT)

    add_string( "sout", NULL, SOUT_TEXT, SOUT_LONGTEXT, true )
    add_bool( "sout-display", false, SOUT_DISPLAY_TEXT,
                                SOUT_DISPLAY_LONGTEXT, true )
    add_bool( "sout-keep", false, SOUT_KEEP_TEXT,
                                SOUT_KEEP_LONGTEXT, true )
    add_bool( "sout-all", true, SOUT_ALL_TEXT,
                                SOUT_ALL_LONGTEXT, true )
    add_bool( "sout-audio", 1, SOUT_AUDIO_TEXT,
                                SOUT_AUDIO_LONGTEXT, true )
    add_bool( "sout-video", 1, SOUT_VIDEO_TEXT,
                                SOUT_VIDEO_LONGTEXT, true )
    add_bool( "sout-spu", 1, SOUT_SPU_TEXT,
                                SOUT_SPU_LONGTEXT, true )
    add_integer( "sout-mux-caching", 1500, SOUT_MUX_CACHING_TEXT,
                                SOUT_MUX_CACHING_LONGTEXT, true )

    set_section( N_("VLM"), NULL )
    add_loadfile("vlm-conf", NULL, VLM_CONF_TEXT, VLM_CONF_LONGTEXT)


    set_subcategory( SUBCAT_SOUT_STREAM )
    add_integer( "sap-interval", 5, ANN_SAPINTV_TEXT,
                               ANN_SAPINTV_LONGTEXT, true )

    set_subcategory( SUBCAT_SOUT_MUX )
    add_module("mux", "sout mux", NULL, MUX_TEXT, MUX_LONGTEXT)
    set_subcategory( SUBCAT_SOUT_ACO )
    add_module("access_output", "sout access", NULL,
               ACCESS_OUTPUT_TEXT, ACCESS_OUTPUT_LONGTEXT)
    add_integer( "ttl", -1, TTL_TEXT, TTL_LONGTEXT, true )
    add_string( "miface", NULL, MIFACE_TEXT, MIFACE_LONGTEXT, true )
    add_obsolete_string( "miface-addr" ) /* since 2.0.0 */
    add_integer( "dscp", 0, DSCP_TEXT, DSCP_LONGTEXT, true )

    set_subcategory( SUBCAT_SOUT_PACKETIZER )
    add_module("packetizer", "packetizer", NULL,
               PACKETIZER_TEXT, PACKETIZER_LONGTEXT)

    set_subcategory( SUBCAT_SOUT_VOD )

/* CPU options */
    set_category( CAT_ADVANCED )
    add_obsolete_bool( "fpu" )
#if defined( __i386__ ) || defined( __x86_64__ )
    add_obsolete_bool( "mmx" ) /* since 2.0.0 */
    add_obsolete_bool( "3dn" ) /* since 2.0.0 */
    add_obsolete_bool( "mmxext" ) /* since 2.0.0 */
    add_obsolete_bool( "sse" ) /* since 2.0.0 */
    add_obsolete_bool( "sse2" ) /* since 2.0.0 */
    add_obsolete_bool( "sse3" ) /* since 2.0.0 */
    add_obsolete_bool( "ssse3" ) /* since 2.0.0 */
    add_obsolete_bool( "sse41" ) /* since 2.0.0 */
    add_obsolete_bool( "sse42" ) /* since 2.0.0 */
#endif
#if defined( __powerpc__ ) || defined( __ppc__ ) || defined( __ppc64__ )
    add_obsolete_bool( "altivec" ) /* since 2.0.0 */
#endif

/* Misc options */
    set_subcategory( SUBCAT_ADVANCED_MISC )
    set_section( N_("Special modules"), NULL )
    add_category_hint(N_("Miscellaneous"), MISC_CAT_LONGTEXT)
    add_module("vod-server", "vod server", NULL,
               VOD_SERVER_TEXT, VOD_SERVER_LONGTEXT)

    set_section( N_("Plugins" ), NULL )
#ifdef HAVE_DYNAMIC_PLUGINS
    add_bool( "plugins-cache", true, PLUGINS_CACHE_TEXT,
              PLUGINS_CACHE_LONGTEXT, true )
        change_volatile ()
    add_bool( "plugins-scan", true, PLUGINS_SCAN_TEXT,
              PLUGINS_SCAN_LONGTEXT, true )
        change_volatile ()
    add_obsolete_string( "plugin-path" ) /* since 2.0.0 */
#endif
    add_obsolete_string( "data-path" ) /* since 2.1.0 */
    add_string( "keystore", NULL, KEYSTORE_TEXT,
                KEYSTORE_LONGTEXT, true )

    set_section( N_("Performance options"), NULL )

#if defined (LIBVLC_USE_PTHREAD)
    add_obsolete_bool( "rt-priority" ) /* since 4.0.0 */
    add_obsolete_integer( "rt-offset" ) /* since 4.0.0 */
#endif

#if defined(HAVE_DBUS)
    add_obsolete_bool( "inhibit" ) /* since 3.0.0 */
#endif

#if defined(_WIN32) || defined(__OS2__)
    add_bool( "high-priority", 0, HPRIORITY_TEXT,
              HPRIORITY_LONGTEXT, false )
#endif

#ifdef _WIN32
    add_string( "clock-source", NULL, CLOCK_SOURCE_TEXT, CLOCK_SOURCE_TEXT, true )
        change_string_list( clock_sources, clock_sources_text )
#endif

/* Playlist options */
    set_category( CAT_PLAYLIST )
    set_subcategory( SUBCAT_PLAYLIST_GENERAL )
    add_category_hint(N_("Playlist"), PLAYLIST_CAT_LONGTEXT)
    add_bool( "random", 0, RANDOM_TEXT, RANDOM_LONGTEXT, false )
        change_short('Z')
        change_safe()
    add_bool( "loop", 0, LOOP_TEXT, LOOP_LONGTEXT, false )
        change_short('L')
        change_safe()
    add_bool( "repeat", 0, REPEAT_TEXT, REPEAT_LONGTEXT, false )
        change_short('R')
        change_safe()
    add_bool( "play-and-exit", 0, PAE_TEXT, PAE_LONGTEXT, false )
    add_bool( "play-and-stop", 0, PAS_TEXT, PAS_LONGTEXT, false )
        change_safe()
    add_bool( "play-and-pause", 0, PAP_TEXT, PAP_LONGTEXT, true )
        change_safe()
    add_bool( "start-paused", 0, SP_TEXT, SP_LONGTEXT, false )
    add_bool( "playlist-autostart", true,
              AUTOSTART_TEXT, AUTOSTART_LONGTEXT, false )
    add_bool( "playlist-cork", true, CORK_TEXT, CORK_LONGTEXT, false )
#if defined(_WIN32) || defined(HAVE_DBUS) || defined(__OS2__)
    add_bool( "one-instance", 0, ONEINSTANCE_TEXT,
              ONEINSTANCE_LONGTEXT, true )
    add_bool( "started-from-file", 0, STARTEDFROMFILE_TEXT,
              STARTEDFROMFILE_LONGTEXT, true )
        change_volatile ()
    add_bool( "one-instance-when-started-from-file", 1,
              ONEINSTANCEWHENSTARTEDFROMFILE_TEXT,
              ONEINSTANCEWHENSTARTEDFROMFILE_TEXT, true )
    add_bool( "playlist-enqueue", 0, PLAYLISTENQUEUE_TEXT,
              PLAYLISTENQUEUE_LONGTEXT, true )
#endif
#ifdef HAVE_DBUS
    add_bool( "dbus", false, DBUS_TEXT, DBUS_LONGTEXT, true )
#endif
    add_bool( "media-library", 0, ML_TEXT, ML_LONGTEXT, false )
    add_bool( "playlist-tree", 0, PLTREE_TEXT, PLTREE_LONGTEXT, false )

    add_string( "open", "", OPEN_TEXT, OPEN_LONGTEXT, false )

    add_bool( "auto-preparse", true, PREPARSE_TEXT,
              PREPARSE_LONGTEXT, false )

    add_integer( "preparse-timeout", 5000, PREPARSE_TIMEOUT_TEXT,
                 PREPARSE_TIMEOUT_LONGTEXT, false )

    add_integer( "preparse-threads", 1, PREPARSE_THREADS_TEXT,
                 PREPARSE_THREADS_LONGTEXT, false )

    add_integer( "fetch-art-threads", 1, FETCH_ART_THREADS_TEXT,
                 FETCH_ART_THREADS_LONGTEXT, false )

    add_obsolete_integer( "album-art" )
    add_bool( "metadata-network-access", false, METADATA_NETWORK_TEXT,
                 METADATA_NETWORK_TEXT, false )

    add_string( "recursive", "collapse" , RECURSIVE_TEXT,
                RECURSIVE_LONGTEXT, false )
        change_string_list( psz_recursive_list, psz_recursive_list_text )
    add_string( "ignore-filetypes", "m3u,db,nfo,ini,jpg,jpeg,ljpg,gif,png,pgm,"
                "pgmyuv,pbm,pam,tga,bmp,pnm,xpm,xcf,pcx,tif,tiff,lbm,sfv,txt,"
                "sub,idx,srt,cue,ssa",
                IGNORE_TEXT, IGNORE_LONGTEXT, false )
    add_bool( "show-hiddenfiles", false,
              SHOW_HIDDENFILES_TEXT, SHOW_HIDDENFILES_LONGTEXT, false )
    add_bool( "extractor-flatten", false,
              "Flatten files listed by extractors (archive)", NULL, true )
        change_volatile()

    set_subcategory( SUBCAT_PLAYLIST_SD )
    add_string( "services-discovery", "", SD_TEXT, SD_LONGTEXT, true )
        change_short('S')

/* Interface options */
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_GENERAL )
    add_integer( "verbose", 0, VERBOSE_TEXT, VERBOSE_LONGTEXT,
                 false )
        change_short('v')
        change_volatile ()
    add_obsolete_string( "verbose-objects" ) /* since 2.1.0 */
#if !defined(_WIN32) && !defined(__OS2__)
    add_obsolete_bool( "daemon" ) /* since 4.0.0 */
        change_short('d')
    add_obsolete_string( "pidfile" ) /* since 4.0.0 */
#endif

#if defined (_WIN32) || defined (__APPLE__)
    add_obsolete_string( "language" ) /* since 2.1.0 */
#endif

    add_bool( "color", true, COLOR_TEXT, COLOR_LONGTEXT, true )
        change_volatile ()
    add_obsolete_bool( "advanced" ) /* since 4.0.0 */
    add_bool( "interact", true, INTERACTION_TEXT,
              INTERACTION_LONGTEXT, false )

    add_bool ( "stats", true, STATS_TEXT, STATS_LONGTEXT, true )

    set_subcategory( SUBCAT_INTERFACE_MAIN )
    add_module_cat("intf", SUBCAT_INTERFACE_MAIN, NULL,
                   INTF_TEXT, INTF_LONGTEXT)
        change_short('I')
    add_module_list_cat("extraintf", SUBCAT_INTERFACE_MAIN, NULL,
                        EXTRAINTF_TEXT, EXTRAINTF_LONGTEXT)


    set_subcategory( SUBCAT_INTERFACE_CONTROL )
    add_module_list_cat("control", SUBCAT_INTERFACE_CONTROL, NULL,
                        CONTROL_TEXT, CONTROL_LONGTEXT)

/* Hotkey options*/
    set_subcategory( SUBCAT_INTERFACE_HOTKEYS )
    add_category_hint(N_("Hot keys"), HOTKEY_CAT_LONGTEXT)

    add_integer( "hotkeys-y-wheel-mode", 0, MOUSE_Y_WHEEL_MODE_TEXT,
                 MOUSE_Y_WHEEL_MODE_LONGTEXT, false )
        change_integer_list( mouse_wheel_values, mouse_wheel_texts )
    add_integer( "hotkeys-x-wheel-mode", 2, MOUSE_X_WHEEL_MODE_TEXT,
                 MOUSE_X_WHEEL_MODE_LONGTEXT, false )
        change_integer_list( mouse_wheel_values, mouse_wheel_texts )
    add_obsolete_integer( "hotkeys-mousewheel-mode" ) /* since 3.0.0 */

#if defined(__APPLE__)
/* Don't use the following combo's */

/*  copy                          "Command+c"
 *  cut                           "Command+x"
 *  paste                         "Command+v"
 *  select all                    "Command+a"
 *  preferences                   "Command+,"
 *  hide vlc                      "Command+h"
 *  hide other                    "Command+Alt+h"
 *  open file                     "Command+Shift+o"
 *  open                          "Command+o"
 *  open disk                     "Command+d"
 *  open network                  "Command+n"
 *  open capture                  "Command+r"
 *  save playlist                 "Command+s"
 *  playlist repeat all           "Command+l"
 *  playlist repeat               "Command+r"
 *  video fit to screen           "Command+3"
 *  minimize window               "Command+m"
 *  close window                  "Command+w"
 *  streaming wizard              "Command+Shift+w"
 *  show controller               "Command+Shift+c"
 *  show playlist                 "Command+Shift+p"
 *  show info                     "Command+i"
 *  show extended controls        "Command+e"
 *  show equaliser                "Command+Shift+e"
 *  show bookmarks                "Command+b"
 *  show messages                 "Command+Shift+m"
 *  show errors and warnings      "Command+Ctrl+m"
 *  help                          "Command+?"
 *  readme / FAQ                  "Command+Alt+?"
 */
#   define KEY_TOGGLE_FULLSCREEN  "Command+f"
#   define KEY_LEAVE_FULLSCREEN   "Esc"
#   define KEY_PLAY_PAUSE         "Space"
#   define KEY_SIMPLE_PAUSE       NULL
#   define KEY_PLAY               NULL
#   define KEY_FASTER             "Command+="
#   define KEY_SLOWER             "Command+-"
#   define KEY_RATE_NORMAL        NULL
#   define KEY_RATE_FASTER_FINE   NULL
#   define KEY_RATE_SLOWER_FINE   NULL
#   define KEY_NEXT               "Command+Right"
#   define KEY_PREV               "Command+Left"
#   define KEY_STOP               "Command+."
#   define KEY_POSITION           "t"
#   define KEY_JUMP_MEXTRASHORT   "Command+Ctrl+Left"
#   define KEY_JUMP_PEXTRASHORT   "Command+Ctrl+Right"
#   define KEY_JUMP_MSHORT        "Command+Alt+Left"
#   define KEY_JUMP_PSHORT        "Command+Alt+Right"
#   define KEY_JUMP_MMEDIUM       "Command+Shift+Left"
#   define KEY_JUMP_PMEDIUM       "Command+Shift+Right"
#   define KEY_JUMP_MLONG         "Command+Shift+Alt+Left"
#   define KEY_JUMP_PLONG         "Command+Shift+Alt+Right"
#   define KEY_FRAME_NEXT         "e"
#   define KEY_NAV_ACTIVATE       "Enter"
#   define KEY_NAV_UP             "Up"
#   define KEY_NAV_DOWN           "Down"
#   define KEY_NAV_LEFT           "Left"
#   define KEY_NAV_RIGHT          "Right"
#   define KEY_QUIT               "Command+q"
#   define KEY_VOL_UP             "Command+Up"
#   define KEY_VOL_DOWN           "Command+Down"
#   define KEY_VOL_MUTE           "Command+Alt+Down"
#   define KEY_SUBDELAY_UP        "j"
#   define KEY_SUBDELAY_DOWN      "h"
#   define KEY_SUBPOS_DOWN        NULL
#   define KEY_SUBPOS_UP          NULL
#   define KEY_SUBTEXT_SCALEUP    "Command+Mouse Wheel Up"
#   define KEY_SUBTEXT_SCALEDOWN  "Command+Mouse Wheel Down"
#   define KEY_SUBTEXT_SCALE      "Command+0"
#   define KEY_SUBSYNC_MARKAUDIO  "Shift+h"
#   define KEY_SUBSYNC_MARKSUB    "Shift+j"
#   define KEY_SUBSYNC_APPLY      "Shift+k"
#   define KEY_SUBSYNC_RESET      "Command+Shift+k"
#   define KEY_AUDIODELAY_UP      "g"
#   define KEY_AUDIODELAY_DOWN    "f"
#   define KEY_AUDIO_TRACK        "l"
#   define KEY_SUBTITLE_TRACK     "s"
#   define KEY_SUBTITLE_TOGGLE    "Shift+s"
#   define KEY_SUBTITLE_CONTROL_S "Command+Shift+v"
#   define KEY_SUBTITLE_REVTRACK  "Alt+s"
#   define KEY_PROGRAM_SID_NEXT   "x"
#   define KEY_PROGRAM_SID_PREV   "Shift+x"
#   define KEY_ASPECT_RATIO       "a"
#   define KEY_CROP               "c"
#   define KEY_TOGGLE_AUTOSCALE   "o"
#   define KEY_SCALE_UP           "Alt+o"
#   define KEY_SCALE_DOWN         "Shift+Alt+o"
#   define KEY_DEINTERLACE        "d"
#   define KEY_DEINTERLACE_MODE   "Shift+d"
#   define KEY_INTF_TOGGLE_FSC    "i"
#   define KEY_INTF_BOSS          NULL
#   define KEY_INTF_POPUP_MENU    "Menu"
#   define KEY_DISC_MENU          "Ctrl+m"
#   define KEY_TITLE_PREV         "Ctrl+p"
#   define KEY_TITLE_NEXT         "Ctrl+n"
#   define KEY_CHAPTER_PREV       "Ctrl+u"
#   define KEY_CHAPTER_NEXT       "Ctrl+d"
#   define KEY_SNAPSHOT           "Command+Alt+s"
#   define KEY_ZOOM               "z"
#   define KEY_UNZOOM             "Shift+z"
#   define KEY_RANDOM             "Command+z"
#   define KEY_LOOP               "Shift+l"

#   define KEY_CROP_TOP           "Alt+i"
#   define KEY_UNCROP_TOP         "Alt+Shift+i"
#   define KEY_CROP_LEFT          "Alt+j"
#   define KEY_UNCROP_LEFT        "Alt+Shift+j"
#   define KEY_CROP_BOTTOM        "Alt+k"
#   define KEY_UNCROP_BOTTOM      "Alt+Shift+k"
#   define KEY_CROP_RIGHT         "Alt+l"
#   define KEY_UNCROP_RIGHT       "Alt+Shift+l"

/* 360° Viewpoint */
#   define KEY_VIEWPOINT_FOV_IN   "Page Up"
#   define KEY_VIEWPOINT_FOV_OUT  "Page Down"

/* the macosx-interface already has bindings */
#   define KEY_ZOOM_QUARTER       NULL
#   define KEY_ZOOM_HALF          "Command+0"
#   define KEY_ZOOM_ORIGINAL      "Command+1"
#   define KEY_ZOOM_DOUBLE        "Command+2"

#   define KEY_SET_BOOKMARK1      "Command+F1"
#   define KEY_SET_BOOKMARK2      "Command+F2"
#   define KEY_SET_BOOKMARK3      "Command+F3"
#   define KEY_SET_BOOKMARK4      "Command+F4"
#   define KEY_SET_BOOKMARK5      "Command+F5"
#   define KEY_SET_BOOKMARK6      "Command+F6"
#   define KEY_SET_BOOKMARK7      "Command+F7"
#   define KEY_SET_BOOKMARK8      "Command+F8"
#   define KEY_SET_BOOKMARK9      NULL
#   define KEY_SET_BOOKMARK10     NULL
#   define KEY_PLAY_BOOKMARK1     "F1"
#   define KEY_PLAY_BOOKMARK2     "F2"
#   define KEY_PLAY_BOOKMARK3     "F3"
#   define KEY_PLAY_BOOKMARK4     "F4"
#   define KEY_PLAY_BOOKMARK5     "F5"
#   define KEY_PLAY_BOOKMARK6     "F6"
#   define KEY_PLAY_BOOKMARK7     "F7"
#   define KEY_PLAY_BOOKMARK8     "F8"
#   define KEY_PLAY_BOOKMARK9     NULL
#   define KEY_PLAY_BOOKMARK10    NULL
#   define KEY_RECORD             "Command+Shift+r"
#   define KEY_WALLPAPER          NULL
#   define KEY_AUDIODEVICE_CYCLE  "Shift+a"
#   define KEY_PLAY_CLEAR         NULL

#else /* Non Mac OS X */
    /*
       You should try to avoid Ctrl + letter key, because they are usually for
       dialogs showing and interface related stuffs.
       It would be nice (less important than previous rule) to try to avoid
       alt + letter key, because they are usually for menu accelerators and you
       don't know how the translator is going to do it.
     */
#   define KEY_TOGGLE_FULLSCREEN  "f"
#   define KEY_LEAVE_FULLSCREEN   "Esc"
#   define KEY_SIMPLE_PAUSE       "Browser Stop"
#   define KEY_PLAY               "Browser Refresh"
#   define KEY_FASTER             "+"
#   define KEY_SLOWER             "-"
#   define KEY_RATE_NORMAL        "="
#   define KEY_RATE_FASTER_FINE   "]"
#   define KEY_RATE_SLOWER_FINE   "["
#ifdef _WIN32
#   define KEY_PLAY_PAUSE         "Space"
#   define KEY_NEXT               "n"
#   define KEY_PREV               "p"
#   define KEY_STOP               "s"
#else
#   define KEY_PLAY_PAUSE         "Space\tMedia Play Pause"
#   define KEY_NEXT               "n\tMedia Next Track"
#   define KEY_PREV               "p\tMedia Prev Track"
#   define KEY_STOP               "s\tMedia Stop"
#endif
#   define KEY_POSITION           "t"
#   define KEY_JUMP_MEXTRASHORT   "Shift+Left"
#   define KEY_JUMP_PEXTRASHORT   "Shift+Right"
#   define KEY_JUMP_MSHORT        "Alt+Left"
#   define KEY_JUMP_PSHORT        "Alt+Right"
#   define KEY_JUMP_MMEDIUM       "Ctrl+Left"
#   define KEY_JUMP_PMEDIUM       "Ctrl+Right"
#   define KEY_JUMP_MLONG         "Ctrl+Alt+Left"
#   define KEY_JUMP_PLONG         "Ctrl+Alt+Right"
#   define KEY_NAV_ACTIVATE       "Enter"
#   define KEY_NAV_UP             "Up"
#   define KEY_NAV_DOWN           "Down"
#   define KEY_NAV_LEFT           "Left"
#   define KEY_NAV_RIGHT          "Right"
#   define KEY_QUIT               "Ctrl+q"

#ifdef _WIN32 /* On Windows, people expect volume keys to control the master */
#   define KEY_VOL_UP             "Ctrl+Up"
#   define KEY_VOL_DOWN           "Ctrl+Down"
#   define KEY_VOL_MUTE           "m"
#   define KEY_FRAME_NEXT         "e"
#else
#   define KEY_VOL_UP             "Ctrl+Up\tVolume Up"
#   define KEY_VOL_DOWN           "Ctrl+Down\tVolume Down"
#   define KEY_VOL_MUTE           "m\tVolume Mute"
#   define KEY_FRAME_NEXT         "e\tBrowser Next"
#endif

#   define KEY_SUBDELAY_UP        "h"
#   define KEY_SUBDELAY_DOWN      "g"
#   define KEY_SUBPOS_DOWN        NULL
#   define KEY_SUBPOS_UP          NULL
#   define KEY_SUBTEXT_SCALEUP    "Ctrl+Mouse Wheel Up"
#   define KEY_SUBTEXT_SCALEDOWN  "Ctrl+Mouse Wheel Down"
#   define KEY_SUBTEXT_SCALE      "Ctrl+0"
#   define KEY_SUBSYNC_MARKAUDIO  "Shift+h"
#   define KEY_SUBSYNC_MARKSUB    "Shift+j"
#   define KEY_SUBSYNC_APPLY      "Shift+k"
#   define KEY_SUBSYNC_RESET      "Ctrl+Shift+k"
#   define KEY_AUDIODELAY_UP      "k"
#   define KEY_AUDIODELAY_DOWN    "j"
#   define KEY_RANDOM             "r"
#   define KEY_LOOP               "l"

#   define KEY_AUDIO_TRACK        "b"
#   define KEY_SUBTITLE_TRACK     "v"
#   define KEY_SUBTITLE_TOGGLE    "Shift+v"
#   define KEY_SUBTITLE_CONTROL_S "Ctrl+Shift+v"
#   define KEY_SUBTITLE_REVTRACK  "Alt+v"
#   define KEY_PROGRAM_SID_NEXT   "x"
#   define KEY_PROGRAM_SID_PREV   "Shift+x"
#   define KEY_ASPECT_RATIO       "a"
#   define KEY_CROP               "c"
#   define KEY_TOGGLE_AUTOSCALE   "o"
#   define KEY_SCALE_UP           "Alt+o"
#   define KEY_SCALE_DOWN         "Alt+Shift+o"
#   define KEY_DEINTERLACE        "d"
#   define KEY_DEINTERLACE_MODE   "Shift+d"
#   define KEY_INTF_TOGGLE_FSC    "i"
#   define KEY_INTF_BOSS          NULL
#   define KEY_INTF_POPUP_MENU    "Menu"
#   define KEY_DISC_MENU          "Shift+m"
#   define KEY_TITLE_PREV         "Shift+o"
#   define KEY_TITLE_NEXT         "Shift+b"
#   define KEY_CHAPTER_PREV       "Shift+p"
#   define KEY_CHAPTER_NEXT       "Shift+n"
#   define KEY_SNAPSHOT           "Shift+s"

#   define KEY_ZOOM               "z"
#   define KEY_UNZOOM             "Shift+z"

#   define KEY_AUDIODEVICE_CYCLE  "Shift+a"

#   define KEY_RECORD             "Shift+r"
#   define KEY_WALLPAPER          "w"

/* Cropping */
#   define KEY_CROP_TOP           "Alt+r"
#   define KEY_UNCROP_TOP         "Alt+Shift+r"
#   define KEY_CROP_LEFT          "Alt+d"
#   define KEY_UNCROP_LEFT        "Alt+Shift+d"
#   define KEY_CROP_BOTTOM        "Alt+c"
#   define KEY_UNCROP_BOTTOM      "Alt+Shift+c"
#   define KEY_CROP_RIGHT         "Alt+f"
#   define KEY_UNCROP_RIGHT       "Alt+Shift+f"

/* 360° Viewpoint */
#   define KEY_VIEWPOINT_FOV_IN   "Page Up"
#   define KEY_VIEWPOINT_FOV_OUT  "Page Down"

/* Zooming */
#   define KEY_ZOOM_QUARTER       "Alt+1"
#   define KEY_ZOOM_HALF          "Alt+2"
#   define KEY_ZOOM_ORIGINAL      "Alt+3"
#   define KEY_ZOOM_DOUBLE        "Alt+4"

/* Bookmarks */
#   define KEY_SET_BOOKMARK1      "Ctrl+F1"
#   define KEY_SET_BOOKMARK2      "Ctrl+F2"
#   define KEY_SET_BOOKMARK3      "Ctrl+F3"
#   define KEY_SET_BOOKMARK4      "Ctrl+F4"
#   define KEY_SET_BOOKMARK5      "Ctrl+F5"
#   define KEY_SET_BOOKMARK6      "Ctrl+F6"
#   define KEY_SET_BOOKMARK7      "Ctrl+F7"
#   define KEY_SET_BOOKMARK8      "Ctrl+F8"
#   define KEY_SET_BOOKMARK9      "Ctrl+F9"
#   define KEY_SET_BOOKMARK10     "Ctrl+F10"
#   define KEY_PLAY_BOOKMARK1     "F1"
#   define KEY_PLAY_BOOKMARK2     "F2"
#   define KEY_PLAY_BOOKMARK3     "F3"
#   define KEY_PLAY_BOOKMARK4     "F4"
#   define KEY_PLAY_BOOKMARK5     "F5"
#   define KEY_PLAY_BOOKMARK6     "F6"
#   define KEY_PLAY_BOOKMARK7     "F7"
#   define KEY_PLAY_BOOKMARK8     "F8"
#   define KEY_PLAY_BOOKMARK9     "F9"
#   define KEY_PLAY_BOOKMARK10    "F10"

/* Playlist clear */
#   define KEY_PLAY_CLEAR         "Ctrl+w"
#endif

    add_key("key-toggle-fullscreen", KEY_TOGGLE_FULLSCREEN,
            TOGGLE_FULLSCREEN_KEY_TEXT, TOGGLE_FULLSCREEN_KEY_LONGTEXT)
    add_key("key-leave-fullscreen", KEY_LEAVE_FULLSCREEN,
            LEAVE_FULLSCREEN_KEY_TEXT, LEAVE_FULLSCREEN_KEY_LONGTEXT)
    add_key("key-play-pause", KEY_PLAY_PAUSE,
            PLAY_PAUSE_KEY_TEXT, PLAY_PAUSE_KEY_LONGTEXT)
    add_key("key-pause", KEY_SIMPLE_PAUSE, PAUSE_KEY_TEXT, PAUSE_KEY_LONGTEXT)
    add_key("key-play", KEY_PLAY, PLAY_KEY_TEXT, PLAY_KEY_LONGTEXT)
    add_key("key-faster", KEY_FASTER, FASTER_KEY_TEXT, FASTER_KEY_LONGTEXT)
    add_key("key-slower", KEY_SLOWER, SLOWER_KEY_TEXT, SLOWER_KEY_LONGTEXT)
    add_key("key-rate-normal", KEY_RATE_NORMAL,
            RATE_NORMAL_KEY_TEXT, RATE_NORMAL_KEY_LONGTEXT)
    add_key("key-rate-faster-fine", KEY_RATE_FASTER_FINE,
            RATE_FASTER_FINE_KEY_TEXT, RATE_FASTER_FINE_KEY_LONGTEXT)
    add_key("key-rate-slower-fine", KEY_RATE_SLOWER_FINE,
            RATE_SLOWER_FINE_KEY_TEXT, RATE_SLOWER_FINE_KEY_LONGTEXT)
    add_key("key-next", KEY_NEXT, NEXT_KEY_TEXT, NEXT_KEY_LONGTEXT)
    add_key("key-prev", KEY_PREV, PREV_KEY_TEXT, PREV_KEY_LONGTEXT)
    add_key("key-stop", KEY_STOP, STOP_KEY_TEXT, STOP_KEY_LONGTEXT)
    add_key("key-position", KEY_POSITION, POSITION_KEY_TEXT,
             POSITION_KEY_LONGTEXT)
    add_key("key-jump-extrashort", KEY_JUMP_MEXTRASHORT,
             JBEXTRASHORT_KEY_TEXT, JBEXTRASHORT_KEY_LONGTEXT)
    add_key("key-jump+extrashort", KEY_JUMP_PEXTRASHORT,
             JFEXTRASHORT_KEY_TEXT, JFEXTRASHORT_KEY_LONGTEXT)
    add_key("key-jump-short", KEY_JUMP_MSHORT,
            JBSHORT_KEY_TEXT, JBSHORT_KEY_LONGTEXT)
    add_key("key-jump+short", KEY_JUMP_PSHORT,
            JFSHORT_KEY_TEXT, JFSHORT_KEY_LONGTEXT)
    add_key("key-jump-medium", KEY_JUMP_MMEDIUM,
            JBMEDIUM_KEY_TEXT, JBMEDIUM_KEY_LONGTEXT)
    add_key("key-jump+medium", KEY_JUMP_PMEDIUM,
            JFMEDIUM_KEY_TEXT, JFMEDIUM_KEY_LONGTEXT)
    add_key("key-jump-long", KEY_JUMP_MLONG,
            JBLONG_KEY_TEXT, JBLONG_KEY_LONGTEXT)
    add_key("key-jump+long", KEY_JUMP_PLONG,
            JFLONG_KEY_TEXT, JFLONG_KEY_LONGTEXT)
    add_key("key-frame-next", KEY_FRAME_NEXT,
            FRAME_NEXT_KEY_TEXT, FRAME_NEXT_KEY_LONGTEXT)
    add_key("key-nav-activate", KEY_NAV_ACTIVATE,
            NAV_ACTIVATE_KEY_TEXT, NAV_ACTIVATE_KEY_LONGTEXT)
    add_key("key-nav-up", KEY_NAV_UP, NAV_UP_KEY_TEXT, NAV_UP_KEY_LONGTEXT)
    add_key("key-nav-down", KEY_NAV_DOWN,
            NAV_DOWN_KEY_TEXT, NAV_DOWN_KEY_LONGTEXT)
    add_key("key-nav-left", KEY_NAV_LEFT,
            NAV_LEFT_KEY_TEXT, NAV_LEFT_KEY_LONGTEXT)
    add_key("key-nav-right", KEY_NAV_RIGHT,
            NAV_RIGHT_KEY_TEXT, NAV_RIGHT_KEY_LONGTEXT)

    add_key("key-disc-menu", KEY_DISC_MENU, DISC_MENU_TEXT, DISC_MENU_LONGTEXT)
    add_key("key-title-prev", KEY_TITLE_PREV,
            TITLE_PREV_TEXT, TITLE_PREV_LONGTEXT)
    add_key("key-title-next", KEY_TITLE_NEXT,
            TITLE_NEXT_TEXT, TITLE_NEXT_LONGTEXT)
    add_key("key-chapter-prev", KEY_CHAPTER_PREV,
            CHAPTER_PREV_TEXT, CHAPTER_PREV_LONGTEXT)
    add_key("key-chapter-next", KEY_CHAPTER_NEXT,
            CHAPTER_NEXT_TEXT, CHAPTER_NEXT_LONGTEXT)
    add_key("key-quit", KEY_QUIT, QUIT_KEY_TEXT, QUIT_KEY_LONGTEXT)
    add_key("key-vol-up", KEY_VOL_UP, VOL_UP_KEY_TEXT, VOL_UP_KEY_LONGTEXT)
    add_key("key-vol-down", KEY_VOL_DOWN,
            VOL_DOWN_KEY_TEXT, VOL_DOWN_KEY_LONGTEXT)
    add_key("key-vol-mute", KEY_VOL_MUTE,
            VOL_MUTE_KEY_TEXT, VOL_MUTE_KEY_LONGTEXT)
    add_key("key-subdelay-up", KEY_SUBDELAY_UP,
            SUBDELAY_UP_KEY_TEXT, SUBDELAY_UP_KEY_LONGTEXT)
    add_key("key-subdelay-down", KEY_SUBDELAY_DOWN,
             SUBDELAY_DOWN_KEY_TEXT, SUBDELAY_DOWN_KEY_LONGTEXT)
    add_key("key-subsync-markaudio", KEY_SUBSYNC_MARKAUDIO,
            SUBSYNC_MARKAUDIO_KEY_TEXT, SUBSYNC_MARKAUDIO_KEY_LONGTEXT)
    add_key("key-subsync-marksub", KEY_SUBSYNC_MARKSUB,
            SUBSYNC_MARKSUB_KEY_TEXT, SUBSYNC_MARKSUB_KEY_LONGTEXT)
    add_key("key-subsync-apply", KEY_SUBSYNC_APPLY,
            SUBSYNC_APPLY_KEY_TEXT, SUBSYNC_APPLY_KEY_LONGTEXT)
    add_key("key-subsync-reset", KEY_SUBSYNC_RESET,
            SUBSYNC_RESET_KEY_TEXT, SUBSYNC_RESET_KEY_LONGTEXT)
    add_key("key-subpos-up", KEY_SUBPOS_UP,
            SUBPOS_UP_KEY_TEXT, SUBPOS_UP_KEY_LONGTEXT)
    add_key("key-subpos-down", KEY_SUBPOS_DOWN,
            SUBPOS_DOWN_KEY_TEXT, SUBPOS_DOWN_KEY_LONGTEXT)
    add_key("key-audiodelay-up", KEY_AUDIODELAY_UP,
            AUDIODELAY_UP_KEY_TEXT, AUDIODELAY_UP_KEY_LONGTEXT)
    add_key("key-audiodelay-down", KEY_AUDIODELAY_DOWN,
            AUDIODELAY_DOWN_KEY_TEXT, AUDIODELAY_DOWN_KEY_LONGTEXT)
    add_key("key-audio-track", KEY_AUDIO_TRACK, AUDIO_TRACK_KEY_TEXT,
            AUDIO_TRACK_KEY_LONGTEXT)
    add_key("key-audiodevice-cycle", KEY_AUDIODEVICE_CYCLE,
            AUDIO_DEVICE_CYCLE_KEY_TEXT,
            AUDIO_DEVICE_CYCLE_KEY_LONGTEXT)
    add_key("key-subtitle-revtrack", KEY_SUBTITLE_REVTRACK,
            SUBTITLE_REVERSE_TRACK_KEY_TEXT, SUBTITLE_REVERSE_TRACK_KEY_LONGTEXT)
    add_key("key-subtitle-track", KEY_SUBTITLE_TRACK,
            SUBTITLE_TRACK_KEY_TEXT, SUBTITLE_TRACK_KEY_LONGTEXT)
    add_key("key-subtitle-toggle", KEY_SUBTITLE_TOGGLE,
            SUBTITLE_TOGGLE_KEY_TEXT, SUBTITLE_TOGGLE_KEY_LONGTEXT)
    add_key("key-subtitle-control-secondary", KEY_SUBTITLE_CONTROL_S,
            SUBTITLE_CONTROL_SECONDARY_KEY_TEXT, SUBTITLE_CONTROL_SECONDARY_KEY_LONGTEXT)
    add_key("key-program-sid-next", KEY_PROGRAM_SID_NEXT,
            PROGRAM_SID_NEXT_KEY_TEXT, PROGRAM_SID_NEXT_KEY_LONGTEXT)
    add_key("key-program-sid-prev", KEY_PROGRAM_SID_PREV,
            PROGRAM_SID_PREV_KEY_TEXT, PROGRAM_SID_PREV_KEY_LONGTEXT)
    add_key("key-aspect-ratio", KEY_ASPECT_RATIO,
            ASPECT_RATIO_KEY_TEXT, ASPECT_RATIO_KEY_LONGTEXT)
    add_key("key-crop", KEY_CROP,
            CROP_KEY_TEXT, CROP_KEY_LONGTEXT)
    add_key("key-toggle-autoscale", KEY_TOGGLE_AUTOSCALE,
            TOGGLE_AUTOSCALE_KEY_TEXT, TOGGLE_AUTOSCALE_KEY_LONGTEXT)
    add_key("key-incr-scalefactor", KEY_SCALE_UP,
            SCALE_UP_KEY_TEXT, SCALE_UP_KEY_LONGTEXT)
    add_key("key-decr-scalefactor", KEY_SCALE_DOWN,
            SCALE_DOWN_KEY_TEXT, SCALE_DOWN_KEY_LONGTEXT)
    add_key("key-deinterlace", KEY_DEINTERLACE,
            DEINTERLACE_KEY_TEXT, DEINTERLACE_KEY_LONGTEXT)
    add_key("key-deinterlace-mode", KEY_DEINTERLACE_MODE,
            DEINTERLACE_MODE_KEY_TEXT, DEINTERLACE_MODE_KEY_LONGTEXT)
    add_key("key-intf-show", KEY_INTF_TOGGLE_FSC,
            INTF_TOGGLE_FSC_KEY_TEXT, INTF_TOGGLE_FSC_KEY_TEXT)
    add_obsolete_inner( "key-intf-hide", CONFIG_ITEM_KEY )

    add_key("key-intf-boss", KEY_INTF_BOSS,
            INTF_BOSS_KEY_TEXT, INTF_BOSS_KEY_LONGTEXT)
    add_key("key-intf-popup-menu", KEY_INTF_POPUP_MENU,
            INTF_POPUP_MENU_KEY_TEXT, INTF_POPUP_MENU_KEY_LONGTEXT)
    add_key("key-snapshot", KEY_SNAPSHOT, SNAP_KEY_TEXT, SNAP_KEY_LONGTEXT)
    add_key("key-record", KEY_RECORD, RECORD_KEY_TEXT, RECORD_KEY_LONGTEXT)
    add_key("key-zoom", KEY_ZOOM, ZOOM_KEY_TEXT, ZOOM_KEY_LONGTEXT)
    add_key("key-unzoom", KEY_UNZOOM, UNZOOM_KEY_TEXT, UNZOOM_KEY_LONGTEXT)
    add_key("key-wallpaper", KEY_WALLPAPER,
            WALLPAPER_KEY_TEXT, WALLPAPER_KEY_LONGTEXT)

    add_key("key-crop-top", KEY_CROP_TOP,
             CROP_TOP_KEY_TEXT, CROP_TOP_KEY_LONGTEXT)
    add_key("key-uncrop-top", KEY_UNCROP_TOP,
            UNCROP_TOP_KEY_TEXT, UNCROP_TOP_KEY_LONGTEXT)
    add_key("key-crop-left", KEY_CROP_LEFT,
            CROP_LEFT_KEY_TEXT, CROP_LEFT_KEY_LONGTEXT)
    add_key("key-uncrop-left", KEY_UNCROP_LEFT,
            UNCROP_LEFT_KEY_TEXT, UNCROP_LEFT_KEY_LONGTEXT)
    add_key("key-crop-bottom", KEY_CROP_BOTTOM,
            CROP_BOTTOM_KEY_TEXT, CROP_BOTTOM_KEY_LONGTEXT)
    add_key("key-uncrop-bottom", KEY_UNCROP_BOTTOM,
             UNCROP_BOTTOM_KEY_TEXT, UNCROP_BOTTOM_KEY_LONGTEXT)
    add_key("key-crop-right", KEY_CROP_RIGHT,
            CROP_RIGHT_KEY_TEXT, CROP_RIGHT_KEY_LONGTEXT)
    add_key("key-uncrop-right", KEY_UNCROP_RIGHT,
            UNCROP_RIGHT_KEY_TEXT, UNCROP_RIGHT_KEY_LONGTEXT)
    add_key("key-random", KEY_RANDOM, RANDOM_KEY_TEXT, RANDOM_KEY_LONGTEXT)
    add_key("key-loop", KEY_LOOP, LOOP_KEY_TEXT, LOOP_KEY_LONGTEXT)

    add_key("key-viewpoint-fov-in", KEY_VIEWPOINT_FOV_IN,
            VIEWPOINT_FOV_IN_KEY_TEXT, VIEWPOINT_FOV_IN_KEY_TEXT)
    add_key("key-viewpoint-fov-out", KEY_VIEWPOINT_FOV_OUT,
            VIEWPOINT_FOV_OUT_KEY_TEXT, VIEWPOINT_FOV_OUT_KEY_TEXT)
    add_key("key-viewpoint-roll-clock", NULL,
            VIEWPOINT_ROLL_CLOCK_KEY_TEXT, VIEWPOINT_ROLL_CLOCK_KEY_TEXT)
    add_key("key-viewpoint-roll-anticlock", NULL,
            VIEWPOINT_ROLL_ANTICLOCK_KEY_TEXT,
            VIEWPOINT_ROLL_ANTICLOCK_KEY_TEXT)

    add_key("key-zoom-quarter", KEY_ZOOM_QUARTER, ZOOM_QUARTER_KEY_TEXT, NULL)
    add_key("key-zoom-half", KEY_ZOOM_HALF, ZOOM_HALF_KEY_TEXT, NULL)
    add_key("key-zoom-original", KEY_ZOOM_ORIGINAL,
            ZOOM_ORIGINAL_KEY_TEXT, NULL)
    add_key("key-zoom-double", KEY_ZOOM_DOUBLE, ZOOM_DOUBLE_KEY_TEXT, NULL)

    set_section ( N_("Jump sizes" ), NULL )
    add_integer( "extrashort-jump-size", 3, JIEXTRASHORT_TEXT,
                                    JIEXTRASHORT_LONGTEXT, false )
    add_integer( "short-jump-size", 10, JISHORT_TEXT,
                                    JISHORT_LONGTEXT, false )
    add_integer( "medium-jump-size", 60, JIMEDIUM_TEXT,
                                    JIMEDIUM_LONGTEXT, false )
    add_integer( "long-jump-size", 300, JILONG_TEXT,
                                    JILONG_LONGTEXT, false )

    /* HACK so these don't get displayed */
    set_category( -1 )
    set_subcategory( -1 )
    add_key("key-set-bookmark1", KEY_SET_BOOKMARK1,
            SET_BOOKMARK1_KEY_TEXT, SET_BOOKMARK_KEY_LONGTEXT)
    add_key("key-set-bookmark2", KEY_SET_BOOKMARK2,
            SET_BOOKMARK2_KEY_TEXT, SET_BOOKMARK_KEY_LONGTEXT)
    add_key("key-set-bookmark3", KEY_SET_BOOKMARK3,
            SET_BOOKMARK3_KEY_TEXT, SET_BOOKMARK_KEY_LONGTEXT)
    add_key("key-set-bookmark4", KEY_SET_BOOKMARK4,
            SET_BOOKMARK4_KEY_TEXT, SET_BOOKMARK_KEY_LONGTEXT)
    add_key("key-set-bookmark5", KEY_SET_BOOKMARK5,
            SET_BOOKMARK5_KEY_TEXT, SET_BOOKMARK_KEY_LONGTEXT)
    add_key("key-set-bookmark6", KEY_SET_BOOKMARK6,
            SET_BOOKMARK6_KEY_TEXT, SET_BOOKMARK_KEY_LONGTEXT)
    add_key("key-set-bookmark7", KEY_SET_BOOKMARK7,
            SET_BOOKMARK7_KEY_TEXT, SET_BOOKMARK_KEY_LONGTEXT)
    add_key("key-set-bookmark8", KEY_SET_BOOKMARK8,
            SET_BOOKMARK8_KEY_TEXT, SET_BOOKMARK_KEY_LONGTEXT)
    add_key("key-set-bookmark9", KEY_SET_BOOKMARK9,
            SET_BOOKMARK9_KEY_TEXT, SET_BOOKMARK_KEY_LONGTEXT)
    add_key("key-set-bookmark10", KEY_SET_BOOKMARK10,
            SET_BOOKMARK10_KEY_TEXT, SET_BOOKMARK_KEY_LONGTEXT)
    add_key("key-play-bookmark1", KEY_PLAY_BOOKMARK1,
            PLAY_BOOKMARK1_KEY_TEXT, PLAY_BOOKMARK_KEY_LONGTEXT)
    add_key("key-play-bookmark2", KEY_PLAY_BOOKMARK2,
            PLAY_BOOKMARK2_KEY_TEXT, PLAY_BOOKMARK_KEY_LONGTEXT)
    add_key("key-play-bookmark3", KEY_PLAY_BOOKMARK3,
            PLAY_BOOKMARK3_KEY_TEXT, PLAY_BOOKMARK_KEY_LONGTEXT)
    add_key("key-play-bookmark4", KEY_PLAY_BOOKMARK4,
            PLAY_BOOKMARK4_KEY_TEXT, PLAY_BOOKMARK_KEY_LONGTEXT)
    add_key("key-play-bookmark5", KEY_PLAY_BOOKMARK5,
            PLAY_BOOKMARK5_KEY_TEXT, PLAY_BOOKMARK_KEY_LONGTEXT)
    add_key("key-play-bookmark6", KEY_PLAY_BOOKMARK6,
            PLAY_BOOKMARK6_KEY_TEXT, PLAY_BOOKMARK_KEY_LONGTEXT)
    add_key("key-play-bookmark7", KEY_PLAY_BOOKMARK7,
            PLAY_BOOKMARK7_KEY_TEXT, PLAY_BOOKMARK_KEY_LONGTEXT)
    add_key("key-play-bookmark8", KEY_PLAY_BOOKMARK8,
            PLAY_BOOKMARK8_KEY_TEXT, PLAY_BOOKMARK_KEY_LONGTEXT)
    add_key("key-play-bookmark9", KEY_PLAY_BOOKMARK9,
            PLAY_BOOKMARK9_KEY_TEXT, PLAY_BOOKMARK_KEY_LONGTEXT)
    add_key("key-play-bookmark10", KEY_PLAY_BOOKMARK10,
            PLAY_BOOKMARK10_KEY_TEXT, PLAY_BOOKMARK_KEY_LONGTEXT)
    add_key("key-clear-playlist", KEY_PLAY_CLEAR,
            PLAY_CLEAR_KEY_TEXT, PLAY_CLEAR_KEY_LONGTEXT)

    add_key("key-subtitle-text-scale-normal", KEY_SUBTEXT_SCALE,
            SUBTEXT_SCALE_KEY_TEXT, SUBTEXT_SCALE_KEY_LONGTEXT)
    add_key("key-subtitle-text-scale-up", KEY_SUBTEXT_SCALEUP,
            SUBTEXT_SCALEUP_KEY_TEXT, SUBTEXT_SCALE_KEY_LONGTEXT)
    add_key("key-subtitle-text-scale-down", KEY_SUBTEXT_SCALEDOWN,
            SUBTEXT_SCALEDOWN_KEY_TEXT, SUBTEXT_SCALE_KEY_LONGTEXT)

    add_string( "bookmark1", NULL,
             BOOKMARK1_TEXT, BOOKMARK_LONGTEXT, false )
    add_string( "bookmark2", NULL,
             BOOKMARK2_TEXT, BOOKMARK_LONGTEXT, false )
    add_string( "bookmark3", NULL,
             BOOKMARK3_TEXT, BOOKMARK_LONGTEXT, false )
    add_string( "bookmark4", NULL,
             BOOKMARK4_TEXT, BOOKMARK_LONGTEXT, false )
    add_string( "bookmark5", NULL,
             BOOKMARK5_TEXT, BOOKMARK_LONGTEXT, false )
    add_string( "bookmark6", NULL,
             BOOKMARK6_TEXT, BOOKMARK_LONGTEXT, false )
    add_string( "bookmark7", NULL,
             BOOKMARK7_TEXT, BOOKMARK_LONGTEXT, false )
    add_string( "bookmark8", NULL,
             BOOKMARK8_TEXT, BOOKMARK_LONGTEXT, false )
    add_string( "bookmark9", NULL,
             BOOKMARK9_TEXT, BOOKMARK_LONGTEXT, false )
    add_string( "bookmark10", NULL,
              BOOKMARK10_TEXT, BOOKMARK_LONGTEXT, false )

#define HELP_TEXT \
    N_("print help for VLC (can be combined with --help-verbose)")
#define FULL_HELP_TEXT \
    N_("Exhaustive help for VLC and its modules")
#define LONGHELP_TEXT \
    N_("print help for VLC and all its modules (can be combined with " \
       "--help-verbose)")
#define HELP_VERBOSE_TEXT \
    N_("ask for extra verbosity when displaying help")
#define LIST_TEXT \
    N_("print a list of available modules")
#define LIST_VERBOSE_TEXT \
    N_("print a list of available modules with extra detail")
#define MODULE_TEXT \
    N_("print help on a specific module (can be combined with " \
       "--help-verbose). Prefix the module name with = for strict " \
       "matches.")
#define IGNORE_CONFIG_TEXT \
    N_("no configuration option will be loaded nor saved to config file")
#define RESET_CONFIG_TEXT \
    N_("reset the current config to the default values")
#define CONFIG_TEXT \
    N_("use alternate config file")
#define RESET_PLUGINS_CACHE_TEXT \
    N_("resets the current plugins cache")
#define VERSION_TEXT \
    N_("print version information")

    add_bool( "help", false, HELP_TEXT, "", false )
        change_short( 'h' )
        change_volatile ()
    add_bool( "full-help", false, FULL_HELP_TEXT, "", false )
        change_short( 'H' )
        change_volatile ()
    add_bool( "longhelp", false, LONGHELP_TEXT, "", false )
        change_volatile ()
    add_bool( "help-verbose", false, HELP_VERBOSE_TEXT, "",
              false )
        change_volatile ()
    add_bool( "list", false, LIST_TEXT, "", false )
        change_short( 'l' )
        change_volatile ()
    add_bool( "list-verbose", false, LIST_VERBOSE_TEXT, "",
              false )
        change_volatile ()
    add_string( "module", NULL, MODULE_TEXT, "", false )
        change_short( 'p' )
        change_volatile ()
    add_bool( "ignore-config", true, IGNORE_CONFIG_TEXT, "", false )
        change_volatile ()
    add_obsolete_bool( "save-config" )
    add_bool( "reset-config", false, RESET_CONFIG_TEXT, "", false )
        change_volatile ()
#ifdef HAVE_DYNAMIC_PLUGINS
    add_bool( "reset-plugins-cache", false,
              RESET_PLUGINS_CACHE_TEXT, "", false )
        change_volatile ()
#endif
    add_bool( "version", false, VERSION_TEXT, "", false )
        change_volatile ()
    add_string( "config", NULL, CONFIG_TEXT, "", false )
        change_volatile ()

    set_description( N_("core program") )
vlc_module_end ()

/*****************************************************************************
 * End configuration.
 *****************************************************************************/

#ifdef HAVE_DYNAMIC_PLUGINS
const char vlc_module_name[] = "main";
#endif
