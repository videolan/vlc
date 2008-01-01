/*****************************************************************************
 * v4l2.c : Video4Linux2 input module for vlc
 *****************************************************************************
 * Copyright (C) 2002-2007 the VideoLAN team
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
 *
 * ALSA support based on parts of
 * http://www.equalarea.com/paul/alsa-audio.html
 * and hints taken from alsa-utils (aplay/arecord)
 * http://www.alsa-project.org
 */

/*
 * TODO: Tuner partial implementation.
 * TODO: Add more MPEG stream params
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include <vlc/vlc.h>
#include <vlc_access.h>
#include <vlc_demux.h>
#include <vlc_input.h>
#include <vlc_vout.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <linux/videodev2.h>

#include <sys/soundcard.h>

#ifdef HAVE_ALSA
# define ALSA_PCM_NEW_HW_PARAMS_API
# define ALSA_PCM_NEW_SW_PARAMS_API
# include <alsa/asoundlib.h>
#endif

/*****************************************************************************
 * Module descriptior
 *****************************************************************************/

static int  DemuxOpen ( vlc_object_t * );
static void DemuxClose( vlc_object_t * );
static int  AccessOpen ( vlc_object_t * );
static void AccessClose( vlc_object_t * );

#define DEV_TEXT N_("Device name")
#define DEV_LONGTEXT N_( \
    "Name of the device to use. " \
    "If you don't specify anything, /dev/video0 will be used.")
#define STANDARD_TEXT N_( "Standard" )
#define STANDARD_LONGTEXT N_( \
    "Video standard (Default, SECAM, PAL, or NTSC)." )
#define CHROMA_TEXT N_("Video input chroma format")
#define CHROMA_LONGTEXT N_( \
    "Force the Video4Linux2 video device to use a specific chroma format " \
    "(eg. I420 or I422 for raw images, MJPEG for M-JPEG compressed input) " \
    "(Complete list: GREY, I240, RV16, RV15, RV24, RV32, YUY2, YUYV, UYVY, " \
    "I41N, I422, I420, I411, I410, MJPG)")
#define INPUT_TEXT N_( "Input" )
#define INPUT_LONGTEXT N_( \
    "Input of the card to use (Usually, 0 = tuner, " \
    "1 = composite, 2 = svideo)." )
#define IOMETHOD_TEXT N_( "IO Method" )
#define IOMETHOD_LONGTEXT N_( \
    "IO Method (READ, MMAP, USERPTR)." )
#define WIDTH_TEXT N_( "Width" )
#define WIDTH_LONGTEXT N_( \
    "Force width (-1 for autodetect)." )
#define HEIGHT_TEXT N_( "Height" )
#define HEIGHT_LONGTEXT N_( \
    "Force height (-1 for autodetect)." )
#define FPS_TEXT N_( "Framerate" )
#define FPS_LONGTEXT N_( "Framerate to capture, if applicable " \
    "(-1 for autodetect)." )

#define CTRL_RESET_TEXT N_( "Reset v4l2 controls" )
#define CTRL_RESET_LONGTEXT N_( \
    "Reset controls to defaults provided by the v4l2 driver." )
#define BRIGHTNESS_TEXT N_( "Brightness" )
#define BRIGHTNESS_LONGTEXT N_( \
    "Brightness of the video input (if supported by v4l2 driver)." )
#define CONTRAST_TEXT N_( "Contrast" )
#define CONTRAST_LONGTEXT N_( \
    "Contrast of the video input (if supported by v4l2 driver)." )
#define SATURATION_TEXT N_( "Saturation" )
#define SATURATION_LONGTEXT N_( \
    "Saturation of the video input (if supported by v4l2 driver)." )
#define HUE_TEXT N_( "Hue" )
#define HUE_LONGTEXT N_( \
    "Hue of the video input (if supported by v4l2 driver)." )
#define GAMMA_TEXT N_( "Gamma" )
#define GAMMA_LONGTEXT N_( \
    "Gamma of the video input (if supported by v4l2 driver)." )

#define ADEV_TEXT N_("Audio device name")
#define ADEV_LONGTEXT N_( \
    "Name of the audio device to use. " \
    "If you don't specify anything, /dev/dsp will be used.")
#define ALSA_TEXT N_( "Use Alsa" )
#define ALSA_LONGTEXT N_( \
    "Use ALSA instead of OSS for audio" )
#define STEREO_TEXT N_( "Stereo" )
#define STEREO_LONGTEXT N_( \
    "Capture the audio stream in stereo." )
#define SAMPLERATE_TEXT N_( "Samplerate" )
#define SAMPLERATE_LONGTEXT N_( \
    "Samplerate of the captured audio stream, in Hz (eg: 11025, 22050, 44100, 48000)" )

#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Caching value for V4L2 captures. This " \
    "value should be set in milliseconds." )

typedef enum {
    IO_METHOD_READ,
    IO_METHOD_MMAP,
    IO_METHOD_USERPTR,
} io_method;

static int i_standards_list[] =
    { V4L2_STD_UNKNOWN, V4L2_STD_SECAM, V4L2_STD_PAL, V4L2_STD_NTSC };
static const char *psz_standards_list_text[] =
    { N_("Default"), N_("SECAM"), N_("PAL"),  N_("NTSC") };

static int i_iomethod_list[] =
    { IO_METHOD_READ, IO_METHOD_MMAP, IO_METHOD_USERPTR };
static const char *psz_iomethod_list_text[] =
    { N_("READ"), N_("MMAP"),  N_("USERPTR") };

#define CFG_PREFIX "v4l2-"

vlc_module_begin();
    set_shortname( _("Video4Linux2") );
    set_description( _("Video4Linux2 input") );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS );

    set_section( N_( "Video input" ), NULL );
    add_string( CFG_PREFIX "dev", "/dev/video0", 0, DEV_TEXT, DEV_LONGTEXT,
                VLC_FALSE );
    add_integer( CFG_PREFIX "standard", 0, NULL, STANDARD_TEXT,
                 STANDARD_LONGTEXT, VLC_FALSE );
        change_integer_list( i_standards_list, psz_standards_list_text, 0 );
    add_string( CFG_PREFIX "chroma", NULL, NULL, CHROMA_TEXT, CHROMA_LONGTEXT,
                VLC_TRUE );
    add_integer( CFG_PREFIX "input", 0, NULL, INPUT_TEXT, INPUT_LONGTEXT,
                VLC_TRUE );
    add_integer( CFG_PREFIX "io", IO_METHOD_MMAP, NULL, IOMETHOD_TEXT,
                 IOMETHOD_LONGTEXT, VLC_TRUE );
        change_integer_list( i_iomethod_list, psz_iomethod_list_text, 0 );
    add_integer( CFG_PREFIX "width", 0, NULL, WIDTH_TEXT,
                WIDTH_LONGTEXT, VLC_TRUE );
    add_integer( CFG_PREFIX "height", 0, NULL, HEIGHT_TEXT,
                HEIGHT_LONGTEXT, VLC_TRUE );
    add_float( CFG_PREFIX "fps", 0, NULL, FPS_TEXT, FPS_LONGTEXT, VLC_TRUE );

    set_section( N_( "Audio input" ), NULL );
    add_string( CFG_PREFIX "adev", "/dev/dsp", 0, ADEV_TEXT, ADEV_LONGTEXT,
                VLC_FALSE );
#ifdef HAVE_ALSA
    add_bool( CFG_PREFIX "alsa", VLC_FALSE, NULL, ALSA_TEXT, ALSA_LONGTEXT,
                VLC_TRUE );
#endif
    add_bool( CFG_PREFIX "stereo", VLC_TRUE, NULL, STEREO_TEXT, STEREO_LONGTEXT,
                VLC_TRUE );
    add_integer( CFG_PREFIX "samplerate", 48000, NULL, SAMPLERATE_TEXT,
                SAMPLERATE_LONGTEXT, VLC_TRUE );
    add_integer( CFG_PREFIX "caching", DEFAULT_PTS_DELAY / 1000, NULL,
                CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );

    set_section( N_( "Controls" ), N_( "v4l2 driver controls" ) );
    add_bool( CFG_PREFIX "controls-reset", VLC_FALSE, NULL, CTRL_RESET_TEXT,
              CTRL_RESET_LONGTEXT, VLC_TRUE );
    add_integer( CFG_PREFIX "brightness", -1, NULL, BRIGHTNESS_TEXT,
                BRIGHTNESS_LONGTEXT, VLC_TRUE );
    add_integer( CFG_PREFIX "contrast", -1, NULL, CONTRAST_TEXT,
                CONTRAST_LONGTEXT, VLC_TRUE );
    add_integer( CFG_PREFIX "saturation", -1, NULL, SATURATION_TEXT,
                SATURATION_LONGTEXT, VLC_TRUE );
    add_integer( CFG_PREFIX "hue", -1, NULL, HUE_TEXT,
                HUE_LONGTEXT, VLC_TRUE );
    add_integer( CFG_PREFIX "gamma", -1, NULL, GAMMA_TEXT,
                GAMMA_LONGTEXT, VLC_TRUE );


    add_shortcut( "v4l2" );
    set_capability( "access_demux", 10 );
    set_callbacks( DemuxOpen, DemuxClose );

    add_submodule();
    set_description( _("Video4Linux2 Compressed A/V") );
    set_capability( "access2", 0 );
    /* use these when open as access_demux fails; VLC will use another demux */
    set_callbacks( AccessOpen, AccessClose );

vlc_module_end();

/*****************************************************************************
 * Access: local prototypes
 *****************************************************************************/

static void CommonClose( vlc_object_t *, demux_sys_t * );
static void ParseMRL( demux_sys_t *, char *, vlc_object_t * );
static void GetV4L2Params( demux_sys_t *, vlc_object_t * );

static int DemuxControl( demux_t *, int, va_list );
static int AccessControl( access_t *, int, va_list );

static int Demux( demux_t * );
static ssize_t AccessRead( access_t *, uint8_t *, size_t );

static block_t* GrabVideo( demux_t *p_demux );
static block_t* ProcessVideoFrame( demux_t *p_demux, uint8_t *p_frame, size_t );
static block_t* GrabAudio( demux_t *p_demux );

static vlc_bool_t IsPixelFormatSupported( demux_t *p_demux,
                                          unsigned int i_pixelformat );

static char* ResolveALSADeviceName( char *psz_device );
static int OpenVideoDev( vlc_object_t *, demux_sys_t *, vlc_bool_t );
static int OpenAudioDev( vlc_object_t *, demux_sys_t *, vlc_bool_t );
static vlc_bool_t ProbeVideoDev( vlc_object_t *, demux_sys_t *,
                                 char *psz_device );
static vlc_bool_t ProbeAudioDev( vlc_object_t *, demux_sys_t *,
                                 char *psz_device );

static int ControlList( vlc_object_t *, int , vlc_bool_t, vlc_bool_t );
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

