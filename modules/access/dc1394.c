/*****************************************************************************
 * dc1394.c: firewire input module
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 *
 * Authors: Xant Majere <xant@xant.net>
 *
 *****************************************************************************
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
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
#include <vlc_vout.h>
#include <vlc_demux.h>


#ifdef HAVE_FCNTL_H
#   include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( WIN32 ) && !defined( UNDER_CE )
#   include <io.h>
#endif

#include <sys/ioctl.h>
#include <sys/soundcard.h>

#include <libraw1394/raw1394.h>
#include <libdc1394/dc1394_control.h>

#define MAX_IEEE1394_HOSTS 32
#define MAX_CAMERA_NODES 32

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );
static void OpenAudioDev( demux_t *p_demux );
static inline void CloseAudioDev( demux_t *p_demux );

vlc_module_begin();
    set_description( N_("dc1394 input") );
    set_capability( "access_demux", 10 );
    add_shortcut( "dc1394" );
    set_callbacks( Open, Close );
vlc_module_end();

typedef struct __dc_camera
{
    int port;
    nodeid_t node;
    u_int64_t uid;
} dc_camera;

typedef struct demux_sys_t
{
    dc1394_cameracapture camera;
    picture_t            pic;
    int                  dma_capture;
#define DMA_OFF 0
#define DMA_ON 1
    int                 num_ports;
    int                 num_cameras;
    int                 selected_camera;
    u_int64_t           selected_uid;

    dc_camera           *camera_nodes;
    dc1394_camerainfo   camera_info;
    dc1394_miscinfo     misc_info;
    raw1394handle_t     fd_video;
    quadlet_t           supported_framerates;

    int                 width;
    int                 height;
    int                 frame_size;
    int                 frame_rate;
    unsigned int        brightness;
    unsigned int        focus;
    char                *dma_device;
    es_out_id_t         *p_es_video;

    /* audio stuff */
    int                 i_sample_rate;
    int                 channels;
    int                 i_audio_max_frame_size;
    int                 fd_audio;
    char                *audio_device;
#define NO_ROTATION 0
#define ROTATION_LEFT 1
#define ROTATION_RIGHT 2
    es_out_id_t         *p_es_audio;
} dc1394_sys;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux );
static int Control( demux_t *, int, va_list );
static block_t *GrabVideo( demux_t *p_demux );
static block_t *GrabAudio( demux_t *p_demux );
static int process_options( demux_t *p_demux);

/*****************************************************************************
 * ScanCameras
 *****************************************************************************/
