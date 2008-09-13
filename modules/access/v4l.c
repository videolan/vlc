/*****************************************************************************
 * v4l.c : Video4Linux input module for vlc
 *****************************************************************************
 * Copyright (C) 2002-2004 the VideoLAN team
 * $Id$
 *
 * Author: Laurent Aimar <fenrir@via.ecp.fr>
 *         Paul Forgey <paulf at aphrodite dot com>
 *         Gildas Bazin <gbazin@videolan.org>
 *         Benjamin Pracht <bigben at videolan dot org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_input.h>
#include <vlc_demux.h>
#include <vlc_access.h>
#include <vlc_vout.h>
#include <vlc_codecs.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>

/* From GStreamer's v4l plugin:
 * Because of some really cool feature in video4linux1, also known as
 * 'not including sys/types.h and sys/time.h', we had to include it
 * ourselves. In all their intelligence, these people decided to fix
 * this in the next version (video4linux2) in such a cool way that it
 * breaks all compilations of old stuff...
 * The real problem is actually that linux/time.h doesn't use proper
 * macro checks before defining types like struct timeval. The proper
 * fix here is to either fuck the kernel header (which is what we do
 * by defining _LINUX_TIME_H, an innocent little hack) or by fixing it
 * upstream, which I'll consider doing later on. If you get compiler
 * errors here, check your linux/time.h && sys/time.h header setup.
*/
#define _LINUX_TIME_H

#include <linux/videodev.h>
#include "videodev_mjpeg.h"

#include <sys/soundcard.h>

/*****************************************************************************
 * Module descriptior
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Caching value for V4L captures. This " \
    "value should be set in milliseconds." )
#define VDEV_TEXT N_("Video device name")
#define VDEV_LONGTEXT N_( \
    "Name of the video device to use. " \
    "If you don't specify anything, no video device will be used.")
#define ADEV_TEXT N_("Audio device name")
#define ADEV_LONGTEXT N_( \
    "Name of the audio device to use. " \
    "If you don't specify anything, no audio device will be used.")
#define CHROMA_TEXT N_("Video input chroma format")
#define CHROMA_LONGTEXT N_( \
    "Force the Video4Linux video device to use a specific chroma format " \
    "(eg. I420 (default), RV24, etc.)")
#define FREQUENCY_TEXT N_( "Frequency" )
#define FREQUENCY_LONGTEXT N_( \
    "Frequency to capture (in kHz), if applicable." )
#define CHANNEL_TEXT N_( "Channel" )
#define CHANNEL_LONGTEXT N_( \
    "Channel of the card to use (Usually, 0 = tuner, " \
    "1 = composite, 2 = svideo)." )
#define NORM_TEXT N_( "Norm" )
#define NORM_LONGTEXT N_( \
    "Norm of the stream (Automatic, SECAM, PAL, or NTSC)." )
#define AUDIO_TEXT N_( "Audio Channel" )
#define AUDIO_LONGTEXT N_( \
    "Audio Channel to use, if there are several audio inputs." )
#define WIDTH_TEXT N_( "Width" )
#define WIDTH_LONGTEXT N_( "Width of the stream to capture " \
    "(-1 for autodetect)." )
#define HEIGHT_TEXT N_( "Height" )
#define HEIGHT_LONGTEXT N_( "Height of the stream to capture " \
    "(-1 for autodetect)." )
#define BRIGHTNESS_TEXT N_( "Brightness" )
#define BRIGHTNESS_LONGTEXT N_( \
    "Brightness of the video input." )
#define HUE_TEXT N_( "Hue" )
#define HUE_LONGTEXT N_( \
    "Hue of the video input." )
#define COLOUR_TEXT N_( "Color" )
#define COLOUR_LONGTEXT N_( \
    "Color of the video input." )
#define CONTRAST_TEXT N_( "Contrast" )
#define CONTRAST_LONGTEXT N_( \
    "Contrast of the video input." )
#define TUNER_TEXT N_( "Tuner" )
#define TUNER_LONGTEXT N_( "Tuner to use, if there are several ones." )
#define SAMPLERATE_TEXT N_( "Samplerate" )
#define SAMPLERATE_LONGTEXT N_( \
    "Samplerate of the captured audio stream, in Hz (eg: 11025, 22050, 44100)" )
#define STEREO_TEXT N_( "Stereo" )
#define STEREO_LONGTEXT N_( \
    "Capture the audio stream in stereo." )
#define MJPEG_TEXT N_( "MJPEG" )
#define MJPEG_LONGTEXT N_(  \
    "Set this option if the capture device outputs MJPEG" )
#define DECIMATION_TEXT N_( "Decimation" )
#define DECIMATION_LONGTEXT N_( \
    "Decimation level for MJPEG streams" )
#define QUALITY_TEXT N_( "Quality" )
#define QUALITY_LONGTEXT N_( "Quality of the stream." )
#define FPS_TEXT N_( "Framerate" )
#define FPS_LONGTEXT N_( "Framerate to capture, if applicable " \
    "(-1 for autodetect)." )

static const int i_norm_list[] =
    { VIDEO_MODE_AUTO, VIDEO_MODE_SECAM, VIDEO_MODE_PAL, VIDEO_MODE_NTSC };
static const char *const psz_norm_list_text[] =
    { N_("Automatic"), N_("SECAM"), N_("PAL"),  N_("NTSC") };

vlc_module_begin();
    set_shortname( N_("Video4Linux") );
    set_description( N_("Video4Linux input") );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS );

    add_integer( "v4l-caching", DEFAULT_PTS_DELAY / 1000, NULL,
                 CACHING_TEXT, CACHING_LONGTEXT, true );
    add_string( "v4l-vdev", "/dev/video", 0, VDEV_TEXT, VDEV_LONGTEXT,
                false );
    add_string( "v4l-adev", "/dev/dsp", 0, ADEV_TEXT, ADEV_LONGTEXT,
                false );
    add_string( "v4l-chroma", NULL, NULL, CHROMA_TEXT, CHROMA_LONGTEXT,
                true );
    add_float( "v4l-fps", -1.0, NULL, FPS_TEXT, FPS_LONGTEXT, true );
    add_integer( "v4l-samplerate", 44100, NULL, SAMPLERATE_TEXT,
                SAMPLERATE_LONGTEXT, true );
    add_integer( "v4l-channel", 0, NULL, CHANNEL_TEXT, CHANNEL_LONGTEXT,
                true );
    add_integer( "v4l-tuner", -1, NULL, TUNER_TEXT, TUNER_LONGTEXT, true );
    add_integer( "v4l-norm", VIDEO_MODE_AUTO, NULL, NORM_TEXT, NORM_LONGTEXT,
                false );
        change_integer_list( i_norm_list, psz_norm_list_text, NULL );
    add_integer( "v4l-frequency", -1, NULL, FREQUENCY_TEXT, FREQUENCY_LONGTEXT,
                false );
    add_integer( "v4l-audio", -1, NULL, AUDIO_TEXT, AUDIO_LONGTEXT, true );
    add_bool( "v4l-stereo", true, NULL, STEREO_TEXT, STEREO_LONGTEXT,
            true );
    add_integer( "v4l-width", 0, NULL, WIDTH_TEXT, WIDTH_LONGTEXT, true );
    add_integer( "v4l-height", 0, NULL, HEIGHT_TEXT, HEIGHT_LONGTEXT,
                true );
    add_integer( "v4l-brightness", -1, NULL, BRIGHTNESS_TEXT,
                BRIGHTNESS_LONGTEXT, true );
    add_integer( "v4l-colour", -1, NULL, COLOUR_TEXT, COLOUR_LONGTEXT,
                true );
    add_integer( "v4l-hue", -1, NULL, HUE_TEXT, HUE_LONGTEXT, true );
    add_integer( "v4l-contrast", -1, NULL, CONTRAST_TEXT, CONTRAST_LONGTEXT,
                true );
    add_bool( "v4l-mjpeg", false, NULL, MJPEG_TEXT, MJPEG_LONGTEXT,
            true );
    add_integer( "v4l-decimation", 1, NULL, DECIMATION_TEXT,
            DECIMATION_LONGTEXT, true );
    add_integer( "v4l-quality", 100, NULL, QUALITY_TEXT, QUALITY_LONGTEXT,
            true );

    add_shortcut( "v4l" );
    set_capability( "access_demux", 10 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Access: local prototypes
 *****************************************************************************/
