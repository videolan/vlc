/*****************************************************************************
 * video.c : Video4Linux2 input module for vlc
 *****************************************************************************
 * Copyright (C) 2002-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Benjamin Pracht <bigben at videolan dot org>
 *          Richard Hosking <richard at hovis dot net>
 *          Antoine Cellerier <dionoea at videolan d.t org>
 *          Dennis Lou <dlou99 at yahoo dot com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*
 * Sections based on the reference V4L2 capture example at
 * http://v4l2spec.bytesex.org/spec/capture-example.html
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <poll.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>

#include "v4l2.h"

/*****************************************************************************
 * Module descriptior
 *****************************************************************************/

#define DEVICE_TEXT N_( "Device" )
#define DEVICE_LONGTEXT N_( \
    "Video device (Default: /dev/video0)." )
#define STANDARD_TEXT N_( "Standard" )
#define STANDARD_LONGTEXT N_( \
    "Video standard (Default, SECAM, PAL, or NTSC)." )
#define CHROMA_TEXT N_("Video input chroma format")
#define CHROMA_LONGTEXT N_( \
    "Force the Video4Linux2 video device to use a specific chroma format " \
    "(eg. I420 or I422 for raw images, MJPG for M-JPEG compressed input) " \
    "(Complete list: GREY, I240, RV16, RV15, RV24, RV32, YUY2, YUYV, UYVY, " \
    "I41N, I422, I420, I411, I410, MJPG)")
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
#define FPS_TEXT N_( "Framerate" )
#define FPS_LONGTEXT N_( "Framerate to capture, if applicable " \
    "(0 for autodetect)." )

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
#define BKLT_COMPENSATE_LONGTEXT N_( "Backlight compensation." )
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
    "To list available controls, increase verbosity (-vvv) " \
    "or use the v4l2-ctl application." )

#define TUNER_TEXT N_("Tuner id")
#define TUNER_LONGTEXT N_( \
    "Tuner id (see debug output)." )
#define FREQUENCY_TEXT N_("Frequency")
#define FREQUENCY_LONGTEXT N_( \
    "Tuner frequency in Hz or kHz (see debug output)" )
#define TUNER_AUDIO_MODE_TEXT N_("Audio mode")
#define TUNER_AUDIO_MODE_LONGTEXT N_( \
    "Tuner audio mono/stereo and track selection." )

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

#define V4L2_DEFAULT "/dev/video0"

vlc_module_begin ()
    set_shortname( N_("Video4Linux2") )
    set_description( N_("Video4Linux2 input") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )

    set_section( N_( "Video input" ), NULL )
    add_loadfile( CFG_PREFIX "dev", "/dev/video0",
                  DEVICE_TEXT, DEVICE_LONGTEXT, false )
        change_safe()
    add_string( CFG_PREFIX "standard", "",
                STANDARD_TEXT, STANDARD_LONGTEXT, false )
        change_string_list( standards_vlc, standards_user, NULL )
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
    add_float( CFG_PREFIX "fps", 0, FPS_TEXT, FPS_LONGTEXT, true )
        change_safe()
    add_obsolete_bool( CFG_PREFIX "use-libv4l2" ) /* since 2.1.0 */

    set_section( N_( "Tuner" ), NULL )
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

    add_shortcut( "v4l2" )
    set_capability( "access_demux", 0 )
    set_callbacks( DemuxOpen, DemuxClose )

    add_submodule ()
    add_shortcut( "v4l2", "v4l2c" )
    set_description( N_("Video4Linux2 Compressed A/V") )
    set_capability( "access", 0 )
    /* use these when open as access_demux fails; VLC will use another demux */
    set_callbacks( AccessOpen, AccessClose )

vlc_module_end ()

/*****************************************************************************
 * Access: local prototypes
 *****************************************************************************/

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

static v4l2_std_id var_InheritStandard (vlc_object_t *obj, const char *varname)
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