static void ScanCameras( dc1394_sys *sys, demux_t *p_demux )
{
    struct raw1394_portinfo portinfo[MAX_IEEE1394_HOSTS];
    raw1394handle_t tempFd;
    dc1394_camerainfo  info;
    dc_camera *node_list = NULL;
    nodeid_t  *nodes = NULL;
    int num_ports = 0;
    int num_cameras = 0;
    int nodecount;
    int i, n;

    memset( &portinfo, 0, sizeof(portinfo) );

    msg_Dbg( p_demux, "Scanning for ieee1394 ports ..." );

    tempFd = raw1394_new_handle();
    if( !tempFd )
        return VLC_EGENERIC;
    raw1394_get_port_info( tempFd, portinfo, MAX_IEEE1394_HOSTS);
    raw1394_destroy_handle( tempFd );

    for( i=0; i < MAX_IEEE1394_HOSTS; i++ )
    {
        /* check if port exists and has at least one node*/
        if( !portinfo[i].nodes )
            continue;
        nodes = NULL;
        nodecount = 0;
        tempFd = dc1394_create_handle( i );

        /* skip this port if we can't obtain a valid handle */
        if( !tempFd )
            continue;
        msg_Dbg( p_demux, "Found ieee1394 port %d (%s) ... "
                          "checking for camera nodes",
                          i, portinfo[i].name );
        num_ports++;

        nodes = dc1394_get_camera_nodes( tempFd, &nodecount, 0 );
        if( nodecount )
        {
            msg_Dbg( p_demux, "Found %d dc1394 cameras on port %d (%s)",
                     nodecount, i, portinfo[i].name );

            if( node_list )
                node_list = realloc( node_list, sizeof(dc_camera) * (num_cameras+nodecount) );
            else
                node_list = malloc( sizeof(dc_camera) * nodecount);

            for( n = 0; n < nodecount; n++ )
            {
                int result = 0;

                result = dc1394_get_camera_info( tempFd, nodes[n], &info );
                if( result == DC1394_SUCCESS )
                {
                    node_list[num_cameras+n].uid = info.euid_64;
                }
                node_list[num_cameras+n].node = nodes[n];
                node_list[num_cameras+n].port = i;
            }
            num_cameras += nodecount;
        }
        else
            msg_Dbg( p_demux, "no cameras found  on port %d (%s)",
                     i, portinfo[i].name );

        if( tempFd )
            dc1394_destroy_handle( tempFd );
    }
    sys->num_ports = num_ports;
    sys->num_cameras = num_cameras;
    sys->camera_nodes = node_list;
}

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    es_format_t fmt;
    int i;
    int i_width;
    int i_height;
    int i_aspect;
    int result = 0;

    /* Set up p_demux */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->info.i_update = 0;
    p_demux->info.i_title = 0;
    p_demux->info.i_seekpoint = 0;

    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;
    memset( p_sys, 0, sizeof( demux_sys_t ) );
    memset( &fmt, 0, sizeof( es_format_t ) );

    /* DEFAULTS */
    p_sys->frame_size = MODE_640x480_YUV422;
    p_sys->width      = 640;
    p_sys->height     = 480;
    p_sys->frame_rate = FRAMERATE_30;
    p_sys->brightness = 200;
    p_sys->focus      = 0;
    p_sys->dma_capture = DMA_ON; /* defaults to VIDEO1394 capture mode */
    p_sys->fd_audio   = -1;
    p_sys->fd_video   = NULL;
    p_sys->camera_nodes = NULL;
    p_sys->selected_camera = 0;
    p_sys->dma_device = NULL;
    p_sys->selected_uid = 0;

    /* PROCESS INPUT OPTIONS */
    if( process_options(p_demux) != VLC_SUCCESS )
    {
        msg_Err( p_demux, "Bad MRL, please check the option line "
                          "(MRL was: %s)",
                          p_demux->psz_path );
        free( p_sys );
        p_demux->p_sys = NULL;
        return VLC_EGENERIC;
    }

    msg_Dbg( p_demux, "Selected camera %d", p_sys->selected_camera );
    msg_Dbg( p_demux, "Selected uid 0x%llx", p_sys->selected_uid );

    ScanCameras( p_sys, p_demux );
    if( !p_sys->camera_nodes )
    {
        msg_Err( p_demux, "No camera found !!" );
        free( p_sys );
        p_demux->p_sys = NULL;
        return VLC_EGENERIC;
    }

    if( p_sys->selected_uid )
    {
        int found = 0;
        for( i=0; i < p_sys->num_cameras; i++ )
        {
            if( p_sys->camera_nodes[i].uid == p_sys->selected_uid )
            {
                p_sys->selected_camera = i;
                found++;
                break;
            }
        }
        if( !found )
        {
            msg_Err( p_demux, "Can't find camera with uid : 0x%llx.",
                     p_sys->selected_uid );
            Close( p_this );
            return VLC_EGENERIC;
        }
    }
    else if( p_sys->selected_camera >= p_sys->num_cameras )
    {
        msg_Err( p_demux, "there are not this many cameras. (%d/%d)",
                          p_sys->selected_camera, p_sys->num_cameras );
        Close( p_this );
        return VLC_EGENERIC;
    }

    p_sys->fd_video = dc1394_create_handle(
                        p_sys->camera_nodes[p_sys->selected_camera].port );
    if( (int)p_sys->fd_video < 0 )
    {
        msg_Err( p_demux, "Can't init dc1394 handle" );
        Close( p_this );
        return VLC_EGENERIC;
    }

    /* get camera info */
    result = dc1394_get_camera_info( p_sys->fd_video,
                        p_sys->camera_nodes[p_sys->selected_camera].node,
                        &p_sys->camera_info );
    if( result != DC1394_SUCCESS )
    {
        msg_Err( p_demux ,"unable to get camera info" );
        Close( p_this );
        return VLC_EGENERIC;
    }

    dc1394_print_camera_info( &p_sys->camera_info );
    result = dc1394_get_camera_misc_info( p_sys->fd_video,
                        p_sys->camera_nodes[p_sys->selected_camera].node,
                        &p_sys->misc_info );
    if( result != DC1394_SUCCESS )
    {
        msg_Err( p_demux, "unable to get camera misc info" );
        Close( p_this );
        return VLC_EGENERIC;
    }

    /* init camera and set some video options  */
    result = dc1394_init_camera( p_sys->camera_info.handle,
                                 p_sys->camera_info.id );
    if( result != DC1394_SUCCESS )
    {
        msg_Err( p_demux, "unable to get init dc1394 camera" );
        Close( p_this );
        return VLC_EGENERIC;
    }

    if( p_sys->focus )
    {
        result = dc1394_set_focus( p_sys->camera_info.handle,
                        p_sys->camera_nodes[p_sys->selected_camera].node,
                        p_sys->focus );
        if( result != DC1394_SUCCESS )
        {
            msg_Err( p_demux, "unable to set initial focus to %u",
                     p_sys->focus );
        }
        msg_Dbg( p_demux, "Initial focus set to %u", p_sys->focus );
    }

    result = dc1394_set_brightness( p_sys->camera_info.handle,
                        p_sys->camera_nodes[p_sys->selected_camera].node,
                        p_sys->brightness );
    if( result != DC1394_SUCCESS )
    {
        msg_Err( p_demux, "unable to set init brightness to %d",
                 p_sys->brightness);
        Close( p_this );
        return VLC_EGENERIC;
    }

    result = dc1394_set_video_framerate( p_sys->camera_info.handle,
                        p_sys->camera_nodes[p_sys->selected_camera].node,
                        p_sys->frame_rate );
    if( result != DC1394_SUCCESS )
    {
        msg_Err( p_demux, "unable to set framerate to %d",
                 p_sys->frame_rate );
        Close( p_this );
        return VLC_EGENERIC;
    }
    p_sys->misc_info.framerate = p_sys->frame_rate;

    result = dc1394_set_video_format( p_sys->camera_info.handle,
                        p_sys->camera_nodes[p_sys->selected_camera].node,
                        FORMAT_VGA_NONCOMPRESSED );
    if( result != DC1394_SUCCESS )
    {
        msg_Err( p_demux, "unable to set video format to VGA_NONCOMPRESSED" );
        Close( p_this );
        return VLC_EGENERIC;
    }
    p_sys->misc_info.format = FORMAT_VGA_NONCOMPRESSED;

    result = dc1394_set_video_mode( p_sys->camera_info.handle,
                        p_sys->camera_nodes[p_sys->selected_camera].node,
                        p_sys->frame_size );
    if( result != DC1394_SUCCESS )
    {
        msg_Err( p_demux, "unable to set video mode" );
        Close( p_this );
        return VLC_EGENERIC;
    }
    p_sys->misc_info.mode = p_sys->frame_size;

    /* reprobe everything */
    result = dc1394_get_camera_info( p_sys->camera_info.handle,
                                     p_sys->camera_info.id,
                                     &p_sys->camera_info );
    if( result != DC1394_SUCCESS )
    {
        msg_Err( p_demux, "Could not get camera basic information!" );
        Close( p_this );
        return VLC_EGENERIC;
    }

    result = dc1394_get_camera_misc_info( p_sys->camera_info.handle,
                                          p_sys->camera_info.id,
                                          &p_sys->misc_info );
    if( result != DC1394_SUCCESS )
    {
        msg_Err( p_demux, "Could not get camera misc information!" );
        Close( p_this );
        return VLC_EGENERIC;
    }

    /* set iso_channel */
    result = dc1394_set_iso_channel_and_speed( p_sys->camera_info.handle,
                                               p_sys->camera_info.id,
                                               p_sys->selected_camera,
                                               SPEED_400 );
    if( result != DC1394_SUCCESS )
    {
        msg_Err( p_demux, "Could not set iso channel!" );
        Close( p_this );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_demux, "Using ISO channel %d", p_sys->misc_info.iso_channel );
    p_sys->misc_info.iso_channel = p_sys->selected_camera;

    /* and setup capture */
    if( p_sys->dma_capture )
    {
        result = dc1394_dma_setup_capture( p_sys->camera_info.handle,
                        p_sys->camera_info.id,
                        p_sys->misc_info.iso_channel,
                        p_sys->misc_info.format,
                        p_sys->misc_info.mode,
                        SPEED_400,
                        p_sys->misc_info.framerate,
                        10, 0,
                        p_sys->dma_device,
                        &p_sys->camera );
        if( result != DC1394_SUCCESS )
        {
            msg_Err( p_demux ,"unable to setup camera" );
            Close( p_this );
            return VLC_EGENERIC;
        }
    }
    else
    {
        result = dc1394_setup_capture( p_sys->camera_info.handle,
                    p_sys->camera_info.id,
                    p_sys->misc_info.iso_channel,
                    p_sys->misc_info.format,
                    p_sys->misc_info.mode,
                    SPEED_400,
                    p_sys->misc_info.framerate,
                    &p_sys->camera );
        if( result != DC1394_SUCCESS)
        {
            msg_Err( p_demux ,"unable to setup camera" );
            Close( p_this );
            return VLC_EGENERIC;
        }
    }

    /* TODO - UYV444 chroma converter is missing, when it will be available
     * fourcc will become variable (and not just a fixed value for UYVY)
     */
    i_width = p_sys->camera.frame_width;
    i_height = p_sys->camera.frame_height;

    i_aspect = vout_InitPicture( VLC_OBJECT(p_demux), &p_sys->pic,
                    VLC_FOURCC('U', 'Y', 'V', 'Y'),
                    i_width, i_height,
                    i_width * VOUT_ASPECT_FACTOR / i_height );

    es_format_Init( &fmt, VIDEO_ES, VLC_FOURCC('U', 'Y', 'V', 'Y') );

    fmt.video.i_width = i_width;
    fmt.video.i_height = i_height;

    msg_Dbg( p_demux, "added new video es %4.4s %dx%d",
             (char*)&fmt.i_codec, fmt.video.i_width, fmt.video.i_height );

    p_sys->p_es_video = es_out_Add( p_demux->out, &fmt );

    if( p_sys->audio_device )
    {
        OpenAudioDev( p_demux );
        if( p_sys->fd_audio >= 0 )
        {
            es_format_t fmt;
            es_format_Init( &fmt, AUDIO_ES, VLC_FOURCC('a','r','a','w') );

            fmt.audio.i_channels = p_sys->channels ? p_sys->channels : 1;
            fmt.audio.i_rate = p_sys->i_sample_rate;
            fmt.audio.i_bitspersample = 16; /* FIXME: hmm, ?? */
            fmt.audio.i_blockalign = fmt.audio.i_channels *
                                     fmt.audio.i_bitspersample / 8;
            fmt.i_bitrate = fmt.audio.i_channels * fmt.audio.i_rate *
                            fmt.audio.i_bitspersample;

            msg_Dbg( p_demux, "new audio es %d channels %dHz",
            fmt.audio.i_channels, fmt.audio.i_rate );

            p_sys->p_es_audio = es_out_Add( p_demux->out, &fmt );
        }
    }

    /* have the camera start sending us data */
    result = dc1394_start_iso_transmission( p_sys->camera_info.handle,
                                            p_sys->camera_info.id );
    if( result != DC1394_SUCCESS )
    {
        msg_Err( p_demux, "unable to start camera iso transmission" );
        if( p_sys->dma_capture )
        {
            dc1394_dma_release_camera( p_sys->fd_video, &p_sys->camera );
        }
        else
        {
            dc1394_release_camera( p_sys->fd_video, &p_sys->camera );
        }
        Close( p_this );
        return VLC_EGENERIC;
    }
    p_sys->misc_info.is_iso_on = DC1394_TRUE;
    return VLC_SUCCESS;
}