static int Demux  ( demux_t * );
static int Control( demux_t *, int, va_list );

static void ParseMRL    ( demux_t * );
static int  OpenVideoDev( demux_t *, char * );
static int  OpenAudioDev( demux_t *, char * );

static block_t *GrabAudio( demux_t * );
static block_t *GrabVideo( demux_t * );

#define MJPEG_BUFFER_SIZE (256*1024)

struct quicktime_mjpeg_app1
{
    uint32_t    i_reserved;             /* set to 0 */
    uint32_t    i_tag;                  /* 'mjpg' */
    uint32_t    i_field_size;           /* offset following EOI */
    uint32_t    i_padded_field_size;    /* offset following EOI+pad */
    uint32_t    i_next_field;           /* offset to next field */
    uint32_t    i_DQT_offset;
    uint32_t    i_DHT_offset;
    uint32_t    i_SOF_offset;
    uint32_t    i_SOS_offset;
    uint32_t    i_data_offset;          /* following SOS marker data */
};

static const struct
{
    int i_v4l;
    int i_fourcc;

} v4lchroma_to_fourcc[] =
{
    { VIDEO_PALETTE_GREY, VLC_FOURCC( 'G', 'R', 'E', 'Y' ) },
    { VIDEO_PALETTE_HI240, VLC_FOURCC( 'I', '2', '4', '0' ) },
    { VIDEO_PALETTE_RGB565, VLC_FOURCC( 'R', 'V', '1', '6' ) },
    { VIDEO_PALETTE_RGB555, VLC_FOURCC( 'R', 'V', '1', '5' ) },
    { VIDEO_PALETTE_RGB24, VLC_FOURCC( 'R', 'V', '2', '4' ) },
    { VIDEO_PALETTE_RGB32, VLC_FOURCC( 'R', 'V', '3', '2' ) },
    { VIDEO_PALETTE_YUV422, VLC_FOURCC( 'Y', 'U', 'Y', '2' ) },
    { VIDEO_PALETTE_YUV422, VLC_FOURCC( 'Y', 'U', 'Y', 'V' ) },
    { VIDEO_PALETTE_YUYV, VLC_FOURCC( 'Y', 'U', 'Y', '2' ) },
    { VIDEO_PALETTE_YUYV, VLC_FOURCC( 'Y', 'U', 'Y', 'V' ) },
    { VIDEO_PALETTE_UYVY, VLC_FOURCC( 'U', 'Y', 'V', 'Y' ) },
    { VIDEO_PALETTE_YUV420, VLC_FOURCC( 'I', '4', '2', 'N' ) },
    { VIDEO_PALETTE_YUV411, VLC_FOURCC( 'I', '4', '1', 'N' ) },
    { VIDEO_PALETTE_RAW, VLC_FOURCC( 'G', 'R', 'A', 'W' ) },
    { VIDEO_PALETTE_YUV422P, VLC_FOURCC( 'I', '4', '2', '2' ) },
    { VIDEO_PALETTE_YUV420P, VLC_FOURCC( 'I', '4', '2', '0' ) },
    { VIDEO_PALETTE_YUV411P, VLC_FOURCC( 'I', '4', '1', '1' ) },
    { 0, 0 }
};

struct demux_sys_t
{
    /* Devices */
    char *psz_device;         /* Main device from MRL, can be video or audio */

    char *psz_vdev;
    int  fd_video;

    char *psz_adev;
    int  fd_audio;

    /* Video properties */
    picture_t pic;

    int i_fourcc;
    int i_channel;
    int i_audio;
    int i_norm;
    int i_tuner;
    int i_frequency;
    int i_width;
    int i_height;

    int i_brightness;
    int i_hue;
    int i_colour;
    int i_contrast;

    float f_fps;            /* <= 0.0 mean to grab at full rate */
    mtime_t i_video_pts;    /* only used when f_fps > 0 */

    bool b_mjpeg;
    int i_decimation;
    int i_quality;

    struct video_capability vid_cap;
    struct video_mbuf       vid_mbuf;
    struct mjpeg_requestbuffers mjpeg_buffers;

    uint8_t *p_video_mmap;
    int     i_frame_pos;

    struct video_mmap   vid_mmap;
    struct video_picture vid_picture;

    int          i_video_frame_size;
    es_out_id_t  *p_es_video;

    /* Audio properties */
    vlc_fourcc_t i_acodec_raw;
    int          i_sample_rate;
    bool   b_stereo;
    int          i_audio_max_frame_size;
    block_t      *p_block_audio;
    es_out_id_t  *p_es_audio;
};

