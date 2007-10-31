/*****************************************************************************
 * v4l2.c : Video4Linux2 input module for vlc
 *****************************************************************************
 * Copyright (C) 2002-2007 the VideoLAN team
 * $Id$
 *
 * Author: Benjamin Pracht <bigben at videolan dot org>
 *         Richard Hosking <richard at hovis dot net>
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

/*
 * TODO: No mjpeg support yet.
 * TODO: Tuner partial implementation.
 * TODO: Alsa input support?
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

/*****************************************************************************
 * Module descriptior
 *****************************************************************************/

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define DEV_TEXT N_("Device name")
#define DEV_LONGTEXT N_( \
    "Name of the device to use. " \
    "If you don't specify anything, /dev/video0 will be used.")
#define ADEV_TEXT N_("Audio device name")
#define ADEV_LONGTEXT N_( \
    "Name of the audio device to use. " \
    "If you don't specify anything, no audio device will be used.")
#define STANDARD_TEXT N_( "Standard" )
#define STANDARD_LONGTEXT N_( \
    "Video standard (Default, SECAM, PAL, or NTSC)." )
#define CHROMA_TEXT N_("Video input chroma format")
#define CHROMA_LONGTEXT N_( \
    "Force the Video4Linux2 video device to use a specific chroma format " \
    "(eg. I420, RV24, etc.)")
#define INPUT_TEXT N_( "Input" )
#define INPUT_LONGTEXT N_( \
    "Input of the card to use (Usually, 0 = tuner, " \
    "1 = composite, 2 = svideo)." )
#define IOMETHOD_TEXT N_( "IO Method" )
#define IOMETHOD_LONGTEXT N_( \
    "IO Method (READ, MMAP, USERPTR)." )
#define FPS_TEXT N_( "Framerate" )
#define FPS_LONGTEXT N_( "Framerate to capture, if applicable " \
    "(-1 for autodetect)." )
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