static void OpenAudioDev( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    char *psz_device = p_sys->audio_device;
    int i_format = AFMT_S16_LE;
    int result;

    p_sys->fd_audio = open( psz_device, O_RDONLY | O_NONBLOCK );
    if( p_sys->fd_audio  < 0 )
    {
        msg_Err( p_demux, "cannot open audio device (%s)", psz_device );
        CloseAudioDev( p_demux );
    }

    if( !p_sys->i_sample_rate )
        p_sys->i_sample_rate = 44100;

    result = ioctl( p_sys->fd_audio, SNDCTL_DSP_SETFMT, &i_format );
    if( (result  < 0) || (i_format != AFMT_S16_LE) )
    {
        msg_Err( p_demux, "cannot set audio format (16b little endian) "
                          "(%d)", i_format );
        CloseAudioDev( p_demux );
    }

    result = ioctl( p_sys->fd_audio, SNDCTL_DSP_CHANNELS, &p_sys->channels );
    if( result < 0 )
    {
        msg_Err( p_demux, "cannot set audio channels count (%d)",
                 p_sys->channels );
        CloseAudioDev( p_demux );
    }

    result = ioctl( p_sys->fd_audio, SNDCTL_DSP_SPEED, &p_sys->i_sample_rate );
    if( result < 0 )
    {
        msg_Err( p_demux, "cannot set audio sample rate (%s)", p_sys->i_sample_rate );
        CloseAudioDev( p_demux );
    }

    msg_Dbg( p_demux, "opened adev=`%s' %s %dHz",
             psz_device,
             (p_sys->channels > 1) ? "stereo" : "mono",
             p_sys->i_sample_rate );

    p_sys->i_audio_max_frame_size = 32 * 1024;
}