static int SetupStandard (vlc_object_t *obj, int fd,
                          const struct v4l2_input *restrict input)
{
#ifdef V4L2_IN_CAP_STD
    if (!(input->capabilities & V4L2_IN_CAP_STD))
    {
        msg_Dbg (obj, "no video standard selection");
        return 0;
    }
#else
    (void) input;
    msg_Dbg (obj, "video standard selection unknown");
#endif
    v4l2_std_id std = var_InheritStandard (obj, CFG_PREFIX"standard");
    if (std == V4L2_STD_UNKNOWN)
    {
        msg_Warn (obj, "video standard not set");
        return 0;
    }
    if (v4l2_ioctl (fd, VIDIOC_S_STD, &std) < 0)
    {
        msg_Err (obj, "cannot set video standard 0x%"PRIx64": %m", std);
        return -1;
    }
    msg_Dbg (obj, "video standard set to 0x%"PRIx64":", std);
    return 0;
}

static int SetupAudio (vlc_object_t *obj, int fd,
                       const struct v4l2_input *restrict input)
{
    if (input->audioset == 0)
    {
        msg_Dbg (obj, "no audio input available");
        return 0;
    }
    msg_Dbg (obj, "available audio inputs: 0x%08"PRIX32, input->audioset);

    uint32_t idx = var_InheritInteger (obj, CFG_PREFIX"audio-input");
    if (idx == (uint32_t)-1)
    {
        msg_Dbg (obj, "no audio input selected");
        return 0;
    }
    if (((1 << idx) & input->audioset) == 0)
    {
        msg_Warn (obj, "skipped unavailable audio input %"PRIu32, idx);
        return -1;
    }

    /* TODO: Enumerate other selectable audio inputs. How to expose them? */
    struct v4l2_audio enumaudio = { .index = idx };

    if (v4l2_ioctl (fd, VIDIOC_ENUMAUDIO, &enumaudio) < 0)
    {
        msg_Err (obj, "cannot get audio input %"PRIu32" properties: %m", idx);
        return -1;
    }

    msg_Dbg (obj, "audio input %s (%"PRIu32") is %s"
             " (capabilities: 0x%08"PRIX32")", enumaudio.name, enumaudio.index,
             (enumaudio.capability & V4L2_AUDCAP_STEREO) ? "Stereo" : "Mono",
             enumaudio.capability);
    if (enumaudio.capability & V4L2_AUDCAP_AVL)
        msg_Dbg (obj, " supports Automatic Volume Level");

    /* TODO: AVL mode */
    struct v4l2_audio audio = { .index = idx };

    if (v4l2_ioctl (fd, VIDIOC_S_AUDIO, &audio) < 0)
    {
        msg_Err (obj, "cannot select audio input %"PRIu32": %m", idx);
        return -1;
    }
    msg_Dbg (obj, "selected audio input %"PRIu32, idx);
    return 0;
}