static struct
{
    unsigned int i_v4l2;
    int i_fourcc;
} v4l2chroma_to_fourcc[] =
{
    /* Raw data types */
    { V4L2_PIX_FMT_GREY,    VLC_FOURCC('G','R','E','Y') },
    { V4L2_PIX_FMT_HI240,   VLC_FOURCC('I','2','4','0') },
    { V4L2_PIX_FMT_RGB565,  VLC_FOURCC('R','V','1','6') },
    { V4L2_PIX_FMT_RGB555,  VLC_FOURCC('R','V','1','5') },
    { V4L2_PIX_FMT_BGR24,   VLC_FOURCC('R','V','2','4') },
    { V4L2_PIX_FMT_BGR32,   VLC_FOURCC('R','V','3','2') },
    { V4L2_PIX_FMT_YUYV,    VLC_FOURCC('Y','U','Y','2') },
    { V4L2_PIX_FMT_YUYV,    VLC_FOURCC('Y','U','Y','V') },
    { V4L2_PIX_FMT_UYVY,    VLC_FOURCC('U','Y','V','Y') },
    { V4L2_PIX_FMT_Y41P,    VLC_FOURCC('I','4','1','N') },
    { V4L2_PIX_FMT_YUV422P, VLC_FOURCC('I','4','2','2') },
    { V4L2_PIX_FMT_YVU420,  VLC_FOURCC('I','4','2','0') },
    { V4L2_PIX_FMT_YUV411P, VLC_FOURCC('I','4','1','1') },
    { V4L2_PIX_FMT_YUV410,  VLC_FOURCC('I','4','1','0') },
    /* Compressed data types */
    { V4L2_PIX_FMT_MJPEG,   VLC_FOURCC('M','J','P','G') },
#if 0
    { V4L2_PIX_FMT_JPEG,    VLC_FOURCC('J','P','E','G') },
    { V4L2_PIX_FMT_DV,      VLC_FOURCC('?','?','?','?') },
    { V4L2_PIX_FMT_MPEG,    VLC_FOURCC('?','?','?','?') },
#endif
    { 0, 0 }
};

static struct
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
    void *  orig_userp;
};

struct demux_sys_t
{
    char *psz_device;  /* Main device from MRL, can be video or audio */

    char *psz_vdev;
    int  i_fd_video;

    char *psz_adev;
    int  i_fd_audio;

    char *psz_requested_chroma;

    /* Video */
    io_method io;

    int i_pts;

    struct v4l2_capability dev_cap;

    int i_input;
    struct v4l2_input *p_inputs;
    int i_selected_input;

    int i_standard;
    struct v4l2_standard *p_standards;
    v4l2_std_id i_selected_standard_id;

    int i_audio;
    /* V4L2 devices cannot have more than 32 audio inputs */
    struct v4l2_audio p_audios[32];

    int i_tuner;
    struct v4l2_tuner *p_tuners;

    int i_codec;
    struct v4l2_fmtdesc *p_codecs;

    struct buffer_t *p_buffers;
    unsigned int i_nbuffers;

    int i_width;
    int i_height;
    float f_fps;            /* <= 0.0 mean to grab at full rate */
    mtime_t i_video_pts;    /* only used when f_fps > 0 */
    int i_fourcc;

    es_out_id_t *p_es_video;

    /* Audio */
    unsigned int i_sample_rate;
    vlc_bool_t b_stereo;
    int i_audio_max_frame_size;
    block_t *p_block_audio;
    es_out_id_t *p_es_audio;

#ifdef HAVE_ALSA
    /* ALSA Audio */
    vlc_bool_t b_use_alsa;
    snd_pcm_t *p_alsa_pcm;
    int i_alsa_frame_size;
    int i_alsa_chunk_size;
#endif
};

#define FIND_VIDEO 1
#define FIND_AUDIO 2