static inline void CloseAudioDev( demux_t *p_demux )
{
    demux_sys_t *p_sys = NULL;

    if( p_demux )
    {
        p_sys = p_demux->p_sys;
        if( p_sys->fd_audio >= 0 )
            close( p_sys->fd_audio );
    }
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;
    int result = 0;

    /* Stop data transmission */
    result = dc1394_stop_iso_transmission( p_sys->fd_video,
                                           p_sys->camera.node );
    if( result != DC1394_SUCCESS )
    {
        msg_Err( p_demux, "couldn't stop the camera" );
    }

    /* Close camera */
    if( p_sys->dma_capture )
    {
        dc1394_dma_unlisten( p_sys->fd_video, &p_sys->camera );
        dc1394_dma_release_camera( p_sys->fd_video, &p_sys->camera );
    }
    else
    {
        dc1394_release_camera( p_sys->fd_video, &p_sys->camera );
    }

    if( p_sys->fd_video )
        dc1394_destroy_handle( p_sys->fd_video );
    CloseAudioDev( p_demux );

    free( p_sys->camera_nodes );
    free( p_sys->audio_device );

    free( p_sys );
}

static void MovePixelUYVY( void *src, int spos, void *dst, int dpos )
{
    char u,v,y;
    u_char  *sc;
    u_char  *dc;

    sc = (u_char *)src + (spos*2);
    if( spos % 2 )
    {
        v = sc[0];
        y = sc[1];
        u = *(sc -2);
    }
    else
    {
        u = sc[0];
        y = sc[1];
        v = sc[2];
    }
    dc = (u_char *)dst+(dpos*2);
    if( dpos % 2 )
    {
        dc[0] = v;
        dc[1] = y;
    }
    else
    {
        dc[0] = u;
        dc[1] = y;
    }
}