static int SetupTuner (vlc_object_t *obj, int fd,
                       const struct v4l2_input *restrict input)
{
    switch (input->type)
    {
        case V4L2_INPUT_TYPE_TUNER:
            msg_Dbg (obj, "tuning required: tuner %"PRIu32, input->tuner);
            break;
        case V4L2_INPUT_TYPE_CAMERA:
            msg_Dbg (obj, "no tuning required (analog baseband input)");
            return 0;
        default:
            msg_Err (obj, "unknown input tuning type %"PRIu32, input->type);
            return 0; // hopefully we can stream regardless...
    }

    struct v4l2_tuner tuner = { .index = input->tuner };

    if (v4l2_ioctl (fd, VIDIOC_G_TUNER, &tuner) < 0)
    {
        msg_Err (obj, "cannot get tuner %"PRIu32" properties: %m",
                 input->tuner);
        return -1;
    }

    /* NOTE: This is overkill. Only video devices currently work, so the
     * type is always analog TV. */
    const char *typename, *mult;
    switch (tuner.type)
    {
        case V4L2_TUNER_RADIO:
            typename = "Radio";
            break;
        case V4L2_TUNER_ANALOG_TV:
            typename = "Analog TV";
            break;
        default:
            typename = "unknown";
    }
    mult = (tuner.capability & V4L2_TUNER_CAP_LOW) ? "" : "k";

    msg_Dbg (obj, "tuner %s (%"PRIu32") is %s", tuner.name, tuner.index,
             typename);
    msg_Dbg (obj, " ranges from %u.%u %sHz to %u.%c %sHz",
             (tuner.rangelow * 125) >> 1, (tuner.rangelow & 1) * 5, mult,
             (tuner.rangehigh * 125) >> 1, (tuner.rangehigh & 1) * 5,
             mult);

    /* TODO: only set video standard if the tuner requires it */

    /* Configure the audio mode */
    /* TODO: Ideally, L1 would be selected for stereo tuners, and L1_L2
     * for mono tuners. When dual-mono is detected after tuning on a stereo
     * tuner, we would fallback to L1_L2 too. Then we would flag dual-mono
     * for the audio E/S. Unfortunately, we have no access to the audio E/S
     * here (it belongs in the slave audio input...). */
    tuner.audmode = var_InheritInteger (obj, CFG_PREFIX"tuner-audio-mode");
    memset (tuner.reserved, 0, sizeof (tuner.reserved));

    if (tuner.capability & V4L2_TUNER_CAP_LANG1)
        msg_Dbg (obj, " supports primary audio language");
    else if (tuner.audmode == V4L2_TUNER_MODE_LANG1)
    {
        msg_Warn (obj, " falling back to stereo mode");
        tuner.audmode = V4L2_TUNER_MODE_STEREO;
    }
    if (tuner.capability & V4L2_TUNER_CAP_LANG2)
        msg_Dbg (obj, " supports secondary audio language or program");
    if (tuner.capability & V4L2_TUNER_CAP_STEREO)
        msg_Dbg (obj, " supports stereo audio");
    else if (tuner.audmode == V4L2_TUNER_MODE_STEREO)
    {
        msg_Warn (obj, " falling back to mono mode");
        tuner.audmode = V4L2_TUNER_MODE_MONO;
    }

    if (v4l2_ioctl (fd, VIDIOC_S_TUNER, &tuner) < 0)
    {
        msg_Err (obj, "cannot set tuner %"PRIu32" audio mode: %m",
                 input->tuner);
        return -1;
    }
    msg_Dbg (obj, "tuner %"PRIu32" audio mode %u set", input->tuner,
             tuner.audmode);

    /* Tune to the requested frequency */
    uint32_t freq = var_InheritInteger (obj, CFG_PREFIX"tuner-frequency");
    if (freq != (uint32_t)-1)
    {
        struct v4l2_frequency frequency = {
            .tuner = input->tuner,
            .type = V4L2_TUNER_ANALOG_TV,
            .frequency = freq * 125 / 2
        };

        if (v4l2_ioctl (fd, VIDIOC_S_FREQUENCY, &frequency) < 0)
        {
            msg_Err (obj, "cannot tuner tuner %u to frequency %u %sHz: %m",
                     input->tuner, freq, mult);
            return -1;
        }
    }
    msg_Dbg (obj, "tuner %"PRIu32" tuned to frequency %"PRIu32" %sHz",
             input->tuner, freq, mult);
    return 0;
}

static int ResetCrop (vlc_object_t *obj, int fd)
{
    struct v4l2_cropcap cropcap = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };

    /* In theory, this ioctl() must work for all video capture devices.
     * In practice, it does not. */
    if (v4l2_ioctl (fd, VIDIOC_CROPCAP, &cropcap) < 0)
    {
        msg_Warn (obj, "cannot get cropping properties: %m");
        return -1;
    }

    /* Reset to the default cropping rectangle */
    struct v4l2_crop crop = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .c = cropcap.defrect,
    };

    if (v4l2_ioctl (fd, VIDIOC_S_CROP, &crop) < 0)
    {
        msg_Warn (obj, "cannot reset cropping limits: %m");
        return -1;
    }
    return 0;
}

