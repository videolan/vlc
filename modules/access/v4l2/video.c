/*****************************************************************************
 * v4l2.c : Video4Linux2 input module for vlc
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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

#include "v4l2.h"
#include <vlc_plugin.h>
#include <vlc_fs.h>
#include <vlc_demux.h>

#include <math.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <poll.h>

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
#define WIDTH_LONGTEXT N_( \
    "Force width (-1 for autodetect, 0 for driver default)." )
#define HEIGHT_TEXT N_( "Height" )
#define HEIGHT_LONGTEXT N_( \
    "Force height (-1 for autodetect, 0 for driver default)." )
#define FPS_TEXT N_( "Framerate" )
#define FPS_LONGTEXT N_( "Framerate to capture, if applicable " \
    "(0 for autodetect)." )

#ifdef HAVE_LIBV4L2
#define LIBV4L2_TEXT N_( "Use libv4l2" )
#define LIBV4L2_LONGTEXT N_( \
    "Force usage of the libv4l2 wrapper." )
#endif

#define CTRL_RESET_TEXT N_( "Reset controls" )
#define CTRL_RESET_LONGTEXT N_( "Reset controls to defaults." )
#define BRIGHTNESS_TEXT N_( "Brightness" )
#define BRIGHTNESS_LONGTEXT N_( "Picture brightness or black level." )
#define CONTRAST_TEXT N_( "Contrast" )
#define CONTRAST_LONGTEXT N_( "Picture contrast or luma gain." )
#define SATURATION_TEXT N_( "Saturation" )
#define SATURATION_LONGTEXT N_( "Picture saturation or chroma gain." )
#define HUE_TEXT N_( "Hue" )
#define HUE_LONGTEXT N_( "Hue or color balance." )
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
#define EXPOSURE_TEXT N_( "Exposure" )
#define EXPOSURE_LONGTEXT N_( \
    "Exposure." )
#define AUTOGAIN_TEXT N_( "Automatic gain/exposure" )
#define AUTOGAIN_LONGTEXT N_( \
    "Automatically set the video gain or exposure." )
#define GAIN_TEXT N_( "Gain" )
#define GAIN_LONGTEXT N_( \
    "Picture gain." )
#define HFLIP_TEXT N_( "Horizontal flip" )
#define HFLIP_LONGTEXT N_( \
    "Flip the picture horizontally." )
#define VFLIP_TEXT N_( "Vertical flip" )
#define VFLIP_LONGTEXT N_( \
    "Flip the picture vertically." )

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
    V4L2_STD_525_60,  V4L2_STD_625_50,
    V4L2_STD_ATSC,

    V4L2_STD_MN,      V4L2_STD_B,        V4L2_STD_GH,       V4L2_STD_DK,

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
    "525_60", "625_50",
    "ATSC",

    /* Areas (PAL/NTSC or PAL/SECAM) */
    "MN", "B", "GH", "DK",

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
    N_("525 lines / 60 Hz"), N_("625 lines / 50 Hz"),
    "ATSC",

    "PAL/NTSC M/N",
    "PAL/SECAM B",    "PAL/SECAM G/H",   "PAL/SECAM D/K",

    "PAL B",          "PAL B1",          "PAL G",           "PAL H",
    "PAL I",          "PAL D",           "PAL D1",          "PAL K",
    "PAL M",          "PAL N",           N_("PAL N Argentina"), "PAL 60",
    "NTSC M",        N_("NTSC M Japan"), "NTSC 443",  N_("NTSC M South Korea"),
    "SECAM B",        "SECAM D",         "SECAM G",         "SECAM H",
    "SECAM K",        "SECAM K1",        "SECAM L",         "SECAM L/C",
    "ATSC 8-VSB",     "ATSC 16-VSB",
};

static const int i_tuner_audio_modes_list[] = {
      -1, V4L2_TUNER_MODE_MONO, V4L2_TUNER_MODE_STEREO,
      V4L2_TUNER_MODE_LANG1, V4L2_TUNER_MODE_LANG2,
      V4L2_TUNER_MODE_SAP, V4L2_TUNER_MODE_LANG1_LANG2 };
static const char *const psz_tuner_audio_modes_list_text[] = {
      N_("Unspecified"),
      N_( "Mono" ),
      N_( "Stereo" ),
      N_( "Primary language (Analog TV tuners only)" ),
      N_( "Secondary language (Analog TV tuners only)" ),
      N_( "Second audio program (Analog TV tuners only)" ),
      N_( "Primary language left, Secondary language right" ) };

#define V4L2_DEFAULT "/dev/video0"

#ifdef HAVE_MAEMO
# define DEFAULT_WIDTH	640
# define DEFAULT_HEIGHT	492
#endif

#ifndef DEFAULT_WIDTH
# define DEFAULT_WIDTH	(-1)
# define DEFAULT_HEIGHT	(-1)
#endif

vlc_module_begin ()
    set_shortname( N_("Video4Linux2") )
    set_description( N_("Video4Linux2 input") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )

    set_section( N_( "Video input" ), NULL )
    add_string( CFG_PREFIX "dev", "/dev/video0", DEVICE_TEXT, DEVICE_LONGTEXT,
                 false )
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
    add_obsolete_integer( CFG_PREFIX "io" ) /* since 1.2.0 */
    add_integer( CFG_PREFIX "width", DEFAULT_WIDTH, WIDTH_TEXT,
                WIDTH_LONGTEXT, true )
        change_safe()
    add_integer( CFG_PREFIX "height", DEFAULT_HEIGHT, HEIGHT_TEXT,
                HEIGHT_LONGTEXT, true )
        change_safe()
    add_string( CFG_PREFIX "aspect-ratio", "4:3", ASPECT_TEXT,
              ASPECT_LONGTEXT, true )
        change_safe()
    add_float( CFG_PREFIX "fps", 0, FPS_TEXT, FPS_LONGTEXT, true )
        change_safe()