/*****************************************************************************
 * Demux:
 *****************************************************************************/
static block_t *GrabVideo( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t     *p_block = NULL;
    int result = 0;

    if( p_sys->dma_capture )
    {
        result = dc1394_dma_single_capture( &p_sys->camera );
        if( result != DC1394_SUCCESS )
        {
            msg_Err( p_demux, "unable to capture a frame" );
            return NULL;
        }
    }
    else
    {
        result = dc1394_single_capture( p_sys->camera_info.handle,
                                        &p_sys->camera );
        if( result != DC1394_SUCCESS )
        {
            msg_Err( p_demux, "unable to capture a frame" );
            return NULL;
        }
    }

    p_block = block_New( p_demux, p_sys->camera.frame_width *
                                  p_sys->camera.frame_height * 2 );
    if( !p_block )
    {
        msg_Err( p_demux, "cannot get block" );
        return NULL;
    }

    if( !p_sys->camera.capture_buffer )
    {
        msg_Err (p_demux, "caputer buffer empty");
        block_Release( p_block );
        return NULL;
    }

    memcpy( p_block->p_buffer, (const char *)p_sys->camera.capture_buffer,
            p_sys->camera.frame_width * p_sys->camera.frame_height * 2 );

    p_block->i_pts = p_block->i_dts = mdate();
    if( p_sys->dma_capture )
        dc1394_dma_done_with_buffer( &p_sys->camera );
    return p_block;
}

