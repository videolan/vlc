/*****************************************************************************
 * v4l2.c : Video4Linux2 input module for VLC
 *****************************************************************************
 * Copyright (C) 2002-2009 VLC authors and VideoLAN
 * Copyright (C) 2011-2012 Rémi Denis-Courmont
 *
 * Authors: Benjamin Pracht <bigben at videolan dot org>
 *          Richard Hosking <richard at hovis dot net>
 *          Antoine Cellerier <dionoea at videolan d.t org>
 *          Dennis Lou <dlou99 at yahoo dot com>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_fs.h>

#include "v4l2.h"

#define VIDEO_DEVICE_TEXT N_( "Video capture device" )
#define VIDEO_DEVICE_LONGTEXT N_("Video capture device node." )
#define VBI_DEVICE_TEXT N_("VBI capture device")
#define VBI_DEVICE_LONGTEXT N_( \
    "The device node where VBI data can be read "   \
    "(for closed captions)." )
#define STANDARD_TEXT N_( "Standard" )
#define STANDARD_LONGTEXT N_( \
    "Video standard (Default, SECAM, PAL, or NTSC)." )
#define CHROMA_TEXT N_("Video input chroma format")
#define CHROMA_LONGTEXT N_( \
    "Force the Video4Linux2 video device to use a specific chroma format " \
    "(eg. I420 or I422 for raw images, MJPG for M-JPEG compressed input). " \
    "Complete list: GREY, I240, RV16, RV15, RV24, RV32, YUY2, YUYV, UYVY, " \
    "I41N, I422, I420, I411, I410, MJPG")
#define INPUT_TEXT N_( "Input" )
#define INPUT_LONGTEXT N_( \
    "Input of the card to use (see debug)." )
#define AUDIO_INPUT_TEXT N_( "Audio input" )
#define AUDIO_INPUT_LONGTEXT N_( \
    "Audio input of the card to use (see debug)." )
#define WIDTH_TEXT N_( "Width" )
#define HEIGHT_TEXT N_( "Height" )
#define SIZE_LONGTEXT N_( \
    "The specified pixel resolution is forced " \
    "(if both width and height are strictly positive)." )
#define FPS_TEXT N_( "Frame rate" )
#define FPS_LONGTEXT N_( "Maximum frame rate to use (0 = no limits)." )

#define RADIO_DEVICE_TEXT N_( "Radio device" )
#define RADIO_DEVICE_LONGTEXT N_("Radio tuner device node." )
#define FREQUENCY_TEXT N_("Frequency")
#define FREQUENCY_LONGTEXT N_( \
    "Tuner frequency in Hz or kHz (see debug output)." )
#define TUNER_AUDIO_MODE_TEXT N_("Audio mode")
#define TUNER_AUDIO_MODE_LONGTEXT N_( \
    "Tuner audio mono/stereo and track selection." )

#define CTRL_RESET_TEXT N_( "Reset controls" )
#define CTRL_RESET_LONGTEXT N_( "Reset controls to defaults." )
#define BRIGHTNESS_TEXT N_( "Brightness" )
#define BRIGHTNESS_LONGTEXT N_( "Picture brightness or black level." )
#define BRIGHTNESS_AUTO_TEXT N_( "Automatic brightness" )
#define BRIGHTNESS_AUTO_LONGTEXT N_( \
    "Automatically adjust the picture brightness." )
#define CONTRAST_TEXT N_( "Contrast" )
#define CONTRAST_LONGTEXT N_( "Picture contrast or luma gain." )
#define SATURATION_TEXT N_( "Saturation" )
#define SATURATION_LONGTEXT N_( "Picture saturation or chroma gain." )
#define HUE_TEXT N_( "Hue" )
#define HUE_LONGTEXT N_( "Hue or color balance." )
#define HUE_AUTO_TEXT N_( "Automatic hue" )
#define HUE_AUTO_LONGTEXT N_( \
    "Automatically adjust the picture hue." )
#define WHITE_BALANCE_TEMP_TEXT N_( "White balance temperature (K)" )
#define WHITE_BALANCE_TEMP_LONGTEXT N_( \
    "White balance temperature as a color temperation in Kelvin " \
    "(2800 is minimum incandescence, 6500 is maximum daylight)." )
#define AUTOWHITEBALANCE_TEXT N_( "Automatic white balance" )
#define AUTOWHITEBALANCE_LONGTEXT N_( \
    "Automatically adjust the picture white balance." )
#define REDBALANCE_TEXT N_( "Red balance" )
#define REDBALANCE_LONGTEXT N_( \
    "Red chroma balance." )
#define BLUEBALANCE_TEXT N_( "Blue balance" )
#define BLUEBALANCE_LONGTEXT N_( \
    "Blue chroma balance." )
#define GAMMA_TEXT N_( "Gamma" )
#define GAMMA_LONGTEXT N_( \
    "Gamma adjust." )
#define AUTOGAIN_TEXT N_( "Automatic gain" )
#define AUTOGAIN_LONGTEXT N_( \
    "Automatically set the video gain." )
#define GAIN_TEXT N_( "Gain" )
#define GAIN_LONGTEXT N_( \
    "Picture gain." )
#define SHARPNESS_TEXT N_( "Sharpness" )
#define SHARPNESS_LONGTEXT N_( "Sharpness filter adjust." )
#define CHROMA_GAIN_TEXT N_( "Chroma gain" )
#define CHROMA_GAIN_LONGTEXT N_( "Chroma gain control." )
#define CHROMA_GAIN_AUTO_TEXT N_( "Automatic chroma gain" )
#define CHROMA_GAIN_AUTO_LONGTEXT N_( \
    "Automatically control the chroma gain." )
#define POWER_FREQ_TEXT N_( "Power line frequency" )
#define POWER_FREQ_LONGTEXT N_( \
    "Power line frequency anti-flicker filter." )
static const int power_freq_vlc[] = { -1,
    V4L2_CID_POWER_LINE_FREQUENCY_DISABLED,
    V4L2_CID_POWER_LINE_FREQUENCY_50HZ,
    V4L2_CID_POWER_LINE_FREQUENCY_60HZ,
    V4L2_CID_POWER_LINE_FREQUENCY_AUTO,
};
static const char *const power_freq_user[] = { N_("Unspecified"),
    N_("Off"), N_("50 Hz"), N_("60 Hz"), N_("Automatic"),
};
#define BKLT_COMPENSATE_TEXT N_( "Backlight compensation" )
#define BKLT_COMPENSATE_LONGTEXT BKLT_COMPENSATE_TEXT
#define BAND_STOP_FILTER_TEXT N_( "Band-stop filter" )
#define BAND_STOP_FILTER_LONGTEXT N_(  \
    "Cut a light band induced by fluorescent lighting (unit undocumented)." )
#define HFLIP_TEXT N_( "Horizontal flip" )
#define HFLIP_LONGTEXT N_( \
    "Flip the picture horizontally." )
#define VFLIP_TEXT N_( "Vertical flip" )
#define VFLIP_LONGTEXT N_( \
    "Flip the picture vertically." )
#define ROTATE_TEXT N_( "Rotate (degrees)" )
#define ROTATE_LONGTEXT N_( "Picture rotation angle (in degrees)." )
#define COLOR_KILLER_TEXT N_( "Color killer" )
#define COLOR_KILLER_LONGTEXT N_( \
    "Enable the color killer, i.e. switch to black & white picture " \
    "whenever the signal is weak." )
#define COLOR_EFFECT_TEXT N_( "Color effect" )
#define COLOR_EFFECT_LONGTEXT N_( "Select a color effect." )
static const int colorfx_vlc[] = { -1, V4L2_COLORFX_NONE,
    V4L2_COLORFX_BW, V4L2_COLORFX_SEPIA, V4L2_COLORFX_NEGATIVE,
    V4L2_COLORFX_EMBOSS, V4L2_COLORFX_SKETCH, V4L2_COLORFX_SKY_BLUE,
    V4L2_COLORFX_GRASS_GREEN, V4L2_COLORFX_SKIN_WHITEN, V4L2_COLORFX_VIVID,
};
static const char *const colorfx_user[] = { N_("Unspecified"), N_("None"),
    N_("Black & white"), N_("Sepia"), N_("Negative"),
    N_("Emboss"), N_("Sketch"), N_("Sky blue"),
    N_("Grass green"), N_("Skin whiten"), N_("Vivid"),
};

#define AUDIO_VOLUME_TEXT N_( "Audio volume" )
#define AUDIO_VOLUME_LONGTEXT N_( \
    "Volume of the audio input." )
#define AUDIO_BALANCE_TEXT N_( "Audio balance" )
#define AUDIO_BALANCE_LONGTEXT N_( \
    "Balance of the audio input." )
#define AUDIO_BASS_TEXT N_( "Bass level" )
#define AUDIO_BASS_LONGTEXT N_( \
    "Bass adjustment of the audio input." )
#define AUDIO_TREBLE_TEXT N_( "Treble level" )
#define AUDIO_TREBLE_LONGTEXT N_( \
    "Treble adjustment of the audio input." )
#define AUDIO_MUTE_TEXT N_( "Mute" )
#define AUDIO_MUTE_LONGTEXT N_( \
    "Mute the audio." )
#define AUDIO_LOUDNESS_TEXT N_( "Loudness mode" )
#define AUDIO_LOUDNESS_LONGTEXT N_( \
    "Loudness mode a.k.a. bass boost." )

#define S_CTRLS_TEXT N_("v4l2 driver controls")
#define S_CTRLS_LONGTEXT N_( \
    "Set the v4l2 driver controls to the values specified using a comma " \
    "separated list optionally encapsulated by curly braces " \
    "(e.g.: {video_bitrate=6000000,audio_crc=0,stream_type=3} ). " \
    "To list available controls, increase verbosity (-vv) " \
    "or use the v4l2-ctl application." )

#define ASPECT_TEXT N_("Picture aspect-ratio n:m")
#define ASPECT_LONGTEXT N_("Define input picture aspect-ratio to use. Default is 4:3" )

static const int tristate_vlc[] = { -1, 0, 1 };
static const char *const tristate_user[] = {
    N_("Unspecified"), N_("Off"), N_("On") };

static const v4l2_std_id standards_v4l2[] = { V4L2_STD_UNKNOWN, V4L2_STD_ALL,
    V4L2_STD_PAL,     V4L2_STD_PAL_BG,   V4L2_STD_PAL_DK,
    V4L2_STD_NTSC,
    V4L2_STD_SECAM,   V4L2_STD_SECAM_DK,
    V4L2_STD_MTS,     V4L2_STD_525_60,  V4L2_STD_625_50,
    V4L2_STD_ATSC,

    V4L2_STD_B,       V4L2_STD_G,        V4L2_STD_H,        V4L2_STD_L,
    V4L2_STD_GH,      V4L2_STD_DK,       V4L2_STD_BG,       V4L2_STD_MN,

    V4L2_STD_PAL_B,   V4L2_STD_PAL_B1,   V4L2_STD_PAL_G,    V4L2_STD_PAL_H,
    V4L2_STD_PAL_I,   V4L2_STD_PAL_D,    V4L2_STD_PAL_D1,   V4L2_STD_PAL_K,
    V4L2_STD_PAL_M,   V4L2_STD_PAL_N,    V4L2_STD_PAL_Nc,   V4L2_STD_PAL_60,
    V4L2_STD_NTSC_M,  V4L2_STD_NTSC_M_JP,V4L2_STD_NTSC_443, V4L2_STD_NTSC_M_KR,
    V4L2_STD_SECAM_B, V4L2_STD_SECAM_D,  V4L2_STD_SECAM_G,  V4L2_STD_SECAM_H,
    V4L2_STD_SECAM_K, V4L2_STD_SECAM_K1, V4L2_STD_SECAM_L,  V4L2_STD_SECAM_LC,
    V4L2_STD_ATSC_8_VSB, V4L2_STD_ATSC_16_VSB,
};
static const char *const standards_vlc[] = { "", "ALL",
    /* Pseudo standards */
    "PAL", "PAL_BG", "PAL_DK",
    "NTSC",
    "SECAM", "SECAM_DK",
    "MTS", "525_60", "625_50",
    "ATSC",

    /* Chroma-agnostic ITU standards (PAL/NTSC or PAL/SECAM) */
    "B",              "G",               "H",               "L",
    "GH",             "DK",              "BG",              "MN",

    /* Individual standards */
    "PAL_B",          "PAL_B1",          "PAL_G",           "PAL_H",
    "PAL_I",          "PAL_D",           "PAL_D1",          "PAL_K",
    "PAL_M",          "PAL_N",           "PAL_Nc",          "PAL_60",
    "NTSC_M",         "NTSC_M_JP",       "NTSC_443",        "NTSC_M_KR",
    "SECAM_B",        "SECAM_D",         "SECAM_G",         "SECAM_H",
    "SECAM_K",        "SECAM_K1",        "SECAM_L",         "SECAM_LC",
    "ATSC_8_VSB",     "ATSC_16_VSB",
};
static const char *const standards_user[] = { N_("Undefined"), N_("All"),
    "PAL",            "PAL B/G",         "PAL D/K",
    "NTSC",
    "SECAM",          "SECAM D/K",
    N_("Multichannel television sound (MTS)"),
    N_("525 lines / 60 Hz"), N_("625 lines / 50 Hz"),
    "ATSC",

    "PAL/SECAM B",    "PAL/SECAM G",     "PAL/SECAM H",     "PAL/SECAM L",
    "PAL/SECAM G/H",  "PAL/SECAM D/K",   "PAL/SECAM B/G",   "PAL/NTSC M/N",

    "PAL B",          "PAL B1",          "PAL G",           "PAL H",
    "PAL I",          "PAL D",           "PAL D1",          "PAL K",
    "PAL M",          "PAL N",           N_("PAL N Argentina"), "PAL 60",
    "NTSC M",        N_("NTSC M Japan"), "NTSC 443",  N_("NTSC M South Korea"),
    "SECAM B",        "SECAM D",         "SECAM G",         "SECAM H",
    "SECAM K",        "SECAM K1",        "SECAM L",         "SECAM L/C",
    "ATSC 8-VSB",     "ATSC 16-VSB",
};