int SetupInput (vlc_object_t *obj, int fd)
{
    struct v4l2_input input;

    input.index = var_InheritInteger (obj, CFG_PREFIX"input");
    if (v4l2_ioctl (fd, VIDIOC_ENUMINPUT, &input) < 0)
    {
        msg_Err (obj, "invalid video input %"PRIu32": %m", input.index);
        return -1;
    }

    const char *typename = "unknown";
    switch (input.type)
    {
        case V4L2_INPUT_TYPE_TUNER:
            typename = "tuner";
            break;
        case V4L2_INPUT_TYPE_CAMERA:
            typename = "camera";
            break;
    }

    msg_Dbg (obj, "video input %s (%"PRIu32") is %s", input.name,
             input.index, typename);

    /* Select input */
    if (v4l2_ioctl (fd, VIDIOC_S_INPUT, &input.index) < 0)
    {
        msg_Err (obj, "cannot select input %"PRIu32": %m", input.index);
        return -1;
    }
    msg_Dbg (obj, "selected input %"PRIu32, input.index);

    SetupStandard (obj, fd, &input);
    SetupTuner (obj, fd, &input);
    SetupAudio (obj, fd, &input);
    return 0;
}

/** Compares two V4L2 fractions. */
static int64_t fcmp (const struct v4l2_fract *a,
                     const struct v4l2_fract *b)
{
    return (uint64_t)a->numerator * b->denominator
         - (uint64_t)b->numerator * a->denominator;
}

static const struct v4l2_fract infinity = { 1, 0 };

/**
 * Finds the highest frame rate possible of a certain V4L2 format.
 * @param fmt V4L2 capture format [IN]
 * @param it V4L2 frame interval [OUT]
 * @return 0 on success, -1 on error.
 */
static int FindMaxRate (vlc_object_t *obj, int fd,
                        const struct v4l2_format *restrict fmt,
                        struct v4l2_fract *restrict it)
{
    struct v4l2_frmivalenum fie = {
        .pixel_format = fmt->fmt.pix.pixelformat,
        .width = fmt->fmt.pix.width,
        .height = fmt->fmt.pix.height,
    };
    /* Mind that maximum rate means minimum interval */

    if (v4l2_ioctl (fd, VIDIOC_ENUM_FRAMEINTERVALS, &fie) < 0)
    {
        msg_Dbg (obj, "  unknown frame intervals: %m");
        /* Frame intervals cannot be enumerated. Set the format and then
         * get the streaming parameters to figure out the default frame
         * interval. This is not necessarily the maximum though. */
        struct v4l2_format dummy_fmt = *fmt;
        struct v4l2_streamparm parm = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };

        if (v4l2_ioctl (fd, VIDIOC_S_FMT, &dummy_fmt) < 0
         || v4l2_ioctl (fd, VIDIOC_G_PARM, &parm) < 0)
        {
            *it = infinity;
            return -1;
        }

        *it = parm.parm.capture.timeperframe;
        msg_Dbg (obj, "  %s frame interval: %"PRIu32"/%"PRIu32,
                 (parm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME)
                 ? "default" : "constant", it->numerator, it->denominator);
    }
    else
    switch (fie.type)
    {
        case V4L2_FRMIVAL_TYPE_DISCRETE:
            *it = infinity;
            do
            {
                if (fcmp (&fie.discrete, it) < 0)
                    *it = fie.discrete;
                fie.index++;
            }
            while (v4l2_ioctl (fd, VIDIOC_ENUM_FRAMEINTERVALS, &fie) >= 0);

            msg_Dbg (obj, "  %s frame interval: %"PRIu32"/%"PRIu32,
                     "discrete", it->numerator, it->denominator);
            break;

        case V4L2_FRMIVAL_TYPE_STEPWISE:
        case V4L2_FRMIVAL_TYPE_CONTINUOUS:
            msg_Dbg (obj, "  frame intervals from %"PRIu32"/%"PRIu32
                     "to %"PRIu32"/%"PRIu32" supported",
                     fie.stepwise.min.numerator, fie.stepwise.min.denominator,
                     fie.stepwise.max.numerator, fie.stepwise.max.denominator);
            if (fie.type == V4L2_FRMIVAL_TYPE_STEPWISE)
                msg_Dbg (obj, "  with %"PRIu32"/%"PRIu32" step",
                         fie.stepwise.step.numerator,
                         fie.stepwise.step.denominator);
            *it = fie.stepwise.min;
            break;
    }
    return 0;
}

#undef SetupFormat
/**
 * Finds the best possible frame rate and resolution.
 * @param fourcc pixel format
 * @param fmt V4L2 capture format [OUT]
 * @param parm V4L2 capture streaming parameters [OUT]
 * @return 0 on success, -1 on failure.
 */