static block_t *GrabAudio( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    struct audio_buf_info buf_info;
    block_t *p_block = NULL;
    int i_read = 0;
    int i_correct = 0;
    int result = 0;

    p_block = block_New( p_demux, p_sys->i_audio_max_frame_size );
    if( !p_block )
    {
        msg_Warn( p_demux, "cannot get buffer" );
        return NULL;
    }

    i_read = read( p_sys->fd_audio, p_block->p_buffer,
                   p_sys->i_audio_max_frame_size );

    if( i_read <= 0 )
        return NULL;

    p_block->i_buffer = i_read;

    /* Correct the date because of kernel buffering */
    i_correct = i_read;
    result = ioctl( p_sys->fd_audio, SNDCTL_DSP_GETISPACE, &buf_info );
    if( result == 0 )
        i_correct += buf_info.bytes;

    p_block->i_pts = p_block->i_dts =
                        mdate() - INT64_C(1000000) * (mtime_t)i_correct /
                        2 / p_sys->channels / p_sys->i_sample_rate;
    return p_block;
}

static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t *p_blocka = NULL;
    block_t *p_blockv = NULL;

    /* Try grabbing audio frames first */
    if( p_sys->fd_audio > 0 )
        p_blocka = GrabAudio( p_demux );

    /* Try grabbing video frame */
    if( (int)p_sys->fd_video > 0 )
        p_blockv = GrabVideo( p_demux );

    if( !p_blocka && !p_blockv )
    {
        /* Sleep so we do not consume all the cpu, 10ms seems
         * like a good value (100fps)
         */
        msleep( 10000 );
        return 1;
    }

    if( p_blocka )
    {
        es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_blocka->i_pts );
        es_out_Send( p_demux->out, p_sys->p_es_audio, p_blocka );
    }

    if( p_blockv )
    {
        es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_blockv->i_pts );
        es_out_Send( p_demux->out, p_sys->p_es_video, p_blockv );
    }
    return 1;
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
            *pi64 = (int64_t)DEFAULT_PTS_DELAY;
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