static int FindMainDevice( vlc_object_t *p_this, demux_sys_t *p_sys,
                           int i_flags, vlc_bool_t b_demux,
                           vlc_bool_t b_forced )
{
    /* Find main device (video or audio) */
    if( p_sys->psz_device && *p_sys->psz_device )
    {
        msg_Dbg( p_this, "main device='%s'", p_sys->psz_device );

        /* Try to open as video device */
        if( i_flags & FIND_VIDEO )
        {
            msg_Dbg( p_this, "trying device '%s' as video", p_sys->psz_device );
            if( ProbeVideoDev( p_this, p_sys, p_sys->psz_device ) )
            {
                msg_Dbg( p_this, "'%s' is a video device", p_sys->psz_device );
                /* Device was a video device */
                if( p_sys->psz_vdev ) free( p_sys->psz_vdev );
                p_sys->psz_vdev = p_sys->psz_device;
                p_sys->psz_device = NULL;
                p_sys->i_fd_video = OpenVideoDev( p_this, p_sys, b_demux );
                if( p_sys->i_fd_video < 0 )
                    return VLC_EGENERIC;
                return VLC_SUCCESS;
            }
        }

        if( i_flags & FIND_AUDIO )
        {
            /* Try to open as audio device */
            msg_Dbg( p_this, "trying device '%s' as audio", p_sys->psz_device );
            if( ProbeAudioDev( p_this, p_sys, p_sys->psz_device ) )
            {
                msg_Dbg( p_this, "'%s' is an audio device", p_sys->psz_device );
                /* Device was an audio device */
                if( p_sys->psz_adev ) free( p_sys->psz_adev );
                p_sys->psz_adev = p_sys->psz_device;
                p_sys->psz_device = NULL;
                p_sys->i_fd_audio = OpenAudioDev( p_this, p_sys, b_demux );
                if( p_sys->i_fd_audio < 0 )
                    return VLC_EGENERIC;
                return VLC_SUCCESS;
            }
        }
    }

    /* If no device opened, only continue if the access was forced */
    if( b_forced == VLC_FALSE
        && !( ( i_flags & FIND_VIDEO && p_sys->i_fd_video >= 0 )
           || ( i_flags & FIND_AUDIO && p_sys->i_fd_audio >= 0 ) ) )
    {
        return VLC_EGENERIC;
    }

    /* Find video device */
    if( i_flags & FIND_VIDEO && p_sys->i_fd_video < 0 )
    {
        if( !p_sys->psz_vdev || !*p_sys->psz_vdev )
        {
            if( p_sys->psz_vdev ) free( p_sys->psz_vdev );
            p_sys->psz_vdev = var_CreateGetString( p_this, "v4l2-dev" );
        }

        msg_Dbg( p_this, "opening '%s' as video", p_sys->psz_vdev );
        if( p_sys->psz_vdev && *p_sys->psz_vdev
         && ProbeVideoDev( p_this, p_sys, p_sys->psz_vdev ) )
        {
            p_sys->i_fd_video = OpenVideoDev( p_this, p_sys, b_demux );
        }
    }

    /* Find audio device */
    if( i_flags & FIND_AUDIO && p_sys->i_fd_audio < 0 )
    {
        if( !p_sys->psz_adev || !*p_sys->psz_adev )
        {
            if( p_sys->psz_adev ) free( p_sys->psz_adev );
            p_sys->psz_adev = var_CreateGetString( p_this, "v4l2-adev" );
        }

        msg_Dbg( p_this, "opening '%s' as audio", p_sys->psz_adev );
        if( p_sys->psz_adev && *p_sys->psz_adev
         && ProbeAudioDev( p_this, p_sys, p_sys->psz_adev ) )
        {
            p_sys->i_fd_audio = OpenAudioDev( p_this, p_sys, b_demux );
        }
    }

    if( !( ( i_flags & FIND_VIDEO && p_sys->i_fd_video >= 0 )
        || ( i_flags & FIND_AUDIO && p_sys->i_fd_audio >= 0 ) ) )
    {
        return VLC_EGENERIC;
    }
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

    /* Only when selected */
    if( *p_demux->psz_access == '\0' ) return VLC_EGENERIC;

    /* Set up p_demux */
    p_demux->pf_control = DemuxControl;
    p_demux->pf_demux = Demux;
    p_demux->info.i_update = 0;
    p_demux->info.i_title = 0;
    p_demux->info.i_seekpoint = 0;

    p_demux->p_sys = p_sys = calloc( 1, sizeof( demux_sys_t ) );
    if( p_sys == NULL ) return VLC_ENOMEM;

    GetV4L2Params(p_sys, (vlc_object_t *) p_demux);

    ParseMRL( p_sys, p_demux->psz_path, (vlc_object_t *) p_demux );

#ifdef HAVE_ALSA
    /* Alsa support available? */
    msg_Dbg( p_demux, "ALSA input support available" );
#endif

    if( FindMainDevice( p_this, p_sys, FIND_VIDEO|FIND_AUDIO,
        VLC_TRUE, !strcmp( p_demux->psz_access, "v4l2" ) ) != VLC_SUCCESS )
    {
        DemuxClose( p_this );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * GetV4L2Params: fill in p_sys parameters (shared by DemuxOpen and AccessOpen)
 *****************************************************************************/
static void GetV4L2Params( demux_sys_t *p_sys, vlc_object_t *p_obj )
{
    p_sys->i_video_pts = -1;

    p_sys->i_selected_standard_id =
        i_standards_list[var_CreateGetInteger( p_obj, "v4l2-standard" )];

    p_sys->i_selected_input = var_CreateGetInteger( p_obj, "v4l2-input" );

    p_sys->io = var_CreateGetInteger( p_obj, "v4l2-io" );

    p_sys->i_width = var_CreateGetInteger( p_obj, "v4l2-width" );
    p_sys->i_height = var_CreateGetInteger( p_obj, "v4l2-height" );

    var_CreateGetBool( p_obj, "v4l2-controls-reset" );

    p_sys->f_fps = var_CreateGetFloat( p_obj, "v4l2-fps" );
    p_sys->i_sample_rate = var_CreateGetInteger( p_obj, "v4l2-samplerate" );
    p_sys->psz_requested_chroma = var_CreateGetString( p_obj, "v4l2-chroma" );

#ifdef HAVE_ALSA
    p_sys->b_use_alsa = var_CreateGetBool( p_obj, "v4l2-alsa" );
#endif

    p_sys->b_stereo = var_CreateGetBool( p_obj, "v4l2-stereo" );

    p_sys->i_pts = var_CreateGetInteger( p_obj, "v4l2-caching" );

    p_sys->psz_device = p_sys->psz_vdev = p_sys->psz_adev = NULL;
    p_sys->i_fd_video = -1;
    p_sys->i_fd_audio = -1;

    p_sys->p_es_video = p_sys->p_es_audio = 0;
    p_sys->p_block_audio = 0;
}

/*****************************************************************************
 * ParseMRL: parse the options contained in the MRL
 *****************************************************************************/
static void ParseMRL( demux_sys_t *p_sys, char *psz_path, vlc_object_t *p_obj )
{
    char *psz_dup = strdup( psz_path );
    char *psz_parser = psz_dup;

    while( *psz_parser && *psz_parser != ':' )
    {
        psz_parser++;
    }

    if( *psz_parser == ':' )
    {
        /* read options */
        for( ;; )
        {
            *psz_parser++ = '\0';

            if( !strncmp( psz_parser, "adev=", strlen( "adev=" ) ) )
            {
                int  i_len;

                psz_parser += strlen( "adev=" );
                if( strchr( psz_parser, ':' ) )
                {
                    i_len = strchr( psz_parser, ':' ) - psz_parser;
                }
                else
                {
                    i_len = strlen( psz_parser );
                }

                p_sys->psz_adev = strndup( psz_parser, i_len );

                psz_parser += i_len;
            }
            else if( !strncmp( psz_parser, "standard=", strlen( "standard=" ) ) )
            {
                psz_parser += strlen( "standard=" );
                if( !strncmp( psz_parser, "pal", strlen( "pal" ) ) )
                {
                    p_sys->i_selected_standard_id = V4L2_STD_PAL;
                    psz_parser += strlen( "pal" );
                }
                else if( !strncmp( psz_parser, "ntsc", strlen( "ntsc" ) ) )
                {
                    p_sys->i_selected_standard_id = V4L2_STD_NTSC;
                    psz_parser += strlen( "ntsc" );
                }
                else if( !strncmp( psz_parser, "secam", strlen( "secam" ) ) )
                {
                    p_sys->i_selected_standard_id = V4L2_STD_SECAM;
                    psz_parser += strlen( "secam" );
                }
                else if( !strncmp( psz_parser, "default", strlen( "default" ) ) )
                {
                    p_sys->i_selected_standard_id = V4L2_STD_UNKNOWN;
                    psz_parser += strlen( "default" );
                }
                else
                {
                    p_sys->i_selected_standard_id = i_standards_list[strtol( psz_parser, &psz_parser, 0 )];
                }
            }
            else if( !strncmp( psz_parser, "chroma=", strlen( "chroma=" ) ) )
            {
                int  i_len;

                psz_parser += strlen( "chroma=" );
                if( strchr( psz_parser, ':' ) )
                {
                    i_len = strchr( psz_parser, ':' ) - psz_parser;
                }
                else
                {
                    i_len = strlen( psz_parser );
                }

                if( p_sys->psz_requested_chroma ) free( p_sys->psz_requested_chroma );
                p_sys->psz_requested_chroma = strndup( psz_parser, i_len );

                psz_parser += i_len;
            }
            else if( !strncmp( psz_parser, "input=", strlen( "input=" ) ) )
            {
                p_sys->i_selected_input = strtol( psz_parser + strlen( "input=" ),
                                       &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "fps=", strlen( "fps=" ) ) )
            {
                p_sys->f_fps = strtof( psz_parser + strlen( "fps=" ),
                                       &psz_parser );
            }
            else if( !strncmp( psz_parser, "io=", strlen( "io=" ) ) )
            {
                psz_parser += strlen( "io=" );
                if( !strncmp( psz_parser, "read", strlen( "read" ) ) )
                {
                    p_sys->io = IO_METHOD_READ;
                    psz_parser += strlen( "read" );
                }
                else if( !strncmp( psz_parser, "mmap", strlen( "mmap" ) ) )
                {
                    p_sys->io = IO_METHOD_MMAP;
                    psz_parser += strlen( "mmap" );
                }
                else if( !strncmp( psz_parser, "userptr", strlen( "userptr" ) ) )
                {
                    p_sys->io = IO_METHOD_USERPTR;
                    psz_parser += strlen( "userptr" );
                }
                else
                {
                    p_sys->io = strtol( psz_parser, &psz_parser, 0 );
                }
            }
            else if( !strncmp( psz_parser, "width=",
                               strlen( "width=" ) ) )
            {
                p_sys->i_width =
                    strtol( psz_parser + strlen( "width=" ),
                            &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "height=",
                               strlen( "height=" ) ) )
            {
                p_sys->i_height =
                    strtol( psz_parser + strlen( "height=" ),
                            &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "controls-reset",
                               strlen( "controls-reset" ) ) )
            {
                var_SetBool( p_obj, "v4l2-controls-reset", VLC_TRUE );
                psz_parser += strlen( "controls-reset" );
            }
#if 0
            else if( !strncmp( psz_parser, "brightness=",
                               strlen( "brightness=" ) ) )
            {
                var_SetInteger( p_obj, "brightness",
                    strtol( psz_parser + strlen( "brightness=" ),
                            &psz_parser, 0 ) );
            }
            else if( !strncmp( psz_parser, "contrast=",
                               strlen( "contrast=" ) ) )
            {
                var_SetInteger( p_obj, "contrast",
                    strtol( psz_parser + strlen( "contrast=" ),
                            &psz_parser, 0 ) );
            }
            else if( !strncmp( psz_parser, "saturation=",
                               strlen( "saturation=" ) ) )
            {
                var_SetInteger( p_obj, "saturation",
                    strtol( psz_parser + strlen( "saturation=" ),
                            &psz_parser, 0 ) );
            }
            else if( !strncmp( psz_parser, "hue=",
                               strlen( "hue=" ) ) )
            {
                var_SetInteger( p_obj, "hue",
                    strtol( psz_parser + strlen( "hue=" ),
                            &psz_parser, 0 ) );
            }
            else if( !strncmp( psz_parser, "gamma=",
                               strlen( "gamma=" ) ) )
            {
                var_SetInteger( p_obj, "gamma",
                    strtol( psz_parser + strlen( "gamma=" ),
                            &psz_parser, 0 ) );
            }
#endif
            else if( !strncmp( psz_parser, "samplerate=",
                               strlen( "samplerate=" ) ) )
            {
                p_sys->i_sample_rate =
                    strtol( psz_parser + strlen( "samplerate=" ),
                            &psz_parser, 0 );
            }
#ifdef HAVE_ALSA
            else if( !strncmp( psz_parser, "alsa", strlen( "alsa" ) ) )
            {
                psz_parser += strlen( "alsa" );
                p_sys->b_use_alsa = VLC_TRUE;
            }
#endif
            else if( !strncmp( psz_parser, "stereo", strlen( "stereo" ) ) )
            {
                psz_parser += strlen( "stereo" );
                p_sys->b_stereo = VLC_TRUE;
            }
            else if( !strncmp( psz_parser, "mono", strlen( "mono" ) ) )
            {
                psz_parser += strlen( "mono" );
                p_sys->b_stereo = VLC_FALSE;
            }
            else if( !strncmp( psz_parser, "caching=", strlen( "caching=" ) ) )
            {
                p_sys->i_pts = strtol( psz_parser + strlen( "caching=" ),
                                       &psz_parser, 0 );
            }
            else
            {
                msg_Warn( p_obj, "unknown option" );
            }

            while( *psz_parser && *psz_parser != ':' )
            {
                psz_parser++;
            }

            if( *psz_parser == '\0' )
            {
                break;
            }
        }
    }

    /* Main device */
    if( *psz_dup )
    {
        p_sys->psz_device = strdup( psz_dup );
    }
    if( psz_dup ) free( psz_dup );
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
    if( p_sys->i_fd_video >= 0 )
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
                ioctl( p_sys->i_fd_video, VIDIOC_DQBUF, &buf ); /* ignore result */
            }

            buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if( ioctl( p_sys->i_fd_video, VIDIOC_STREAMOFF, &buf_type ) < 0 ) {
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
                if( munmap( p_sys->p_buffers[i].start, p_sys->p_buffers[i].length ) )
                {
                    msg_Err( p_this, "munmap failed" );
                }
            }
            break;

        case IO_METHOD_USERPTR:
            for( i = 0; i < p_sys->i_nbuffers; ++i )
            {
               free( p_sys->p_buffers[i].orig_userp );
            }
            break;
        }
        free( p_sys->p_buffers );
    }

    CommonClose( p_this, p_sys );
}

static void CommonClose( vlc_object_t *p_this, demux_sys_t *p_sys )
{
    /* Close */
    if( p_sys->i_fd_video >= 0 ) close( p_sys->i_fd_video );
#ifdef HAVE_ALSA
    if( p_sys->b_use_alsa )
    {
        if( p_sys->p_alsa_pcm ) snd_pcm_close( p_sys->p_alsa_pcm );
    }
    else
#endif
    {
        if( p_sys->i_fd_audio >= 0 ) close( p_sys->i_fd_audio );
    }

    if( p_sys->p_block_audio ) block_Release( p_sys->p_block_audio );
    if( p_sys->psz_device ) free( p_sys->psz_device );
    if( p_sys->psz_vdev ) free( p_sys->psz_vdev );
    if( p_sys->psz_adev ) free( p_sys->psz_adev );
    if( p_sys->p_standards ) free( p_sys->p_standards );
    if( p_sys->p_inputs ) free( p_sys->p_inputs );
    if( p_sys->p_tuners ) free( p_sys->p_tuners );
    if( p_sys->p_codecs ) free( p_sys->p_codecs );
    if( p_sys->psz_requested_chroma ) free( p_sys->psz_requested_chroma );

    free( p_sys );
}

/*****************************************************************************
 * AccessOpen: opens v4l2 device, access2 callback
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

    p_access->pf_read = AccessRead;
    p_access->pf_block = NULL;
    p_access->pf_seek = NULL;
    p_access->pf_control = AccessControl;
    p_access->info.i_update = 0;
    p_access->info.i_size = 0;
    p_access->info.i_pos = 0;
    p_access->info.b_eof = VLC_FALSE;
    p_access->info.i_title = 0;
    p_access->info.i_seekpoint = 0;

    p_sys = calloc( 1, sizeof( demux_sys_t ) );
    p_access->p_sys = (access_sys_t *) p_sys;
    if( p_sys == NULL ) return VLC_ENOMEM;

    GetV4L2Params( p_sys, (vlc_object_t *) p_access );

    ParseMRL( p_sys, p_access->psz_path, (vlc_object_t *) p_access );

    if( FindMainDevice( p_this, p_sys, FIND_VIDEO,
        VLC_FALSE, !strcmp( p_access->psz_access, "v4l2" ) ) != VLC_SUCCESS )
    {
        AccessClose( p_this );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DemuxControl:
 *****************************************************************************/
static int DemuxControl( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    vlc_bool_t *pb;
    int64_t    *pi64;

    switch( i_query )
    {
        /* Special for access_demux */
        case DEMUX_CAN_PAUSE:
        case DEMUX_SET_PAUSE_STATE:
        case DEMUX_CAN_CONTROL_PACE:
            pb = (vlc_bool_t*)va_arg( args, vlc_bool_t * );
            *pb = VLC_FALSE;
            return VLC_SUCCESS;

        case DEMUX_GET_PTS_DELAY:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = (int64_t)p_sys->i_pts * 1000;
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = mdate();
            return VLC_SUCCESS;

        /* TODO implement others */
        default:
            return VLC_EGENERIC;
    }

    return VLC_EGENERIC;
}

/*****************************************************************************
 * AccessControl: access2 callback
 *****************************************************************************/