static const int i_tuner_audio_modes_list[] = {
      V4L2_TUNER_MODE_MONO, V4L2_TUNER_MODE_STEREO,
      V4L2_TUNER_MODE_LANG1, V4L2_TUNER_MODE_LANG2, V4L2_TUNER_MODE_LANG1_LANG2
};
static const char *const psz_tuner_audio_modes_list_text[] = {
      N_("Mono"),
      N_("Stereo"),
      N_("Primary language"),
      N_("Secondary language or program"),
      N_("Dual mono" )
};

vlc_module_begin ()
    set_shortname( N_("V4L") )
    set_description( N_("Video4Linux input") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )

    set_section( N_( "Video input" ), NULL )
    add_loadfile( CFG_PREFIX "dev", "/dev/video0",
                  VIDEO_DEVICE_TEXT, VIDEO_DEVICE_LONGTEXT, false )
        change_safe()
#ifdef ZVBI_COMPILED
    add_loadfile( CFG_PREFIX "vbidev", NULL,
                  VBI_DEVICE_TEXT, VBI_DEVICE_LONGTEXT, false )
#endif
    add_string( CFG_PREFIX "standard", "",
                STANDARD_TEXT, STANDARD_LONGTEXT, false )
        change_string_list( standards_vlc, standards_user )
        change_safe()
    add_string( CFG_PREFIX "chroma", NULL, CHROMA_TEXT, CHROMA_LONGTEXT,
                true )
        change_safe()
    add_integer( CFG_PREFIX "input", 0, INPUT_TEXT, INPUT_LONGTEXT,
                true )
        change_integer_range( 0, 0xFFFFFFFE )
        change_safe()
    add_integer( CFG_PREFIX "audio-input", -1, AUDIO_INPUT_TEXT,
                 AUDIO_INPUT_LONGTEXT, true )
        change_integer_range( -1, 0xFFFFFFFE )
        change_safe()
    add_obsolete_integer( CFG_PREFIX "io" ) /* since 2.0.0 */
    add_integer( CFG_PREFIX "width", 0, WIDTH_TEXT, SIZE_LONGTEXT, false )
        change_integer_range( 0, VOUT_MAX_WIDTH )
        change_safe()
    add_integer( CFG_PREFIX "height", 0, HEIGHT_TEXT, SIZE_LONGTEXT, false )
        change_integer_range( 0, VOUT_MAX_WIDTH )
        change_safe()
    add_string( CFG_PREFIX "aspect-ratio", "4:3", ASPECT_TEXT,
              ASPECT_LONGTEXT, true )
        change_safe()
    add_string( CFG_PREFIX "fps", "60", FPS_TEXT, FPS_LONGTEXT, false )
        change_safe()
    add_obsolete_bool( CFG_PREFIX "use-libv4l2" ) /* since 2.1.0 */

    set_section( N_( "Tuner" ), NULL )
    add_loadfile( CFG_PREFIX "radio-dev", "/dev/radio0",
                  RADIO_DEVICE_TEXT, RADIO_DEVICE_LONGTEXT, false )
        change_safe()
    add_obsolete_integer( CFG_PREFIX "tuner" ) /* since 2.1.0 */
    add_integer( CFG_PREFIX "tuner-frequency", -1, FREQUENCY_TEXT,
                 FREQUENCY_LONGTEXT, true )
        change_integer_range( -1, 0xFFFFFFFE )
        change_safe()
    add_integer( CFG_PREFIX "tuner-audio-mode", V4L2_TUNER_MODE_LANG1,
                 TUNER_AUDIO_MODE_TEXT, TUNER_AUDIO_MODE_LONGTEXT, true )
        change_integer_list( i_tuner_audio_modes_list,
                             psz_tuner_audio_modes_list_text )
        change_safe()

    set_section( N_( "Controls" ),
                 N_( "Video capture controls (if supported by the device)" ) )
    add_bool( CFG_PREFIX "controls-reset", false, CTRL_RESET_TEXT,
              CTRL_RESET_LONGTEXT, true )
        change_safe()
    add_integer( CFG_PREFIX "brightness", -1, BRIGHTNESS_TEXT,
                 BRIGHTNESS_LONGTEXT, true )
    add_integer( CFG_PREFIX "brightness-auto", -1,
                 BRIGHTNESS_AUTO_TEXT, BRIGHTNESS_AUTO_LONGTEXT, true )
        change_integer_list( tristate_vlc, tristate_user )
    add_integer( CFG_PREFIX "contrast", -1, CONTRAST_TEXT,
                 CONTRAST_LONGTEXT, true )
    add_integer( CFG_PREFIX "saturation", -1, SATURATION_TEXT,
                 SATURATION_LONGTEXT, true )
    add_integer( CFG_PREFIX "hue", -1, HUE_TEXT,
                 HUE_LONGTEXT, true )
    add_integer( CFG_PREFIX "hue-auto", -1,
                 HUE_AUTO_TEXT, HUE_AUTO_LONGTEXT, true )
        change_integer_list( tristate_vlc, tristate_user )
    add_obsolete_integer( CFG_PREFIX "black-level" ) /* since Linux 2.6.26 */
    add_integer( CFG_PREFIX "white-balance-temperature", -1,
                 WHITE_BALANCE_TEMP_TEXT, WHITE_BALANCE_TEMP_LONGTEXT, true )
        /* Ideally, the range should be 2800-6500 */
        change_integer_range( -1, 6500 )
    add_integer( CFG_PREFIX "auto-white-balance", -1,
                 AUTOWHITEBALANCE_TEXT, AUTOWHITEBALANCE_LONGTEXT, true )
        change_integer_list( tristate_vlc, tristate_user )
    add_obsolete_integer( CFG_PREFIX"do-white-balance" ) /* since 2.0.0 */
    add_integer( CFG_PREFIX "red-balance", -1, REDBALANCE_TEXT,
                 REDBALANCE_LONGTEXT, true )
    add_integer( CFG_PREFIX "blue-balance", -1, BLUEBALANCE_TEXT,
                 BLUEBALANCE_LONGTEXT, true )
    add_integer( CFG_PREFIX "gamma", -1, GAMMA_TEXT,
                 GAMMA_LONGTEXT, true )
    add_integer( CFG_PREFIX "autogain", -1, AUTOGAIN_TEXT,
                 AUTOGAIN_LONGTEXT, true )
        change_integer_list( tristate_vlc, tristate_user )
    add_integer( CFG_PREFIX "gain", -1, GAIN_TEXT,
                 GAIN_LONGTEXT, true )
    add_integer( CFG_PREFIX "sharpness", -1,
                 SHARPNESS_TEXT, SHARPNESS_LONGTEXT, true )
    add_integer( CFG_PREFIX "chroma-gain", -1,
                 CHROMA_GAIN_TEXT, CHROMA_GAIN_LONGTEXT, true )
    add_integer( CFG_PREFIX "chroma-gain-auto", -1,
                 CHROMA_GAIN_AUTO_TEXT, CHROMA_GAIN_AUTO_LONGTEXT, true )
    add_integer( CFG_PREFIX"power-line-frequency", -1,
                 POWER_FREQ_TEXT, POWER_FREQ_LONGTEXT, true )
        change_integer_list( power_freq_vlc, power_freq_user )
    add_integer( CFG_PREFIX"backlight-compensation", -1,
                 BKLT_COMPENSATE_TEXT, BKLT_COMPENSATE_LONGTEXT, true )
    add_integer( CFG_PREFIX "band-stop-filter", -1,
                 BAND_STOP_FILTER_TEXT, BAND_STOP_FILTER_LONGTEXT, true )
    add_bool( CFG_PREFIX "hflip", false, HFLIP_TEXT, HFLIP_LONGTEXT, true )
    add_bool( CFG_PREFIX "vflip", false, VFLIP_TEXT, VFLIP_LONGTEXT, true )
    add_integer( CFG_PREFIX "rotate", -1, ROTATE_TEXT, ROTATE_LONGTEXT, true )
        change_integer_range( -1, 359 )
    add_obsolete_integer( CFG_PREFIX "hcenter" ) /* since Linux 2.6.26 */
    add_obsolete_integer( CFG_PREFIX "vcenter" ) /* since Linux 2.6.26 */
    add_integer( CFG_PREFIX"color-killer", -1,
                 COLOR_KILLER_TEXT, COLOR_KILLER_LONGTEXT, true )
        change_integer_list( tristate_vlc, tristate_user )
    add_integer( CFG_PREFIX"color-effect", -1,
                 COLOR_EFFECT_TEXT, COLOR_EFFECT_LONGTEXT, true )
        change_integer_list( colorfx_vlc, colorfx_user )

    add_integer( CFG_PREFIX "audio-volume", -1, AUDIO_VOLUME_TEXT,
                AUDIO_VOLUME_LONGTEXT, true )
    add_integer( CFG_PREFIX "audio-balance", -1, AUDIO_BALANCE_TEXT,
                AUDIO_BALANCE_LONGTEXT, true )
    add_bool( CFG_PREFIX "audio-mute", false, AUDIO_MUTE_TEXT,
              AUDIO_MUTE_LONGTEXT, true )
    add_integer( CFG_PREFIX "audio-bass", -1, AUDIO_BASS_TEXT,
                AUDIO_BASS_LONGTEXT, true )
    add_integer( CFG_PREFIX "audio-treble", -1, AUDIO_TREBLE_TEXT,
                AUDIO_TREBLE_LONGTEXT, true )
    add_bool( CFG_PREFIX "audio-loudness", false, AUDIO_LOUDNESS_TEXT,
              AUDIO_LOUDNESS_LONGTEXT, true )
    add_string( CFG_PREFIX "set-ctrls", NULL, S_CTRLS_TEXT,
              S_CTRLS_LONGTEXT, true )
        change_safe()

    add_obsolete_string( CFG_PREFIX "adev" )
    add_obsolete_integer( CFG_PREFIX "audio-method" )
    add_obsolete_bool( CFG_PREFIX "stereo" )
    add_obsolete_integer( CFG_PREFIX "samplerate" )

    add_shortcut( "v4l", "v4l2" )
    set_capability( "access_demux", 0 )
    set_callbacks( DemuxOpen, DemuxClose )

    add_submodule ()
    add_shortcut( "v4l", "v4l2", "v4l2c" )
    set_description( N_("Video4Linux compressed A/V input") )
    set_capability( "access", 0 )
    /* use these when open as access_demux fails; VLC will use another demux */
    set_callbacks( AccessOpen, AccessClose )

    add_submodule ()
    add_shortcut ("radio" /*, "fm", "am" */)
    set_description (N_("Video4Linux radio tuner"))
    set_capability ("access_demux", 0)
    set_callbacks (RadioOpen, RadioClose)