#ifdef HAVE_LIBV4L2
    add_bool( CFG_PREFIX "use-libv4l2", false, LIBV4L2_TEXT, LIBV4L2_LONGTEXT, true );
#endif

    set_section( N_( "Tuner" ), NULL )
    add_integer( CFG_PREFIX "tuner", 0, TUNER_TEXT, TUNER_LONGTEXT,
                 true )
        change_integer_range( 0, 0xFFFFFFFE )
        change_safe()
    add_integer( CFG_PREFIX "tuner-frequency", -1, FREQUENCY_TEXT,
                 FREQUENCY_LONGTEXT, true )
        change_integer_range( -1, 0xFFFFFFFE )
        change_safe()
    add_integer( CFG_PREFIX "tuner-audio-mode", -1, TUNER_AUDIO_MODE_TEXT,
                 TUNER_AUDIO_MODE_LONGTEXT, true )
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
    add_integer( CFG_PREFIX "contrast", -1, CONTRAST_TEXT,
                 CONTRAST_LONGTEXT, true )
    add_integer( CFG_PREFIX "saturation", -1, SATURATION_TEXT,
                 SATURATION_LONGTEXT, true )
    add_integer( CFG_PREFIX "hue", -1, HUE_TEXT,
                 HUE_LONGTEXT, true )
    add_obsolete_integer( CFG_PREFIX "black-level" ) /* since Linux 2.6.26 */
    add_integer( CFG_PREFIX "auto-white-balance", -1,
                 AUTOWHITEBALANCE_TEXT, AUTOWHITEBALANCE_LONGTEXT, true )
        change_integer_list( tristate_vlc, tristate_user )
    add_obsolete_integer( CFG_PREFIX"do-white-balance" ) /* since 1.2.0 */
    add_integer( CFG_PREFIX "red-balance", -1, REDBALANCE_TEXT,
                 REDBALANCE_LONGTEXT, true )
    add_integer( CFG_PREFIX "blue-balance", -1, BLUEBALANCE_TEXT,
                 BLUEBALANCE_LONGTEXT, true )
    add_integer( CFG_PREFIX "gamma", -1, GAMMA_TEXT,
                 GAMMA_LONGTEXT, true )
    add_integer( CFG_PREFIX "exposure", -1, EXPOSURE_TEXT,
                 EXPOSURE_LONGTEXT, true )
    add_integer( CFG_PREFIX "autogain", -1, AUTOGAIN_TEXT,
                 AUTOGAIN_LONGTEXT, true )
        change_integer_list( tristate_vlc, tristate_user )
    add_integer( CFG_PREFIX "gain", -1, GAIN_TEXT,
                 GAIN_LONGTEXT, true )
    add_bool( CFG_PREFIX "hflip", false, HFLIP_TEXT, HFLIP_LONGTEXT, true )
    add_bool( CFG_PREFIX "vflip", false, VFLIP_TEXT, VFLIP_LONGTEXT, true )
    add_obsolete_integer( CFG_PREFIX "hcenter" ) /* since Linux 2.6.26 */
    add_obsolete_integer( CFG_PREFIX "vcenter" ) /* since Linux 2.6.26 */
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

static block_t* ProcessVideoFrame( vlc_object_t *p_demux, uint8_t *p_frame, size_t );

static const struct
{
    unsigned int i_v4l2;
    vlc_fourcc_t i_fourcc;
    int i_rmask;
    int i_gmask;
    int i_bmask;
} v4l2chroma_to_fourcc[] =
{
    /* Raw data types */
    { V4L2_PIX_FMT_GREY,    VLC_CODEC_GREY, 0, 0, 0 },
    { V4L2_PIX_FMT_HI240,   VLC_FOURCC('I','2','4','0'), 0, 0, 0 },
    { V4L2_PIX_FMT_RGB555,  VLC_CODEC_RGB15, 0x001f,0x03e0,0x7c00 },
    { V4L2_PIX_FMT_RGB565,  VLC_CODEC_RGB16, 0x001f,0x07e0,0xf800 },
    /* Won't work since we don't know how to handle such gmask values
     * correctly
    { V4L2_PIX_FMT_RGB555X, VLC_CODEC_RGB15, 0x007c,0xe003,0x1f00 },
    { V4L2_PIX_FMT_RGB565X, VLC_CODEC_RGB16, 0x00f8,0xe007,0x1f00 },
    */
    { V4L2_PIX_FMT_BGR24,   VLC_CODEC_RGB24, 0xff0000,0xff00,0xff },
    { V4L2_PIX_FMT_RGB24,   VLC_CODEC_RGB24, 0xff,0xff00,0xff0000 },
    { V4L2_PIX_FMT_BGR32,   VLC_CODEC_RGB32, 0xff0000,0xff00,0xff },
    { V4L2_PIX_FMT_RGB32,   VLC_CODEC_RGB32, 0xff,0xff00,0xff0000 },
    { V4L2_PIX_FMT_YUYV,    VLC_CODEC_YUYV, 0, 0, 0 },
    { V4L2_PIX_FMT_UYVY,    VLC_CODEC_UYVY, 0, 0, 0 },
    { V4L2_PIX_FMT_Y41P,    VLC_FOURCC('I','4','1','N'), 0, 0, 0 },
    { V4L2_PIX_FMT_YUV422P, VLC_CODEC_I422, 0, 0, 0 },
    { V4L2_PIX_FMT_YVU420,  VLC_CODEC_YV12, 0, 0, 0 },
    { V4L2_PIX_FMT_YUV411P, VLC_CODEC_I411, 0, 0, 0 },
    { V4L2_PIX_FMT_YUV410,  VLC_CODEC_I410, 0, 0, 0 },

    /* Raw data types, not in V4L2 spec but still in videodev2.h and supported
     * by VLC */
    { V4L2_PIX_FMT_YUV420,  VLC_CODEC_I420, 0, 0, 0 },
    /* FIXME { V4L2_PIX_FMT_RGB444,  VLC_CODEC_RGB32 }, */

    /* Compressed data types */
    { V4L2_PIX_FMT_MJPEG,   VLC_CODEC_MJPG, 0, 0, 0 },
    { V4L2_PIX_FMT_JPEG,    VLC_CODEC_JPEG, 0, 0, 0 },
#if 0
    { V4L2_PIX_FMT_DV,      VLC_FOURCC('?','?','?','?') },
    { V4L2_PIX_FMT_MPEG,    VLC_FOURCC('?','?','?','?') },
#endif
    { 0, 0, 0, 0, 0 }
};