static int process_options( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    char *psz_dup;
    char *psz_parser;
    char *token = NULL;
    char *state = NULL;
    float rate_f;

    if( strncmp(p_demux->psz_access, "dc1394", 6) != 0 )
        return VLC_EGENERIC;

    psz_dup = strdup( p_demux->psz_path );
    psz_parser = psz_dup;
    for( token = strtok_r( psz_parser,":",&state); token;
         token = strtok_r( NULL, ":", &state ) )
    {
        if( strncmp( token, "size=", strlen("size=") ) == 0 )
        {
            token += strlen("size=");
            if( strncmp( token, "160x120", 7 ) == 0 )
            {
                /* TODO - UYV444 chroma converter is needed ...
                    * video size of 160x120 is temporarily disabled
                    */
                msg_Err( p_demux,
                    "video size of 160x120 is actually disabled for lack of chroma "
                    "support. It will relased ASAP, until then try an higher size "
                    "(320x240 and 640x480 are fully supported)" );
                free( psz_dup );
                return VLC_EGENERIC;
#if 0
                p_sys->frame_size = MODE_160x120_YUV444;
                p_sys->width = 160;
                p_sys->height = 120;
#endif
            }
            else if( strncmp( token, "320x240", 7 ) == 0 )
            {
                p_sys->frame_size = MODE_320x240_YUV422;
                p_sys->width = 320;
                p_sys->height = 240;
            }
            else if( strncmp( token, "640x480", 7 ) == 0 )
            {
                p_sys->frame_size = MODE_640x480_YUV422;
                p_sys->width = 640;
                p_sys->height = 480;
            }
            else
            {
                msg_Err( p_demux,
                    "This program currently suppots frame sizes of"
                    " 160x120, 320x240, and 640x480. "
                    "Please specify one of them. You have specified %s.",
                    token );
                free( psz_dup );
                return VLC_EGENERIC;
            }
            msg_Dbg( p_demux, "Requested video size : %s",token );
        }
        else if( strncmp( token, "fps=", strlen( "fps=" ) ) == 0 )
        {
            token += strlen("fps=");
            sscanf( token, "%g", &rate_f );
            if( rate_f == 1.875 )
                p_sys->frame_rate = FRAMERATE_1_875;
            else if( rate_f == 3.75 )
                p_sys->frame_rate = FRAMERATE_3_75;
            else if( rate_f == 7.5 )
                p_sys->frame_rate = FRAMERATE_7_5;
            else if( rate_f == 15 )
                p_sys->frame_rate = FRAMERATE_15;
            else if( rate_f == 30 )
                p_sys->frame_rate = FRAMERATE_30;
            else if( rate_f == 60 )
                p_sys->frame_rate = FRAMERATE_60;
            else
            {
                msg_Err( p_demux ,
                    "This program supports framerates of"
                    " 1.875, 3.75, 7.5, 15, 30, 60. "
                    "Please specify one of them. You have specified %s.",
                    token);
                free( psz_dup );
                return VLC_EGENERIC;
            }
            msg_Dbg( p_demux, "Requested frame rate : %s",token );
        }
        else if( strncmp( token, "brightness=", strlen( "brightness=" ) ) == 0 )
        {
            int nr = 0;
            token += strlen("brightness=");
            nr = sscanf( token, "%u", &p_sys->brightness);
            if( nr != 1 )
            {
                msg_Err( p_demux, "Bad brightness value '%s', "
                                  "must be an unsigned integer.",
                                  token );
                free( psz_dup );
                return VLC_EGENERIC;
            }
        }
#if 0
        else if( strncmp( token, "controller=", strlen( "controller=" ) ) == 0 )
        {
            int nr = 0;
            token += strlen("controller=");
            nr = sscanf( token, "%u", &p_sys->controller );
            if( nr != 1)
            {
                msg_Err(p_demux, "Bad controller value '%s', "
                                 "must be an unsigned integer.",
                                 token );
                return VLC_EGENERIC;
            }
        }
#endif
        else if( strncmp( token, "camera=", strlen( "camera=" ) ) == 0 )
        {
            int nr = 0;
            token += strlen("camera=");
            nr = sscanf(token,"%u",&p_sys->selected_camera);
            if( nr != 1)
            {
                msg_Err( p_demux, "Bad camera number '%s', "
                                  "must be an unsigned integer.",
                                  token );
                free( psz_dup );
                return VLC_EGENERIC;
            }
        }
        else if( strncmp( token, "capture=", strlen( "capture=" ) ) == 0)
        {
            token += strlen("capture=");
            if( strncmp(token, "raw1394",7) == 0 )
            {
                msg_Dbg( p_demux, "DMA capture disabled!" );
                p_sys->dma_capture = DMA_OFF;
            }
            else if( strncmp(token,"video1394",9) == 0 )
            {
                msg_Dbg( p_demux, "DMA capture enabled!" );
                p_sys->dma_capture = DMA_ON;
            }
            else
            {
                msg_Err(p_demux, "Bad capture method value '%s', "
                                 "it can be 'raw1394' or 'video1394'.",
                                token );
                free( psz_dup );
                return VLC_EGENERIC;
            }
        }
        else if( strncmp( token, "adev=", strlen( "adev=" ) ) == 0 )
        {
            token += strlen("adev=");
            p_sys->audio_device = strdup(token);
            msg_Dbg( p_demux, "Using audio device '%s'.", token );
        }
        else if( strncmp( token, "samplerate=", strlen( "samplerate=" ) ) == 0 )
        {
            token += strlen("samplerate=");
            sscanf( token, "%d", &p_sys->i_sample_rate );
        }
        else if( strncmp( token, "channels=", strlen("channels=" ) ) == 0 )
        {
            token += strlen("channels=");
            sscanf( token, "%d", &p_sys->channels );
        }
        else if( strncmp( token, "focus=", strlen("focus=" ) ) == 0)
        {
            token += strlen("focus=");
            sscanf( token, "%u", &p_sys->focus );
        }
        else if( strncmp( token, "uid=", strlen("uid=") ) == 0)
        {
            token += strlen("uid=");
            sscanf( token, "0x%llx", &p_sys->selected_uid );
        }
    }
    free( psz_dup );
    return VLC_SUCCESS;
}