vlc_module_end ()

/**
 * Parses a V4L2 MRL into VLC object variables.
 */
void ParseMRL( vlc_object_t *obj, const char *mrl )
{
    const char *p = strchr( mrl, ':' );
    char *dev = NULL;

    if( p != NULL )
    {
        var_LocationParse( obj, p + 1, CFG_PREFIX );
        if( p > mrl )
            dev = strndup( mrl, p - mrl );
    }
    else
    {
        if( mrl[0] )
            dev = strdup( mrl );
    }

    if( dev != NULL )
    {
        var_Create( obj, CFG_PREFIX"dev", VLC_VAR_STRING );
        var_SetString( obj, CFG_PREFIX"dev", dev );
        free( dev );
    }
}

int OpenDevice (vlc_object_t *obj, const char *path, uint32_t *restrict caps)
{
    msg_Dbg (obj, "opening device '%s'", path);

    int rawfd = vlc_open (path, O_RDWR);
    if (rawfd == -1)
    {
        msg_Err (obj, "cannot open device '%s': %s", path,
                 vlc_strerror_c(errno));
        return -1;
    }

    int fd = v4l2_fd_open (rawfd, 0);
    if (fd == -1)
    {
        msg_Warn (obj, "cannot initialize user-space library: %s",
                  vlc_strerror_c(errno));
        /* fallback to direct kernel mode anyway */
        fd = rawfd;
    }

    /* Get device capabilites */
    struct v4l2_capability cap;
    if (v4l2_ioctl (fd, VIDIOC_QUERYCAP, &cap) < 0)
    {
        msg_Err (obj, "cannot get device capabilities: %s",
                 vlc_strerror_c(errno));
        v4l2_close (fd);
        return -1;
    }

    msg_Dbg (obj, "device %s using driver %s (version %u.%u.%u) on %s",
            cap.card, cap.driver, (cap.version >> 16) & 0xFF,
            (cap.version >> 8) & 0xFF, cap.version & 0xFF, cap.bus_info);

    if (cap.capabilities & V4L2_CAP_DEVICE_CAPS)
    {
        msg_Dbg (obj, " with capabilities 0x%08"PRIX32" "
                 "(overall 0x%08"PRIX32")", cap.device_caps, cap.capabilities);
        *caps = cap.device_caps;
    }
    else
    {
        msg_Dbg (obj, " with unknown capabilities  "
                 "(overall 0x%08"PRIX32")", cap.capabilities);
        *caps = cap.capabilities;
    }
    return fd;
}

v4l2_std_id var_InheritStandard (vlc_object_t *obj, const char *varname)
{
    char *name = var_InheritString (obj, varname);
    if (name == NULL)
        return V4L2_STD_UNKNOWN;

    const size_t n = sizeof (standards_vlc) / sizeof (*standards_vlc);

    static_assert (sizeof (standards_vlc) / sizeof (*standards_vlc)
                         == sizeof (standards_v4l2) / sizeof (*standards_v4l2),
                   "Inconsistent standards tables");
    static_assert (sizeof (standards_vlc) / sizeof (*standards_vlc)
                         == sizeof (standards_user) / sizeof (*standards_user),
                   "Inconsistent standards tables");

    for (size_t i = 0; i < n; i++)
        if (strcasecmp (name, standards_vlc[i]) == 0)
        {
            free (name);
            return standards_v4l2[i];
        }

    /* Backward compatibility with old versions using V4L2 magic numbers */
    char *end;
    v4l2_std_id std = strtoull (name, &end, 0);
    if (*end != '\0')
    {
        msg_Err (obj, "unknown video standard \"%s\"", name);
        std = V4L2_STD_UNKNOWN;
    }
    free (name);
    return std;
}