/**
 * List of V4L2 chromas were confident enough to use as fallbacks if the
 * user hasn't provided a --v4l2-chroma value.
 *
 * Try YUV chromas first, then RGB little endian and MJPEG as last resort.
 */
static const uint32_t p_chroma_fallbacks[] =
{ V4L2_PIX_FMT_YUV420, V4L2_PIX_FMT_YVU420, V4L2_PIX_FMT_YUV422P,
  V4L2_PIX_FMT_YUYV, V4L2_PIX_FMT_UYVY, V4L2_PIX_FMT_BGR24,
  V4L2_PIX_FMT_BGR32, V4L2_PIX_FMT_MJPEG, V4L2_PIX_FMT_JPEG };

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

/*****************************************************************************
 * GrabVideo: Grab a video frame
 *****************************************************************************/
block_t* GrabVideo( vlc_object_t *p_demux, demux_sys_t *p_sys )
{
    block_t *p_block;
    struct v4l2_buffer buf;

    /* Grab Video Frame */
    switch( p_sys->io )
    {
    case IO_METHOD_MMAP:
        memset( &buf, 0, sizeof(buf) );
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        /* Wait for next frame */
        if (v4l2_ioctl( p_sys->i_fd, VIDIOC_DQBUF, &buf ) < 0 )
        {
            switch( errno )
            {
            case EAGAIN:
                return NULL;
            case EIO:
                /* Could ignore EIO, see spec. */
                /* fall through */
            default:
                msg_Err( p_demux, "Failed to wait (VIDIOC_DQBUF)" );
                return NULL;
               }
        }

        if( buf.index >= p_sys->i_nbuffers ) {
            msg_Err( p_demux, "Failed capturing new frame as i>=nbuffers" );
            return NULL;
        }

        p_block = ProcessVideoFrame( p_demux, p_sys->p_buffers[buf.index].start, buf.bytesused );
        if( !p_block )
            return NULL;

        /* Unlock */
        if( v4l2_ioctl( p_sys->i_fd, VIDIOC_QBUF, &buf ) < 0 )
        {
            msg_Err( p_demux, "Failed to unlock (VIDIOC_QBUF)" );
            block_Release( p_block );
            return NULL;
        }

        break;

    case IO_METHOD_USERPTR:
        memset( &buf, 0, sizeof(buf) );
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_USERPTR;

        /* Wait for next frame */
        if (v4l2_ioctl( p_sys->i_fd, VIDIOC_DQBUF, &buf ) < 0 )
        {
            switch( errno )
            {
            case EAGAIN:
                return NULL;
            case EIO:
                /* Could ignore EIO, see spec. */
                /* fall through */
            default:
                msg_Err( p_demux, "Failed to wait (VIDIOC_DQBUF)" );
                return NULL;
            }
        }

        /* Find frame? */
        unsigned int i;
        for( i = 0; i < p_sys->i_nbuffers; i++ )
        {
            if( buf.m.userptr == (unsigned long)p_sys->p_buffers[i].start &&
                buf.length == p_sys->p_buffers[i].length ) break;
        }

        if( i >= p_sys->i_nbuffers )
        {
            msg_Err( p_demux, "Failed capturing new frame as i>=nbuffers" );
            return NULL;
        }

        p_block = ProcessVideoFrame( p_demux, (uint8_t*)buf.m.userptr, buf.bytesused );
        if( !p_block )
            return NULL;

        /* Unlock */
        if( v4l2_ioctl( p_sys->i_fd, VIDIOC_QBUF, &buf ) < 0 )
        {
            msg_Err( p_demux, "Failed to unlock (VIDIOC_QBUF)" );
            block_Release( p_block );
            return NULL;
        }
        break;
    default:
        assert(0);
    }
    return p_block;
}

/*****************************************************************************
 * ProcessVideoFrame: Helper function to take a buffer and copy it into
 * a new block
 *****************************************************************************/