static int AccessControl( access_t *p_access, int i_query, va_list args )
{
    vlc_bool_t   *pb_bool;
    int          *pi_int;
    int64_t      *pi_64;
    demux_sys_t  *p_sys = (demux_sys_t *) p_access->p_sys;

    switch( i_query )
    {
        /* */
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = VLC_FALSE;
            break;
        case ACCESS_CAN_PAUSE:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = VLC_FALSE;
            break;
        case ACCESS_CAN_CONTROL_PACE:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = VLC_FALSE;
            break;

        /* */
        case ACCESS_GET_MTU:
            pi_int = (int*)va_arg( args, int * );
            *pi_int = 0;
            break;

        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = (int64_t) p_sys->i_pts * 1000;
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
 * AccessRead: access2 callback
 ******************************************************************************/
static ssize_t AccessRead( access_t * p_access, uint8_t * p_buffer, size_t i_len )
{
    demux_sys_t *p_sys = (demux_sys_t *) p_access->p_sys;
    struct pollfd ufd;
    int i_ret;

    ufd.fd = p_sys->i_fd_video;
    ufd.events = POLLIN;

    if( p_access->info.b_eof )
        return 0;

    do
    {
        if( p_access->b_die )
            return 0;

        ufd.revents = 0;
    }
    while( ( i_ret = poll( &ufd, 1, 500 ) ) == 0 );

    if( i_ret < 0 )
    {
        msg_Err( p_access, "Polling error (%m)." );
        return -1;
    }

    i_ret = read( p_sys->i_fd_video, p_buffer, i_len );
    if( i_ret == 0 )
    {
        p_access->info.b_eof = VLC_TRUE;
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
    es_out_id_t *p_es = p_sys->p_es_audio;
    block_t *p_block = NULL;

    /* Try grabbing audio frames first */
    if( p_sys->i_fd_audio < 0 || !( p_block = GrabAudio( p_demux ) ) )
    {
        /* Try grabbing video frame */
        p_es = p_sys->p_es_video;
        if( p_sys->i_fd_video > 0 ) p_block = GrabVideo( p_demux );
    }

    if( !p_block )
    {
        /* Sleep so we do not consume all the cpu, 10ms seems
         * like a good value (100fps) */
        msleep( 10 );
        return 1;
    }

    es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_block->i_pts );
    es_out_Send( p_demux->out, p_es, p_block );

    return 1;
}

/*****************************************************************************
 * GrabVideo: Grab a video frame
 *****************************************************************************/
static block_t* GrabVideo( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    block_t *p_block = NULL;
    struct v4l2_buffer buf;
    ssize_t i_ret;

    if( p_sys->f_fps >= 0.1 && p_sys->i_video_pts > 0 )
    {
        mtime_t i_dur = (mtime_t)((double)1000000 / (double)p_sys->f_fps);

        /* Did we wait long enough ? (frame rate reduction) */
        if( p_sys->i_video_pts + i_dur > mdate() ) return 0;
    }

    /* Grab Video Frame */
    switch( p_sys->io )
    {
    case IO_METHOD_READ:
        i_ret = read( p_sys->i_fd_video, p_sys->p_buffers[0].start, p_sys->p_buffers[0].length );
        if( i_ret == -1 )
        {
            switch( errno )
            {
            case EAGAIN:
                return 0;
            case EIO:
                /* Could ignore EIO, see spec. */
                /* fall through */
            default:
                msg_Err( p_demux, "Failed to read frame" );
                return 0;
               }
        }

        p_block = ProcessVideoFrame( p_demux, (uint8_t*)p_sys->p_buffers[0].start, i_ret );
        if( !p_block ) return 0;

        break;

    case IO_METHOD_MMAP:
        memset( &buf, 0, sizeof(buf) );
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        /* Wait for next frame */
        if (ioctl( p_sys->i_fd_video, VIDIOC_DQBUF, &buf ) < 0 )
        {
            switch( errno )
            {
            case EAGAIN:
                return 0;
            case EIO:
                /* Could ignore EIO, see spec. */
                /* fall through */
            default:
                msg_Err( p_demux, "Failed to wait (VIDIOC_DQBUF)" );
                return 0;
               }
        }

        if( buf.index >= p_sys->i_nbuffers ) {
            msg_Err( p_demux, "Failed capturing new frame as i>=nbuffers" );
            return 0;
        }

        p_block = ProcessVideoFrame( p_demux, p_sys->p_buffers[buf.index].start, buf.bytesused );
        if( !p_block ) return 0;

        /* Unlock */
        if( ioctl( p_sys->i_fd_video, VIDIOC_QBUF, &buf ) < 0 )
        {
            msg_Err (p_demux, "Failed to unlock (VIDIOC_QBUF)");
            return 0;
        }

        break;

    case IO_METHOD_USERPTR:
        memset( &buf, 0, sizeof(buf) );
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_USERPTR;

        /* Wait for next frame */
        if (ioctl( p_sys->i_fd_video, VIDIOC_DQBUF, &buf ) < 0 )
        {
            switch( errno )
            {
            case EAGAIN:
                return 0;
            case EIO:
                /* Could ignore EIO, see spec. */
                /* fall through */
            default:
                msg_Err( p_demux, "Failed to wait (VIDIOC_DQBUF)" );
                return 0;
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
            return 0;
        }

        p_block = ProcessVideoFrame( p_demux, (uint8_t*)buf.m.userptr, buf.bytesused );
        if( !p_block ) return 0;

        /* Unlock */
        if( ioctl( p_sys->i_fd_video, VIDIOC_QBUF, &buf ) < 0 )
        {
            msg_Err (p_demux, "Failed to unlock (VIDIOC_QBUF)");
            return 0;
        }

        break;

    }

    /* Timestamp */
    p_sys->i_video_pts = p_block->i_pts = p_block->i_dts = mdate();

    return p_block;
}

/*****************************************************************************
 * ProcessVideoFrame: Helper function to take a buffer and copy it into
 * a new block
 *****************************************************************************/
static block_t* ProcessVideoFrame( demux_t *p_demux, uint8_t *p_frame, size_t i_size )
{
    block_t *p_block;

    if( !p_frame ) return 0;

    /* New block */
    if( !( p_block = block_New( p_demux, i_size ) ) )
    {
        msg_Warn( p_demux, "Cannot get new block" );
        return 0;
    }

    /* Copy frame */
    memcpy( p_block->p_buffer, p_frame, i_size );

    return p_block;
}

/*****************************************************************************
 * GrabAudio: Grab an audio frame
 *****************************************************************************/
static block_t* GrabAudio( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    struct audio_buf_info buf_info;
    int i_read, i_correct;
    block_t *p_block;

    if( p_sys->p_block_audio ) p_block = p_sys->p_block_audio;
    else p_block = block_New( p_demux, p_sys->i_audio_max_frame_size );

    if( !p_block )
    {
        msg_Warn( p_demux, "cannot get block" );
        return 0;
    }

    p_sys->p_block_audio = p_block;

#ifdef HAVE_ALSA
    if( p_sys->b_use_alsa )
    {
        /* ALSA */
        i_read = snd_pcm_readi( p_sys->p_alsa_pcm, p_block->p_buffer, p_sys->i_alsa_chunk_size );
        if( i_read <= 0 )
        {
            int i_resume;
            switch( i_read )
            {
                case -EAGAIN:
                    break;
                case -EPIPE:
                    /* xrun */
                    snd_pcm_prepare( p_sys->p_alsa_pcm );
                    break;
                case -ESTRPIPE:
                    /* suspend */
                    i_resume = snd_pcm_resume( p_sys->p_alsa_pcm );
                    if( i_resume < 0 && i_resume != -EAGAIN ) snd_pcm_prepare( p_sys->p_alsa_pcm );
                    break;
                default:
                    msg_Err( p_demux, "Failed to read alsa frame (%s)", snd_strerror( i_read ) );
                    return 0;
            }
        }
        else
        {
            /* convert from frames to bytes */
            i_read *= p_sys->i_alsa_frame_size;
        }
    }
    else
#endif
    {
        /* OSS */
        i_read = read( p_sys->i_fd_audio, p_block->p_buffer,
                    p_sys->i_audio_max_frame_size );
    }

    if( i_read <= 0 ) return 0;

    p_block->i_buffer = i_read;
    p_sys->p_block_audio = 0;

    /* Correct the date because of kernel buffering */
    i_correct = i_read;
#ifdef HAVE_ALSA
    if( !p_sys->b_use_alsa )
#endif
    {
        /* OSS */
        if( ioctl( p_sys->i_fd_audio, SNDCTL_DSP_GETISPACE, &buf_info ) == 0 )
        {
            i_correct += buf_info.bytes;
        }
    }
#ifdef HAVE_ALSA
    else
    {
        /* ALSA */
        int i_err;
        snd_pcm_sframes_t delay = 0;
        if( ( i_err = snd_pcm_delay( p_sys->p_alsa_pcm, &delay ) ) >= 0 )
        {
            int i_correction_delta = delay * p_sys->i_alsa_frame_size;
            /* Test for overrun */
            if( i_correction_delta>p_sys->i_audio_max_frame_size )
            {
                msg_Warn( p_demux, "ALSA read overrun" );
                i_correction_delta = p_sys->i_audio_max_frame_size;
                snd_pcm_prepare( p_sys->p_alsa_pcm );
            }
            i_correct += i_correction_delta;
        }
        else
        {
            /* delay failed so reset */
            msg_Warn( p_demux, "ALSA snd_pcm_delay failed (%s)", snd_strerror( i_err ) );
            snd_pcm_prepare( p_sys->p_alsa_pcm );
        }
    }
#endif

    /* Timestamp */
    p_block->i_pts = p_block->i_dts =
        mdate() - I64C(1000000) * (mtime_t)i_correct /
        2 / ( p_sys->b_stereo ? 2 : 1) / p_sys->i_sample_rate;

    return p_block;
}

/*****************************************************************************
 * Helper function to initalise video IO using the Read method
 *****************************************************************************/
static int InitRead( demux_t *p_demux, int i_fd, unsigned int i_buffer_size )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    p_sys->p_buffers = calloc( 1, sizeof( *p_sys->p_buffers ) );
    if( !p_sys->p_buffers )
    {
        msg_Err( p_demux, "Out of memory" );
        goto open_failed;
    }

    p_sys->p_buffers[0].length = i_buffer_size;
    p_sys->p_buffers[0].start = malloc( i_buffer_size );
    if( !p_sys->p_buffers[0].start )
    {
        msg_Err( p_demux, "Out of memory" );
        goto open_failed;
    }

    return VLC_SUCCESS;

open_failed:
    return VLC_EGENERIC;

}

/*****************************************************************************
 * Helper function to initalise video IO using the mmap method
 *****************************************************************************/
static int InitMmap( demux_t *p_demux, int i_fd )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    struct v4l2_requestbuffers req;

    memset( &req, 0, sizeof(req) );
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if( ioctl( i_fd, VIDIOC_REQBUFS, &req ) < 0 )
    {
        msg_Err( p_demux, "device does not support mmap i/o" );
        goto open_failed;
    }

    if( req.count < 2 )
    {
        msg_Err( p_demux, "Insufficient buffer memory" );
        goto open_failed;
    }

    p_sys->p_buffers = calloc( req.count, sizeof( *p_sys->p_buffers ) );
    if( !p_sys->p_buffers )
    {
        msg_Err( p_demux, "Out of memory" );
        goto open_failed;
    }

    for( p_sys->i_nbuffers = 0; p_sys->i_nbuffers < req.count; ++p_sys->i_nbuffers )
    {
        struct v4l2_buffer buf;

        memset( &buf, 0, sizeof(buf) );
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = p_sys->i_nbuffers;

        if( ioctl( i_fd, VIDIOC_QUERYBUF, &buf ) < 0 )
        {
            msg_Err( p_demux, "VIDIOC_QUERYBUF" );
            goto open_failed;
        }

        p_sys->p_buffers[p_sys->i_nbuffers].length = buf.length;
        p_sys->p_buffers[p_sys->i_nbuffers].start =
            mmap( NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, i_fd, buf.m.offset );

        if( p_sys->p_buffers[p_sys->i_nbuffers].start == MAP_FAILED )
        {
            msg_Err( p_demux, "mmap failed (%m)" );
            goto open_failed;
        }
    }

    return VLC_SUCCESS;

open_failed:
    return VLC_EGENERIC;

}

/*****************************************************************************
 * Helper function to initalise video IO using the userbuf method
 *****************************************************************************/
static int InitUserP( demux_t *p_demux, int i_fd, unsigned int i_buffer_size )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    struct v4l2_requestbuffers req;
    unsigned int i_page_size;

    i_page_size = getpagesize();
    i_buffer_size = ( i_buffer_size + i_page_size - 1 ) & ~( i_page_size - 1);

    memset( &req, 0, sizeof(req) );
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_USERPTR;

    if( ioctl( i_fd, VIDIOC_REQBUFS, &req ) < 0 )
    {
        msg_Err( p_demux, "device does not support user pointer i/o" );
        goto open_failed;
    }

    p_sys->p_buffers = calloc( 4, sizeof( *p_sys->p_buffers ) );
    if( !p_sys->p_buffers )
    {
        msg_Err( p_demux, "Out of memory" );
        goto open_failed;
    }

    for( p_sys->i_nbuffers = 0; p_sys->i_nbuffers < 4; ++p_sys->i_nbuffers )
    {
        p_sys->p_buffers[p_sys->i_nbuffers].length = i_buffer_size;
        p_sys->p_buffers[p_sys->i_nbuffers].start =
            vlc_memalign( &p_sys->p_buffers[p_sys->i_nbuffers].orig_userp,
                /* boundary */ i_page_size, i_buffer_size );

        if( !p_sys->p_buffers[p_sys->i_nbuffers].start )
        {
            msg_Err( p_demux, "out of memory" );
            goto open_failed;
        }
    }

    return VLC_SUCCESS;

open_failed:
    return VLC_EGENERIC;

}