/*****************************************************************************
 * Open: opens v4l device
 *****************************************************************************
 *
 * url: <video device>::::
 *
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    vlc_value_t val;

    /* Only when selected */
    if( *p_demux->psz_access == '\0' )
        return VLC_EGENERIC;

    /* Set up p_demux */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->info.i_update = 0;
    p_demux->info.i_title = 0;
    p_demux->info.i_seekpoint = 0;
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    memset( p_sys, 0, sizeof( demux_sys_t ) );

    var_Create( p_demux, "v4l-audio", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_demux, "v4l-audio", &val );
    p_sys->i_audio          = val.i_int;

    var_Create( p_demux, "v4l-channel", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_demux, "v4l-channel", &val );
    p_sys->i_channel        = val.i_int;

    var_Create( p_demux, "v4l-norm", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_demux, "v4l-norm", &val );
    p_sys->i_norm           = val.i_int;

    var_Create( p_demux, "v4l-tuner", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_demux, "v4l-tuner", &val );
    p_sys->i_tuner          = val.i_int;

    var_Create( p_demux, "v4l-frequency",
                                    VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_demux, "v4l-frequency", &val );
    p_sys->i_frequency      = val.i_int;

    var_Create( p_demux, "v4l-fps", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT );
    var_Get( p_demux, "v4l-fps", &val );
    p_sys->f_fps            = val.f_float;

    var_Create( p_demux, "v4l-width", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_demux, "v4l-width", &val );
    p_sys->i_width          = val.i_int;

    var_Create( p_demux, "v4l-height", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_demux, "v4l-height", &val );
    p_sys->i_height         = val.i_int;

    p_sys->i_video_pts      = -1;

    var_Create( p_demux, "v4l-brightness", VLC_VAR_INTEGER |
                                                        VLC_VAR_DOINHERIT );
    var_Get( p_demux, "v4l-brightness", &val );
    p_sys->i_brightness     = val.i_int;

    var_Create( p_demux, "v4l-hue", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_demux, "v4l-hue", &val );
    p_sys->i_hue            = -1;

    var_Create( p_demux, "v4l-colour", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_demux, "v4l-colour", &val );
    p_sys->i_colour         = val.i_int;

    var_Create( p_demux, "v4l-contrast", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_demux, "v4l-contrast", &val );
    p_sys->i_contrast       = val.i_int;

    var_Create( p_demux, "v4l-mjpeg", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Get( p_demux, "v4l-mjpeg", &val );
    p_sys->b_mjpeg     = val.b_bool;

    var_Create( p_demux, "v4l-decimation", VLC_VAR_INTEGER |
                                                            VLC_VAR_DOINHERIT );
    var_Get( p_demux, "v4l-decimation", &val );
    p_sys->i_decimation = val.i_int;

    var_Create( p_demux, "v4l-quality", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_demux, "v4l-quality", &val );
    p_sys->i_quality = val.i_int;

    var_Create( p_demux, "v4l-samplerate",
                                    VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_demux, "v4l-samplerate", &val );
    p_sys->i_sample_rate  = val.i_int;

    var_Create( p_demux, "v4l-stereo", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Get( p_demux, "v4l-stereo", &val );
    p_sys->b_stereo       = val.b_bool;

    p_sys->psz_device = p_sys->psz_vdev = p_sys->psz_adev = NULL;
    p_sys->fd_video = -1;
    p_sys->fd_audio = -1;

    p_sys->p_es_video = p_sys->p_es_audio = 0;
    p_sys->p_block_audio = 0;

    ParseMRL( p_demux );

    /* Find main device (video or audio) */
    if( p_sys->psz_device && *p_sys->psz_device )
    {
        msg_Dbg( p_demux, "main device=`%s'", p_sys->psz_device );

        /* Try to open as video device */
        p_sys->fd_video = OpenVideoDev( p_demux, p_sys->psz_device );

        if( p_sys->fd_video < 0 )
        {
            /* Try to open as audio device */
            p_sys->fd_audio = OpenAudioDev( p_demux, p_sys->psz_device );
            if( p_sys->fd_audio >= 0 )
            {
                free( p_sys->psz_adev );
                p_sys->psz_adev = p_sys->psz_device;
                p_sys->psz_device = NULL;
            }
        }
        else
        {
            free( p_sys->psz_vdev );
            p_sys->psz_vdev = p_sys->psz_device;
            p_sys->psz_device = NULL;
        }
    }

    /* If no device opened, only continue if the access was forced */
    if( p_sys->fd_video < 0 && p_sys->fd_audio < 0 )
    {
        if( strcmp( p_demux->psz_access, "v4l" ) )
        {
            Close( p_this );
            return VLC_EGENERIC;
        }
    }

    /* Find video device */
    if( p_sys->fd_video < 0 )
    {
        if( !p_sys->psz_vdev || !*p_sys->psz_vdev )
        {
            free( p_sys->psz_vdev );
            p_sys->psz_vdev = var_CreateGetString( p_demux, "v4l-vdev" );;
        }

        if( p_sys->psz_vdev && *p_sys->psz_vdev )
        {
            p_sys->fd_video = OpenVideoDev( p_demux, p_sys->psz_vdev );
        }
    }

    /* Find audio device */
    if( p_sys->fd_audio < 0 )
    {
        if( !p_sys->psz_adev || !*p_sys->psz_adev )
        {
            free( p_sys->psz_adev );
            p_sys->psz_adev = var_CreateGetString( p_demux, "v4l-adev" );;
        }

        if( p_sys->psz_adev && *p_sys->psz_adev )
        {
            p_sys->fd_audio = OpenAudioDev( p_demux, p_sys->psz_adev );
        }
    }

    if( p_sys->fd_video < 0 && p_sys->fd_audio < 0 )
    {
        Close( p_this );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_demux, "v4l grabbing started" );

    /* Declare elementary streams */
    if( p_sys->fd_video >= 0 )
    {
        es_format_t fmt;
        es_format_Init( &fmt, VIDEO_ES, p_sys->i_fourcc );
        fmt.video.i_width  = p_sys->i_width;
        fmt.video.i_height = p_sys->i_height;
        fmt.video.i_aspect = 4 * VOUT_ASPECT_FACTOR / 3;

        /* Setup rgb mask for RGB formats */
        switch( p_sys->i_fourcc )
        {
            case VLC_FOURCC('R','V','1','5'):
                fmt.video.i_rmask = 0x001f;
                fmt.video.i_gmask = 0x03e0;
                fmt.video.i_bmask = 0x7c00;
                break;
            case VLC_FOURCC('R','V','1','6'):
                fmt.video.i_rmask = 0x001f;
                fmt.video.i_gmask = 0x07e0;
                fmt.video.i_bmask = 0xf800;
                break;
            case VLC_FOURCC('R','V','2','4'):
            case VLC_FOURCC('R','V','3','2'):
                fmt.video.i_rmask = 0x00ff0000;
                fmt.video.i_gmask = 0x0000ff00;
                fmt.video.i_bmask = 0x000000ff;
                break;
        }

        msg_Dbg( p_demux, "added new video es %4.4s %dx%d",
                 (char*)&fmt.i_codec, fmt.video.i_width, fmt.video.i_height );
        p_sys->p_es_video = es_out_Add( p_demux->out, &fmt );
    }

    if( p_sys->fd_audio >= 0 )
    {
        es_format_t fmt;
        es_format_Init( &fmt, AUDIO_ES, VLC_FOURCC('a','r','a','w') );

        fmt.audio.i_channels = p_sys->b_stereo ? 2 : 1;
        fmt.audio.i_rate = p_sys->i_sample_rate;
        fmt.audio.i_bitspersample = 16; // FIXME ?
        fmt.audio.i_blockalign = fmt.audio.i_channels *
            fmt.audio.i_bitspersample / 8;
        fmt.i_bitrate = fmt.audio.i_channels * fmt.audio.i_rate *
            fmt.audio.i_bitspersample;

        msg_Dbg( p_demux, "new audio es %d channels %dHz",
                 fmt.audio.i_channels, fmt.audio.i_rate );

        p_sys->p_es_audio = es_out_Add( p_demux->out, &fmt );
    }

    /* Update default_pts to a suitable value for access */
    var_Create( p_demux, "v4l-caching", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close device, free resources
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys   = p_demux->p_sys;

    free( p_sys->psz_device );
    free( p_sys->psz_vdev );
    free( p_sys->psz_adev );
    if( p_sys->fd_video >= 0 ) close( p_sys->fd_video );
    if( p_sys->fd_audio >= 0 ) close( p_sys->fd_audio );
    if( p_sys->p_block_audio ) block_Release( p_sys->p_block_audio );

    if( p_sys->b_mjpeg )
    {
        int i_noframe = -1;
        ioctl( p_sys->fd_video, MJPIOC_QBUF_CAPT, &i_noframe );
    }

    if( p_sys->p_video_mmap && p_sys->p_video_mmap != MAP_FAILED )
    {
        if( p_sys->b_mjpeg )
            munmap( p_sys->p_video_mmap, p_sys->mjpeg_buffers.size *
                    p_sys->mjpeg_buffers.count );
        else
            munmap( p_sys->p_video_mmap, p_sys->vid_mbuf.size );
    }

    free( p_sys );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    bool *pb;
    int64_t    *pi64;

    switch( i_query )
    {
        /* Special for access_demux */
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_SEEK:
        case DEMUX_SET_PAUSE_STATE:
        case DEMUX_CAN_CONTROL_PACE:
            pb = (bool*)va_arg( args, bool * );
            *pb = false;
            return VLC_SUCCESS;

        case DEMUX_GET_PTS_DELAY:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = (int64_t)var_GetInteger( p_demux, "v4l-caching" ) * 1000;
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
 * Demux:
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    es_out_id_t  *p_es = p_sys->p_es_audio;
    block_t *p_block = NULL;

    /* Try grabbing audio frames first */
    if( p_sys->fd_audio < 0 || !( p_block = GrabAudio( p_demux ) ) )
    {
        /* Try grabbing video frame */
        p_es = p_sys->p_es_video;
        if( p_sys->fd_video > 0 ) p_block = GrabVideo( p_demux );
    }

    if( !p_block )
    {
        /* Sleep so we do not consume all the cpu, 10ms seems
         * like a good value (100fps) */
        msleep( 10000 );
        return 1;
    }

    es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_block->i_pts );
    es_out_Send( p_demux->out, p_es, p_block );

    return 1;
}

/*****************************************************************************
 * ParseMRL: parse the options contained in the MRL
 *****************************************************************************/
static void ParseMRL( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    char *psz_dup = strdup( p_demux->psz_path );
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
            if( !strncmp( psz_parser, "channel=", strlen( "channel=" ) ) )
            {
                p_sys->i_channel = strtol( psz_parser + strlen( "channel=" ),
                                           &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "norm=", strlen( "norm=" ) ) )
            {
                psz_parser += strlen( "norm=" );
                if( !strncmp( psz_parser, "pal", strlen( "pal" ) ) )
                {
                    p_sys->i_norm = VIDEO_MODE_PAL;
                    psz_parser += strlen( "pal" );
                }
                else if( !strncmp( psz_parser, "ntsc", strlen( "ntsc" ) ) )
                {
                    p_sys->i_norm = VIDEO_MODE_NTSC;
                    psz_parser += strlen( "ntsc" );
                }
                else if( !strncmp( psz_parser, "secam", strlen( "secam" ) ) )
                {
                    p_sys->i_norm = VIDEO_MODE_SECAM;
                    psz_parser += strlen( "secam" );
                }
                else if( !strncmp( psz_parser, "auto", strlen( "auto" ) ) )
                {
                    p_sys->i_norm = VIDEO_MODE_AUTO;
                    psz_parser += strlen( "auto" );
                }
                else
                {
                    p_sys->i_norm = strtol( psz_parser, &psz_parser, 0 );
                }
            }
            else if( !strncmp( psz_parser, "frequency=",
                               strlen( "frequency=" ) ) )
            {
                p_sys->i_frequency =
                    strtol( psz_parser + strlen( "frequency=" ),
                            &psz_parser, 0 );
                if( p_sys->i_frequency < 30000 )
                {
                    msg_Warn( p_demux, "v4l syntax has changed : "
                              "'frequency' is now channel frequency in kHz");
                }
            }
            else if( !strncmp( psz_parser, "audio=", strlen( "audio=" ) ) )
            {
                p_sys->i_audio = strtol( psz_parser + strlen( "audio=" ),
                                         &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "size=", strlen( "size=" ) ) )
            {
                psz_parser += strlen( "size=" );
                if( !strncmp( psz_parser, "subqcif", strlen( "subqcif" ) ) )
                {
                    p_sys->i_width  = 128;
                    p_sys->i_height = 96;
                }
                else if( !strncmp( psz_parser, "qsif", strlen( "qsif" ) ) )
                {
                    p_sys->i_width  = 160;
                    p_sys->i_height = 120;
                }
                else if( !strncmp( psz_parser, "qcif", strlen( "qcif" ) ) )
                {
                    p_sys->i_width  = 176;
                    p_sys->i_height = 144;
                }
                else if( !strncmp( psz_parser, "sif", strlen( "sif" ) ) )
                {
                    p_sys->i_width  = 320;
                    p_sys->i_height = 244;
                }
                else if( !strncmp( psz_parser, "cif", strlen( "cif" ) ) )
                {
                    p_sys->i_width  = 352;
                    p_sys->i_height = 288;
                }
                else if( !strncmp( psz_parser, "vga", strlen( "vga" ) ) )
                {
                    p_sys->i_width  = 640;
                    p_sys->i_height = 480;
                }
                else
                {
                    /* widthxheight */
                    p_sys->i_width = strtol( psz_parser, &psz_parser, 0 );
                    if( *psz_parser == 'x' || *psz_parser == 'X')
                    {
                        p_sys->i_height = strtol( psz_parser + 1,
                                                  &psz_parser, 0 );
                    }
                    msg_Dbg( p_demux, "WxH %dx%d", p_sys->i_width,
                             p_sys->i_height );
                }
            }
            else if( !strncmp( psz_parser, "brightness=", strlen( "brightness=" ) ) )
            {
                p_sys->i_brightness = strtol( psz_parser + strlen( "brightness=" ),
                                              &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "colour=", strlen( "colour=" ) ) )
            {
                p_sys->i_colour = strtol( psz_parser + strlen( "colour=" ),
                                          &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "hue=", strlen( "hue=" ) ) )
            {
                p_sys->i_hue = strtol( psz_parser + strlen( "hue=" ),
                                       &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "contrast=", strlen( "contrast=" ) ) )
            {
                p_sys->i_contrast = strtol( psz_parser + strlen( "contrast=" ),
                                            &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "tuner=", strlen( "tuner=" ) ) )
            {
                p_sys->i_tuner = strtol( psz_parser + strlen( "tuner=" ),
                                         &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "adev=", strlen( "adev=" ) ) )
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
            else if( !strncmp( psz_parser, "samplerate=",
                               strlen( "samplerate=" ) ) )
            {
                p_sys->i_sample_rate =
                    strtol( psz_parser + strlen( "samplerate=" ),
                            &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "stereo", strlen( "stereo" ) ) )
            {
                psz_parser += strlen( "stereo" );

                p_sys->b_stereo = true;
            }
            else if( !strncmp( psz_parser, "mono", strlen( "mono" ) ) )
            {
                psz_parser += strlen( "mono" );

                p_sys->b_stereo = false;
            }
            else if( !strncmp( psz_parser, "mjpeg", strlen( "mjpeg" ) ) )
            {
                psz_parser += strlen( "mjpeg" );

                p_sys->b_mjpeg = true;
            }
            else if( !strncmp( psz_parser, "decimation=",
                        strlen( "decimation=" ) ) )
            {
                p_sys->i_decimation =
                    strtol( psz_parser + strlen( "decimation=" ),
                            &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "quality=",
                        strlen( "quality=" ) ) )
            {
                p_sys->i_quality =
                    strtol( psz_parser + strlen( "quality=" ),
                            &psz_parser, 0 );
            }
            else if( !strncmp( psz_parser, "fps=", strlen( "fps=" ) ) )
            {
                p_sys->f_fps = strtof( psz_parser + strlen( "fps=" ),
                                       &psz_parser );
            }
            else
            {
                msg_Warn( p_demux, "unknown option" );
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

    if( *psz_dup )
    {
        p_sys->psz_device = strdup( psz_dup );
    }
    free( psz_dup );
}

/*****************************************************************************
 * OpenVideoDev:
 *****************************************************************************/
static int OpenVideoDev( demux_t *p_demux, char *psz_device )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_fd;

    struct video_channel vid_channel;
    struct mjpeg_params mjpeg;
    int i;

    if( ( i_fd = open( psz_device, O_RDWR ) ) < 0 )
    {
        msg_Err( p_demux, "cannot open device (%m)" );
        goto vdev_failed;
    }

    if( ioctl( i_fd, VIDIOCGCAP, &p_sys->vid_cap ) < 0 )
    {
        msg_Err( p_demux, "cannot get capabilities (%m)" );
        goto vdev_failed;
    }

    msg_Dbg( p_demux,
             "V4L device %s %d channels %d audios %d < w < %d %d < h < %d",
             p_sys->vid_cap.name,
             p_sys->vid_cap.channels,
             p_sys->vid_cap.audios,
             p_sys->vid_cap.minwidth,  p_sys->vid_cap.maxwidth,
             p_sys->vid_cap.minheight, p_sys->vid_cap.maxheight );

    if( p_sys->i_channel < 0 || p_sys->i_channel >= p_sys->vid_cap.channels )
    {
        msg_Dbg( p_demux, "invalid channel, falling back on channel 0" );
        p_sys->i_channel = 0;
    }
    if( p_sys->vid_cap.audios && p_sys->i_audio >= p_sys->vid_cap.audios )
    {
        msg_Dbg( p_demux, "invalid audio, falling back with no audio" );
        p_sys->i_audio = -1;
    }

    if( p_sys->i_width < p_sys->vid_cap.minwidth ||
        p_sys->i_width > p_sys->vid_cap.maxwidth )
    {
        msg_Dbg( p_demux, "invalid width %i", p_sys->i_width );
        p_sys->i_width = 0;
    }
    if( p_sys->i_height < p_sys->vid_cap.minheight ||
        p_sys->i_height > p_sys->vid_cap.maxheight )
    {
        msg_Dbg( p_demux, "invalid height %i", p_sys->i_height );
        p_sys->i_height = 0;
    }

    if( !( p_sys->vid_cap.type & VID_TYPE_CAPTURE ) )
    {
        msg_Err( p_demux, "cannot grab" );
        goto vdev_failed;
    }

    vid_channel.channel = p_sys->i_channel;
    if( ioctl( i_fd, VIDIOCGCHAN, &vid_channel ) < 0 )
    {
        msg_Err( p_demux, "cannot get channel infos (%m)" );
        goto vdev_failed;
    }
    msg_Dbg( p_demux,
             "setting channel %s(%d) %d tuners flags=0x%x type=0x%x norm=0x%x",
             vid_channel.name, vid_channel.channel, vid_channel.tuners,
             vid_channel.flags, vid_channel.type, vid_channel.norm );

    if( p_sys->i_tuner >= vid_channel.tuners )
    {
        msg_Dbg( p_demux, "invalid tuner, falling back on tuner 0" );
        p_sys->i_tuner = 0;
    }

    vid_channel.norm = p_sys->i_norm;
    if( ioctl( i_fd, VIDIOCSCHAN, &vid_channel ) < 0 )
    {
        msg_Err( p_demux, "cannot set channel (%m)" );
        goto vdev_failed;
    }

    if( vid_channel.flags & VIDEO_VC_TUNER )
    {

        /* set tuner */
#if 0
        struct video_tuner vid_tuner;
        if( p_sys->i_tuner >= 0 )
        {
            vid_tuner.tuner = p_sys->i_tuner;
            if( ioctl( i_fd, VIDIOCGTUNER, &vid_tuner ) < 0 )
            {
                msg_Err( p_demux, "cannot get tuner (%m)" );
                goto vdev_failed;
            }
            msg_Dbg( p_demux, "tuner %s low=%d high=%d, flags=0x%x "
                     "mode=0x%x signal=0x%x",
                     vid_tuner.name, vid_tuner.rangelow, vid_tuner.rangehigh,
                     vid_tuner.flags, vid_tuner.mode, vid_tuner.signal );

            msg_Dbg( p_demux, "setting tuner %s (%d)",
                     vid_tuner.name, vid_tuner.tuner );

            /* FIXME FIXME to be checked FIXME FIXME */
            //vid_tuner.mode = p_sys->i_norm;
            if( ioctl( i_fd, VIDIOCSTUNER, &vid_tuner ) < 0 )
            {
                msg_Err( p_demux, "cannot set tuner (%m)" );
                goto vdev_failed;
            }
        }
#endif

        /* Show a warning if frequency is < than 30000.
         * User is certainly usint old syntax. */


        /* set frequency */
        if( p_sys->i_frequency >= 0 )
        {
            int driver_frequency = p_sys->i_frequency * 16 /1000;
            if( ioctl( i_fd, VIDIOCSFREQ, &driver_frequency ) < 0 )
            {
                msg_Err( p_demux, "cannot set frequency (%m)" );
                goto vdev_failed;
            }
            msg_Dbg( p_demux, "frequency %d (%d)", p_sys->i_frequency,
                                                   driver_frequency );
        }
    }

    /* set audio */
    if( vid_channel.flags & VIDEO_VC_AUDIO )
    {
        struct video_audio      vid_audio;

        /* XXX TODO volume, balance, ... */
        if( p_sys->i_audio >= 0 )
        {
            vid_audio.audio = p_sys->i_audio;
            if( ioctl( i_fd, VIDIOCGAUDIO, &vid_audio ) < 0 )
            {
                msg_Err( p_demux, "cannot get audio (%m)" );
                goto vdev_failed;
            }

            /* unmute audio */
            vid_audio.flags &= ~VIDEO_AUDIO_MUTE;

            if( ioctl( i_fd, VIDIOCSAUDIO, &vid_audio ) < 0 )
            {
                msg_Err( p_demux, "cannot set audio (%m)" );
                goto vdev_failed;
            }
        }

    }

    /* establish basic params with input and norm before feeling width
     * or height */
    if( p_sys->b_mjpeg )
    {
        struct quicktime_mjpeg_app1 *p_app1;
        int32_t i_offset;

        if( ioctl( i_fd, MJPIOC_G_PARAMS, &mjpeg ) < 0 )
        {
            msg_Err( p_demux, "cannot get mjpeg params (%m)" );
            goto vdev_failed;
        }
        mjpeg.input = p_sys->i_channel;
        mjpeg.norm  = p_sys->i_norm;
        mjpeg.decimation = p_sys->i_decimation;

        if( p_sys->i_width )
            mjpeg.img_width = p_sys->i_width / p_sys->i_decimation;
        if( p_sys->i_height )
            mjpeg.img_height = p_sys->i_height / p_sys->i_decimation;

        /* establish Quicktime APP1 marker while we are here */
        mjpeg.APPn = 1;
        mjpeg.APP_len = 40;

        /* aligned */
        p_app1 = (struct quicktime_mjpeg_app1 *)mjpeg.APP_data;
        p_app1->i_reserved = 0;
        p_app1->i_tag = VLC_FOURCC( 'm','j','p','g' );
        p_app1->i_field_size = 0;
        p_app1->i_padded_field_size = 0;
        p_app1->i_next_field = 0;
        /* XXX WARNING XXX */
        /* these's nothing magic about these values.  We are dangerously
         * assuming the encoder card is encoding mjpeg-a and is not throwing
         * in marker tags we aren't expecting.  It's bad enough we have to
         * search through the jpeg output for every frame we grab just to
         * find the first field's end marker, so we take this risk to boost
         * performance.
         * This is really something the driver could do for us because this
         * does conform to standards outside of Apple Quicktime.
         */
        i_offset = 0x2e;
        p_app1->i_DQT_offset = hton32( i_offset );
        i_offset = 0xb4;
        p_app1->i_DHT_offset = hton32( i_offset );
        i_offset = 0x258;
        p_app1->i_SOF_offset = hton32( i_offset );
        i_offset = 0x26b;
        p_app1->i_SOS_offset = hton32( i_offset );
        i_offset = 0x279;
        p_app1->i_data_offset = hton32( i_offset );

        /* SOF and SOS aren't specified by the mjpeg API because they aren't
         * optional.  They will be present in the output. */
        mjpeg.jpeg_markers = JPEG_MARKER_DHT | JPEG_MARKER_DQT;

        if( ioctl( i_fd, MJPIOC_S_PARAMS, &mjpeg ) < 0 )
        {
            msg_Err( p_demux, "cannot set mjpeg params (%m)" );
            goto vdev_failed;
        }

        p_sys->i_width = mjpeg.img_width * mjpeg.HorDcm;
        p_sys->i_height = mjpeg.img_height * mjpeg.VerDcm *
            mjpeg.field_per_buff;
    }

    /* fix width/height */
    if( !p_sys->b_mjpeg && ( p_sys->i_width == 0 || p_sys->i_height == 0 ) )
    {
        struct video_window vid_win;

        if( ioctl( i_fd, VIDIOCGWIN, &vid_win ) < 0 )
        {
            msg_Err( p_demux, "cannot get win (%m)" );
            goto vdev_failed;
        }
        p_sys->i_width  = vid_win.width;
        p_sys->i_height = vid_win.height;

        if( !p_sys->i_width || !p_sys->i_height )
        {
            p_sys->i_width = p_sys->vid_cap.maxwidth;
            p_sys->i_height = p_sys->vid_cap.maxheight;
        }

        if( !p_sys->i_width || !p_sys->i_height )
        {
            msg_Err( p_demux, "invalid video size (%ix%i)",
                     p_sys->i_width, p_sys->i_height );
            goto vdev_failed;
        }

        msg_Dbg( p_demux, "will use %dx%d", p_sys->i_width, p_sys->i_height );
    }

    if( !p_sys->b_mjpeg )
    {
        /* set hue/color/.. */
        if( ioctl( i_fd, VIDIOCGPICT, &p_sys->vid_picture ) == 0 )
        {
            struct video_picture vid_picture = p_sys->vid_picture;

            if( p_sys->i_brightness >= 0 && p_sys->i_brightness < 65536 )
            {
                vid_picture.brightness = p_sys->i_brightness;
            }
            if( p_sys->i_colour >= 0 && p_sys->i_colour < 65536 )
            {
                vid_picture.colour = p_sys->i_colour;
            }
            if( p_sys->i_hue >= 0 && p_sys->i_hue < 65536 )
            {
                vid_picture.hue = p_sys->i_hue;
            }
            if( p_sys->i_contrast  >= 0 && p_sys->i_contrast < 65536 )
            {
                vid_picture.contrast = p_sys->i_contrast;
            }
            if( ioctl( i_fd, VIDIOCSPICT, &vid_picture ) == 0 )
            {
                msg_Dbg( p_demux, "v4l device uses brightness: %d",
                         vid_picture.brightness );
                msg_Dbg( p_demux, "v4l device uses colour: %d",
                         vid_picture.colour );
                msg_Dbg( p_demux, "v4l device uses hue: %d", vid_picture.hue );
                msg_Dbg( p_demux, "v4l device uses contrast: %d",
                         vid_picture.contrast );
                p_sys->vid_picture = vid_picture;
            }
        }

        /* Find out video format used by device */
        if( ioctl( i_fd, VIDIOCGPICT, &p_sys->vid_picture ) == 0 )
        {
            struct video_picture vid_picture = p_sys->vid_picture;
            char *psz;
            int i;

            p_sys->i_fourcc = 0;

            psz = var_CreateGetString( p_demux, "v4l-chroma" );
            if( strlen( psz ) >= 4 )
            {
                vid_picture.palette = 0;
                int i_chroma = VLC_FOURCC( psz[0], psz[1], psz[2], psz[3] );

                /* Find out v4l chroma code */
                for( i = 0; v4lchroma_to_fourcc[i].i_v4l != 0; i++ )
                {
                    if( v4lchroma_to_fourcc[i].i_fourcc == i_chroma )
                    {
                        vid_picture.palette = v4lchroma_to_fourcc[i].i_v4l;
                        break;
                    }
                }
            }
            free( psz );

            if( vid_picture.palette &&
                !ioctl( i_fd, VIDIOCSPICT, &vid_picture ) )
            {
                p_sys->vid_picture = vid_picture;
            }
            else
            {
                /* Try to set the format to something easy to encode */
                vid_picture.palette = VIDEO_PALETTE_YUV420P;
                if( ioctl( i_fd, VIDIOCSPICT, &vid_picture ) == 0 )
                {
                    p_sys->vid_picture = vid_picture;
                }
                else
                {
                    vid_picture.palette = VIDEO_PALETTE_YUV422P;
                    if( ioctl( i_fd, VIDIOCSPICT, &vid_picture ) == 0 )
                    {
                        p_sys->vid_picture = vid_picture;
                    }
                }
            }

            /* Find out final format */
            for( i = 0; v4lchroma_to_fourcc[i].i_v4l != 0; i++ )
            {
                if( v4lchroma_to_fourcc[i].i_v4l == p_sys->vid_picture.palette)
                {
                    p_sys->i_fourcc = v4lchroma_to_fourcc[i].i_fourcc;
                    break;
                }
            }
        }
        else
        {
            msg_Err( p_demux, "ioctl VIDIOCGPICT failed" );
            goto vdev_failed;
        }
    }

    if( p_sys->b_mjpeg )
    {
        int i;

        p_sys->mjpeg_buffers.count = 8;
        p_sys->mjpeg_buffers.size = MJPEG_BUFFER_SIZE;

        if( ioctl( i_fd, MJPIOC_REQBUFS, &p_sys->mjpeg_buffers ) < 0 )
        {
            msg_Err( p_demux, "mmap unsupported" );
            goto vdev_failed;
        }

        p_sys->p_video_mmap = mmap( 0,
                p_sys->mjpeg_buffers.size * p_sys->mjpeg_buffers.count,
                PROT_READ | PROT_WRITE, MAP_SHARED, i_fd, 0 );
        if( p_sys->p_video_mmap == MAP_FAILED )
        {
            msg_Err( p_demux, "mmap failed" );
            goto vdev_failed;
        }

        p_sys->i_fourcc  = VLC_FOURCC( 'm','j','p','g' );
        p_sys->i_frame_pos = -1;

        /* queue up all the frames */
        for( i = 0; i < (int)p_sys->mjpeg_buffers.count; i++ )
        {
            if( ioctl( i_fd, MJPIOC_QBUF_CAPT, &i ) < 0 )
            {
                msg_Err( p_demux, "unable to queue frame" );
                goto vdev_failed;
            }
        }
    }
    else
    {
        /* Fill in picture_t fields */
        vout_InitPicture( VLC_OBJECT(p_demux), &p_sys->pic, p_sys->i_fourcc,
                          p_sys->i_width, p_sys->i_height, p_sys->i_width *
                          VOUT_ASPECT_FACTOR / p_sys->i_height );
        if( !p_sys->pic.i_planes )
        {
            msg_Err( p_demux, "unsupported chroma" );
            goto vdev_failed;
        }
        p_sys->i_video_frame_size = 0;
        for( i = 0; i < p_sys->pic.i_planes; i++ )
        {
            p_sys->i_video_frame_size += p_sys->pic.p[i].i_visible_lines *
              p_sys->pic.p[i].i_visible_pitch;
        }

        msg_Dbg( p_demux, "v4l device uses frame size: %i",
                 p_sys->i_video_frame_size );
        msg_Dbg( p_demux, "v4l device uses chroma: %4.4s",
                (char*)&p_sys->i_fourcc );

        /* Allocate mmap buffer */
        if( ioctl( i_fd, VIDIOCGMBUF, &p_sys->vid_mbuf ) < 0 )
        {
            msg_Err( p_demux, "mmap unsupported" );
            goto vdev_failed;
        }

        p_sys->p_video_mmap = mmap( 0, p_sys->vid_mbuf.size,
                                    PROT_READ|PROT_WRITE, MAP_SHARED,
                                    i_fd, 0 );
        if( p_sys->p_video_mmap == MAP_FAILED )
        {
            /* FIXME -> normal read */
            msg_Err( p_demux, "mmap failed" );
            goto vdev_failed;
        }

        /* init grabbing */
        p_sys->vid_mmap.frame  = 0;
        p_sys->vid_mmap.width  = p_sys->i_width;
        p_sys->vid_mmap.height = p_sys->i_height;
        p_sys->vid_mmap.format = p_sys->vid_picture.palette;
        if( ioctl( i_fd, VIDIOCMCAPTURE, &p_sys->vid_mmap ) < 0 )
        {
            msg_Warn( p_demux, "%4.4s refused", (char*)&p_sys->i_fourcc );
            msg_Err( p_demux, "chroma selection failed" );
            goto vdev_failed;
        }
    }
    return i_fd;

vdev_failed:

    if( i_fd >= 0 ) close( i_fd );
    return -1;
}

/*****************************************************************************
 * OpenAudioDev:
 *****************************************************************************/
static int OpenAudioDev( demux_t *p_demux, char *psz_device )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_fd, i_format;

    if( (i_fd = open( psz_device, O_RDONLY | O_NONBLOCK )) < 0 )
    {
        msg_Err( p_demux, "cannot open audio device (%m)" );
        goto adev_fail;
    }

    i_format = AFMT_S16_LE;
    if( ioctl( i_fd, SNDCTL_DSP_SETFMT, &i_format ) < 0
        || i_format != AFMT_S16_LE )
    {
        msg_Err( p_demux, "cannot set audio format (16b little endian) "
                 "(%m)" );
        goto adev_fail;
    }

    if( ioctl( i_fd, SNDCTL_DSP_STEREO,
               &p_sys->b_stereo ) < 0 )
    {
        msg_Err( p_demux, "cannot set audio channels count (%m)" );
        goto adev_fail;
    }

    if( ioctl( i_fd, SNDCTL_DSP_SPEED,
               &p_sys->i_sample_rate ) < 0 )
    {
        msg_Err( p_demux, "cannot set audio sample rate (%m)" );
        goto adev_fail;
    }

    msg_Dbg( p_demux, "opened adev=`%s' %s %dHz",
             psz_device, p_sys->b_stereo ? "stereo" : "mono",
             p_sys->i_sample_rate );

    p_sys->i_audio_max_frame_size = 6 * 1024;

    return i_fd;

 adev_fail:

    if( i_fd >= 0 ) close( i_fd );
    return -1;
}

/*****************************************************************************
 * GrabAudio: grab audio
 *****************************************************************************/
static block_t *GrabAudio( demux_t *p_demux )
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

    i_read = read( p_sys->fd_audio, p_block->p_buffer,
                   p_sys->i_audio_max_frame_size );

    if( i_read <= 0 ) return 0;

    p_block->i_buffer = i_read;
    p_sys->p_block_audio = 0;

    /* Correct the date because of kernel buffering */
    i_correct = i_read;
    if( ioctl( p_sys->fd_audio, SNDCTL_DSP_GETISPACE, &buf_info ) == 0 )
    {
        i_correct += buf_info.bytes;
    }

    p_block->i_pts = p_block->i_dts =
        mdate() - INT64_C(1000000) * (mtime_t)i_correct /
        2 / ( p_sys->b_stereo ? 2 : 1) / p_sys->i_sample_rate;

    return p_block;
}