int SetupFormat (vlc_object_t *obj, int fd, uint32_t fourcc,
                 struct v4l2_format *restrict fmt,
                 struct v4l2_streamparm *restrict parm)
{
    memset (fmt, 0, sizeof (*fmt));
    fmt->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    memset (parm, 0, sizeof (*parm));
    parm->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (v4l2_ioctl (fd, VIDIOC_G_FMT, fmt) < 0)
    {
        msg_Err (obj, "cannot get default format: %m");
        return -1;
    }
    fmt->fmt.pix.pixelformat = fourcc;

    struct v4l2_frmsizeenum fse = {
        .pixel_format = fourcc,
    };
    struct v4l2_fract best_it = infinity;
    uint64_t best_area = 0;

    uint32_t width = var_InheritInteger (obj, CFG_PREFIX"width");
    uint32_t height = var_InheritInteger (obj, CFG_PREFIX"height");
    if (width > 0 && height > 0)
    {
        fmt->fmt.pix.width = width;
        fmt->fmt.pix.height = height;
        msg_Dbg (obj, " requested frame size: %"PRIu32"x%"PRIu32,
                 width, height);
        FindMaxRate (obj, fd, fmt, &best_it);
    }
    else
    if (v4l2_ioctl (fd, VIDIOC_ENUM_FRAMESIZES, &fse) < 0)
    {
        /* Fallback to current format, try to maximize frame rate */
        msg_Dbg (obj, " unknown frame sizes: %m");
        msg_Dbg (obj, " current frame size: %"PRIu32"x%"PRIu32,
                 fmt->fmt.pix.width, fmt->fmt.pix.height);
        FindMaxRate (obj, fd, fmt, &best_it);
    }
    else
    switch (fse.type)
    {
        case V4L2_FRMSIZE_TYPE_DISCRETE:
            do
            {
                struct v4l2_fract cur_it;

                msg_Dbg (obj, " frame size %"PRIu32"x%"PRIu32,
                         fse.discrete.width, fse.discrete.height);
                FindMaxRate (obj, fd, fmt, &cur_it);

                int64_t c = fcmp (&cur_it, &best_it);
                uint64_t area = fse.discrete.width * fse.discrete.height;
                if (c < 0 || (c == 0 && area > best_area))
                {
                    best_it = cur_it;
                    best_area = area;
                    fmt->fmt.pix.width = fse.discrete.width;
                    fmt->fmt.pix.height = fse.discrete.height;
                }

                fse.index++;
            }
            while (v4l2_ioctl (fd, VIDIOC_ENUM_FRAMESIZES, &fse) >= 0);

            msg_Dbg (obj, " best discrete frame size: %"PRIu32"x%"PRIu32,
                     fmt->fmt.pix.width, fmt->fmt.pix.height);
            break;

        case V4L2_FRMSIZE_TYPE_STEPWISE:
        case V4L2_FRMSIZE_TYPE_CONTINUOUS:
            msg_Dbg (obj, " frame sizes from %"PRIu32"x%"PRIu32" to "
                     "%"PRIu32"x%"PRIu32" supported",
                     fse.stepwise.min_width, fse.stepwise.min_height,
                     fse.stepwise.max_width, fse.stepwise.max_height);
            if (fse.type == V4L2_FRMSIZE_TYPE_STEPWISE)
                msg_Dbg (obj, "  with %"PRIu32"x%"PRIu32" steps",
                         fse.stepwise.step_width, fse.stepwise.step_height);

            /* FIXME: slow and dumb */
            for (uint32_t width =  fse.stepwise.min_width;
                          width <= fse.stepwise.max_width;
                          width += fse.stepwise.step_width)
                for (uint32_t height =  fse.stepwise.min_height;
                              height <= fse.stepwise.max_width;
                              height += fse.stepwise.step_height)
                {
                    struct v4l2_fract cur_it;

                    FindMaxRate (obj, fd, fmt, &cur_it);

                    int64_t c = fcmp (&cur_it, &best_it);
                    uint64_t area = width * height;

                    if (c < 0 || (c == 0 && area > best_area))
                    {
                        best_it = cur_it;
                        best_area = area;
                        fmt->fmt.pix.width = width;
                        fmt->fmt.pix.height = height;
                    }
                }

            msg_Dbg (obj, " best frame size: %"PRIu32"x%"PRIu32,
                     fmt->fmt.pix.width, fmt->fmt.pix.height);
            break;
    }