static block_t* ProcessVideoFrame( vlc_object_t *p_demux, uint8_t *p_frame, size_t i_size )
{
    block_t *p_block;

    if( !p_frame ) return NULL;

    /* New block */
    if( !( p_block = block_New( p_demux, i_size ) ) )
    {
        msg_Warn( p_demux, "Cannot get new block" );
        return NULL;
    }

    /* Copy frame */
    memcpy( p_block->p_buffer, p_frame, i_size );

    return p_block;
}

/*****************************************************************************
 * Helper function to initalise video IO using the mmap method
 *****************************************************************************/
static int InitMmap( vlc_object_t *p_demux, demux_sys_t *p_sys, int i_fd )
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

/*****************************************************************************
 * Helper function to initalise video IO using the userbuf method
 *****************************************************************************/
static int InitUserP( vlc_object_t *p_demux, demux_sys_t *p_sys, int i_fd, unsigned int i_buffer_size )
{
    struct v4l2_requestbuffers req;
    unsigned int i_page_size;

    i_page_size = sysconf(_SC_PAGESIZE);
    i_buffer_size = ( i_buffer_size + i_page_size - 1 ) & ~( i_page_size - 1);

    memset( &req, 0, sizeof(req) );
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_USERPTR;

    if( v4l2_ioctl( i_fd, VIDIOC_REQBUFS, &req ) < 0 )
    {
        msg_Err( p_demux, "device does not support user pointer i/o" );
        return -1;
    }

    p_sys->p_buffers = calloc( 4, sizeof( *p_sys->p_buffers ) );
    if( !p_sys->p_buffers )
        return -1;

    for( p_sys->i_nbuffers = 0; p_sys->i_nbuffers < 4; ++p_sys->i_nbuffers )
    {
        p_sys->p_buffers[p_sys->i_nbuffers].length = i_buffer_size;
        if( posix_memalign( &p_sys->p_buffers[p_sys->i_nbuffers].start,
                /* boundary */ i_page_size, i_buffer_size ) )
            return -1;
    }

    return 0;
}

/**
 * \return true if the specified V4L2 pixel format is
 * in the array of supported formats returned by the driver
 */
static bool IsPixelFormatSupported( struct v4l2_fmtdesc *codecs, size_t n,
                                    unsigned int i_pixelformat )
{
    for( size_t i = 0; i < n; i++ )
        if( codecs[i].pixelformat == i_pixelformat )
            return true;
    return false;
}


static int InitVideo( vlc_object_t *p_obj, int i_fd, demux_sys_t *p_sys,
                      bool b_demux );

/**
 * Opens and sets up a video device
 * \return file descriptor or -1 on error
 */
int OpenVideo( vlc_object_t *obj, demux_sys_t *sys, bool b_demux )
{
    char *path = var_InheritString( obj, CFG_PREFIX"dev" );
    if( unlikely(path == NULL) )
        return -1; /* probably OOM */

    msg_Dbg( obj, "opening device '%s'", path );

    int fd = vlc_open( path, O_RDWR );
    if( fd == -1 )
    {
        msg_Err( obj, "cannot open device '%s': %m", path );
        free( path );
        return -1;
    }
    free( path );
#ifdef HAVE_LIBV4L2
    if( !var_InheritBool( obj, CFG_PREFIX "use-libv4l2" ) )
    {
        msg_Dbg( obj, "trying kernel V4L2" );
        if( InitVideo( obj, fd, sys, b_demux ) == 0 )
            return fd;
    }
    msg_Dbg( obj, "trying library V4L2" );
    /* Note the v4l2_xxx functions are designed so that if they get passed an
       unknown fd, the will behave exactly as their regular xxx counterparts,
       so if v4l2_fd_open fails, we continue as normal (missing the libv4l2
       custom cam format to normal formats conversion). Chances are big we will
       still fail then though, as normally v4l2_fd_open only fails if the
       device is not a v4l2 device. */
    int libfd = v4l2_fd_open( fd, 0 );
    if( libfd == -1 )
        goto error;
    fd = libfd;
#endif
    if( InitVideo( obj, fd, sys, b_demux ) )
        goto error;

    return fd;
error:
    close( fd );
    return -1;
}