vlc_module_begin();
    set_shortname( _("Video4Linux2") );
    set_description( _("Video4Linux2 input") );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS );

    add_string( "v4l2-dev", "/dev/video0", 0, DEV_TEXT, DEV_LONGTEXT,
                VLC_FALSE );
    add_string( "v4l2-adev", "/dev/dsp", 0, DEV_TEXT, DEV_LONGTEXT,
                VLC_FALSE );
    add_integer( "v4l2-standard", 0, NULL, STANDARD_TEXT, STANDARD_LONGTEXT,
                VLC_FALSE );
        change_integer_list( i_standards_list, psz_standards_list_text, 0 );
    add_string( "v4l2-chroma", NULL, NULL, CHROMA_TEXT, CHROMA_LONGTEXT,
                VLC_TRUE );
    add_integer( "v4l2-input", 0, NULL, INPUT_TEXT, INPUT_LONGTEXT,
                VLC_TRUE );
    add_integer( "v4l2-io", IO_METHOD_MMAP, NULL, IOMETHOD_TEXT,
                 IOMETHOD_LONGTEXT, VLC_FALSE );
        change_integer_list( i_iomethod_list, psz_iomethod_list_text, 0 );
    add_float( "v4l2-fps", 0, NULL, FPS_TEXT, FPS_LONGTEXT, VLC_TRUE );
    add_bool( "v4l2-stereo", VLC_TRUE, NULL, STEREO_TEXT, STEREO_LONGTEXT,
                VLC_TRUE );
    add_integer( "v4l2-samplerate", 48000, NULL, SAMPLERATE_TEXT,
                SAMPLERATE_LONGTEXT, VLC_TRUE );
    add_integer( "v4l2-caching", DEFAULT_PTS_DELAY / 1000, NULL,
                CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );

    add_shortcut( "v4l2" );
    set_capability( "access_demux", 10 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Access: local prototypes
 *****************************************************************************/

static void ParseMRL( demux_t * );

static int Control( demux_t *, int, va_list );

static int Demux( demux_t * );
static block_t* GrabVideo( demux_t *p_demux );
static block_t* ProcessVideoFrame( demux_t *p_demux, uint8_t *p_frame );
static block_t* GrabAudio( demux_t *p_demux );

vlc_bool_t IsChromaSupported( demux_t *p_demux, unsigned int i_v4l2 );
unsigned int GetFourccFromString( char *psz_fourcc );

static int OpenVideoDev( demux_t *, char *psz_device );
static int OpenAudioDev( demux_t *, char *psz_device );
static vlc_bool_t ProbeVideoDev( demux_t *, char *psz_device );
static vlc_bool_t ProbeAudioDev( demux_t *, char *psz_device );

static struct
{
    unsigned int i_v4l2;
    int i_fourcc;
} v4l2chroma_to_fourcc[] =
{
    { V4L2_PIX_FMT_GREY, VLC_FOURCC( 'G', 'R', 'E', 'Y' ) },
    { V4L2_PIX_FMT_HI240, VLC_FOURCC( 'I', '2', '4', '0' ) },
    { V4L2_PIX_FMT_RGB565, VLC_FOURCC( 'R', 'V', '1', '6' ) },
    { V4L2_PIX_FMT_RGB555, VLC_FOURCC( 'R', 'V', '1', '5' ) },
    { V4L2_PIX_FMT_BGR24, VLC_FOURCC( 'R', 'V', '2', '4' ) },
    { V4L2_PIX_FMT_BGR32, VLC_FOURCC( 'R', 'V', '3', '2' ) },
    { V4L2_PIX_FMT_YUYV, VLC_FOURCC( 'Y', 'U', 'Y', '2' ) },
    { V4L2_PIX_FMT_YUYV, VLC_FOURCC( 'Y', 'U', 'Y', 'V' ) },
    { V4L2_PIX_FMT_UYVY, VLC_FOURCC( 'U', 'Y', 'V', 'Y' ) },
    { V4L2_PIX_FMT_Y41P, VLC_FOURCC( 'I', '4', '1', 'N' ) },
    { V4L2_PIX_FMT_YUV422P, VLC_FOURCC( 'I', '4', '2', '2' ) },
    { V4L2_PIX_FMT_YVU420, VLC_FOURCC( 'I', '4', '2', '0' ) },
    { V4L2_PIX_FMT_YUV411P, VLC_FOURCC( 'I', '4', '1', '1' ) },
    { V4L2_PIX_FMT_YUV410, VLC_FOURCC( 'I', '4', '1', '0' ) },
    { 0, 0 }
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

    picture_t pic;
    int i_video_frame_size;

    es_out_id_t *p_es_video;

    /* Audio */
    int i_sample_rate;
    vlc_bool_t b_stereo;
    int i_audio_max_frame_size;
    block_t *p_block_audio;
    es_out_id_t *p_es_audio;
};

/*****************************************************************************
 * Open: opens v4l2 device
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
    char *psz;

    /* Only when selected */
    if( *p_demux->psz_access == '\0' ) return VLC_EGENERIC;

    /* Set up p_demux */
    p_demux->pf_control = Control;
    p_demux->pf_demux = Demux;
    p_demux->info.i_update = 0;
    p_demux->info.i_title = 0;
    p_demux->info.i_seekpoint = 0;

    p_demux->p_sys = p_sys = calloc( 1, sizeof( demux_sys_t ) );
    if( p_sys == NULL ) return VLC_ENOMEM;

    p_sys->i_video_pts = -1;

    var_Create( p_demux, "v4l2-standard", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_demux, "v4l2-standard", &val );
    p_sys->i_selected_standard_id = i_standards_list[val.i_int];

    p_sys->i_selected_input = var_CreateGetInteger( p_demux, "v4l2-input" );

    p_sys->io = var_CreateGetInteger( p_demux, "v4l2-io" );

    var_Create( p_demux, "v4l2-fps", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT );
    var_Get( p_demux, "v4l2-fps", &val );
    p_sys->f_fps = val.f_float;

    var_Create( p_demux, "v4l2-samplerate", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_demux, "v4l2-samplerate", &val );
    p_sys->i_sample_rate = val.i_int;

    psz = var_CreateGetString( p_demux, "v4l2-chroma" );
    p_sys->i_fourcc = GetFourccFromString( psz );
    free( psz );

    var_Create( p_demux, "v4l2-stereo", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Get( p_demux, "v4l2-stereo", &val );
    p_sys->b_stereo = val.b_bool;

    p_sys->i_pts = var_CreateGetInteger( p_demux, "v4l2-caching" );

    p_sys->psz_device = p_sys->psz_vdev = p_sys->psz_adev = NULL;
    p_sys->i_fd_video = -1;
    p_sys->i_fd_audio = -1;

    p_sys->p_es_video = p_sys->p_es_audio = 0;
    p_sys->p_block_audio = 0;

    ParseMRL( p_demux );

    /* Find main device (video or audio) */
    if( p_sys->psz_device && *p_sys->psz_device )
    {
        msg_Dbg( p_demux, "main device='%s'", p_sys->psz_device );

        /* Try to open as video device */
        msg_Dbg( p_demux, "trying device '%s' as video", p_sys->psz_device );
        if( ProbeVideoDev( p_demux, p_sys->psz_device ) )
        {
            msg_Dbg( p_demux, "'%s' is a video device", p_sys->psz_device );
            /* Device was a video device */
            if( p_sys->psz_vdev ) free( p_sys->psz_vdev );
            p_sys->psz_vdev = p_sys->psz_device;
            p_sys->psz_device = NULL;
            p_sys->i_fd_video = OpenVideoDev( p_demux, p_sys->psz_vdev );
            if( p_sys->i_fd_video < 0 )
            {
                Close( p_this );
                return VLC_EGENERIC;
            }
        }
        else
        {
            /* Try to open as audio device */
            msg_Dbg( p_demux, "trying device '%s' as audio", p_sys->psz_device );
            if( ProbeAudioDev( p_demux, p_sys->psz_device ) )
            {
                msg_Dbg( p_demux, "'%s' is an audio device", p_sys->psz_device );
                /* Device was an audio device */
                if( p_sys->psz_adev ) free( p_sys->psz_adev );
                p_sys->psz_adev = p_sys->psz_device;
                p_sys->psz_device = NULL;
                p_sys->i_fd_audio = OpenAudioDev( p_demux, p_sys->psz_adev );
                if( p_sys->i_fd_audio < 0 )
                {
                    Close( p_this );
                    return VLC_EGENERIC;
                }
            }
        }
    }

    /* If no device opened, only continue if the access was forced */
    if( p_sys->i_fd_video < 0 && p_sys->i_fd_audio < 0 )
    {
        if( strcmp( p_demux->psz_access, "v4l2" ) )
        {
            Close( p_this );
            return VLC_EGENERIC;
        }
    }

    /* Find video device */
    if( p_sys->i_fd_video < 0 )
    {
        if( !p_sys->psz_vdev || !*p_sys->psz_vdev )
        {
            if( p_sys->psz_vdev ) free( p_sys->psz_vdev );
            p_sys->psz_vdev = var_CreateGetString( p_demux, "v4l2-dev" );;
        }

        if( p_sys->psz_vdev && *p_sys->psz_vdev && ProbeVideoDev( p_demux, p_sys->psz_vdev ) )
        {
            p_sys->i_fd_video = OpenVideoDev( p_demux, p_sys->psz_vdev );
        }
    }

    /* Find audio device */
    if( p_sys->i_fd_audio < 0 )
    {
        if( !p_sys->psz_adev || !*p_sys->psz_adev )
        {
            if( p_sys->psz_adev ) free( p_sys->psz_adev );
            p_sys->psz_adev = var_CreateGetString( p_demux, "v4l2-adev" );;
        }

        if( p_sys->psz_adev && *p_sys->psz_adev && ProbeAudioDev( p_demux, p_sys->psz_adev ) )
        {
            p_sys->i_fd_audio = OpenAudioDev( p_demux, p_sys->psz_adev );
        }
    }

    if( p_sys->i_fd_video < 0 && p_sys->i_fd_audio < 0 )
    {
        Close( p_this );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
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

                char* chroma = strndup( psz_parser, i_len );
                p_sys->i_fourcc = GetFourccFromString( chroma );
                free( chroma );

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
                                       &psz_parser, DEFAULT_PTS_DELAY / 1000 );
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
static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys   = p_demux->p_sys;
    struct v4l2_buffer buf;
    enum v4l2_buf_type buf_type;
    unsigned int i;

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
                buf.memory = V4L2_MEMORY_USERPTR;
                ioctl( p_sys->i_fd_video, VIDIOC_DQBUF, &buf ); /* ignore result */
            }

            buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if( ioctl( p_sys->i_fd_video, VIDIOC_STREAMOFF, &buf_type ) < 0 ) {
                msg_Err( p_demux, "VIDIOC_STREAMOFF failed" );
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
                    msg_Err( p_demux, "munmap failed" );
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

    /* Close */
    if( p_sys->i_fd_video >= 0 ) close( p_sys->i_fd_video );
    if( p_sys->i_fd_audio >= 0 ) close( p_sys->i_fd_audio );

    if( p_sys->p_block_audio ) block_Release( p_sys->p_block_audio );
    if( p_sys->psz_device ) free( p_sys->psz_device );
    if( p_sys->psz_vdev ) free( p_sys->psz_vdev );
    if( p_sys->psz_adev ) free( p_sys->psz_adev );
    if( p_sys->p_standards ) free( p_sys->p_standards );
    if( p_sys->p_inputs ) free( p_sys->p_inputs );
    if( p_sys->p_tuners ) free( p_sys->p_tuners );
    if( p_sys->p_codecs ) free( p_sys->p_codecs );

    free( p_sys );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
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
        if( read( p_sys->i_fd_video, p_sys->p_buffers[0].start, p_sys->p_buffers[0].length ) )
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

        p_block = ProcessVideoFrame( p_demux, (uint8_t*)p_sys->p_buffers[0].start );
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

        p_block = ProcessVideoFrame( p_demux, p_sys->p_buffers[buf.index].start );
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

        if( i >= p_sys->i_nbuffers ) {
            msg_Err( p_demux, "Failed capturing new frame as i>=nbuffers" );
            return 0;
        }

        p_block = ProcessVideoFrame( p_demux, (uint8_t*)buf.m.userptr );
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
static block_t* ProcessVideoFrame( demux_t *p_demux, uint8_t *p_frame )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t *p_block;

    if( !p_frame ) return 0;

    /* New block */
    if( !( p_block = block_New( p_demux, p_sys->i_video_frame_size ) ) )
    {
        msg_Warn( p_demux, "Cannot get new block" );
        return 0;
    }

    /* Copy frame */
    memcpy( p_block->p_buffer, p_frame, p_sys->i_video_frame_size );

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

    /* Copied from v4l.c */

    if( p_sys->p_block_audio ) p_block = p_sys->p_block_audio;
    else p_block = block_New( p_demux, p_sys->i_audio_max_frame_size );

    if( !p_block )
    {
        msg_Warn( p_demux, "cannot get block" );
        return 0;
    }

    p_sys->p_block_audio = p_block;

    i_read = read( p_sys->i_fd_audio, p_block->p_buffer,
                   p_sys->i_audio_max_frame_size );

    if( i_read <= 0 ) return 0;

    p_block->i_buffer = i_read;
    p_sys->p_block_audio = 0;

    /* Correct the date because of kernel buffering */
    i_correct = i_read;
    if( ioctl( p_sys->i_fd_audio, SNDCTL_DSP_GETISPACE, &buf_info ) == 0 )
    {
        i_correct += buf_info.bytes;
    }

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
 * GetFourccFromString: Returns the fourcc code from the given string
 *****************************************************************************/
unsigned int GetFourccFromString( char *psz_fourcc )
{
    if( strlen( psz_fourcc ) >= 4 )
    {
        int i_chroma = VLC_FOURCC( psz_fourcc[0], psz_fourcc[1], psz_fourcc[2], psz_fourcc[3] );

        /* Find out v4l2 chroma code */
        for( int i = 0; v4l2chroma_to_fourcc[i].i_fourcc != 0; i++ )
        {
            if( v4l2chroma_to_fourcc[i].i_fourcc == i_chroma )
            {
                return v4l2chroma_to_fourcc[i].i_fourcc;
            }
        }
    }

    return 0;
}

/*****************************************************************************
 * IsChromaSupported: returns true if the specified V4L2 chroma constant is
 * in the array of supported chromas returned by the driver
 *****************************************************************************/
vlc_bool_t IsChromaSupported( demux_t *p_demux, unsigned int i_chroma )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    for( int i_index = 0; i_index < p_sys->i_codec; i_index++ )
    {
        if( p_sys->p_codecs[i_index].pixelformat == i_chroma ) return VLC_TRUE;
    }

    return VLC_FALSE;
}

/*****************************************************************************
 * OpenVideoDev: open and set up the video device and probe for capabilities
 *****************************************************************************/
int OpenVideoDev( demux_t *p_demux, char *psz_device )
{
    int i_fd;
    demux_sys_t *p_sys = p_demux->p_sys;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format fmt;
    unsigned int i_min;
    enum v4l2_buf_type buf_type;

    if( ( i_fd = open( psz_device, O_RDWR ) ) < 0 )
    {
        msg_Err( p_demux, "cannot open device (%m)" );
        goto open_failed;
    }

    /* Select standard */

    if( p_sys->i_selected_standard_id != V4L2_STD_UNKNOWN )
    {
        if( ioctl( i_fd, VIDIOC_S_STD, &p_sys->i_selected_standard_id ) < 0 )
        {
           msg_Err( p_demux, "cannot set input (%m)" );
           goto open_failed;
        }
        msg_Dbg( p_demux, "Set standard" );
    }

    /* Select input */

    if( p_sys->i_selected_input > p_sys->i_input )
    {
        msg_Warn( p_demux, "invalid input. Using the default one" );
        p_sys->i_selected_input = 0;
    }

    if( ioctl( i_fd, VIDIOC_S_INPUT, &p_sys->i_selected_input ) < 0 )
    {
       msg_Err( p_demux, "cannot set input (%m)" );
       goto open_failed;
    }

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
    if( !p_sys->i_width && !p_sys->i_height ) {
        memset( &fmt, 0, sizeof(fmt) );
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        if ( ioctl( i_fd, VIDIOC_G_FMT, &fmt ) < 0 )
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

    fmt.fmt.pix.width = p_sys->i_width;
    fmt.fmt.pix.height = p_sys->i_height;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    /* Test and set Chroma */
    fmt.fmt.pix.pixelformat = 0;
    if( p_sys->i_fourcc )
    {
        /* User specified chroma */
        for( int i = 0; v4l2chroma_to_fourcc[i].i_v4l2 != 0; i++ )
        {
            if( v4l2chroma_to_fourcc[i].i_fourcc == p_sys->i_fourcc )
            {
                fmt.fmt.pix.pixelformat = v4l2chroma_to_fourcc[i].i_v4l2;
                break;
            }
        }
        /* Try and set user chroma */
        if( !IsChromaSupported( p_demux, fmt.fmt.pix.pixelformat ) || ( fmt.fmt.pix.pixelformat && ioctl( i_fd, VIDIOC_S_FMT, &fmt ) < 0 ) )
        {
            msg_Warn( p_demux, "Driver is unable to use specified chroma %4.4s. Using defaults.", (char *)&fmt.fmt.pix.pixelformat );
            fmt.fmt.pix.pixelformat = 0;
        }
    }

    /* If no user specified chroma, find best */
    if( !fmt.fmt.pix.pixelformat )
    {
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YVU420;
        if( !IsChromaSupported( p_demux, fmt.fmt.pix.pixelformat ) || ioctl( i_fd, VIDIOC_S_FMT, &fmt ) < 0 )
        {
            fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV422P;
            if( !IsChromaSupported( p_demux, fmt.fmt.pix.pixelformat ) || ioctl( i_fd, VIDIOC_S_FMT, &fmt ) < 0 )
            {
                fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
                if( !IsChromaSupported( p_demux, fmt.fmt.pix.pixelformat ) || ioctl( i_fd, VIDIOC_S_FMT, &fmt ) < 0 )
                {
                    msg_Err( p_demux, "Could not select any of the default chromas!" );
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

    /* Init vout Picture */
    vout_InitPicture( VLC_OBJECT(p_demux), &p_sys->pic, p_sys->i_fourcc,
        p_sys->i_width, p_sys->i_height, p_sys->i_width *
        VOUT_ASPECT_FACTOR / p_sys->i_height );
    if( !p_sys->pic.i_planes )
    {
        msg_Err( p_demux, "unsupported chroma" );
        goto open_failed;
    }
    p_sys->i_video_frame_size = 0;
    for( int i = 0; i < p_sys->pic.i_planes; i++ )
    {
         p_sys->i_video_frame_size += p_sys->pic.p[i].i_visible_lines * p_sys->pic.p[i].i_visible_pitch;
    }

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

            if( ioctl( i_fd, VIDIOC_QBUF, &buf ) < 0 ) {
                msg_Err( p_demux, "VIDIOC_QBUF failed" );
                goto open_failed;
            }
        }

        buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if( ioctl( i_fd, VIDIOC_STREAMON, &buf_type ) < 0 ) {
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

            if( ioctl( i_fd, VIDIOC_QBUF, &buf ) < 0 ) {
                msg_Err( p_demux, "VIDIOC_QBUF failed" );
                goto open_failed;
            }
        }

        buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if( ioctl( i_fd, VIDIOC_STREAMON, &buf_type ) < 0 ) {
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
    if( i_fd ) close( i_fd );
    return -1;

}

/*****************************************************************************
 * OpenAudioDev: open and set up the audio device and probe for capabilities
 *****************************************************************************/
int OpenAudioDev( demux_t *p_demux, char *psz_device )
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

    es_format_t fmt;
    es_format_Init( &fmt, AUDIO_ES, VLC_FOURCC('a','r','a','w') );

    fmt.audio.i_channels = p_sys->b_stereo ? 2 : 1;
    fmt.audio.i_rate = p_sys->i_sample_rate;
    fmt.audio.i_bitspersample = 16; /* FIXME ? */
    fmt.audio.i_blockalign = fmt.audio.i_channels * fmt.audio.i_bitspersample / 8;
    fmt.i_bitrate = fmt.audio.i_channels * fmt.audio.i_rate * fmt.audio.i_bitspersample;

    msg_Dbg( p_demux, "new audio es %d channels %dHz",
      fmt.audio.i_channels, fmt.audio.i_rate );

    p_sys->p_es_audio = es_out_Add( p_demux->out, &fmt );

    return i_fd;

 adev_fail:

    if( i_fd >= 0 ) close( i_fd );
    return -1;

}

/*****************************************************************************
 * ProbeVideoDev: probe video for capabilities
 *****************************************************************************/
vlc_bool_t ProbeVideoDev( demux_t *p_demux, char *psz_device )
{
    int i_index;
    int i_standard;

    int i_fd;
    demux_sys_t *p_sys = p_demux->p_sys;

    if( ( i_fd = open( psz_device, O_RDWR ) ) < 0 )
    {
        msg_Err( p_demux, "cannot open video device (%m)" );
        goto open_failed;
    }

    /* Get device capabilites */

    if( ioctl( i_fd, VIDIOC_QUERYCAP, &p_sys->dev_cap ) < 0 )
    {
        msg_Err( p_demux, "cannot get video capabilities (%m)" );
        goto open_failed;
    }

    msg_Dbg( p_demux, "V4L2 device: %s using driver: %s (version: %u.%u.%u) on %s",
                            p_sys->dev_cap.card,
                            p_sys->dev_cap.driver,
                            (p_sys->dev_cap.version >> 16) & 0xFF,
                            (p_sys->dev_cap.version >> 8) & 0xFF,
                            p_sys->dev_cap.version & 0xFF,
                            p_sys->dev_cap.bus_info );

    msg_Dbg( p_demux, "the device has the capabilities: (%c) Video Capure, "
                                                       "(%c) Audio, "
                                                       "(%c) Tuner",
             ( p_sys->dev_cap.capabilities & V4L2_CAP_VIDEO_CAPTURE  ? 'X':' '),
             ( p_sys->dev_cap.capabilities & V4L2_CAP_AUDIO  ? 'X':' '),
             ( p_sys->dev_cap.capabilities & V4L2_CAP_TUNER  ? 'X':' ') );

    msg_Dbg( p_demux, "supported I/O methods are: (%c) Read/Write, "
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
        while( ioctl( i_fd, VIDIOC_S_INPUT, &p_sys->i_input ) >= 0 )
        {
            p_sys->i_input++;
        }

        p_sys->p_inputs = calloc( 1, p_sys->i_input * sizeof( struct v4l2_input ) );
        if( !p_sys->p_inputs ) goto open_failed;

        for( i_index = 0; i_index < p_sys->i_input; i_index++ )
        {
            p_sys->p_inputs[i_index].index = i_index;

            if( ioctl( i_fd, VIDIOC_ENUMINPUT, &p_sys->p_inputs[i_index] ) )
            {
                msg_Err( p_demux, "cannot get video input characteristics (%m)" );
                goto open_failed;
            }
            msg_Dbg( p_demux, "video input %i (%s) has type: %s",
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
                msg_Err( p_demux, "cannot get video input standards (%m)" );
                goto open_failed;
            }
            msg_Dbg( p_demux, "video standard %i is: %s",
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
                msg_Err( p_demux, "cannot get audio input characteristics (%m)" );
                goto open_failed;
            }

            msg_Dbg( p_demux, "audio device %i (%s) is %s",
                                p_sys->i_audio,
                                p_sys->p_audios[p_sys->i_audio].name,
                                p_sys->p_audios[p_sys->i_audio].capability &
                                                    V4L2_AUDCAP_STEREO ?
                                        "Stereo" : "Mono" );

            p_sys->i_audio++;
        }
    }

    if( p_sys->dev_cap.capabilities & V4L2_CAP_TUNER )
    {
        struct v4l2_tuner tuner;

        memset( &tuner, 0, sizeof(tuner) );
        while( ioctl( i_fd, VIDIOC_S_TUNER, &tuner ) >= 0 )
        {
            p_sys->i_tuner++;
            tuner.index = p_sys->i_tuner;
        }

        p_sys->p_tuners = calloc( 1, p_sys->i_tuner * sizeof( struct v4l2_tuner ) );
        if( !p_sys->p_tuners ) goto open_failed;

        for( i_index = 0; i_index < p_sys->i_tuner; i_index++ )
        {
            p_sys->p_tuners[i_index].index = i_index;

            if( ioctl( i_fd, VIDIOC_G_TUNER, &p_sys->p_tuners[i_index] ) )
            {
                msg_Err( p_demux, "cannot get tuner characteristics (%m)" );
                goto open_failed;
            }
            msg_Dbg( p_demux, "tuner %i (%s) has type: %s, "
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
                msg_Err( p_demux, "cannot get codec description (%m)" );
                goto open_failed;
            }

            msg_Dbg( p_demux, "device supports Codec %s",
                                p_sys->p_codecs[i_index].description );
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
vlc_bool_t ProbeAudioDev( demux_t *p_demux, char *psz_device )
{
    int i_fd = 0;
    int i_caps;

    if( ( i_fd = open( psz_device, O_RDONLY | O_NONBLOCK ) ) < 0 )
    {
        msg_Err( p_demux, "cannot open audio device (%m)" );
        goto open_failed;
    }

    /* this will fail if the device is video */
    if( ioctl( i_fd, SNDCTL_DSP_GETCAPS, &i_caps ) < 0 )
    {
        msg_Err( p_demux, "cannot get audio caps (%m)" );
        goto open_failed;
    }

    if( i_fd ) close( i_fd );
    return VLC_TRUE;

open_failed:
    if( i_fd ) close( i_fd );
    return VLC_FALSE;

}

