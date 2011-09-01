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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_access.h>
#include <vlc_charset.h>
#include <vlc_fs.h>
#include <vlc_demux.h>
#include <vlc_input.h>

#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#if defined(HAVE_LINUX_VIDEODEV2_H)
#   include <linux/videodev2.h>
#elif defined(HAVE_SYS_VIDEOIO_H)
#   include <sys/videoio.h>
#else
#   error "No Video4Linux2 headers found."
#endif

#include <poll.h>

#ifdef HAVE_LIBV4L2
#   include <libv4l2.h>
#endif

/*****************************************************************************
 * Module descriptior
 *****************************************************************************/

static int  DemuxOpen ( vlc_object_t * );
static void DemuxClose( vlc_object_t * );
static int  AccessOpen ( vlc_object_t * );
static void AccessClose( vlc_object_t * );

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

#define CTRL_RESET_TEXT N_( "Reset v4l2 controls" )
#define CTRL_RESET_LONGTEXT N_( \
    "Reset controls to defaults provided by the v4l2 driver." )
#define BRIGHTNESS_TEXT N_( "Brightness" )
#define BRIGHTNESS_LONGTEXT N_( \
    "Brightness of the video input (if supported by the v4l2 driver)." )
#define CONTRAST_TEXT N_( "Contrast" )
#define CONTRAST_LONGTEXT N_( \
    "Contrast of the video input (if supported by the v4l2 driver)." )
#define SATURATION_TEXT N_( "Saturation" )
#define SATURATION_LONGTEXT N_( \
    "Saturation of the video input (if supported by the v4l2 driver)." )
#define HUE_TEXT N_( "Hue" )
#define HUE_LONGTEXT N_( \
    "Hue of the video input (if supported by the v4l2 driver)." )
#define BLACKLEVEL_TEXT N_( "Black level" )
#define BLACKLEVEL_LONGTEXT N_( \
    "Black level of the video input (if supported by the v4l2 driver)." )
#define AUTOWHITEBALANCE_TEXT N_( "Auto white balance" )
#define AUTOWHITEBALANCE_LONGTEXT N_( \
    "Automatically set the white balance of the video input " \
    "(if supported by the v4l2 driver)." )
#define DOWHITEBALANCE_TEXT N_( "Do white balance" )
#define DOWHITEBALANCE_LONGTEXT N_( \
    "Trigger a white balancing action, useless if auto white balance is " \
    "activated (if supported by the v4l2 driver)." )
#define REDBALANCE_TEXT N_( "Red balance" )
#define REDBALANCE_LONGTEXT N_( \
    "Red balance of the video input (if supported by the v4l2 driver)." )
#define BLUEBALANCE_TEXT N_( "Blue balance" )
#define BLUEBALANCE_LONGTEXT N_( \
    "Blue balance of the video input (if supported by the v4l2 driver)." )
#define GAMMA_TEXT N_( "Gamma" )
#define GAMMA_LONGTEXT N_( \
    "Gamma of the video input (if supported by the v4l2 driver)." )
#define EXPOSURE_TEXT N_( "Exposure" )
#define EXPOSURE_LONGTEXT N_( \
    "Exposure of the video input (if supported by the v4L2 driver)." )
#define AUTOGAIN_TEXT N_( "Auto gain" )
#define AUTOGAIN_LONGTEXT N_( \
    "Automatically set the video input's gain (if supported by the " \
    "v4l2 driver)." )
#define GAIN_TEXT N_( "Gain" )
#define GAIN_LONGTEXT N_( \
    "Video input's gain (if supported by the v4l2 driver)." )
#define HFLIP_TEXT N_( "Horizontal flip" )
#define HFLIP_LONGTEXT N_( \
    "Flip the video horizontally (if supported by the v4l2 driver)." )
#define VFLIP_TEXT N_( "Vertical flip" )
#define VFLIP_LONGTEXT N_( \
    "Flip the video vertically (if supported by the v4l2 driver)." )
#define HCENTER_TEXT N_( "Horizontal centering" )
#define HCENTER_LONGTEXT N_( \
    "Set the camera's horizontal centering (if supported by the v4l2 driver)." )
#define VCENTER_TEXT N_( "Vertical centering" )
#define VCENTER_LONGTEXT N_( \
    "Set the camera's vertical centering (if supported by the v4l2 driver)." )

#define AUDIO_VOLUME_TEXT N_( "Volume" )
#define AUDIO_VOLUME_LONGTEXT N_( \
    "Volume of the audio input (if supported by the v4l2 driver)." )
#define AUDIO_BALANCE_TEXT N_( "Balance" )
#define AUDIO_BALANCE_LONGTEXT N_( \
    "Balance of the audio input (if supported by the v4l2 driver)." )
#define AUDIO_MUTE_TEXT N_( "Mute" )
#define AUDIO_MUTE_LONGTEXT N_( \
    "Mute audio input (if supported by the v4l2 driver)." )
#define AUDIO_BASS_TEXT N_( "Bass" )
#define AUDIO_BASS_LONGTEXT N_( \
    "Bass level of the audio input (if supported by the v4l2 driver)." )
#define AUDIO_TREBLE_TEXT N_( "Treble" )
#define AUDIO_TREBLE_LONGTEXT N_( \
    "Treble level of the audio input (if supported by the v4l2 driver)." )
#define AUDIO_LOUDNESS_TEXT N_( "Loudness" )
#define AUDIO_LOUDNESS_LONGTEXT N_( \
    "Loudness of the audio input (if supported by the v4l2 driver)." )

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