    /* Set the final format */
    if (v4l2_ioctl (fd, VIDIOC_S_FMT, fmt) < 0)
    {
        msg_Err (obj, "cannot set format: %m");
        return -1;
    }

    /* Now that the final format is set, fetch and override parameters */
    if (v4l2_ioctl (fd, VIDIOC_G_PARM, parm) < 0)
    {
        msg_Err (obj, "cannot get streaming parameters: %m");
        return -1;
    }
    parm->parm.capture.capturemode = 0; /* normal video mode */
    parm->parm.capture.extendedmode = 0;
    if (best_it.denominator != 0)
        parm->parm.capture.timeperframe = best_it;
    if (v4l2_ioctl (fd, VIDIOC_S_PARM, parm) < 0)
        msg_Warn (obj, "cannot set streaming parameters: %m");

    ResetCrop (obj, fd); /* crop depends on frame size */

    return 0;
}


/*****************************************************************************
 * GrabVideo: Grab a video frame
 *****************************************************************************/
block_t* GrabVideo (vlc_object_t *demux, demux_sys_t *sys)
{
    struct v4l2_buffer buf = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_MMAP,
    };

    /* Wait for next frame */
    if (v4l2_ioctl (sys->i_fd, VIDIOC_DQBUF, &buf) < 0)
    {
        switch (errno)
        {
            case EAGAIN:
                return NULL;
            case EIO:
                /* Could ignore EIO, see spec. */
                /* fall through */
            default:
                msg_Err (demux, "dequeue error: %m");
                return NULL;
        }
    }

    if (buf.index >= sys->i_nbuffers) {
        msg_Err (demux, "Failed capturing new frame as i>=nbuffers");
        return NULL;
    }

    /* Copy frame */
    block_t *block = block_Alloc (buf.bytesused);
    if (unlikely(block == NULL))
        return NULL;
    memcpy (block->p_buffer, sys->p_buffers[buf.index].start, buf.bytesused);

    /* Unlock */
    if (v4l2_ioctl (sys->i_fd, VIDIOC_QBUF, &buf) < 0)
    {
        msg_Err (demux, "queue error: %m");
        block_Release (block);
        return NULL;
    }
    return block;
}

/*****************************************************************************
 * Helper function to initalise video IO using the mmap method
 *****************************************************************************/
int InitMmap( vlc_object_t *p_demux, demux_sys_t *p_sys, int i_fd )
{
    struct v4l2_requestbuffers req;

    memset( &req, 0, sizeof(req) );
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if( v4l2_ioctl( i_fd, VIDIOC_REQBUFS, &req ) < 0 )
    {
        msg_Err( p_demux, "device does not support mmap I/O" );
        return -1;
    }

    if( req.count < 2 )
    {
        msg_Err( p_demux, "insufficient buffers" );
        return -1;
    }

    p_sys->p_buffers = calloc( req.count, sizeof( *p_sys->p_buffers ) );
    if( unlikely(!p_sys->p_buffers) )
        return -1;

    for( p_sys->i_nbuffers = 0; p_sys->i_nbuffers < req.count; ++p_sys->i_nbuffers )
    {
        struct v4l2_buffer buf;

        memset( &buf, 0, sizeof(buf) );
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = p_sys->i_nbuffers;

        if( v4l2_ioctl( i_fd, VIDIOC_QUERYBUF, &buf ) < 0 )
        {
            msg_Err( p_demux, "VIDIOC_QUERYBUF: %m" );
            return -1;
        }

        p_sys->p_buffers[p_sys->i_nbuffers].length = buf.length;
        p_sys->p_buffers[p_sys->i_nbuffers].start =
            v4l2_mmap( NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, i_fd, buf.m.offset );

        if( p_sys->p_buffers[p_sys->i_nbuffers].start == MAP_FAILED )
        {
            msg_Err( p_demux, "mmap failed: %m" );
            return -1;
        }
    }

    return 0;
}