static int InitVideo( vlc_object_t *p_obj, int i_fd, demux_sys_t *p_sys,
                      bool b_demux )
{
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format fmt;
    unsigned int i_min;
    enum v4l2_buf_type buf_type;
    es_format_t es_fmt;

    /* Get device capabilites */
    struct v4l2_capability cap;
    if( v4l2_ioctl( i_fd, VIDIOC_QUERYCAP, &cap ) < 0 )
    {
        msg_Err( p_obj, "cannot get video capabilities: %m" );
        return -1;
    }

    msg_Dbg( p_obj, "device %s using driver %s (version %u.%u.%u) on %s",
            cap.card, cap.driver, (cap.version >> 16) & 0xFF,
            (cap.version >> 8) & 0xFF, cap.version & 0xFF, cap.bus_info );
    msg_Dbg( p_obj, "the device has the capabilities: 0x%08X",
             cap.capabilities );
    msg_Dbg( p_obj, " (%c) Video Capture, (%c) Audio, (%c) Tuner, (%c) Radio",
             ( cap.capabilities & V4L2_CAP_VIDEO_CAPTURE  ? 'X':' '),
             ( cap.capabilities & V4L2_CAP_AUDIO  ? 'X':' '),
             ( cap.capabilities & V4L2_CAP_TUNER  ? 'X':' '),
             ( cap.capabilities & V4L2_CAP_RADIO  ? 'X':' ') );
    msg_Dbg( p_obj, " (%c) Read/Write, (%c) Streaming, (%c) Asynchronous",
            ( cap.capabilities & V4L2_CAP_READWRITE ? 'X':' ' ),
            ( cap.capabilities & V4L2_CAP_STREAMING ? 'X':' ' ),
            ( cap.capabilities & V4L2_CAP_ASYNCIO ? 'X':' ' ) );

    if( cap.capabilities & V4L2_CAP_STREAMING )
        p_sys->io = IO_METHOD_MMAP;
    else if( cap.capabilities & V4L2_CAP_READWRITE )
        p_sys->io = IO_METHOD_READ;
    else
    {
        msg_Err( p_obj, "no supported I/O method" );
        return -1;
    }

    /* Now, enumerate all the video inputs. This is useless at the moment
       since we have no way to present that info to the user except with
       debug messages */
    if( cap.capabilities & V4L2_CAP_VIDEO_CAPTURE )
    {
        struct v4l2_input input;
        unsigned index = var_InheritInteger( p_obj, CFG_PREFIX"input" );

        input.index = 0;
        while( v4l2_ioctl( i_fd, VIDIOC_ENUMINPUT, &input ) >= 0 )
        {
            msg_Dbg( p_obj, "video input %u (%s) has type: %s %c",
                     input.index, input.name,
                     input.type == V4L2_INPUT_TYPE_TUNER
                          ? "Tuner adapter" : "External analog input",
                     input.index == index ? '*' : ' ' );
            input.index++;
        }

        /* Select input */
        if( v4l2_ioctl( i_fd, VIDIOC_S_INPUT, &index ) < 0 )
        {
            msg_Err( p_obj, "cannot set input %u: %m", index );
            return -1;
        }
        msg_Dbg( p_obj, "input set to %u", index );
    }

    /* Select standard */
    bool bottom_first;
    const char *stdname = var_InheritString( p_obj, CFG_PREFIX"standard" );
    if( stdname != NULL )
    {
        v4l2_std_id std = strtoull( stdname, NULL, 0 );
        if( std == 0 )
        {
            const size_t n = sizeof(standards_vlc) / sizeof(*standards_vlc);
            assert( n == sizeof(standards_v4l2) / sizeof(*standards_v4l2) );
            assert( n == sizeof(standards_user) / sizeof(*standards_user) );
            for( size_t i = 0; i < n; i++ )
                if( strcasecmp( stdname, standards_vlc[i] ) == 0 )
                {
                    std = standards_v4l2[i];
                    break;
                }
        }

        if( v4l2_ioctl( i_fd, VIDIOC_S_STD, &std ) < 0
         || v4l2_ioctl( i_fd, VIDIOC_G_STD, &std ) < 0 )
        {
            msg_Err( p_obj, "cannot set standard 0x%"PRIx64": %m", std );
            return -1;
        }
        msg_Dbg( p_obj, "standard set to 0x%"PRIx64":", std );
        bottom_first = std == V4L2_STD_NTSC;
    }
    else
        bottom_first = false;

    /* Set audio input */
    if( cap.capabilities & V4L2_CAP_AUDIO )
    {
        struct v4l2_audio audio = { .index = 0 };
        uint32_t idx = var_InheritInteger( p_obj, CFG_PREFIX"audio-input" );

        /* Probe audio inputs */
        while( v4l2_ioctl( i_fd, VIDIOC_ENUMAUDIO, &audio ) >= 0 )
        {
            msg_Dbg( p_obj, "audio input %u (%s) is %s%s %c", audio.index,
                     audio.name,
                     audio.capability & V4L2_AUDCAP_STEREO ? "Stereo" : "Mono",
                     audio.capability & V4L2_AUDCAP_AVL
                         ? " (Automatic Volume Level supported)" : "",
                     audio.index == idx );
            audio.index++;
        }

        if( idx != (uint32_t)-1 )
        {
            memset( &audio, 0, sizeof(audio) );
            audio.index = idx;
            /* TODO: AVL support (audio.mode) */

            if( v4l2_ioctl( i_fd, VIDIOC_S_AUDIO, &audio ) < 0 )
            {
                msg_Err( p_obj, "cannot set audio input %"PRIu32": %m", idx );
                return -1;
            }
            msg_Dbg( p_obj, "audio input set to %"PRIu32, idx );
        }
    }

    /* List tuner caps */
    if( cap.capabilities & V4L2_CAP_TUNER )
    {
        struct v4l2_tuner tuner;
        uint32_t idx = var_CreateGetInteger( p_obj, CFG_PREFIX"tuner" );
        enum v4l2_tuner_type type = V4L2_TUNER_RADIO;

        tuner.index = 0;
        while( v4l2_ioctl( i_fd, VIDIOC_G_TUNER, &tuner ) >= 0 )
        {
            if( tuner.index == idx )
                type = tuner.type;

            const char *unit =
                (tuner.capability & V4L2_TUNER_CAP_LOW) ? "Hz" : "kHz";
            msg_Dbg( p_obj, "tuner %u (%s) has type: %s, "
                     "frequency range: %.1f %s -> %.1f %s", tuner.index,
                     tuner.name,
                     tuner.type == V4L2_TUNER_RADIO ? "Radio" : "Analog TV",
                     tuner.rangelow * 62.5, unit,
                     tuner.rangehigh * 62.5, unit );

            struct v4l2_frequency frequency = { .tuner = tuner.index };
            if( v4l2_ioctl( i_fd, VIDIOC_G_FREQUENCY, &frequency ) < 0 )
            {
                msg_Err( p_obj, "cannot get tuner frequency: %m" );
                return -1;
            }
            msg_Dbg( p_obj, "tuner %u (%s) frequency: %.1f %s", tuner.index,
                     tuner.name, frequency.frequency * 62.5, unit );
            tuner.index++;
        }

        /* Tune the tuner */
        uint32_t freq = var_InheritInteger( p_obj,
                                            CFG_PREFIX"tuner-frequency" );
        if( freq != (uint32_t)-1 )
        {
            struct v4l2_frequency frequency = {
                .tuner = idx,
                .type = type,
                .frequency = freq / 62.5,
            };

            if( v4l2_ioctl( i_fd, VIDIOC_S_FREQUENCY, &frequency ) < 0 )
            {
                msg_Err( p_obj, "cannot set tuner frequency: %m" );
                return -1;
            }
            msg_Dbg( p_obj, "tuner frequency set" );
        }

        /* Set the tuner audio mode */
        int32_t audmode = var_InheritInteger( p_obj,
                                              CFG_PREFIX"tuner-audio-mode" );
        if( audmode >= 0 )
        {
            struct v4l2_tuner tuner = {
                .index = idx,
                .audmode = audmode,
            };

            if( v4l2_ioctl( i_fd, VIDIOC_S_TUNER, &tuner ) < 0 )
            {
                msg_Err( p_obj, "cannot set tuner audio mode: %m" );
                return -1;
            }
            msg_Dbg( p_obj, "tuner audio mode set" );
        }
    }

    /* Probe for available chromas */
    struct v4l2_fmtdesc *codecs = NULL;
    uint_fast32_t ncodec = 0;
    if( cap.capabilities & V4L2_CAP_VIDEO_CAPTURE )
    {
        struct v4l2_fmtdesc codec = {
            .index = 0,
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        };

        while( v4l2_ioctl( i_fd, VIDIOC_ENUM_FMT, &codec ) >= 0 )
            codec.index = ++ncodec;

        codecs = malloc( ncodec * sizeof( *codecs ) );
        if( unlikely(codecs == NULL) )
            ncodec = 0;

        for( uint_fast32_t i = 0; i < ncodec; i++ )
        {
            codecs[i].index = i;
            codecs[i].type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

            if( v4l2_ioctl( i_fd, VIDIOC_ENUM_FMT, &codecs[i] ) < 0 )
            {
                msg_Err( p_obj, "cannot get codec description: %m" );
                goto error;
            }

            /* only print if vlc supports the format */
            char fourcc_v4l2[5];
            memset( fourcc_v4l2, 0, sizeof( fourcc_v4l2 ) );
            vlc_fourcc_to_char( codecs[i].pixelformat, fourcc_v4l2 );

            bool b_codec_supported = false;
            for( unsigned j = 0; v4l2chroma_to_fourcc[j].i_v4l2 != 0; j++ )
            {
                if( v4l2chroma_to_fourcc[j].i_v4l2 == codecs[i].pixelformat )
                {
                    char fourcc[5];
                    memset( fourcc, 0, sizeof( fourcc ) );
                    vlc_fourcc_to_char( v4l2chroma_to_fourcc[j].i_fourcc,
                                        fourcc );
                    msg_Dbg( p_obj, "device supports chroma %4.4s [%s, %s]",
                             fourcc, codecs[i].description, fourcc_v4l2 );
                    b_codec_supported = true;
                }
            }
            if( !b_codec_supported )
            {
                msg_Dbg( p_obj, "device codec %4.4s (%s) not supported",
                         fourcc_v4l2, codecs[i].description );
            }
        }
    }

    /* TODO: Move the resolution stuff up here */
    /* if MPEG encoder card, no need to do anything else after this */
    p_sys->controls = ControlsInit( p_obj, i_fd );

    /* Reset Cropping */
    memset( &cropcap, 0, sizeof(cropcap) );
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if( v4l2_ioctl( i_fd, VIDIOC_CROPCAP, &cropcap ) >= 0 )
    {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect; /* reset to default */
        if( crop.c.width > 0 && crop.c.height > 0 ) /* Fix for fm tuners */
        {
            if( v4l2_ioctl( i_fd, VIDIOC_S_CROP, &crop ) < 0 )
            {
                switch( errno )
                {
                    case EINVAL:
                        /* Cropping not supported. */
                        break;
                    default:
                        /* Errors ignored. */
                        break;
                }
            }
        }
    }

    /* Try and find default resolution if not specified */
    int width = var_InheritInteger( p_obj, CFG_PREFIX"width" );
    int height = var_InheritInteger( p_obj, CFG_PREFIX"height" );

    memset( &fmt, 0, sizeof(fmt) );
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if( width <= 0 || height <= 0 )
    {
        /* Use current width and height settings */
        if( v4l2_ioctl( i_fd, VIDIOC_G_FMT, &fmt ) < 0 )
        {
            msg_Err( p_obj, "cannot get default width and height: %m" );
            goto error;
        }

        msg_Dbg( p_obj, "found default width and height of %ux%u",
                 fmt.fmt.pix.width, fmt.fmt.pix.height );

        if( width < 0 || height < 0 )
        {
            msg_Dbg( p_obj, "will try to find optimal width and height" );
        }
    }
    else
    {
        /* Use user specified width and height */
        msg_Dbg( p_obj, "trying specified size %dx%d", width, height );
        fmt.fmt.pix.width = width;
        fmt.fmt.pix.height = height;
    }

    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    float f_fps;
    if (b_demux)
    {
        char *reqchroma = var_InheritString( p_obj, CFG_PREFIX"chroma" );

        /* Test and set Chroma */
        fmt.fmt.pix.pixelformat = 0;
        if( reqchroma != NULL )
        {
            /* User specified chroma */
            const vlc_fourcc_t i_requested_fourcc =
                vlc_fourcc_GetCodecFromString( VIDEO_ES, reqchroma );

            for( int i = 0; v4l2chroma_to_fourcc[i].i_v4l2 != 0; i++ )
            {
                if( v4l2chroma_to_fourcc[i].i_fourcc == i_requested_fourcc )
                {
                    fmt.fmt.pix.pixelformat = v4l2chroma_to_fourcc[i].i_v4l2;
                    break;
                }
            }
            /* Try and set user chroma */
            bool b_error = !IsPixelFormatSupported( codecs, ncodec,
                                                    fmt.fmt.pix.pixelformat );
            if( !b_error && fmt.fmt.pix.pixelformat )
            {
                if( v4l2_ioctl( i_fd, VIDIOC_S_FMT, &fmt ) < 0 )
                {
                    fmt.fmt.pix.field = V4L2_FIELD_ANY;
                    if( v4l2_ioctl( i_fd, VIDIOC_S_FMT, &fmt ) < 0 )
                    {
                        fmt.fmt.pix.field = V4L2_FIELD_NONE;
                        b_error = true;
                    }
                }
            }
            if( b_error )
            {
                msg_Warn( p_obj, "requested chroma %s not supported. "
                          " Trying default.", reqchroma );
                fmt.fmt.pix.pixelformat = 0;
            }
            free( reqchroma );
        }

        /* If no user specified chroma, find best */
        /* This also decides if MPEG encoder card or not */
        if( !fmt.fmt.pix.pixelformat )
        {
            unsigned int i;
            for( i = 0; i < ARRAY_SIZE( p_chroma_fallbacks ); i++ )
            {
                fmt.fmt.pix.pixelformat = p_chroma_fallbacks[i];
                if( IsPixelFormatSupported( codecs, ncodec,
                                            fmt.fmt.pix.pixelformat ) )
                {
                    if( v4l2_ioctl( i_fd, VIDIOC_S_FMT, &fmt ) >= 0 )
                        break;
                    fmt.fmt.pix.field = V4L2_FIELD_ANY;
                    if( v4l2_ioctl( i_fd, VIDIOC_S_FMT, &fmt ) >= 0 )
                        break;
                    fmt.fmt.pix.field = V4L2_FIELD_NONE;
                }
            }
            if( i == ARRAY_SIZE( p_chroma_fallbacks ) )
            {
                msg_Warn( p_obj, "Could not select any of the default chromas; attempting to open as MPEG encoder card (access)" );
                goto error;
            }
        }

        if( width < 0 || height < 0 )
        {
            f_fps = var_InheritFloat( p_obj, CFG_PREFIX"fps" );
            if( f_fps <= 0. )
            {
                f_fps = GetAbsoluteMaxFrameRate( p_obj, i_fd,
                                                 fmt.fmt.pix.pixelformat );
                msg_Dbg( p_obj, "Found maximum framerate of %f", f_fps );
            }
            uint32_t i_width, i_height;
            GetMaxDimensions( p_obj, i_fd,
                              fmt.fmt.pix.pixelformat, f_fps,
                              &i_width, &i_height );
            if( i_width || i_height )
            {
                msg_Dbg( p_obj, "Found optimal dimensions for framerate %f "
                                  "of %ux%u", f_fps, i_width, i_height );
                fmt.fmt.pix.width = i_width;
                fmt.fmt.pix.height = i_height;
                if( v4l2_ioctl( i_fd, VIDIOC_S_FMT, &fmt ) < 0 )
                {
                    msg_Err( p_obj, "Cannot set size to optimal dimensions "
                                    "%ux%u", i_width, i_height );
                    goto error;
                }
            }
            else
            {
                msg_Warn( p_obj, "Could not find optimal width and height, "
                                 "falling back to driver default." );
            }
        }
    }

    width = fmt.fmt.pix.width;
    height = fmt.fmt.pix.height;

    if( v4l2_ioctl( i_fd, VIDIOC_G_FMT, &fmt ) < 0 ) {;}
    /* Print extra info */
    msg_Dbg( p_obj, "Driver requires at most %d bytes to store a complete image", fmt.fmt.pix.sizeimage );
    /* Check interlacing */
    switch( fmt.fmt.pix.field )
    {
        case V4L2_FIELD_NONE:
            msg_Dbg( p_obj, "Interlacing setting: progressive" );
            break;
        case V4L2_FIELD_TOP:
            msg_Dbg( p_obj, "Interlacing setting: top field only" );
            break;
        case V4L2_FIELD_BOTTOM:
            msg_Dbg( p_obj, "Interlacing setting: bottom field only" );
            break;
        case V4L2_FIELD_INTERLACED:
            msg_Dbg( p_obj, "Interlacing setting: interleaved (bottom top if M/NTSC, top bottom otherwise)" );
            if( bottom_first )
                p_sys->i_block_flags = BLOCK_FLAG_BOTTOM_FIELD_FIRST;
            else
                p_sys->i_block_flags = BLOCK_FLAG_TOP_FIELD_FIRST;
            break;
        case V4L2_FIELD_SEQ_TB:
            msg_Dbg( p_obj, "Interlacing setting: sequential top bottom (TODO)" );
            break;
        case V4L2_FIELD_SEQ_BT:
            msg_Dbg( p_obj, "Interlacing setting: sequential bottom top (TODO)" );
            break;
        case V4L2_FIELD_ALTERNATE:
            msg_Dbg( p_obj, "Interlacing setting: alternate fields (TODO)" );
            height *= 2;
            break;
        case V4L2_FIELD_INTERLACED_TB:
            msg_Dbg( p_obj, "Interlacing setting: interleaved top bottom" );
            p_sys->i_block_flags = BLOCK_FLAG_TOP_FIELD_FIRST;
            break;
        case V4L2_FIELD_INTERLACED_BT:
            msg_Dbg( p_obj, "Interlacing setting: interleaved bottom top" );
            p_sys->i_block_flags = BLOCK_FLAG_BOTTOM_FIELD_FIRST;
            break;
        default:
            msg_Warn( p_obj, "Interlacing setting: unknown type (%d)",
                      fmt.fmt.pix.field );
            break;
    }

    /* Look up final fourcc */
    p_sys->i_fourcc = 0;
    for( int i = 0; v4l2chroma_to_fourcc[i].i_fourcc != 0; i++ )
    {
        if( v4l2chroma_to_fourcc[i].i_v4l2 == fmt.fmt.pix.pixelformat )
        {
            p_sys->i_fourcc = v4l2chroma_to_fourcc[i].i_fourcc;
            es_format_Init( &es_fmt, VIDEO_ES, p_sys->i_fourcc );
            es_fmt.video.i_rmask = v4l2chroma_to_fourcc[i].i_rmask;
            es_fmt.video.i_gmask = v4l2chroma_to_fourcc[i].i_gmask;
            es_fmt.video.i_bmask = v4l2chroma_to_fourcc[i].i_bmask;
            break;
        }
    }

    /* Buggy driver paranoia */
    i_min = fmt.fmt.pix.width * 2;
    if( fmt.fmt.pix.bytesperline < i_min )
        fmt.fmt.pix.bytesperline = i_min;
    i_min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if( fmt.fmt.pix.sizeimage < i_min )
        fmt.fmt.pix.sizeimage = i_min;

    /* Init I/O method */
    switch( p_sys->io )
    {
    case IO_METHOD_READ:
        p_sys->blocksize = fmt.fmt.pix.sizeimage;
        break;

    case IO_METHOD_MMAP:
        if( InitMmap( p_obj, p_sys, i_fd ) )
            goto error;
        for (unsigned int i = 0; i < p_sys->i_nbuffers; ++i)
        {
            struct v4l2_buffer buf;

            memset( &buf, 0, sizeof(buf) );
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;

            if( v4l2_ioctl( i_fd, VIDIOC_QBUF, &buf ) < 0 )
            {
                msg_Err( p_obj, "VIDIOC_QBUF failed" );
                goto error;
            }
        }

        buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if( v4l2_ioctl( i_fd, VIDIOC_STREAMON, &buf_type ) < 0 )
        {
            msg_Err( p_obj, "VIDIOC_STREAMON failed" );
            goto error;
        }
        break;

    case IO_METHOD_USERPTR:
        if( InitUserP( p_obj, p_sys, i_fd, fmt.fmt.pix.sizeimage ) )
            goto error;
        for( unsigned int i = 0; i < p_sys->i_nbuffers; ++i )
        {
            struct v4l2_buffer buf;

            memset( &buf, 0, sizeof(buf) );
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_USERPTR;
            buf.index = i;
            buf.m.userptr = (unsigned long)p_sys->p_buffers[i].start;
            buf.length = p_sys->p_buffers[i].length;

            if( v4l2_ioctl( i_fd, VIDIOC_QBUF, &buf ) < 0 )
            {
                msg_Err( p_obj, "VIDIOC_QBUF failed" );
                goto error;
            }
        }

        buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if( v4l2_ioctl( i_fd, VIDIOC_STREAMON, &buf_type ) < 0 )
        {
            msg_Err( p_obj, "VIDIOC_STREAMON failed" );
            goto error;
        }
        break;
    }

    free( codecs );

    if( b_demux )
    {
        int ar = 4 * VOUT_ASPECT_FACTOR / 3;
        char *str = var_InheritString( p_obj, CFG_PREFIX"aspect-ratio" );
        if( likely(str != NULL) )
        {
            const char *delim = strchr( str, ':' );
            if( delim )
                ar = atoi( str ) * VOUT_ASPECT_FACTOR / atoi( delim + 1 );
            free( str );
        }

        /* Add */
        es_fmt.video.i_width  = width;
        es_fmt.video.i_height = height;

        /* Get aspect-ratio */
        es_fmt.video.i_sar_num = ar * es_fmt.video.i_height;
        es_fmt.video.i_sar_den = VOUT_ASPECT_FACTOR * es_fmt.video.i_width;

        /* Framerate */
        es_fmt.video.i_frame_rate = lround(f_fps * 1000000.);
        es_fmt.video.i_frame_rate_base = 1000000;

        demux_t *p_demux = (demux_t *) p_obj;
        msg_Dbg( p_obj, "added new video es %4.4s %dx%d",
            (char*)&es_fmt.i_codec, es_fmt.video.i_width, es_fmt.video.i_height );
        msg_Dbg( p_obj, " frame rate: %f", f_fps );

        p_sys->p_es = es_out_Add( p_demux->out, &es_fmt );
    }
    return 0;

error:
    free( codecs );
    return -1;
}