typedef enum {
    IO_METHOD_READ=1,
    IO_METHOD_MMAP,
    IO_METHOD_USERPTR,
} io_method;

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
#define CFG_PREFIX "v4l2-"

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
    add_integer( CFG_PREFIX "audio-input", 0, AUDIO_INPUT_TEXT,
                 AUDIO_INPUT_LONGTEXT, true )
        change_integer_range( 0, 0xFFFFFFFE )
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
        change_safe()
    add_integer( CFG_PREFIX "tuner-audio-mode", -1, TUNER_AUDIO_MODE_TEXT,
                 TUNER_AUDIO_MODE_LONGTEXT, true )
        change_integer_list( i_tuner_audio_modes_list,
                             psz_tuner_audio_modes_list_text )
        change_safe()

    set_section( N_( "Controls" ),
                 N_( "v4l2 driver controls, if supported by your v4l2 driver." ) )
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
    add_integer( CFG_PREFIX "black-level", -1, BLACKLEVEL_TEXT,
                 BLACKLEVEL_LONGTEXT, true )
    add_integer( CFG_PREFIX "auto-white-balance", -1,
                 AUTOWHITEBALANCE_TEXT, AUTOWHITEBALANCE_LONGTEXT, true )
    add_integer( CFG_PREFIX "do-white-balance", -1, DOWHITEBALANCE_TEXT,
                 DOWHITEBALANCE_LONGTEXT, true )
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
    add_integer( CFG_PREFIX "gain", -1, GAIN_TEXT,
                 GAIN_LONGTEXT, true )
    add_integer( CFG_PREFIX "hflip", -1, HFLIP_TEXT,
                 HFLIP_LONGTEXT, true )
    add_integer( CFG_PREFIX "vflip", -1, VFLIP_TEXT,
                 VFLIP_LONGTEXT, true )
    add_integer( CFG_PREFIX "hcenter", -1, HCENTER_TEXT,
                 HCENTER_LONGTEXT, true )
    add_integer( CFG_PREFIX "vcenter", -1, VCENTER_TEXT,
                 VCENTER_LONGTEXT, true )
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
    add_integer( CFG_PREFIX "audio-loudness", -1, AUDIO_LOUDNESS_TEXT,
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

static void CommonClose( vlc_object_t *, demux_sys_t * );
static char *ParseMRL( vlc_object_t *, const char * );
static void GetV4L2Params( demux_sys_t *, vlc_object_t * );
static void SetAvailControlsByString( vlc_object_t *, demux_sys_t *, int );

static int DemuxControl( demux_t *, int, va_list );
static int AccessControl( access_t *, int, va_list );

static int Demux( demux_t * );
static block_t *AccessRead( access_t * );
static ssize_t AccessReadStream( access_t * p_access, uint8_t * p_buffer, size_t i_len );

static block_t* GrabVideo( vlc_object_t *p_demux, demux_sys_t *p_sys );
static block_t* ProcessVideoFrame( vlc_object_t *p_demux, uint8_t *p_frame, size_t );

static bool IsPixelFormatSupported( demux_t *p_demux,
                                          unsigned int i_pixelformat );

static int OpenVideoDev( vlc_object_t *, demux_sys_t *, bool );
static int ProbeVideoDev( vlc_object_t *, demux_sys_t *, int );

static int ControlList( vlc_object_t *, int , bool, bool );
static int Control( vlc_object_t *, int i_fd,
                    const char *psz_name, int i_cid, int i_value );

static int DemuxControlCallback( vlc_object_t *p_this, const char *psz_var,
                                 vlc_value_t oldval, vlc_value_t newval,
                                 void *p_data );
static int DemuxControlResetCallback( vlc_object_t *p_this, const char *psz_var,
                                      vlc_value_t oldval, vlc_value_t newval,
                                      void *p_data );
static int AccessControlCallback( vlc_object_t *p_this, const char *psz_var,
                                  vlc_value_t oldval, vlc_value_t newval,
                                  void *p_data );
static int AccessControlResetCallback( vlc_object_t *p_this,
                                       const char *psz_var, vlc_value_t oldval,
                                       vlc_value_t newval, void *p_data );

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

static const struct
{
    const char *psz_name;
    unsigned int i_cid;
} controls[] =
{
    { "brightness", V4L2_CID_BRIGHTNESS },
    { "contrast", V4L2_CID_CONTRAST },
    { "saturation", V4L2_CID_SATURATION },
    { "hue", V4L2_CID_HUE },
    { "audio-volume", V4L2_CID_AUDIO_VOLUME },
    { "audio-balance", V4L2_CID_AUDIO_BALANCE },
    { "audio-bass", V4L2_CID_AUDIO_BASS },
    { "audio-treble", V4L2_CID_AUDIO_TREBLE },
    { "audio-mute", V4L2_CID_AUDIO_MUTE },
    { "audio-loudness", V4L2_CID_AUDIO_LOUDNESS },
    { "black-level", V4L2_CID_BLACK_LEVEL },
    { "auto-white-balance", V4L2_CID_AUTO_WHITE_BALANCE },
    { "do-white-balance", V4L2_CID_DO_WHITE_BALANCE },
    { "red-balance", V4L2_CID_RED_BALANCE },
    { "blue-balance", V4L2_CID_BLUE_BALANCE },
    { "gamma", V4L2_CID_GAMMA },
    { "exposure", V4L2_CID_EXPOSURE },
    { "autogain", V4L2_CID_AUTOGAIN },
    { "gain", V4L2_CID_GAIN },
    { "hflip", V4L2_CID_HFLIP },
    { "vflip", V4L2_CID_VFLIP },
    { "hcenter", V4L2_CID_HCENTER },
    { "vcenter", V4L2_CID_VCENTER },
    { NULL, 0 }
};

struct buffer_t
{
    void *  start;
    size_t  length;
};

struct demux_sys_t
{
    char *psz_device;  /* Main device from MRL */
    int  i_fd;

    char *psz_requested_chroma;

    /* Video */
    io_method io;

    unsigned i_selected_input;
    char *psz_standard;

    uint32_t i_audio;
    /* V4L2 devices cannot have more than 32 audio inputs */
    struct v4l2_audio p_audios[32];
    unsigned i_selected_audio_input;

    unsigned i_codec;
    struct v4l2_fmtdesc *p_codecs;

    struct buffer_t *p_buffers;
    unsigned int i_nbuffers;

    int i_width;
    int i_height;
    unsigned int i_aspect;
    float f_fps;            /* <= 0.0 mean to grab at full rate */
    int i_fourcc;
    uint32_t i_block_flags;

    es_out_id_t *p_es;

    /* Tuner */
    uint32_t i_tuner;
    enum v4l2_tuner_type i_tuner_type;
    int i_frequency;
    int i_tuner_audio_mode;

    /* Controls */
    char *psz_set_ctrls;

#ifdef HAVE_LIBV4L2
    bool b_libv4l2;
#endif
};

#ifndef HAVE_LIBV4L2
#   define v4l2_close close
#   define v4l2_dup dup
#   define v4l2_ioctl ioctl
#   define v4l2_read read
#   define v4l2_mmap mmap
#   define v4l2_munmap munmap
#endif

static int FindMainDevice( vlc_object_t *p_this, demux_sys_t *p_sys,
                           bool b_demux )
{
    /* TODO: if using default device, loop through all /dev/video* until
     * one works */
    p_sys->i_fd = OpenVideoDev( p_this, p_sys, b_demux );
    if( p_sys->i_fd < 0 ) return VLC_EGENERIC;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * DemuxOpen: opens v4l2 device, access_demux callback
 *****************************************************************************
 *
 * url: <video device>::::
 *
 *****************************************************************************/
static int DemuxOpen( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;

    /* Set up p_demux */
    p_demux->pf_control = DemuxControl;
    p_demux->pf_demux = Demux;
    p_demux->info.i_update = 0;
    p_demux->info.i_title = 0;
    p_demux->info.i_seekpoint = 0;

    p_demux->p_sys = p_sys = calloc( 1, sizeof( demux_sys_t ) );
    if( p_sys == NULL ) return VLC_ENOMEM;

    p_sys->psz_device = ParseMRL( p_this, p_demux->psz_location );
    GetV4L2Params( p_sys, p_this );

#ifdef HAVE_LIBV4L2
    if( !var_InheritBool( p_this, CFG_PREFIX "use-libv4l2" ) )
    {
        msg_Dbg( p_this, "Trying direct kernel v4l2" );
        p_sys->b_libv4l2 = false;
        if( FindMainDevice( p_this, p_sys, true ) == VLC_SUCCESS)
            return VLC_SUCCESS;
    }

    msg_Dbg( p_this, "Trying libv4l2 wrapper" );
    p_sys->b_libv4l2 = true;
#endif
    if( FindMainDevice( p_this, p_sys, true ) == VLC_SUCCESS)
        return VLC_SUCCESS;

    DemuxClose( p_this );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * GetV4L2Params: fill in p_sys parameters (shared by DemuxOpen and AccessOpen)
 *****************************************************************************/
static void GetV4L2Params( demux_sys_t *p_sys, vlc_object_t *p_obj )
{
    if( p_sys->psz_device == NULL )
        p_sys->psz_device = var_CreateGetNonEmptyString( p_obj, "v4l2-dev" );

    p_sys->i_selected_input = var_CreateGetInteger( p_obj, "v4l2-input" );
    p_sys->i_selected_audio_input =
        var_CreateGetInteger( p_obj, "v4l2-audio-input" );

    p_sys->i_width = var_CreateGetInteger( p_obj, "v4l2-width" );
    p_sys->i_height = var_CreateGetInteger( p_obj, "v4l2-height" );

    var_Create( p_obj, "v4l2-controls-reset", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    p_sys->f_fps = var_CreateGetFloat( p_obj, "v4l2-fps" );
    p_sys->psz_requested_chroma = var_CreateGetString( p_obj, "v4l2-chroma" );

    p_sys->i_tuner = var_CreateGetInteger( p_obj, "v4l2-tuner" );
    p_sys->i_tuner_type = V4L2_TUNER_RADIO; /* non-trap default value */
    p_sys->i_frequency = var_CreateGetInteger( p_obj, "v4l2-tuner-frequency" );
    p_sys->i_tuner_audio_mode = var_CreateGetInteger( p_obj, "v4l2-tuner-audio-mode" );

    p_sys->psz_set_ctrls = var_CreateGetString( p_obj, "v4l2-set-ctrls" );

    char *psz_aspect = var_CreateGetString( p_obj, "v4l2-aspect-ratio" );
    char *psz_delim = !EMPTY_STR(psz_aspect) ? strchr( psz_aspect, ':' ) : NULL;
    if( psz_delim )
    {
        p_sys->i_aspect = atoi( psz_aspect ) * VOUT_ASPECT_FACTOR / atoi( psz_delim + 1 );
    }
    else
    {
        p_sys->i_aspect = 4 * VOUT_ASPECT_FACTOR / 3 ;

    }
    free( psz_aspect );

    p_sys->i_fd = -1;

    p_sys->p_es = NULL;
}

/**
 * Parses a V4L2 MRL.
 * \return device node path (use free()) or NULL if not specified
 */
static char *ParseMRL( vlc_object_t *obj, const char *mrl )
{
    const char *p = strchr( mrl, ':' );

    if( p != NULL )
    {
        var_LocationParse( obj, p + 1, CFG_PREFIX );
        if( p > mrl )
            return strndup( mrl, p - mrl );
    }
    else
    {
        if( mrl[0] )
            return strdup( mrl );
    }
    return NULL;
}

/*****************************************************************************
 * Close: close device, free resources
 *****************************************************************************/
static void AccessClose( vlc_object_t *p_this )
{
    access_t    *p_access = (access_t *)p_this;
    demux_sys_t *p_sys   = (demux_sys_t *) p_access->p_sys;

    CommonClose( p_this, p_sys );
}

static void DemuxClose( vlc_object_t *p_this )
{
    struct v4l2_buffer buf;
    enum v4l2_buf_type buf_type;
    unsigned int i;

    demux_t     *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys   = p_demux->p_sys;

    /* Stop video capture */
    if( p_sys->i_fd >= 0 )
    {
        switch( p_sys->io )
        {
        case IO_METHOD_READ:
            /* Nothing to do */
            break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
            /* Some drivers 'hang' internally if this is not done before streamoff */
            for( unsigned int i = 0; i < p_sys->i_nbuffers; i++ )
            {
                memset( &buf, 0, sizeof(buf) );
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = ( p_sys->io == IO_METHOD_USERPTR ) ?
                    V4L2_MEMORY_USERPTR : V4L2_MEMORY_MMAP;
                v4l2_ioctl( p_sys->i_fd, VIDIOC_DQBUF, &buf ); /* ignore result */
            }

            buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if( v4l2_ioctl( p_sys->i_fd, VIDIOC_STREAMOFF, &buf_type ) < 0 ) {
                msg_Err( p_this, "VIDIOC_STREAMOFF failed" );
            }

            break;
        }
    }

    /* Free Video Buffers */
    if( p_sys->p_buffers ) {
        switch( p_sys->io )
        {
        case IO_METHOD_READ:
            free( p_sys->p_buffers[0].start );
            break;

        case IO_METHOD_MMAP:
            for( i = 0; i < p_sys->i_nbuffers; ++i )
            {
                if( v4l2_munmap( p_sys->p_buffers[i].start, p_sys->p_buffers[i].length ) )
                {
                    msg_Err( p_this, "munmap failed" );
                }
            }
            break;

        case IO_METHOD_USERPTR:
            for( i = 0; i < p_sys->i_nbuffers; ++i )
            {
               free( p_sys->p_buffers[i].start );
            }
            break;
        }
        free( p_sys->p_buffers );
    }

    CommonClose( p_this, p_sys );
}

static void CommonClose( vlc_object_t *p_this, demux_sys_t *p_sys )
{
    (void)p_this;
    /* Close */
    if( p_sys->i_fd >= 0 ) v4l2_close( p_sys->i_fd );
    free( p_sys->psz_device );
    free( p_sys->psz_standard );
    free( p_sys->p_codecs );
    free( p_sys->psz_requested_chroma );
    free( p_sys->psz_set_ctrls );

    free( p_sys );
}

/*****************************************************************************
 * AccessOpen: opens v4l2 device, access callback
 *****************************************************************************
 *
 * url: <video device>::::
 *
 *****************************************************************************/
static int AccessOpen( vlc_object_t * p_this )
{
    access_t *p_access = (access_t*) p_this;
    demux_sys_t * p_sys;

    /* Only when selected */
    if( *p_access->psz_access == '\0' ) return VLC_EGENERIC;

    access_InitFields( p_access );
    p_sys = calloc( 1, sizeof( demux_sys_t ));
    if( !p_sys ) return VLC_ENOMEM;
    p_access->p_sys = (access_sys_t*)p_sys;

    p_sys->psz_device = ParseMRL( p_this, p_access->psz_location );
    GetV4L2Params( p_sys, p_this );

#ifdef HAVE_LIBV4L2
    if( !var_InheritBool( p_this, CFG_PREFIX "use-libv4l2" ) )
    {
        msg_Dbg( p_this, "Trying direct kernel v4l2" );
        p_sys->b_libv4l2 = false;
        if( FindMainDevice( p_this, p_sys, false ) == VLC_SUCCESS)
        {
            if( p_sys->io == IO_METHOD_READ )
            {
                ACCESS_SET_CALLBACKS( AccessReadStream, NULL, AccessControl, NULL );
            }
            else
            {
                ACCESS_SET_CALLBACKS( NULL, AccessRead, AccessControl, NULL );
            }
            return VLC_SUCCESS;
        }
    }

    msg_Dbg( p_this, "Trying libv4l2 wrapper" );
    p_sys->b_libv4l2 = true;
#endif
    if( FindMainDevice( p_this, p_sys, false ) == VLC_SUCCESS )
    {
        if( p_sys->io == IO_METHOD_READ )
        {
            ACCESS_SET_CALLBACKS( AccessReadStream, NULL, AccessControl, NULL );
        }
        else
        {
            ACCESS_SET_CALLBACKS( NULL, AccessRead, AccessControl, NULL );
        }
        return VLC_SUCCESS;
    }

    AccessClose( p_this );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * DemuxControl:
 *****************************************************************************/
static int DemuxControl( demux_t *p_demux, int i_query, va_list args )
{
    switch( i_query )
    {
        /* Special for access_demux */
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_SEEK:
        case DEMUX_CAN_CONTROL_PACE:
            *va_arg( args, bool * ) = false;
            return VLC_SUCCESS;

        case DEMUX_GET_PTS_DELAY:
            *va_arg(args,int64_t *) = INT64_C(1000)
                * var_InheritInteger( p_demux, "live-caching" );
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            *va_arg( args, int64_t * ) = mdate();
            return VLC_SUCCESS;

        /* TODO implement others */
        default:
            return VLC_EGENERIC;
    }

    return VLC_EGENERIC;
}

/*****************************************************************************
 * AccessControl: access callback
 *****************************************************************************/
static int AccessControl( access_t *p_access, int i_query, va_list args )
{
    switch( i_query )
    {
        /* */
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
            *va_arg( args, bool* ) = false;
            break;

        /* */
        case ACCESS_GET_PTS_DELAY:
            *va_arg(args,int64_t *) = INT64_C(1000)
                * var_InheritInteger( p_access, "live-caching" );
            break;

        /* */
        case ACCESS_SET_PAUSE_STATE:
            /* Nothing to do */
            break;

        case ACCESS_GET_TITLE_INFO:
        case ACCESS_SET_TITLE:
        case ACCESS_SET_SEEKPOINT:
        case ACCESS_SET_PRIVATE_ID_STATE:
        case ACCESS_GET_CONTENT_TYPE:
        case ACCESS_GET_META:
            return VLC_EGENERIC;

        default:
            msg_Warn( p_access, "Unimplemented query in control(%d).", i_query);
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * AccessRead: access callback
 ******************************************************************************/
static block_t *AccessRead( access_t * p_access )
{
    demux_sys_t *p_sys = (demux_sys_t *)p_access->p_sys;

    struct pollfd fd;
    fd.fd = p_sys->i_fd;
    fd.events = POLLIN|POLLPRI;
    fd.revents = 0;

    /* Wait for data */
    if( poll( &fd, 1, 500 ) > 0 ) /* Timeout after 0.5 seconds since I don't know if pf_demux can be blocking. */
        return GrabVideo( VLC_OBJECT(p_access), p_sys );

    return NULL;
}

static ssize_t AccessReadStream( access_t * p_access, uint8_t * p_buffer, size_t i_len )
{
    demux_sys_t *p_sys = (demux_sys_t *) p_access->p_sys;
    struct pollfd ufd;
    int i_ret;

    ufd.fd = p_sys->i_fd;
    ufd.events = POLLIN;

    if( p_access->info.b_eof )
        return 0;

    do
    {
        if( !vlc_object_alive (p_access) )
            return 0;

        ufd.revents = 0;
    }
    while( ( i_ret = poll( &ufd, 1, 500 ) ) == 0 );

    if( i_ret < 0 )
    {
        if( errno != EINTR )
            msg_Err( p_access, "poll error" );
        return -1;
    }

    i_ret = v4l2_read( p_sys->i_fd, p_buffer, i_len );
    if( i_ret == 0 )
    {
        p_access->info.b_eof = true;
    }
    else if( i_ret > 0 )
    {
        p_access->info.i_pos += i_ret;
    }

    return i_ret;
}

/*****************************************************************************
 * Demux: Processes the audio or video frame
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    struct pollfd fd;
    fd.fd = p_sys->i_fd;
    fd.events = POLLIN|POLLPRI;
    fd.revents = 0;

    /* Wait for data */
    /* Timeout after 0.5 seconds since I don't know if pf_demux can be blocking. */
    while( poll( &fd, 1, 500 ) == -1 )
        if( errno != EINTR )
        {
            msg_Err( p_demux, "poll error: %m" );
            return -1;
        }

    if( fd.revents )
    {
         block_t *p_block = GrabVideo( VLC_OBJECT(p_demux), p_sys );
         if( p_block )
         {
             es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_block->i_pts );
             es_out_Send( p_demux->out, p_sys->p_es, p_block );
        }
    }

    return 1;
}

/*****************************************************************************
 * GrabVideo: Grab a video frame
 *****************************************************************************/
static block_t* GrabVideo( vlc_object_t *p_demux, demux_sys_t *p_sys )
{
    block_t *p_block;
    struct v4l2_buffer buf;
    ssize_t i_ret;

    /* Grab Video Frame */
    switch( p_sys->io )
    {
    case IO_METHOD_READ:
        i_ret = v4l2_read( p_sys->i_fd, p_sys->p_buffers[0].start, p_sys->p_buffers[0].length );
        if( i_ret == -1 )
        {
            switch( errno )
            {
            case EAGAIN:
                return NULL;
            case EIO:
                /* Could ignore EIO, see spec. */
                /* fall through */
            default:
                msg_Err( p_demux, "Failed to read frame" );
                return 0;
               }
        }

        p_block = ProcessVideoFrame( p_demux, (uint8_t*)p_sys->p_buffers[0].start, i_ret );
        if( !p_block )
            return NULL;

        break;

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
    }

    /* Timestamp */
    p_block->i_pts = p_block->i_dts = mdate();
    p_block->i_flags |= p_sys->i_block_flags;

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
 * Helper function to initalise video IO using the Read method
 *****************************************************************************/
static int InitRead( vlc_object_t *p_demux, demux_sys_t *p_sys, unsigned int i_buffer_size )
{
    (void)p_demux;

    p_sys->p_buffers = calloc( 1, sizeof( *p_sys->p_buffers ) );
    if( unlikely(p_sys->p_buffers == NULL) )
        return -1;

    p_sys->p_buffers[0].length = i_buffer_size;
    p_sys->p_buffers[0].start = malloc( i_buffer_size );
    if( !p_sys->p_buffers[0].start )
        return -1;

    return 0;
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

/*****************************************************************************
 * IsPixelFormatSupported: returns true if the specified V4L2 pixel format is
 * in the array of supported formats returned by the driver
 *****************************************************************************/
static bool IsPixelFormatSupported( demux_t *p_demux, unsigned int i_pixelformat )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    for( unsigned i_index = 0; i_index < p_sys->i_codec; i_index++ )
    {
        if( p_sys->p_codecs[i_index].pixelformat == i_pixelformat )
            return true;
    }

    return false;
}

static float GetMaxFrameRate( int i_fd, uint32_t i_pixel_format,
                              uint32_t i_width, uint32_t i_height )
{
#ifdef VIDIOC_ENUM_FRAMEINTERVALS
    /* This is new in Linux 2.6.19 */
    struct v4l2_frmivalenum frmival;
    memset( &frmival, 0, sizeof(frmival) );
    frmival.pixel_format = i_pixel_format;
    frmival.width = i_width;
    frmival.height = i_height;
    if( v4l2_ioctl( i_fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival ) >= 0 )
    {
        switch( frmival.type )
        {
            case V4L2_FRMIVAL_TYPE_DISCRETE:
            {
                float f_fps_max = -1;
                do
                {
                    float f_fps = (float)frmival.discrete.denominator
                                / (float)frmival.discrete.numerator;
                    if( f_fps > f_fps_max ) f_fps_max = f_fps;
                    frmival.index++;
                } while( v4l2_ioctl( i_fd, VIDIOC_ENUM_FRAMEINTERVALS,
                                     &frmival ) >= 0 );
                return f_fps_max;
            }
            case V4L2_FRMSIZE_TYPE_STEPWISE:
            case V4L2_FRMIVAL_TYPE_CONTINUOUS:
                return __MAX( (float)frmival.stepwise.max.denominator
                            / (float)frmival.stepwise.max.numerator,
                              (float)frmival.stepwise.min.denominator
                            / (float)frmival.stepwise.min.numerator );
        }
    }
#endif
    return -1.;
}

static float GetAbsoluteMaxFrameRate( demux_t *p_demux, int i_fd,
                                      uint32_t i_pixel_format )
{
    float f_fps_max = -1.;
#ifdef VIDIOC_ENUM_FRAMESIZES
    /* This is new in Linux 2.6.19 */
    struct v4l2_frmsizeenum frmsize;
    memset( &frmsize, 0, sizeof(frmsize) );
    frmsize.pixel_format = i_pixel_format;
    if( v4l2_ioctl( i_fd, VIDIOC_ENUM_FRAMESIZES, &frmsize ) >= 0 )
    {
        switch( frmsize.type )
        {
            case V4L2_FRMSIZE_TYPE_DISCRETE:
                do
                {
                    frmsize.index++;
                    float f_fps = GetMaxFrameRate( i_fd, i_pixel_format,
                                                   frmsize.discrete.width,
                                                   frmsize.discrete.height );
                    if( f_fps > f_fps_max ) f_fps_max = f_fps;
                } while( v4l2_ioctl( i_fd, VIDIOC_ENUM_FRAMESIZES,
                         &frmsize ) >= 0 );
                break;
            case V4L2_FRMSIZE_TYPE_STEPWISE:
            {
                uint32_t i_width = frmsize.stepwise.min_width;
                uint32_t i_height = frmsize.stepwise.min_height;
                for( ;
                     i_width <= frmsize.stepwise.max_width &&
                     i_height <= frmsize.stepwise.max_width;
                     i_width += frmsize.stepwise.step_width,
                     i_height += frmsize.stepwise.step_height )
                {
                    float f_fps = GetMaxFrameRate( i_fd, i_pixel_format,
                                                   i_width, i_height );
                    if( f_fps > f_fps_max ) f_fps_max = f_fps;
                }
                break;
            }
            case V4L2_FRMSIZE_TYPE_CONTINUOUS:
                /* FIXME */
                msg_Err( p_demux, "GetAbsoluteMaxFrameRate implementation for V4L2_FRMSIZE_TYPE_CONTINUOUS isn't correct" );
                 f_fps_max = GetMaxFrameRate( i_fd, i_pixel_format,
                                              frmsize.stepwise.max_width,
                                              frmsize.stepwise.max_height );
                break;
        }
    }
#endif
    return f_fps_max;
}

static void GetMaxDimensions( demux_t *p_demux, int i_fd,
                              uint32_t i_pixel_format, float f_fps_min,
                              uint32_t *pi_width, uint32_t *pi_height )
{
    *pi_width = 0;
    *pi_height = 0;
#ifdef VIDIOC_ENUM_FRAMESIZES
    /* This is new in Linux 2.6.19 */
    struct v4l2_frmsizeenum frmsize;
    memset( &frmsize, 0, sizeof(frmsize) );
    frmsize.pixel_format = i_pixel_format;
    if( v4l2_ioctl( i_fd, VIDIOC_ENUM_FRAMESIZES, &frmsize ) >= 0 )
    {
        switch( frmsize.type )
        {
            case V4L2_FRMSIZE_TYPE_DISCRETE:
                do
                {
                    frmsize.index++;
                    float f_fps = GetMaxFrameRate( i_fd, i_pixel_format,
                                                   frmsize.discrete.width,
                                                   frmsize.discrete.height );
                    if( f_fps >= f_fps_min &&
                        frmsize.discrete.width > *pi_width )
                    {
                        *pi_width = frmsize.discrete.width;
                        *pi_height = frmsize.discrete.height;
                    }
                } while( v4l2_ioctl( i_fd, VIDIOC_ENUM_FRAMESIZES,
                         &frmsize ) >= 0 );
                break;
            case V4L2_FRMSIZE_TYPE_STEPWISE:
            {
                uint32_t i_width = frmsize.stepwise.min_width;
                uint32_t i_height = frmsize.stepwise.min_height;
                for( ;
                     i_width <= frmsize.stepwise.max_width &&
                     i_height <= frmsize.stepwise.max_width;
                     i_width += frmsize.stepwise.step_width,
                     i_height += frmsize.stepwise.step_height )
                {
                    float f_fps = GetMaxFrameRate( i_fd, i_pixel_format,
                                                   i_width, i_height );
                    if( f_fps >= f_fps_min && i_width > *pi_width )
                    {
                        *pi_width = i_width;
                        *pi_height = i_height;
                    }
                }
                break;
            }
            case V4L2_FRMSIZE_TYPE_CONTINUOUS:
                /* FIXME */
                msg_Err( p_demux, "GetMaxDimension implementation for V4L2_FRMSIZE_TYPE_CONTINUOUS isn't correct" );
                float f_fps = GetMaxFrameRate( i_fd, i_pixel_format,
                                               frmsize.stepwise.max_width,
                                               frmsize.stepwise.max_height );
                if( f_fps >= f_fps_min &&
                    frmsize.stepwise.max_width > *pi_width )
                {
                    *pi_width = frmsize.stepwise.max_width;
                    *pi_height = frmsize.stepwise.max_height;
                }
                break;
        }
    }
#endif
}

/*****************************************************************************
 * OpenVideoDev: open and set up the video device and probe for capabilities
 *****************************************************************************/
static int OpenVideoDev( vlc_object_t *p_obj, demux_sys_t *p_sys, bool b_demux )
{
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format fmt;
    unsigned int i_min;
    enum v4l2_buf_type buf_type;
    const char *psz_device = p_sys->psz_device;
    es_format_t es_fmt;

    msg_Dbg( p_obj, "opening device '%s'", psz_device );

    int i_fd = vlc_open( psz_device, O_RDWR );
    if( i_fd == -1 )
    {
        msg_Err( p_obj, "cannot open device %s: %m", psz_device );
        return -1;
    }

#ifdef HAVE_LIBV4L2
    /* Note the v4l2_xxx functions are designed so that if they get passed an
       unknown fd, the will behave exactly as their regular xxx counterparts,
       so if v4l2_fd_open fails, we continue as normal (missing the libv4l2
       custom cam format to normal formats conversion). Chances are big we will
       still fail then though, as normally v4l2_fd_open only fails if the
       device is not a v4l2 device. */
    if( p_sys->b_libv4l2 )
    {
        int libv4l2_fd;
        libv4l2_fd = v4l2_fd_open( i_fd, 0 );
        if( libv4l2_fd != -1 )
            i_fd = libv4l2_fd;
    }
#endif

    if( ProbeVideoDev( p_obj, p_sys, i_fd ) )
        goto error;

    /* Select input */
    if( v4l2_ioctl( i_fd, VIDIOC_S_INPUT, &p_sys->i_selected_input ) < 0 )
    {
        msg_Err( p_obj, "cannot set input %u: %m", p_sys->i_selected_input );
        goto error;
    }
    msg_Dbg( p_obj, "input set to %u", p_sys->i_selected_input );

    /* Select standard */
    bool bottom_first;
    const char *stdname = p_sys->psz_standard;
    if( stdname == NULL )
        stdname = var_InheritString( p_obj, CFG_PREFIX"standard" );
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
            goto error;
        }
        msg_Dbg( p_obj, "standard set to 0x%"PRIx64":", std );
        bottom_first = std == V4L2_STD_NTSC;
    }
    else
        bottom_first = false;

    /* Tune the tuner */
    if( p_sys->i_frequency >= 0 )
    {
        struct v4l2_frequency frequency = {
            .tuner = p_sys->i_tuner,
            .type = p_sys->i_tuner_type,
            .frequency = p_sys->i_frequency / 62.5,
        };

        if( v4l2_ioctl( i_fd, VIDIOC_S_FREQUENCY, &frequency ) < 0 )
        {
            msg_Err( p_obj, "cannot set tuner frequency: %m" );
            goto error;
        }
        msg_Dbg( p_obj, "tuner frequency set" );
    }

    /* Set the tuner's audio mode */
    if( p_sys->i_tuner_audio_mode >= 0 )
    {
        struct v4l2_tuner tuner = {
            .index = p_sys->i_tuner,
            .audmode = p_sys->i_tuner_audio_mode,
        };

        if( v4l2_ioctl( i_fd, VIDIOC_S_TUNER, &tuner ) < 0 )
        {
            msg_Err( p_obj, "cannot set tuner audio mode: %m" );
            goto error;
        }
        msg_Dbg( p_obj, "tuner audio mode set" );
    }

    /* Set audio input */

    if( p_sys->i_audio > 0 )
    {
        if( p_sys->i_selected_audio_input >= p_sys->i_audio )
        {
            msg_Warn( p_obj, "invalid audio input: using default instead" );
            p_sys->i_selected_audio_input = 0;
        }

        if( v4l2_ioctl( i_fd, VIDIOC_S_AUDIO,
                        &p_sys->p_audios[p_sys->i_selected_audio_input] ) < 0 )
        {
            msg_Err( p_obj, "cannot set audio input %u: %m",
                     p_sys->i_selected_audio_input );
            goto error;
        }
        msg_Dbg( p_obj, "audio input set to %u",
                 p_sys->i_selected_audio_input );
    }


    /* TODO: Move the resolution stuff up here */
    /* if MPEG encoder card, no need to do anything else after this */
    ControlList( p_obj, i_fd, var_GetBool( p_obj, "v4l2-controls-reset" ),
                 b_demux );
    SetAvailControlsByString( p_obj, p_sys, i_fd );

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
    memset( &fmt, 0, sizeof(fmt) );
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if( p_sys->i_width <= 0 || p_sys->i_height <= 0 )
    {
        /* Use current width and height settings */
        if( v4l2_ioctl( i_fd, VIDIOC_G_FMT, &fmt ) < 0 )
        {
            msg_Err( p_obj, "cannot get default width and height: %m" );
            goto error;
        }

        msg_Dbg( p_obj, "found default width and height of %ux%u",
                 fmt.fmt.pix.width, fmt.fmt.pix.height );

        if( p_sys->i_width < 0 || p_sys->i_height < 0 )
        {
            msg_Dbg( p_obj, "will try to find optimal width and height" );
        }
    }
    else
    {
        /* Use user specified width and height */
        msg_Dbg( p_obj, "trying specified size %dx%d",
                 p_sys->i_width, p_sys->i_height );
        fmt.fmt.pix.width = p_sys->i_width;
        fmt.fmt.pix.height = p_sys->i_height;
    }

    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (b_demux)
    {
        demux_t *p_demux = (demux_t *) p_obj;

        /* Test and set Chroma */
        fmt.fmt.pix.pixelformat = 0;
        if( p_sys->psz_requested_chroma && *p_sys->psz_requested_chroma )
        {
            /* User specified chroma */
            const vlc_fourcc_t i_requested_fourcc =
                vlc_fourcc_GetCodecFromString( VIDEO_ES, p_sys->psz_requested_chroma );

            for( int i = 0; v4l2chroma_to_fourcc[i].i_v4l2 != 0; i++ )
            {
                if( v4l2chroma_to_fourcc[i].i_fourcc == i_requested_fourcc )
                {
                    fmt.fmt.pix.pixelformat = v4l2chroma_to_fourcc[i].i_v4l2;
                    break;
                }
            }
            /* Try and set user chroma */
            bool b_error = !IsPixelFormatSupported( p_demux, fmt.fmt.pix.pixelformat );
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
                msg_Warn( p_demux, "Driver is unable to use specified chroma %s. Trying defaults.", p_sys->psz_requested_chroma );
                fmt.fmt.pix.pixelformat = 0;
            }
        }

        /* If no user specified chroma, find best */
        /* This also decides if MPEG encoder card or not */
        if( !fmt.fmt.pix.pixelformat )
        {
            unsigned int i;
            for( i = 0; i < ARRAY_SIZE( p_chroma_fallbacks ); i++ )
            {
                fmt.fmt.pix.pixelformat = p_chroma_fallbacks[i];
                if( IsPixelFormatSupported( p_demux, fmt.fmt.pix.pixelformat ) )
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
                msg_Warn( p_demux, "Could not select any of the default chromas; attempting to open as MPEG encoder card (access)" );
                goto error;
            }
        }

        if( p_sys->i_width < 0 || p_sys->i_height < 0 )
        {
            if( p_sys->f_fps <= 0 )
            {
                p_sys->f_fps = GetAbsoluteMaxFrameRate( p_demux, i_fd,
                                                        fmt.fmt.pix.pixelformat );
                msg_Dbg( p_demux, "Found maximum framerate of %f", p_sys->f_fps );
            }
            uint32_t i_width, i_height;
            GetMaxDimensions( p_demux, i_fd,
                              fmt.fmt.pix.pixelformat, p_sys->f_fps,
                              &i_width, &i_height );
            if( i_width || i_height )
            {
                msg_Dbg( p_demux, "Found optimal dimensions for framerate %f "
                                  "of %ux%u", p_sys->f_fps, i_width, i_height );
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

    p_sys->i_width = fmt.fmt.pix.width;
    p_sys->i_height = fmt.fmt.pix.height;

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
            p_sys->i_height = p_sys->i_height * 2;
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

#ifdef VIDIOC_ENUM_FRAMEINTERVALS
    /* This is new in Linux 2.6.19 */
    /* List supported frame rates */
    struct v4l2_frmivalenum frmival;
    memset( &frmival, 0, sizeof(frmival) );
    frmival.pixel_format = fmt.fmt.pix.pixelformat;
    frmival.width = p_sys->i_width;
    frmival.height = p_sys->i_height;
    if( v4l2_ioctl( i_fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival ) >= 0 )
    {
        char psz_fourcc[5];
        memset( &psz_fourcc, 0, sizeof( psz_fourcc ) );
        vlc_fourcc_to_char( p_sys->i_fourcc, &psz_fourcc );
        msg_Dbg( p_obj, "supported frame intervals for %4.4s, %dx%d:",
                 psz_fourcc, frmival.width, frmival.height );
        switch( frmival.type )
        {
            case V4L2_FRMIVAL_TYPE_DISCRETE:
                do
                {
                    msg_Dbg( p_obj, "    supported frame interval: %d/%d",
                             frmival.discrete.numerator,
                             frmival.discrete.denominator );
                    frmival.index++;
                } while( v4l2_ioctl( i_fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival ) >= 0 );
                break;
            case V4L2_FRMIVAL_TYPE_STEPWISE:
                msg_Dbg( p_obj, "    supported frame intervals: %d/%d to "
                         "%d/%d using %d/%d increments",
                         frmival.stepwise.min.numerator,
                         frmival.stepwise.min.denominator,
                         frmival.stepwise.max.numerator,
                         frmival.stepwise.max.denominator,
                         frmival.stepwise.step.numerator,
                         frmival.stepwise.step.denominator );
                break;
            case V4L2_FRMIVAL_TYPE_CONTINUOUS:
                msg_Dbg( p_obj, "    supported frame intervals: %d/%d to %d/%d",
                         frmival.stepwise.min.numerator,
                         frmival.stepwise.min.denominator,
                         frmival.stepwise.max.numerator,
                         frmival.stepwise.max.denominator );
                break;
        }
    }
#endif


    /* Init IO method */
    switch( p_sys->io )
    {
    case IO_METHOD_READ:
        if( b_demux && InitRead( p_obj, p_sys, fmt.fmt.pix.sizeimage ) )
            goto error;
        break;

    case IO_METHOD_MMAP:
        if( InitMmap( p_obj, p_sys, i_fd ) )
            goto error;
        break;

    case IO_METHOD_USERPTR:
        if( InitUserP( p_obj, p_sys, i_fd, fmt.fmt.pix.sizeimage ) )
            goto error;
        break;
    }

    if( b_demux )
    {
        /* Add */
        es_fmt.video.i_width  = p_sys->i_width;
        es_fmt.video.i_height = p_sys->i_height;

        /* Get aspect-ratio */
        es_fmt.video.i_sar_num = p_sys->i_aspect    * es_fmt.video.i_height;
        es_fmt.video.i_sar_den = VOUT_ASPECT_FACTOR * es_fmt.video.i_width;

        /* Framerate */
        es_fmt.video.i_frame_rate = p_sys->f_fps * INT64_C(1000000);
        es_fmt.video.i_frame_rate_base = INT64_C(1000000);

        demux_t *p_demux = (demux_t *) p_obj;
        msg_Dbg( p_demux, "added new video es %4.4s %dx%d",
            (char*)&es_fmt.i_codec, es_fmt.video.i_width, es_fmt.video.i_height );
        p_sys->p_es = es_out_Add( p_demux->out, &es_fmt );
    }

    /* Start Capture */

    switch( p_sys->io )
    {
    case IO_METHOD_READ:
        /* Nothing to do */
        break;

    case IO_METHOD_MMAP:
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

    /* report fps */
    if( p_sys->f_fps >= 0.1 )
    {
        msg_Dbg( p_obj, "User set fps=%f", p_sys->f_fps );
    }

    return i_fd;

error:
    v4l2_close( i_fd );
    return -1;
}

/*****************************************************************************
 * ProbeVideoDev: probe video for capabilities
 *****************************************************************************/
static int ProbeVideoDev( vlc_object_t *p_obj, demux_sys_t *p_sys, int i_fd )
{
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
    msg_Dbg( p_obj, "the device has the capabilities:"
             " (%c) Video Capture, (%c) Audio, (%c) Tuner, (%c) Radio",
             ( cap.capabilities & V4L2_CAP_VIDEO_CAPTURE  ? 'X':' '),
             ( cap.capabilities & V4L2_CAP_AUDIO  ? 'X':' '),
             ( cap.capabilities & V4L2_CAP_TUNER  ? 'X':' '),
             ( cap.capabilities & V4L2_CAP_RADIO  ? 'X':' ') );
    msg_Dbg( p_obj, "supported I/O methods are:"
             " (%c) Read/Write, (%c) Streaming, (%c) Asynchronous",
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

    if( cap.capabilities & V4L2_CAP_RDS_CAPTURE )
        msg_Dbg( p_obj, "device supports RDS" );

#ifdef V4L2_CAP_HW_FREQ_SEEK
    if( cap.capabilities & V4L2_CAP_HW_FREQ_SEEK )
        msg_Dbg( p_obj, "device supports hardware frequency seeking" );
#endif
    if( cap.capabilities & V4L2_CAP_VBI_CAPTURE )
        msg_Dbg( p_obj, "device supports raw VBI capture" );

    if( cap.capabilities & V4L2_CAP_SLICED_VBI_CAPTURE )
        msg_Dbg( p_obj, "device supports sliced VBI capture" );

    /* Now, enumerate all the video inputs. This is useless at the moment
       since we have no way to present that info to the user except with
       debug messages */

    if( cap.capabilities & V4L2_CAP_VIDEO_CAPTURE )
    {
        struct v4l2_input input;

        input.index = 0;
        while( v4l2_ioctl( i_fd, VIDIOC_ENUMINPUT, &input ) >= 0 )
        {
            msg_Dbg( p_obj, "video input %u (%s) has type: %s %c",
                     input.index, input.name,
                     input.type == V4L2_INPUT_TYPE_TUNER
                         ? "Tuner adapter" : "External analog input",
                     input.index == p_sys->i_selected_input ? '*' : ' ' );
            input.index++;
        }
    }

    /* initialize the structures for the ioctls */
    for( unsigned i_index = 0; i_index < 32; i_index++ )
    {
        p_sys->p_audios[i_index].index = i_index;
    }

    /* Probe audio inputs */
    if( cap.capabilities & V4L2_CAP_AUDIO )
    {
        while( p_sys->i_audio < 32 &&
               v4l2_ioctl( i_fd, VIDIOC_S_AUDIO, &p_sys->p_audios[p_sys->i_audio] ) >= 0 )
        {
            if( v4l2_ioctl( i_fd, VIDIOC_G_AUDIO, &p_sys->p_audios[ p_sys->i_audio] ) < 0 )
            {
                msg_Err( p_obj, "cannot get audio input characteristics: %m" );
                return -1;
            }

            msg_Dbg( p_obj, "audio input %u (%s) is %s %s %c",
                                p_sys->i_audio,
                                p_sys->p_audios[p_sys->i_audio].name,
                                p_sys->p_audios[p_sys->i_audio].capability &
                                                    V4L2_AUDCAP_STEREO ?
                                        "Stereo" : "Mono",
                                p_sys->p_audios[p_sys->i_audio].capability &
                                                    V4L2_AUDCAP_AVL ?
                                    "(Automatic Volume Level supported)" : "",
                                p_sys->i_audio == (unsigned)p_sys->i_selected_audio_input ? '*' : ' ' );

            p_sys->i_audio++;
        }
    }

    /* List tuner caps */
    if( cap.capabilities & V4L2_CAP_TUNER )
    {
        struct v4l2_tuner tuner;

        tuner.index = 0;
        while( v4l2_ioctl( i_fd, VIDIOC_G_TUNER, &tuner ) >= 0 )
        {
            if( tuner.index == p_sys->i_tuner )
                p_sys->i_tuner_type = tuner.type;

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
    }

    /* Probe for available chromas */
    if( cap.capabilities & V4L2_CAP_VIDEO_CAPTURE )
    {
        struct v4l2_fmtdesc codec;

        unsigned i_index = 0;
        memset( &codec, 0, sizeof(codec) );
        codec.index = i_index;
        codec.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        while( v4l2_ioctl( i_fd, VIDIOC_ENUM_FMT, &codec ) >= 0 )
        {
            if( codec.index != i_index )
                break;
            i_index++;
            codec.index = i_index;
        }

        p_sys->i_codec = i_index;

        free( p_sys->p_codecs );
        p_sys->p_codecs = calloc( 1, p_sys->i_codec * sizeof( struct v4l2_fmtdesc ) );

        for( i_index = 0; i_index < p_sys->i_codec; i_index++ )
        {
            p_sys->p_codecs[i_index].index = i_index;
            p_sys->p_codecs[i_index].type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

            if( v4l2_ioctl( i_fd, VIDIOC_ENUM_FMT, &p_sys->p_codecs[i_index] ) < 0 )
            {
                msg_Err( p_obj, "cannot get codec description: %m" );
                return -1;
            }

            /* only print if vlc supports the format */
            char psz_fourcc_v4l2[5];
            memset( &psz_fourcc_v4l2, 0, sizeof( psz_fourcc_v4l2 ) );
            vlc_fourcc_to_char( p_sys->p_codecs[i_index].pixelformat,
                                &psz_fourcc_v4l2 );
            bool b_codec_supported = false;
            for( int i = 0; v4l2chroma_to_fourcc[i].i_v4l2 != 0; i++ )
            {
                if( v4l2chroma_to_fourcc[i].i_v4l2 == p_sys->p_codecs[i_index].pixelformat )
                {
                    b_codec_supported = true;

                    char psz_fourcc[5];
                    memset( &psz_fourcc, 0, sizeof( psz_fourcc ) );
                    vlc_fourcc_to_char( v4l2chroma_to_fourcc[i].i_fourcc,
                                        &psz_fourcc );
                    msg_Dbg( p_obj, "device supports chroma %4.4s [%s, %s]",
                                psz_fourcc,
                                p_sys->p_codecs[i_index].description,
                                psz_fourcc_v4l2 );

#ifdef VIDIOC_ENUM_FRAMESIZES
                    /* This is new in Linux 2.6.19 */
                    /* List valid frame sizes for this format */
                    struct v4l2_frmsizeenum frmsize;
                    memset( &frmsize, 0, sizeof(frmsize) );
                    frmsize.pixel_format = p_sys->p_codecs[i_index].pixelformat;
                    if( v4l2_ioctl( i_fd, VIDIOC_ENUM_FRAMESIZES, &frmsize ) < 0 )
                    {
                        /* Not all devices support this ioctl */
                        msg_Warn( p_obj, "Unable to query for frame sizes" );
                    }
                    else
                    {
                        switch( frmsize.type )
                        {
                            case V4L2_FRMSIZE_TYPE_DISCRETE:
                                do
                                {
                                    msg_Dbg( p_obj,
                "    device supports size %dx%d",
                frmsize.discrete.width, frmsize.discrete.height );
                                    frmsize.index++;
                                } while( v4l2_ioctl( i_fd, VIDIOC_ENUM_FRAMESIZES, &frmsize ) >= 0 );
                                break;
                            case V4L2_FRMSIZE_TYPE_STEPWISE:
                                msg_Dbg( p_obj,
                "    device supports sizes %dx%d to %dx%d using %dx%d increments",
                frmsize.stepwise.min_width, frmsize.stepwise.min_height,
                frmsize.stepwise.max_width, frmsize.stepwise.max_height,
                frmsize.stepwise.step_width, frmsize.stepwise.step_height );
                                break;
                            case V4L2_FRMSIZE_TYPE_CONTINUOUS:
                                msg_Dbg( p_obj,
                "    device supports all sizes %dx%d to %dx%d",
                frmsize.stepwise.min_width, frmsize.stepwise.min_height,
                frmsize.stepwise.max_width, frmsize.stepwise.max_height );
                                break;
                        }
                    }
#endif
                }
            }
            if( !b_codec_supported )
            {
                    msg_Dbg( p_obj,
                         "device codec %4.4s (%s) not supported",
                         psz_fourcc_v4l2,
                         p_sys->p_codecs[i_index].description );
            }
        }
    }
    return 0;
}

static void name2var( unsigned char *name )
{
    for( ; *name; name++ )
        *name = (*name == ' ') ? '_' : tolower( *name );
}

/*****************************************************************************
 * Print a user-class v4l2 control's details, create the relevant variable,
 * change the value if needed.
 *****************************************************************************/
static void ControlListPrint( vlc_object_t *p_obj, int i_fd,
                              struct v4l2_queryctrl queryctrl,
                              bool b_reset, bool b_demux )
{
    struct v4l2_querymenu querymenu;
    unsigned int i_mid;

    int i;
    int i_val;

    char *psz_name;
    vlc_value_t val, val2;

    if( queryctrl.flags & V4L2_CTRL_FLAG_GRABBED )
        msg_Dbg( p_obj, "    control is busy" );
    if( queryctrl.flags & V4L2_CTRL_FLAG_READ_ONLY )
        msg_Dbg( p_obj, "    control is read-only" );

    for( i = 0; controls[i].psz_name != NULL; i++ )
        if( controls[i].i_cid == queryctrl.id ) break;

    if( controls[i].psz_name )
    {
        psz_name = strdup( controls[i].psz_name );
        char psz_cfg_name[40];
        sprintf( psz_cfg_name, CFG_PREFIX "%s", psz_name );
        i_val = var_CreateGetInteger( p_obj, psz_cfg_name );
        var_Destroy( p_obj, psz_cfg_name );
    }
    else
    {
        psz_name = strdup( (const char *)queryctrl.name );
        name2var( (unsigned char *)psz_name );
        i_val = -1;
    }

    switch( queryctrl.type )
    {
        case V4L2_CTRL_TYPE_INTEGER:
            msg_Dbg( p_obj, "    integer control" );
            msg_Dbg( p_obj,
                     "    valid values: %d to %d by steps of %d",
                     queryctrl.minimum, queryctrl.maximum,
                     queryctrl.step );

            var_Create( p_obj, psz_name,
                        VLC_VAR_INTEGER | VLC_VAR_HASMIN | VLC_VAR_HASMAX
                      | VLC_VAR_HASSTEP | VLC_VAR_ISCOMMAND );
            val.i_int = queryctrl.minimum;
            var_Change( p_obj, psz_name, VLC_VAR_SETMIN, &val, NULL );
            val.i_int = queryctrl.maximum;
            var_Change( p_obj, psz_name, VLC_VAR_SETMAX, &val, NULL );
            val.i_int = queryctrl.step;
            var_Change( p_obj, psz_name, VLC_VAR_SETSTEP, &val, NULL );
            break;
        case V4L2_CTRL_TYPE_BOOLEAN:
            msg_Dbg( p_obj, "    boolean control" );
            var_Create( p_obj, psz_name,
                        VLC_VAR_BOOL | VLC_VAR_ISCOMMAND );
            break;
        case V4L2_CTRL_TYPE_MENU:
            msg_Dbg( p_obj, "    menu control" );
            var_Create( p_obj, psz_name,
                        VLC_VAR_INTEGER | VLC_VAR_HASCHOICE
                      | VLC_VAR_ISCOMMAND );
            memset( &querymenu, 0, sizeof( querymenu ) );
            for( i_mid = queryctrl.minimum;
                 i_mid <= (unsigned)queryctrl.maximum;
                 i_mid++ )
            {
                querymenu.index = i_mid;
                querymenu.id = queryctrl.id;
                if( v4l2_ioctl( i_fd, VIDIOC_QUERYMENU, &querymenu ) >= 0 )
                {
                    msg_Dbg( p_obj, "        %d: %s",
                             querymenu.index, querymenu.name );
                    val.i_int = querymenu.index;
                    val2.psz_string = (char *)querymenu.name;
                    var_Change( p_obj, psz_name,
                                VLC_VAR_ADDCHOICE, &val, &val2 );
                }
            }
            break;
        case V4L2_CTRL_TYPE_BUTTON:
            msg_Dbg( p_obj, "    button control" );
            var_Create( p_obj, psz_name,
                        VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
            break;
        case V4L2_CTRL_TYPE_CTRL_CLASS:
            msg_Dbg( p_obj, "    control class" );
            var_Create( p_obj, psz_name, VLC_VAR_VOID );
            break;
        default:
            msg_Dbg( p_obj, "    unknown control type (FIXME)" );
            /* FIXME */
            break;
    }

    switch( queryctrl.type )
    {
        case V4L2_CTRL_TYPE_INTEGER:
        case V4L2_CTRL_TYPE_BOOLEAN:
        case V4L2_CTRL_TYPE_MENU:
            {
                struct v4l2_control control;
                msg_Dbg( p_obj, "    default value: %d",
                         queryctrl.default_value );
                memset( &control, 0, sizeof( control ) );
                control.id = queryctrl.id;
                if( v4l2_ioctl( i_fd, VIDIOC_G_CTRL, &control ) >= 0 )
                {
                    msg_Dbg( p_obj, "    current value: %d", control.value );
                }
                if( i_val == -1 )
                {
                    i_val = control.value;
                    if( b_reset && queryctrl.default_value != control.value )
                    {
                        msg_Dbg( p_obj, "    reset value to default" );
                        Control( p_obj, i_fd, psz_name,
                                 queryctrl.id, queryctrl.default_value );
                    }
                }
                else
                {
                    Control( p_obj, i_fd, psz_name, queryctrl.id, i_val );
                }
            }
            break;
        default:
            break;
    }

    val.psz_string = (char *)queryctrl.name;
    var_Change( p_obj, psz_name, VLC_VAR_SETTEXT, &val, NULL );
    val.i_int = queryctrl.id;
    val2.psz_string = (char *)psz_name;
    var_Change( p_obj, "allcontrols", VLC_VAR_ADDCHOICE, &val, &val2 );
    /* bad things happen changing MPEG mid-stream
     * so don't add to Ext Settings GUI */
    if( V4L2_CTRL_ID2CLASS( queryctrl.id ) != V4L2_CTRL_CLASS_MPEG )
        var_Change( p_obj, "controls", VLC_VAR_ADDCHOICE, &val, &val2 );

    switch( var_Type( p_obj, psz_name ) & VLC_VAR_TYPE )
    {
        case VLC_VAR_BOOL:
            var_SetBool( p_obj, psz_name, i_val );
            break;
        case VLC_VAR_INTEGER:
            var_SetInteger( p_obj, psz_name, i_val );
            break;
        case VLC_VAR_VOID:
            break;
        default:
            msg_Warn( p_obj, "FIXME: %s %s %d", __FILE__, __func__,
                      __LINE__ );
            break;
    }

    if( b_demux )
        var_AddCallback( p_obj, psz_name,
                        DemuxControlCallback, (void*)(intptr_t)queryctrl.id );
    else
        var_AddCallback( p_obj, psz_name,
                        AccessControlCallback, (void*)(intptr_t)queryctrl.id );

    free( psz_name );
}

/*****************************************************************************
 * List all user-class v4l2 controls, set them to the user specified
 * value and create the relevant variables to enable runtime changes
 *****************************************************************************/
static int ControlList( vlc_object_t *p_obj, int i_fd, bool b_reset,
                        bool b_demux )
{
    struct v4l2_queryctrl queryctrl;
    int i_cid;

    memset( &queryctrl, 0, sizeof( queryctrl ) );

    /* A list of available controls (aka the variable name) will be
     * stored as choices in the "allcontrols" variable. We'll thus be able
     * to use those to create an appropriate interface
     * A list of available controls that can be changed mid-stream will
     * be stored in the "controls" variable */
    var_Create( p_obj, "controls", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
    var_Create( p_obj, "allcontrols", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );

    var_Create( p_obj, "controls-update", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );

    /* Add a control to reset all controls to their default values */
    vlc_value_t val, val2;
    var_Create( p_obj, "controls-reset", VLC_VAR_VOID | VLC_VAR_ISCOMMAND );
    val.psz_string = _( "Reset controls to default" );
    var_Change( p_obj, "controls-reset", VLC_VAR_SETTEXT, &val, NULL );
    val.i_int = -1;
    val2.psz_string = (char *)"controls-reset";
    var_Change( p_obj, "controls", VLC_VAR_ADDCHOICE, &val, &val2 );
    if (b_demux)
        var_AddCallback( p_obj, "controls-reset", DemuxControlResetCallback, NULL );
    else
        var_AddCallback( p_obj, "controls-reset", AccessControlResetCallback, NULL );

    queryctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
    if( v4l2_ioctl( i_fd, VIDIOC_QUERYCTRL, &queryctrl ) >= 0 )
    {
        msg_Dbg( p_obj, "Extended control API supported by v4l2 driver" );

        /* List extended controls */
        queryctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
        while( v4l2_ioctl( i_fd, VIDIOC_QUERYCTRL, &queryctrl ) >= 0 )
        {
            if( queryctrl.type == V4L2_CTRL_TYPE_CTRL_CLASS )
            {
                msg_Dbg( p_obj, "%s", queryctrl.name );
                queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
                continue;
            }
            switch( V4L2_CTRL_ID2CLASS( queryctrl.id ) )
            {
                case V4L2_CTRL_CLASS_USER:
                    msg_Dbg( p_obj, "Available control: %s (%x)",
                             queryctrl.name, queryctrl.id );
                    break;
                case V4L2_CTRL_CLASS_MPEG:
                    name2var( queryctrl.name );
                    msg_Dbg( p_obj, "Available MPEG control: %s (%x)",
                             queryctrl.name, queryctrl.id );
                    break;
                default:
                    msg_Dbg( p_obj, "Available private control: %s (%x)",
                             queryctrl.name, queryctrl.id );
                    break;
            }
            ControlListPrint( p_obj, i_fd, queryctrl, b_reset, b_demux );
            queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
        }
    }
    else
    {
        msg_Dbg( p_obj, "Extended control API not supported by v4l2 driver" );

        /* List public controls */
        for( i_cid = V4L2_CID_BASE;
             i_cid < V4L2_CID_LASTP1;
             i_cid ++ )
        {
            queryctrl.id = i_cid;
            if( v4l2_ioctl( i_fd, VIDIOC_QUERYCTRL, &queryctrl ) >= 0 )
            {
                if( queryctrl.flags & V4L2_CTRL_FLAG_DISABLED )
                    continue;
                msg_Dbg( p_obj, "Available control: %s (%x)",
                         queryctrl.name, queryctrl.id );
                ControlListPrint( p_obj, i_fd, queryctrl, b_reset, b_demux );
            }
        }

        /* List private controls */
        for( i_cid = V4L2_CID_PRIVATE_BASE;
             ;
             i_cid ++ )
        {
            queryctrl.id = i_cid;
            if( v4l2_ioctl( i_fd, VIDIOC_QUERYCTRL, &queryctrl ) >= 0 )
            {
                if( queryctrl.flags & V4L2_CTRL_FLAG_DISABLED )
                    continue;
                msg_Dbg( p_obj, "Available private control: %s (%x)",
                         queryctrl.name, queryctrl.id );
                ControlListPrint( p_obj, i_fd, queryctrl, b_reset, b_demux );
            }
            else
                break;
        }
    }

    return VLC_SUCCESS;
}

static void SetAvailControlsByString( vlc_object_t *p_obj, demux_sys_t *p_sys,
                                      int i_fd )
{
    char *psz_parser = p_sys->psz_set_ctrls;
    vlc_value_t val, text, name;

    if( psz_parser == NULL )
        return;

    if( *psz_parser == '{' )
        psz_parser++;

    int i_ret = var_Change( p_obj, "allcontrols", VLC_VAR_GETCHOICES,
                            &val, &text );
    if( i_ret < 0 )
    {
        msg_Err( p_obj, "Oops, can't find 'allcontrols' variable." );
        return;
    }

    while( *psz_parser && *psz_parser != '}' )
    {
        char *psz_delim, *psz_assign;

        while( *psz_parser == ',' || *psz_parser == ' ' )
            psz_parser++;

        psz_delim = strchr( psz_parser, ',' );
        if( psz_delim == NULL )
            psz_delim = strchr( psz_parser, '}' );
        if( psz_delim == NULL )
            psz_delim = psz_parser + strlen( psz_parser );

        psz_assign = memchr( psz_parser, '=', psz_delim - psz_parser );
        if( psz_assign == NULL )
        {
            char *psz_name = strndup( psz_parser, psz_delim - psz_parser );
            msg_Err( p_obj, "%s missing '='", psz_name );
            free( psz_name );
            psz_parser = psz_delim + 1;
            continue;
        }

        for( int i = 0;
             i < val.p_list->i_count ;//&& psz_parser < psz_assign;
             i++ )
        {
            const char *psz_var = text.p_list->p_values[i].psz_string;
            int i_cid = val.p_list->p_values[i].i_int;
            var_Change( p_obj, psz_var, VLC_VAR_GETTEXT, &name, NULL );
            const char *psz_name = name.psz_string;

            int i_availstrlen = strlen( psz_name );
            int i_parsestrlen = psz_assign - psz_parser;
            int i_maxstrlen = __MAX( i_availstrlen, i_parsestrlen);

            if( !strncasecmp( psz_name, psz_parser, i_maxstrlen ) )
            {
                Control( p_obj, i_fd, psz_name, i_cid,
                         strtol( ++psz_assign, &psz_parser, 0) );
            }
            free( name.psz_string );
        }

        if( psz_parser < psz_assign )
        {
            char *psz_name = strndup( psz_parser, psz_assign - psz_parser );
            msg_Err( p_obj, "Control %s not available", psz_name );
            free( psz_name );
            psz_parser = ( *psz_delim ) ? ( psz_delim + 1 ) : psz_delim;
        }
    }
    var_FreeList( &val, &text );
}

/*****************************************************************************
 * Reset all user-class v4l2 controls to their default value
 *****************************************************************************/
static int ControlReset( vlc_object_t *p_obj, int i_fd )
{
    struct v4l2_queryctrl queryctrl;
    int i_cid;
    memset( &queryctrl, 0, sizeof( queryctrl ) );

    queryctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
    if( v4l2_ioctl( i_fd, VIDIOC_QUERYCTRL, &queryctrl ) >= 0 )
    {
        /* Extended control API supported */
        queryctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
        while( v4l2_ioctl( i_fd, VIDIOC_QUERYCTRL, &queryctrl ) >= 0 )
        {
            if( queryctrl.type == V4L2_CTRL_TYPE_CTRL_CLASS
             || V4L2_CTRL_ID2CLASS( queryctrl.id ) == V4L2_CTRL_CLASS_MPEG )
            {
                queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
                continue;
            }
            struct v4l2_control control;
            memset( &control, 0, sizeof( control ) );
            control.id = queryctrl.id;
            if( v4l2_ioctl( i_fd, VIDIOC_G_CTRL, &control ) >= 0
             && queryctrl.default_value != control.value )
            {
                int i;
                for( i = 0; controls[i].psz_name != NULL; i++ )
                    if( controls[i].i_cid == queryctrl.id ) break;
                name2var( queryctrl.name );
                Control( p_obj, i_fd,
                         controls[i].psz_name ? controls[i].psz_name
                          : (const char *)queryctrl.name,
                         queryctrl.id, queryctrl.default_value );
            }
            queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
        }
    }
    else
    {

        /* public controls */
        for( i_cid = V4L2_CID_BASE;
             i_cid < V4L2_CID_LASTP1;
             i_cid ++ )
        {
            queryctrl.id = i_cid;
            if( v4l2_ioctl( i_fd, VIDIOC_QUERYCTRL, &queryctrl ) >= 0 )
            {
                struct v4l2_control control;
                if( queryctrl.flags & V4L2_CTRL_FLAG_DISABLED )
                    continue;
                memset( &control, 0, sizeof( control ) );
                control.id = queryctrl.id;
                if( v4l2_ioctl( i_fd, VIDIOC_G_CTRL, &control ) >= 0
                 && queryctrl.default_value != control.value )
                {
                    int i;
                    for( i = 0; controls[i].psz_name != NULL; i++ )
                        if( controls[i].i_cid == queryctrl.id ) break;
                    name2var( queryctrl.name );
                    Control( p_obj, i_fd,
                             controls[i].psz_name ? controls[i].psz_name
                              : (const char *)queryctrl.name,
                             queryctrl.id, queryctrl.default_value );
                }
            }
        }

        /* private controls */
        for( i_cid = V4L2_CID_PRIVATE_BASE;
             ;
             i_cid ++ )
        {
            queryctrl.id = i_cid;
            if( v4l2_ioctl( i_fd, VIDIOC_QUERYCTRL, &queryctrl ) >= 0 )
            {
                struct v4l2_control control;
                if( queryctrl.flags & V4L2_CTRL_FLAG_DISABLED )
                    continue;
                memset( &control, 0, sizeof( control ) );
                control.id = queryctrl.id;
                if( v4l2_ioctl( i_fd, VIDIOC_G_CTRL, &control ) >= 0
                 && queryctrl.default_value != control.value )
                {
                    name2var( queryctrl.name );
                    Control( p_obj, i_fd, (const char *)queryctrl.name,
                             queryctrl.id, queryctrl.default_value );
                }
            }
            else
                break;
        }
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Issue user-class v4l2 controls
 *****************************************************************************/
static int Control( vlc_object_t *p_obj, int i_fd,
                    const char *psz_name, int i_cid, int i_value )
{
    struct v4l2_queryctrl queryctrl;
    struct v4l2_control control;
    struct v4l2_ext_control ext_control;
    struct v4l2_ext_controls ext_controls;

    if( i_value == -1 )
        return VLC_SUCCESS;

    memset( &queryctrl, 0, sizeof( queryctrl ) );

    queryctrl.id = i_cid;

    if( v4l2_ioctl( i_fd, VIDIOC_QUERYCTRL, &queryctrl ) < 0
        || queryctrl.flags & V4L2_CTRL_FLAG_DISABLED )
    {
        msg_Dbg( p_obj, "%s (%x) control is not supported.", psz_name, i_cid );
        return VLC_EGENERIC;
    }

    memset( &control, 0, sizeof( control ) );
    memset( &ext_control, 0, sizeof( ext_control ) );
    memset( &ext_controls, 0, sizeof( ext_controls ) );
    control.id = i_cid;
    ext_control.id = i_cid;
    ext_controls.ctrl_class = V4L2_CTRL_ID2CLASS( i_cid );
    ext_controls.count = 1;
    ext_controls.controls = &ext_control;

    int i_ret = -1;

    if( i_value >= queryctrl.minimum && i_value <= queryctrl.maximum )
    {
        ext_control.value = i_value;
        if( v4l2_ioctl( i_fd, VIDIOC_S_EXT_CTRLS, &ext_controls ) < 0 )
        {
            control.value = i_value;
            if( v4l2_ioctl( i_fd, VIDIOC_S_CTRL, &control ) < 0 )
            {
                msg_Err( p_obj, "unable to set %s (%x) to %d (%m)",
                         psz_name, i_cid, i_value );
                return VLC_EGENERIC;
            }
            i_ret = v4l2_ioctl( i_fd, VIDIOC_G_CTRL, &control );
        }
        else
        {
            i_ret = v4l2_ioctl( i_fd, VIDIOC_G_EXT_CTRLS, &ext_controls );
            control.value = ext_control.value;
        }
    }

    if( i_ret >= 0 )
    {
        vlc_value_t val;
        msg_Dbg( p_obj, "video %s: %d", psz_name, control.value );
        switch( var_Type( p_obj, psz_name ) & VLC_VAR_TYPE )
        {
            case VLC_VAR_BOOL:
                val.b_bool = control.value;
                var_Change( p_obj, psz_name, VLC_VAR_SETVALUE, &val, NULL );
                var_TriggerCallback( p_obj, "controls-update" );
                break;
            case VLC_VAR_INTEGER:
                val.i_int = control.value;
                var_Change( p_obj, psz_name, VLC_VAR_SETVALUE, &val, NULL );
                var_TriggerCallback( p_obj, "controls-update" );
                break;
        }
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * On the fly change settings callback
 *****************************************************************************/
static int DemuxControlCallback( vlc_object_t *p_this,
    const char *psz_var, vlc_value_t oldval, vlc_value_t newval,
    void *p_data )
{
    (void)oldval;
    demux_t *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_cid = (long int)p_data;

    int i_fd = p_sys->i_fd;

    if( i_fd < 0 )
        return VLC_EGENERIC;

    Control( p_this, i_fd, psz_var, i_cid, newval.i_int );

    return VLC_EGENERIC;
}

static int DemuxControlResetCallback( vlc_object_t *p_this,
    const char *psz_var, vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    (void)psz_var;    (void)oldval;    (void)newval;    (void)p_data;
    demux_t *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    int i_fd = p_sys->i_fd;

    if( i_fd < 0 )
        return VLC_EGENERIC;

    ControlReset( p_this, i_fd );

    return VLC_EGENERIC;
}

static int AccessControlCallback( vlc_object_t *p_this,
    const char *psz_var, vlc_value_t oldval, vlc_value_t newval,
    void *p_data )
{
    (void)oldval;
    access_t *p_access = (access_t *)p_this;
    demux_sys_t *p_sys = (demux_sys_t *) p_access->p_sys;
    int i_cid = (long int)p_data;

    int i_fd = p_sys->i_fd;

    if( i_fd < 0 )
        return VLC_EGENERIC;

    Control( p_this, i_fd, psz_var, i_cid, newval.i_int );

    return VLC_EGENERIC;
}

static int AccessControlResetCallback( vlc_object_t *p_this,
    const char *psz_var, vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    (void)psz_var;     (void)oldval;     (void)newval;     (void)p_data;
    access_t *p_access = (access_t *)p_this;
    demux_sys_t *p_sys = (demux_sys_t *) p_access->p_sys;

    int i_fd = p_sys->i_fd;

    if( i_fd < 0 )
        return VLC_EGENERIC;

    ControlReset( p_this, i_fd );

    return VLC_EGENERIC;
}