/*****************************************************************************
 * IsPixelFormatSupported: returns true if the specified V4L2 pixel format is
 * in the array of supported formats returned by the driver
 *****************************************************************************/
static vlc_bool_t IsPixelFormatSupported( demux_t *p_demux, unsigned int i_pixelformat )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    for( int i_index = 0; i_index < p_sys->i_codec; i_index++ )
    {
        if( p_sys->p_codecs[i_index].pixelformat == i_pixelformat ) return VLC_TRUE;
    }

    return VLC_FALSE;
}

/*****************************************************************************
 * OpenVideoDev: open and set up the video device and probe for capabilities
 *****************************************************************************/
static int OpenVideoDev( vlc_object_t *p_obj, demux_sys_t *p_sys, vlc_bool_t b_demux )
{
    int i_fd;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format fmt;
    unsigned int i_min;
    enum v4l2_buf_type buf_type;
    char *psz_device = p_sys->psz_vdev;

    if( ( i_fd = open( psz_device, O_RDWR ) ) < 0 )
    {
        msg_Err( p_obj, "cannot open device (%m)" );
        goto open_failed;
    }

    /* Select standard */

    if( p_sys->i_selected_standard_id != V4L2_STD_UNKNOWN )
    {
        if( ioctl( i_fd, VIDIOC_S_STD, &p_sys->i_selected_standard_id ) < 0 )
        {
            msg_Err( p_obj, "cannot set standard (%m)" );
            goto open_failed;
        }
        msg_Dbg( p_obj, "Set standard" );
    }

    /* Select input */

    if( p_sys->i_selected_input > p_sys->i_input )
    {
        msg_Warn( p_obj, "invalid input. Using the default one" );
        p_sys->i_selected_input = 0;
    }

    if( ioctl( i_fd, VIDIOC_S_INPUT, &p_sys->i_selected_input ) < 0 )
    {
        msg_Err( p_obj, "cannot set input (%m)" );
        goto open_failed;
    }

    /* TODO: Move the resolution stuff up here */
    /* if MPEG encoder card, no need to do anything else after this */
    ControlList( p_obj, i_fd,
                  var_GetBool( p_obj, "v4l2-controls-reset" ), b_demux );
    if( VLC_FALSE == b_demux)
    {
        return i_fd;
    }

    demux_t *p_demux = (demux_t *) p_obj;

    /* Verify device support for the various IO methods */
    switch( p_sys->io )
    {
        case IO_METHOD_READ:
            if( !(p_sys->dev_cap.capabilities & V4L2_CAP_READWRITE) )
            {
                msg_Err( p_demux, "device does not support read i/o" );
                goto open_failed;
            }
            break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
            if( !(p_sys->dev_cap.capabilities & V4L2_CAP_STREAMING) )
            {
                msg_Err( p_demux, "device does not support streaming i/o" );
                goto open_failed;
            }
            break;

        default:
            msg_Err( p_demux, "io method not supported" );
            goto open_failed;
    }

    /* Reset Cropping */
    memset( &cropcap, 0, sizeof(cropcap) );
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if( ioctl( i_fd, VIDIOC_CROPCAP, &cropcap ) >= 0 )
    {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect; /* reset to default */
        if( ioctl( i_fd, VIDIOC_S_CROP, &crop ) < 0 )
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

    /* Try and find default resolution if not specified */
    memset( &fmt, 0, sizeof(fmt) );
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if( p_sys->i_width <= 0 || p_sys->i_height <= 0 )
    {
        if( ioctl( i_fd, VIDIOC_G_FMT, &fmt ) < 0 )
        {
            msg_Err( p_demux, "Cannot get default width and height." );
            goto open_failed;
        }

        p_sys->i_width = fmt.fmt.pix.width;
        p_sys->i_height = fmt.fmt.pix.height;

        if( fmt.fmt.pix.field == V4L2_FIELD_ALTERNATE )
        {
            p_sys->i_height = p_sys->i_height * 2;
        }
    }
    else
    {
        msg_Dbg( p_demux, "trying specified size %dx%d", p_sys->i_width, p_sys->i_height );
    }

    fmt.fmt.pix.width = p_sys->i_width;
    fmt.fmt.pix.height = p_sys->i_height;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    /* Test and set Chroma */
    fmt.fmt.pix.pixelformat = 0;
    if( p_sys->psz_requested_chroma && strlen( p_sys->psz_requested_chroma ) > 0 )
    {
        /* User specified chroma */
        if( strlen( p_sys->psz_requested_chroma ) >= 4 )
        {
            int i_requested_fourcc = VLC_FOURCC(
                p_sys->psz_requested_chroma[0], p_sys->psz_requested_chroma[1],
                p_sys->psz_requested_chroma[2], p_sys->psz_requested_chroma[3] );
            for( int i = 0; v4l2chroma_to_fourcc[i].i_v4l2 != 0; i++ )
            {
                if( v4l2chroma_to_fourcc[i].i_fourcc == i_requested_fourcc )
                {
                    fmt.fmt.pix.pixelformat = v4l2chroma_to_fourcc[i].i_v4l2;
                    break;
                }
            }
        }
        /* Try and set user chroma */
        if( !IsPixelFormatSupported( p_demux, fmt.fmt.pix.pixelformat ) || ( fmt.fmt.pix.pixelformat && ioctl( i_fd, VIDIOC_S_FMT, &fmt ) < 0 ) )
        {
            msg_Warn( p_demux, "Driver is unable to use specified chroma %s. Trying defaults.", p_sys->psz_requested_chroma );
            fmt.fmt.pix.pixelformat = 0;
        }
    }

    /* If no user specified chroma, find best */
    /* This also decides if MPEG encoder card or not */
    if( !fmt.fmt.pix.pixelformat )
    {
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YVU420;
        if( !IsPixelFormatSupported( p_demux, fmt.fmt.pix.pixelformat ) || ioctl( i_fd, VIDIOC_S_FMT, &fmt ) < 0 )
        {
            fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV422P;
            if( !IsPixelFormatSupported( p_demux, fmt.fmt.pix.pixelformat ) || ioctl( i_fd, VIDIOC_S_FMT, &fmt ) < 0 )
            {
                fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
                if( !IsPixelFormatSupported( p_demux, fmt.fmt.pix.pixelformat ) || ioctl( i_fd, VIDIOC_S_FMT, &fmt ) < 0 )
                {
                    msg_Warn( p_demux, "Could not select any of the default chromas; attempting to open as MPEG encoder card (access2)" );
                    goto open_failed;
                }
            }
        }
    }

    /* Reassign width, height and chroma incase driver override */
    p_sys->i_width = fmt.fmt.pix.width;
    p_sys->i_height = fmt.fmt.pix.height;

    /* Look up final fourcc */
    p_sys->i_fourcc = 0;
    for( int i = 0; v4l2chroma_to_fourcc[i].i_fourcc != 0; i++ )
    {
        if( v4l2chroma_to_fourcc[i].i_v4l2 == fmt.fmt.pix.pixelformat )
        {
            p_sys->i_fourcc = v4l2chroma_to_fourcc[i].i_fourcc;
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
    frmival.index = 0;
    frmival.pixel_format = fmt.fmt.pix.pixelformat;
    frmival.width = p_sys->i_width;
    frmival.height = p_sys->i_height;
    if( ioctl( i_fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival ) >= 0 )
    {
        char sz_fourcc[5];
        memset( &sz_fourcc, 0, sizeof( sz_fourcc ) );
        vlc_fourcc_to_char( p_sys->i_fourcc, &sz_fourcc );
        msg_Dbg( p_demux, "supported frame intervals for %4s, %dx%d:",
                 sz_fourcc, frmival.width, frmival.height );
        switch( frmival.type )
        {
            case V4L2_FRMIVAL_TYPE_DISCRETE:
                do
                {
                    msg_Dbg( p_demux, "    supported frame interval: %d/%d",
                             frmival.discrete.numerator,
                             frmival.discrete.denominator );
                    frmival.index++;
                } while( ioctl( i_fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival ) >= 0 );
                break;
            case V4L2_FRMIVAL_TYPE_STEPWISE:
                msg_Dbg( p_demux, "    supported frame intervals: %d/%d to "
                         "%d/%d using %d/%d increments",
                         frmival.stepwise.min.numerator,
                         frmival.stepwise.min.denominator,
                         frmival.stepwise.max.numerator,
                         frmival.stepwise.max.denominator,
                         frmival.stepwise.step.numerator,
                         frmival.stepwise.step.denominator );
                break;
            case V4L2_FRMIVAL_TYPE_CONTINUOUS:
                msg_Dbg( p_demux, "    supported frame intervals: %d/%d to %d/%d",
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
        if( InitRead( p_demux, i_fd, fmt.fmt.pix.sizeimage ) != VLC_SUCCESS ) goto open_failed;
        break;

    case IO_METHOD_MMAP:
        if( InitMmap( p_demux, i_fd ) != VLC_SUCCESS ) goto open_failed;
        break;

    case IO_METHOD_USERPTR:
        if( InitUserP( p_demux, i_fd, fmt.fmt.pix.sizeimage ) != VLC_SUCCESS ) goto open_failed;
        break;

    }

    /* Add */
    es_format_t es_fmt;
    es_format_Init( &es_fmt, VIDEO_ES, p_sys->i_fourcc );
    es_fmt.video.i_width  = p_sys->i_width;
    es_fmt.video.i_height = p_sys->i_height;
    es_fmt.video.i_aspect = 4 * VOUT_ASPECT_FACTOR / 3;

    /* Setup rgb mask for RGB formats */
    if( p_sys->i_fourcc == VLC_FOURCC( 'R','V','2','4' ) )
    {
        /* This is in BGR format */
        es_fmt.video.i_bmask = 0x00ff0000;
        es_fmt.video.i_gmask = 0x0000ff00;
        es_fmt.video.i_rmask = 0x000000ff;
    }

    msg_Dbg( p_demux, "added new video es %4.4s %dx%d",
        (char*)&es_fmt.i_codec, es_fmt.video.i_width, es_fmt.video.i_height );
    p_sys->p_es_video = es_out_Add( p_demux->out, &es_fmt );

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

            if( ioctl( i_fd, VIDIOC_QBUF, &buf ) < 0 )
            {
                msg_Err( p_demux, "VIDIOC_QBUF failed" );
                goto open_failed;
            }
        }

        buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if( ioctl( i_fd, VIDIOC_STREAMON, &buf_type ) < 0 )
        {
            msg_Err( p_demux, "VIDIOC_STREAMON failed" );
            goto open_failed;
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

            if( ioctl( i_fd, VIDIOC_QBUF, &buf ) < 0 )
            {
                msg_Err( p_demux, "VIDIOC_QBUF failed" );
                goto open_failed;
            }
        }

        buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if( ioctl( i_fd, VIDIOC_STREAMON, &buf_type ) < 0 )
        {
            msg_Err( p_demux, "VIDIOC_STREAMON failed" );
            goto open_failed;
        }

        break;
    }

    /* report fps */
    if( p_sys->f_fps >= 0.1 )
    {
        msg_Dbg( p_demux, "User set fps=%f", p_sys->f_fps );
    }

    return i_fd;

open_failed:
    if( i_fd >= 0 ) close( i_fd );
    return -1;

}

#ifdef HAVE_ALSA
/*****************************************************************************
 * ResolveALSADeviceName: Change any . to : in the ALSA device name
 *****************************************************************************/
static char *ResolveALSADeviceName( char *psz_device )
{
    char* psz_alsa_name = strdup( psz_device );
    for( unsigned int i = 0; i < strlen( psz_device ); i++ )
    {
        if( psz_alsa_name[i] == '.' ) psz_alsa_name[i] = ':';
    }
    return psz_alsa_name;
}
#endif

/*****************************************************************************
 * OpenAudioDev: open and set up the audio device and probe for capabilities
 *****************************************************************************/
static int OpenAudioDev( vlc_object_t *p_this, demux_sys_t *p_sys,
                         vlc_bool_t b_demux )
{
    char *psz_device = p_sys->psz_adev;
    int i_fd = 0;
    int i_format;
#ifdef HAVE_ALSA
    p_sys->p_alsa_pcm = NULL;
    char* psz_alsa_device_name = ResolveALSADeviceName( psz_device );
    snd_pcm_hw_params_t *p_hw_params = NULL;
    snd_pcm_uframes_t buffer_size;
    snd_pcm_uframes_t chunk_size;
#endif

#ifdef HAVE_ALSA
    if( p_sys->b_use_alsa )
    {
        /* ALSA */

        int i_err;

        if( ( i_err = snd_pcm_open( &p_sys->p_alsa_pcm, psz_alsa_device_name,
            SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK ) ) < 0)
        {
            msg_Err( p_this, "Cannot open ALSA audio device %s (%s)",
                     psz_alsa_device_name, snd_strerror( i_err ) );
            goto adev_fail;
        }

        if( ( i_err = snd_pcm_nonblock( p_sys->p_alsa_pcm, 1 ) ) < 0)
        {
            msg_Err( p_this, "Cannot set ALSA nonblock (%s)",
                     snd_strerror( i_err ) );
            goto adev_fail;
        }

        /* Begin setting hardware parameters */

        if( ( i_err = snd_pcm_hw_params_malloc( &p_hw_params ) ) < 0 )
        {
            msg_Err( p_this,
                     "ALSA: cannot allocate hardware parameter structure (%s)",
                     snd_strerror( i_err ) );
            goto adev_fail;
        }

        if( ( i_err = snd_pcm_hw_params_any( p_sys->p_alsa_pcm, p_hw_params ) ) < 0 )
        {
            msg_Err( p_this,
                    "ALSA: cannot initialize hardware parameter structure (%s)",
                     snd_strerror( i_err ) );
            goto adev_fail;
        }

        /* Set Interleaved access */
        if( ( i_err = snd_pcm_hw_params_set_access( p_sys->p_alsa_pcm, p_hw_params, SND_PCM_ACCESS_RW_INTERLEAVED ) ) < 0 )
        {
            msg_Err( p_this, "ALSA: cannot set access type (%s)",
                     snd_strerror( i_err ) );
            goto adev_fail;
        }

        /* Set 16 bit little endian */
        if( ( i_err = snd_pcm_hw_params_set_format( p_sys->p_alsa_pcm, p_hw_params, SND_PCM_FORMAT_S16_LE ) ) < 0 )
        {
            msg_Err( p_this, "ALSA: cannot set sample format (%s)",
                     snd_strerror( i_err ) );
            goto adev_fail;
        }

        /* Set sample rate */
#ifdef HAVE_ALSA_NEW_API
        i_err = snd_pcm_hw_params_set_rate_near( p_sys->p_alsa_pcm, p_hw_params, &p_sys->i_sample_rate, NULL );
#else
        i_err = snd_pcm_hw_params_set_rate_near( p_sys->p_alsa_pcm, p_hw_params, p_sys->i_sample_rate, NULL );
#endif
        if( i_err < 0 )
        {
            msg_Err( p_this, "ALSA: cannot set sample rate (%s)",
                     snd_strerror( i_err ) );
            goto adev_fail;
        }

        /* Set channels */
        unsigned int channels = p_sys->b_stereo ? 2 : 1;
        if( ( i_err = snd_pcm_hw_params_set_channels( p_sys->p_alsa_pcm, p_hw_params, channels ) ) < 0 )
        {
            channels = ( channels==1 ) ? 2 : 1;
            msg_Warn( p_this, "ALSA: cannot set channel count (%s). "
                      "Trying with channels=%d",
                      snd_strerror( i_err ),
                      channels );
            if( ( i_err = snd_pcm_hw_params_set_channels( p_sys->p_alsa_pcm, p_hw_params, channels ) ) < 0 )
            {
                msg_Err( p_this, "ALSA: cannot set channel count (%s)",
                         snd_strerror( i_err ) );
                goto adev_fail;
            }
            p_sys->b_stereo = ( channels == 2 );
        }

        /* Set metrics for buffer calculations later */
        unsigned int buffer_time;
        if( ( i_err = snd_pcm_hw_params_get_buffer_time_max(p_hw_params, &buffer_time, 0) ) < 0 )
        {
            msg_Err( p_this, "ALSA: cannot get buffer time max (%s)",
                     snd_strerror( i_err ) );
            goto adev_fail;
        }
        if (buffer_time > 500000) buffer_time = 500000;

        /* Set period time */
        unsigned int period_time = buffer_time / 4;
#ifdef HAVE_ALSA_NEW_API
        i_err = snd_pcm_hw_params_set_period_time_near( p_sys->p_alsa_pcm, p_hw_params, &period_time, 0 );
#else
        i_err = snd_pcm_hw_params_set_period_time_near( p_sys->p_alsa_pcm, p_hw_params, period_time, 0 );
#endif
        if( i_err < 0 )
        {
            msg_Err( p_this, "ALSA: cannot set period time (%s)",
                     snd_strerror( i_err ) );
            goto adev_fail;
        }

        /* Set buffer time */
#ifdef HAVE_ALSA_NEW_API
        i_err = snd_pcm_hw_params_set_buffer_time_near( p_sys->p_alsa_pcm, p_hw_params, &buffer_time, 0 );
#else
        i_err = snd_pcm_hw_params_set_buffer_time_near( p_sys->p_alsa_pcm, p_hw_params, buffer_time, 0 );
#endif
        if( i_err < 0 )
        {
            msg_Err( p_this, "ALSA: cannot set buffer time (%s)",
                     snd_strerror( i_err ) );
            goto adev_fail;
        }

        /* Apply new hardware parameters */
        if( ( i_err = snd_pcm_hw_params( p_sys->p_alsa_pcm, p_hw_params ) ) < 0 )
        {
            msg_Err( p_this, "ALSA: cannot set hw parameters (%s)",
                     snd_strerror( i_err ) );
            goto adev_fail;
        }

        /* Get various buffer metrics */
        snd_pcm_hw_params_get_period_size( p_hw_params, &chunk_size, 0 );
        snd_pcm_hw_params_get_buffer_size( p_hw_params, &buffer_size );
        if (chunk_size == buffer_size)
        {
            msg_Err( p_this,
                     "ALSA: period cannot equal buffer size (%lu == %lu)",
                     chunk_size, buffer_size);
            goto adev_fail;
        }

        int bits_per_sample = snd_pcm_format_physical_width(SND_PCM_FORMAT_S16_LE);
        int bits_per_frame = bits_per_sample * channels;

        p_sys->i_alsa_chunk_size = chunk_size;
        p_sys->i_alsa_frame_size = (bits_per_sample / 8) * channels;
        p_sys->i_audio_max_frame_size = chunk_size * bits_per_frame / 8; 

        snd_pcm_hw_params_free( p_hw_params );
        p_hw_params = NULL;

        /* Prep device */
        if( ( i_err = snd_pcm_prepare( p_sys->p_alsa_pcm ) ) < 0 )
        {
            msg_Err( p_this,
                     "ALSA: cannot prepare audio interface for use (%s)",
                     snd_strerror( i_err ) );
            goto adev_fail;
        }

        /* Return a fake handle so other tests work */
        i_fd = 1;

    }
    else
#endif /* HAVE_ALSA */
    {
        /* OSS */

        if( (i_fd = open( psz_device, O_RDONLY | O_NONBLOCK )) < 0 )
        {
            msg_Err( p_this, "cannot open OSS audio device (%m)" );
            goto adev_fail;
        }

        i_format = AFMT_S16_LE;
        if( ioctl( i_fd, SNDCTL_DSP_SETFMT, &i_format ) < 0
            || i_format != AFMT_S16_LE )
        {
            msg_Err( p_this,
                     "cannot set audio format (16b little endian) (%m)" );
            goto adev_fail;
        }

        if( ioctl( i_fd, SNDCTL_DSP_STEREO,
                   &p_sys->b_stereo ) < 0 )
        {
            msg_Err( p_this, "cannot set audio channels count (%m)" );
            goto adev_fail;
        }

        if( ioctl( i_fd, SNDCTL_DSP_SPEED,
                   &p_sys->i_sample_rate ) < 0 )
        {
            msg_Err( p_this, "cannot set audio sample rate (%m)" );
            goto adev_fail;
        }

        p_sys->i_audio_max_frame_size = 6 * 1024;
    }

    msg_Dbg( p_this, "opened adev=`%s' %s %dHz",
             psz_device, p_sys->b_stereo ? "stereo" : "mono",
             p_sys->i_sample_rate );

    es_format_t fmt;
    es_format_Init( &fmt, AUDIO_ES, VLC_FOURCC('a','r','a','w') );

    fmt.audio.i_channels = p_sys->b_stereo ? 2 : 1;
    fmt.audio.i_rate = p_sys->i_sample_rate;
    fmt.audio.i_bitspersample = 16;
    fmt.audio.i_blockalign = fmt.audio.i_channels * fmt.audio.i_bitspersample / 8;
    fmt.i_bitrate = fmt.audio.i_channels * fmt.audio.i_rate * fmt.audio.i_bitspersample;

    msg_Dbg( p_this, "new audio es %d channels %dHz",
             fmt.audio.i_channels, fmt.audio.i_rate );

    if( b_demux )
    {
        demux_t *p_demux = (demux_t *)p_this;
        p_sys->p_es_audio = es_out_Add( p_demux->out, &fmt );
    }

#ifdef HAVE_ALSA
    free( psz_alsa_device_name );
#endif

    return i_fd;

 adev_fail:

    if( i_fd >= 0 ) close( i_fd );

#ifdef HAVE_ALSA
    if( p_hw_params ) snd_pcm_hw_params_free( p_hw_params );
    if( p_sys->p_alsa_pcm ) snd_pcm_close( p_sys->p_alsa_pcm );
    free( psz_alsa_device_name );
#endif

    return -1;

}

/*****************************************************************************
 * ProbeVideoDev: probe video for capabilities
 *****************************************************************************/
static vlc_bool_t ProbeVideoDev( vlc_object_t *p_obj, demux_sys_t *p_sys,
                                 char *psz_device )
{
    int i_index;
    int i_standard;

    int i_fd;

    if( ( i_fd = open( psz_device, O_RDWR ) ) < 0 )
    {
        msg_Err( p_obj, "cannot open video device (%m)" );
        goto open_failed;
    }

    /* Get device capabilites */

    if( ioctl( i_fd, VIDIOC_QUERYCAP, &p_sys->dev_cap ) < 0 )
    {
        msg_Err( p_obj, "cannot get video capabilities (%m)" );
        goto open_failed;
    }

    msg_Dbg( p_obj, "V4L2 device: %s using driver: %s (version: %u.%u.%u) on %s",
                            p_sys->dev_cap.card,
                            p_sys->dev_cap.driver,
                            (p_sys->dev_cap.version >> 16) & 0xFF,
                            (p_sys->dev_cap.version >> 8) & 0xFF,
                            p_sys->dev_cap.version & 0xFF,
                            p_sys->dev_cap.bus_info );

    msg_Dbg( p_obj, "the device has the capabilities: (%c) Video Capure, "
                                                       "(%c) Audio, "
                                                       "(%c) Tuner",
             ( p_sys->dev_cap.capabilities & V4L2_CAP_VIDEO_CAPTURE  ? 'X':' '),
             ( p_sys->dev_cap.capabilities & V4L2_CAP_AUDIO  ? 'X':' '),
             ( p_sys->dev_cap.capabilities & V4L2_CAP_TUNER  ? 'X':' ') );

    msg_Dbg( p_obj, "supported I/O methods are: (%c) Read/Write, "
                                                 "(%c) Streaming, "
                                                 "(%c) Asynchronous",
            ( p_sys->dev_cap.capabilities & V4L2_CAP_READWRITE ? 'X':' ' ),
            ( p_sys->dev_cap.capabilities & V4L2_CAP_STREAMING ? 'X':' ' ),
            ( p_sys->dev_cap.capabilities & V4L2_CAP_ASYNCIO ? 'X':' ' ) );

    /* Now, enumerate all the video inputs. This is useless at the moment
       since we have no way to present that info to the user except with
       debug messages */

    if( p_sys->dev_cap.capabilities & V4L2_CAP_VIDEO_CAPTURE )
    {
        struct v4l2_input t_input;
        t_input.index = 0;
        while( ioctl( i_fd, VIDIOC_ENUMINPUT, &t_input ) >= 0 )
        {
            p_sys->i_input++;
            t_input.index = p_sys->i_input;
        }

        p_sys->p_inputs = calloc( 1, p_sys->i_input * sizeof( struct v4l2_input ) );
        if( !p_sys->p_inputs ) goto open_failed;

        for( i_index = 0; i_index < p_sys->i_input; i_index++ )
        {
            p_sys->p_inputs[i_index].index = i_index;

            if( ioctl( i_fd, VIDIOC_ENUMINPUT, &p_sys->p_inputs[i_index] ) )
            {
                msg_Err( p_obj, "cannot get video input characteristics (%m)" );
                goto open_failed;
            }
            msg_Dbg( p_obj, "video input %i (%s) has type: %s",
                                i_index,
                                p_sys->p_inputs[i_index].name,
                                p_sys->p_inputs[i_index].type
                                        == V4L2_INPUT_TYPE_TUNER ?
                                        "Tuner adapter" :
                                        "External analog input" );
        }
    }

    /* Probe video standards */
    if( p_sys->dev_cap.capabilities & V4L2_CAP_VIDEO_CAPTURE )
    {
        struct v4l2_standard t_standards;
        t_standards.index = 0;
        while( ioctl( i_fd, VIDIOC_ENUMSTD, &t_standards ) >=0 )
        {
            p_sys->i_standard++;
            t_standards.index = p_sys->i_standard;
        }

        p_sys->p_standards = calloc( 1, p_sys->i_standard * sizeof( struct v4l2_standard ) );
        if( !p_sys->p_standards ) goto open_failed;

        for( i_standard = 0; i_standard < p_sys->i_standard; i_standard++ )
        {
            p_sys->p_standards[i_standard].index = i_standard;

            if( ioctl( i_fd, VIDIOC_ENUMSTD, &p_sys->p_standards[i_standard] ) )
            {
                msg_Err( p_obj, "cannot get video input standards (%m)" );
                goto open_failed;
            }
            msg_Dbg( p_obj, "video standard %i is: %s",
                                i_standard,
                                p_sys->p_standards[i_standard].name);
        }
    }

    /* initialize the structures for the ioctls */
    for( i_index = 0; i_index < 32; i_index++ )
    {
        p_sys->p_audios[i_index].index = i_index;
    }

    /* Probe audio inputs */
    if( p_sys->dev_cap.capabilities & V4L2_CAP_AUDIO )
    {
        while( p_sys->i_audio < 32 &&
               ioctl( i_fd, VIDIOC_S_AUDIO, &p_sys->p_audios[p_sys->i_audio] ) >= 0 )
        {
            if( ioctl( i_fd, VIDIOC_G_AUDIO, &p_sys->p_audios[ p_sys->i_audio] ) < 0 )
            {
                msg_Err( p_obj, "cannot get audio input characteristics (%m)" );
                goto open_failed;
            }

            msg_Dbg( p_obj, "audio device %i (%s) is %s",
                                p_sys->i_audio,
                                p_sys->p_audios[p_sys->i_audio].name,
                                p_sys->p_audios[p_sys->i_audio].capability &
                                                    V4L2_AUDCAP_STEREO ?
                                        "Stereo" : "Mono" );

            p_sys->i_audio++;
        }
    }

    /* List tuner caps */
    if( p_sys->dev_cap.capabilities & V4L2_CAP_TUNER )
    {
        struct v4l2_tuner tuner;
        memset( &tuner, 0, sizeof(tuner) );
        while( ioctl( i_fd, VIDIOC_G_TUNER, &tuner ) >= 0 )
        {
            p_sys->i_tuner++;
            memset( &tuner, 0, sizeof(tuner) );
            tuner.index = p_sys->i_tuner;
        }

        p_sys->p_tuners = calloc( 1, p_sys->i_tuner * sizeof( struct v4l2_tuner ) );
        if( !p_sys->p_tuners ) goto open_failed;

        for( i_index = 0; i_index < p_sys->i_tuner; i_index++ )
        {
            p_sys->p_tuners[i_index].index = i_index;

            if( ioctl( i_fd, VIDIOC_G_TUNER, &p_sys->p_tuners[i_index] ) )
            {
                msg_Err( p_obj, "cannot get tuner characteristics (%m)" );
                goto open_failed;
            }
            msg_Dbg( p_obj, "tuner %i (%s) has type: %s, "
                              "frequency range: %.1f %s -> %.1f %s",
                                i_index,
                                p_sys->p_tuners[i_index].name,
                                p_sys->p_tuners[i_index].type
                                        == V4L2_TUNER_RADIO ?
                                        "Radio" : "Analog TV",
                                p_sys->p_tuners[i_index].rangelow * 62.5,
                                p_sys->p_tuners[i_index].capability &
                                        V4L2_TUNER_CAP_LOW ?
                                        "Hz" : "kHz",
                                p_sys->p_tuners[i_index].rangehigh * 62.5,
                                p_sys->p_tuners[i_index].capability &
                                        V4L2_TUNER_CAP_LOW ?
                                        "Hz" : "kHz" );
        }
    }

    /* Probe for available chromas */
    if( p_sys->dev_cap.capabilities & V4L2_CAP_VIDEO_CAPTURE )
    {
        struct v4l2_fmtdesc codec;

        i_index = 0;
        memset( &codec, 0, sizeof(codec) );
        codec.index = i_index;
        codec.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        while( ioctl( i_fd, VIDIOC_ENUM_FMT, &codec ) >= 0 )
        {
            i_index++;
            codec.index = i_index;
        }

        p_sys->i_codec = i_index;

        p_sys->p_codecs = calloc( 1, p_sys->i_codec * sizeof( struct v4l2_fmtdesc ) );

        for( i_index = 0; i_index < p_sys->i_codec; i_index++ )
        {
            p_sys->p_codecs[i_index].index = i_index;
            p_sys->p_codecs[i_index].type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

            if( ioctl( i_fd, VIDIOC_ENUM_FMT, &p_sys->p_codecs[i_index] ) < 0 )
            {
                msg_Err( p_obj, "cannot get codec description (%m)" );
                goto open_failed;
            }

            /* only print if vlc supports the format */
            vlc_bool_t b_codec_supported = VLC_FALSE;
            for( int i = 0; v4l2chroma_to_fourcc[i].i_v4l2 != 0; i++ )
            {
                if( v4l2chroma_to_fourcc[i].i_v4l2 == p_sys->p_codecs[i_index].pixelformat )
                {
                    b_codec_supported = VLC_TRUE;

                    char sz_fourcc[5];
                    memset( &sz_fourcc, 0, sizeof( sz_fourcc ) );
                    vlc_fourcc_to_char( v4l2chroma_to_fourcc[i].i_fourcc, &sz_fourcc );
                    msg_Dbg( p_obj, "device supports chroma %4s [%s]",
                                sz_fourcc,
                                p_sys->p_codecs[i_index].description );

#ifdef VIDIOC_ENUM_FRAMESIZES
                    /* This is new in Linux 2.6.19 */
                    /* List valid frame sizes for this format */
                    struct v4l2_frmsizeenum frmsize;
                    frmsize.index = 0;
                    frmsize.pixel_format = p_sys->p_codecs[i_index].pixelformat;
                    if( ioctl( i_fd, VIDIOC_ENUM_FRAMESIZES, &frmsize ) < 0 )
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
                                } while( ioctl( i_fd, VIDIOC_ENUM_FRAMESIZES, &frmsize ) >= 0 );
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
                msg_Dbg( p_obj, "device codec %s not supported as access_demux",
                    p_sys->p_codecs[i_index].description );
            }

        }
    }


    if( i_fd >= 0 ) close( i_fd );
    return VLC_TRUE;

open_failed:

    if( i_fd >= 0 ) close( i_fd );
    return VLC_FALSE;

}

/*****************************************************************************
 * ProbeAudioDev: probe audio for capabilities
 *****************************************************************************/
static vlc_bool_t ProbeAudioDev( vlc_object_t *p_this, demux_sys_t *p_sys,
                                 char *psz_device )
{
    int i_fd = 0;
    int i_caps;

#ifdef HAVE_ALSA
    if( p_sys->b_use_alsa )
    {
        /* ALSA */

        int i_err;
        snd_pcm_t *p_alsa_pcm;
        char* psz_alsa_device_name = ResolveALSADeviceName( psz_device );

        if( ( i_err = snd_pcm_open( &p_alsa_pcm, psz_alsa_device_name, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK ) ) < 0 )
        {
            msg_Err( p_this, "cannot open device %s for ALSA audio (%s)", psz_alsa_device_name, snd_strerror( i_err ) );
            free( psz_alsa_device_name );
            goto open_failed;
        }

        snd_pcm_close( p_alsa_pcm );
        free( psz_alsa_device_name );
    }
    else
#endif /* HAVE_ALSA */
    {
        /* OSS */

        if( ( i_fd = open( psz_device, O_RDONLY | O_NONBLOCK ) ) < 0 )
        {
            msg_Err( p_this, "cannot open device %s for OSS audio (%m)", psz_device );
            goto open_failed;
        }

        /* this will fail if the device is video */
        if( ioctl( i_fd, SNDCTL_DSP_GETCAPS, &i_caps ) < 0 )
        {
            msg_Err( p_this, "cannot get audio caps (%m)" );
            goto open_failed;
        }

        if( i_fd >= 0 ) close( i_fd );
    }

    return VLC_TRUE;

open_failed:
    if( i_fd >= 0 ) close( i_fd );
    return VLC_FALSE;
}

/*****************************************************************************
 * Print a user-class v4l2 control's details, create the relevant variable,
 * change the value if needed.
 *****************************************************************************/
static void ControlListPrint( vlc_object_t *p_obj, int i_fd,
                              struct v4l2_queryctrl queryctrl,
                              vlc_bool_t b_reset, vlc_bool_t b_demux )
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
        char *psz_buf;
        psz_name = strdup( (const char *)queryctrl.name );
        for( psz_buf = psz_name; *psz_buf; psz_buf++ )
        {
            if( *psz_buf == ' ' ) *psz_buf = '-';
        }
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
                if( ioctl( i_fd, VIDIOC_QUERYMENU, &querymenu ) >= 0 )
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
                if( ioctl( i_fd, VIDIOC_G_CTRL, &control ) >= 0 )
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
                    Control( p_obj, i_fd, psz_name,
                                  queryctrl.id, i_val );
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

    if (b_demux)
        var_AddCallback( p_obj, psz_name,
                        DemuxControlCallback, (void*)queryctrl.id );
    else
        var_AddCallback( p_obj, psz_name,
                        AccessControlCallback, (void*)queryctrl.id );

    free( psz_name );
}

/*****************************************************************************
 * List all user-class v4l2 controls, set them to the user specified
 * value and create the relevant variables to enable runtime changes
 *****************************************************************************/
static int ControlList( vlc_object_t *p_obj, int i_fd,
                        vlc_bool_t b_reset, vlc_bool_t b_demux )
{
    struct v4l2_queryctrl queryctrl;
    int i_cid;

    memset( &queryctrl, 0, sizeof( queryctrl ) );

    /* A list of available controls (aka the variable name) will be
     * stored as choices in the "controls" variable. We'll thus be able
     * to use those to create an appropriate interface */
    var_Create( p_obj, "controls", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );

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

    /* List public controls */
    for( i_cid = V4L2_CID_BASE;
         i_cid < V4L2_CID_LASTP1;
         i_cid ++ )
    {
        queryctrl.id = i_cid;
        if( ioctl( i_fd, VIDIOC_QUERYCTRL, &queryctrl ) >= 0 )
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
        if( ioctl( i_fd, VIDIOC_QUERYCTRL, &queryctrl ) >= 0 )
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
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Reset all user-class v4l2 controls to their default value
 *****************************************************************************/
static int ControlReset( vlc_object_t *p_obj, int i_fd )
{
    struct v4l2_queryctrl queryctrl;
    int i_cid;

    memset( &queryctrl, 0, sizeof( queryctrl ) );

    /* public controls */
    for( i_cid = V4L2_CID_BASE;
         i_cid < V4L2_CID_LASTP1;
         i_cid ++ )
    {
        queryctrl.id = i_cid;
        if( ioctl( i_fd, VIDIOC_QUERYCTRL, &queryctrl ) >= 0 )
        {
            struct v4l2_control control;
            if( queryctrl.flags & V4L2_CTRL_FLAG_DISABLED )
                continue;
            memset( &control, 0, sizeof( control ) );
            control.id = queryctrl.id;
            if( ioctl( i_fd, VIDIOC_G_CTRL, &control ) >= 0
             && queryctrl.default_value != control.value )
            {
                int i;
                for( i = 0; controls[i].psz_name != NULL; i++ )
                    if( controls[i].i_cid == queryctrl.id ) break;
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
        if( ioctl( i_fd, VIDIOC_QUERYCTRL, &queryctrl ) >= 0 )
        {
            struct v4l2_control control;
            if( queryctrl.flags & V4L2_CTRL_FLAG_DISABLED )
                continue;
            memset( &control, 0, sizeof( control ) );
            control.id = queryctrl.id;
            if( ioctl( i_fd, VIDIOC_G_CTRL, &control ) >= 0
             && queryctrl.default_value != control.value )
            {
                Control( p_obj, i_fd, (const char *)queryctrl.name,
                         queryctrl.id, queryctrl.default_value );
            }
        }
        else
            break;
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

    if( i_value == -1 )
        return VLC_SUCCESS;

    memset( &queryctrl, 0, sizeof( queryctrl ) );

    queryctrl.id = i_cid;

    if( ioctl( i_fd, VIDIOC_QUERYCTRL, &queryctrl ) < 0
        || queryctrl.flags & V4L2_CTRL_FLAG_DISABLED )
    {
        msg_Dbg( p_obj, "%s (%x) control is not supported.", psz_name,
                 i_cid );
        return VLC_EGENERIC;
    }

    memset( &control, 0, sizeof( control ) );
    control.id = i_cid;

    if( i_value >= 0 )
    {
        control.value = i_value;
        if( ioctl( i_fd, VIDIOC_S_CTRL, &control ) < 0 )
        {
            msg_Err( p_obj, "unable to set %s to %d (%m)", psz_name,
                     i_value );
            return VLC_EGENERIC;
        }
    }
    if( ioctl( i_fd, VIDIOC_G_CTRL, &control ) >= 0 )
    {
        vlc_value_t val;
        msg_Dbg( p_obj, "video %s: %d", psz_name, control.value );
        switch( var_Type( p_obj, psz_name ) & VLC_VAR_TYPE )
        {
            case VLC_VAR_BOOL:
                val.b_bool = control.value;
                var_Change( p_obj, psz_name, VLC_VAR_SETVALUE, &val, NULL );
                var_SetVoid( p_obj, "controls-update" );
                break;
            case VLC_VAR_INTEGER:
                val.i_int = control.value;
                var_Change( p_obj, psz_name, VLC_VAR_SETVALUE, &val, NULL );
                var_SetVoid( p_obj, "controls-update" );
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
    demux_t *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_cid = (int)p_data;

    int i_fd = p_sys->i_fd_video;

    if( i_fd < 0 )
        return VLC_EGENERIC;

    Control( p_this, i_fd, psz_var, i_cid, newval.i_int );

    return VLC_EGENERIC;
}

static int DemuxControlResetCallback( vlc_object_t *p_this,
    const char *psz_var, vlc_value_t oldval, vlc_value_t newval,
    void *p_data )
{
    demux_t *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    int i_fd = p_sys->i_fd_video;

    if( i_fd < 0 )
        return VLC_EGENERIC;

    ControlReset( p_this, i_fd );

    return VLC_EGENERIC;
}

static int AccessControlCallback( vlc_object_t *p_this,
    const char *psz_var, vlc_value_t oldval, vlc_value_t newval,
    void *p_data )
{
    access_t *p_access = (access_t *)p_this;
    demux_sys_t *p_sys = (demux_sys_t *) p_access->p_sys;
    int i_cid = (int)p_data;

    int i_fd = p_sys->i_fd_video;

    if( i_fd < 0 )
        return VLC_EGENERIC;

    Control( p_this, i_fd, psz_var, i_cid, newval.i_int );

    return VLC_EGENERIC;
}

static int AccessControlResetCallback( vlc_object_t *p_this,
    const char *psz_var, vlc_value_t oldval, vlc_value_t newval,
    void *p_data )
{
    access_t *p_access = (access_t *)p_this;
    demux_sys_t *p_sys = (demux_sys_t *) p_access->p_sys;

    int i_fd = p_sys->i_fd_video;

    if( i_fd < 0 )
        return VLC_EGENERIC;

    ControlReset( p_this, i_fd );

    return VLC_EGENERIC;
}