/*****************************************************************************
 * GrabVideo:
 *****************************************************************************/
static uint8_t *GrabCapture( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_captured_frame = p_sys->i_frame_pos;

    p_sys->vid_mmap.frame = (p_sys->i_frame_pos + 1) % p_sys->vid_mbuf.frames;

    while( ioctl( p_sys->fd_video, VIDIOCMCAPTURE, &p_sys->vid_mmap ) < 0 )
    {
        if( errno != EAGAIN )
        {
            msg_Err( p_demux, "failed capturing new frame" );
            return NULL;
        }

        if( !vlc_object_alive (p_demux) )
        {
            return NULL;
        }

        msg_Dbg( p_demux, "grab failed, trying again" );
    }

    while( ioctl(p_sys->fd_video, VIDIOCSYNC, &p_sys->i_frame_pos) < 0 )
    {
        if( errno != EAGAIN && errno != EINTR )
        {
            msg_Err( p_demux, "failed syncing new frame" );
            return NULL;
        }
    }

    p_sys->i_frame_pos = p_sys->vid_mmap.frame;
    /* leave i_video_frame_size alone */
    return p_sys->p_video_mmap + p_sys->vid_mbuf.offsets[i_captured_frame];
}

static uint8_t *GrabMJPEG( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    struct mjpeg_sync sync;
    uint8_t *p_frame, *p_field, *p;
    uint16_t tag;
    uint32_t i_size;
    struct quicktime_mjpeg_app1 *p_app1 = NULL;

    /* re-queue the last frame we sync'd */
    if( p_sys->i_frame_pos != -1 )
    {
        while( ioctl( p_sys->fd_video, MJPIOC_QBUF_CAPT,
                                       &p_sys->i_frame_pos ) < 0 )
        {
            if( errno != EAGAIN && errno != EINTR )
            {
                msg_Err( p_demux, "failed capturing new frame" );
                return NULL;
            }
        }
    }

    /* sync on the next frame */
    while( ioctl( p_sys->fd_video, MJPIOC_SYNC, &sync ) < 0 )
    {
        if( errno != EAGAIN && errno != EINTR )
        {
            msg_Err( p_demux, "failed syncing new frame" );
            return NULL;
        }
    }

    p_sys->i_frame_pos = sync.frame;
    p_frame = p_sys->p_video_mmap + p_sys->mjpeg_buffers.size * sync.frame;

    /* p_frame now points to the data.  fix up the Quicktime APP1 marker */
    tag = 0xffd9;
    tag = hton16( tag );
    p_field = p_frame;

    /* look for EOI */
    p = memmem( p_field, sync.length, &tag, 2 );

    if( p )
    {
        p += 2; /* data immediately following EOI */
        /* UNALIGNED! */
        p_app1 = (struct quicktime_mjpeg_app1 *)(p_field + 6);

        i_size = ((uint32_t)(p - p_field));
        i_size = hton32( i_size );
        memcpy( &p_app1->i_field_size, &i_size, 4 );

        while( *p == 0xff && *(p+1) == 0xff )
            p++;

        i_size = ((uint32_t)(p - p_field));
        i_size = hton32( i_size );
        memcpy( &p_app1->i_padded_field_size, &i_size, 4 );
    }

    tag = 0xffd8;
    tag = hton16( tag );
    p_field = memmem( p, sync.length - (size_t)(p - p_frame), &tag, 2 );

    if( p_field )
    {
        i_size = (uint32_t)(p_field - p_frame);
        i_size = hton32( i_size );
        memcpy( &p_app1->i_next_field, &i_size, 4 );

        /* UNALIGNED! */
        p_app1 = (struct quicktime_mjpeg_app1 *)(p_field + 6);
        tag = 0xffd9;
        tag = hton16( tag );
        p = memmem( p_field, sync.length - (size_t)(p_field - p_frame),
                &tag, 2 );

        if( !p )
        {
            /* sometimes the second field doesn't have the EOI.  just put it
             * there
             */
            p = p_frame + sync.length;
            memcpy( p, &tag, 2 );
            sync.length += 2;
        }

        p += 2;
        i_size = (uint32_t)(p - p_field);
        i_size = hton32( i_size );
        memcpy( &p_app1->i_field_size, &i_size, 4 );
        i_size = (uint32_t)(sync.length - (uint32_t)(p_field - p_frame));
        i_size = hton32( i_size );
        memcpy( &p_app1->i_padded_field_size, &i_size, 4 );
    }

    p_sys->i_video_frame_size = sync.length;
    return p_frame;
}

static block_t *GrabVideo( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    uint8_t     *p_frame;
    block_t     *p_block;

    if( p_sys->f_fps >= 0.1 && p_sys->i_video_pts > 0 )
    {
        mtime_t i_dur = (mtime_t)((double)1000000 / (double)p_sys->f_fps);

        /* Did we wait long enough ? (frame rate reduction) */
        if( p_sys->i_video_pts + i_dur > mdate() ) return 0;
    }

    if( p_sys->b_mjpeg ) p_frame = GrabMJPEG( p_demux );
    else p_frame = GrabCapture( p_demux );

    if( !p_frame ) return 0;

    if( !( p_block = block_New( p_demux, p_sys->i_video_frame_size ) ) )
    {
        msg_Warn( p_demux, "cannot get block" );
        return 0;
    }

    memcpy( p_block->p_buffer, p_frame, p_sys->i_video_frame_size );
    p_sys->i_video_pts = p_block->i_pts = p_block->i_dts = mdate();

    return p_block;
}
