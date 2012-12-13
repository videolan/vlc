/*****************************************************************************
 * dc1394.c: IIDC (DCAM) FireWire input module
 *****************************************************************************
 * Copyright (C) 2006-2009 VideoLAN
 *
 * Authors: Xant Majere <xant@xant.net>
 *          Rob Shortt <rob@tvcentric.com> - libdc1394 V2 API updates
 *          Frederic Benoist <fridorik@gmail.com> - updates from Rob's work
 *
 *****************************************************************************
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>

#include <dc1394/dc1394.h>

#define MAX_IEEE1394_HOSTS 32
#define MAX_CAMERA_NODES 32

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin()
    set_shortname( N_("DC1394") )
    set_description( N_("IIDC Digital Camera (FireWire) input") )
    set_capability( "access_demux", 10 )
    set_callbacks( Open, Close )
vlc_module_end()

struct demux_sys_t
{
    /* camera info */
    dc1394_t            *p_dccontext;
    uint32_t            num_cameras;
    dc1394camera_t      *camera;
    int                 selected_camera;
    uint64_t            selected_uid;
    uint32_t            dma_buffers;
    dc1394featureset_t  features;
    bool                reset_bus;

    /* video info */
    char                *video_device;
    dc1394video_mode_t  video_mode;
    int                 width;
    int                 height;
    int                 frame_size;
    int                 frame_rate;
    unsigned int        brightness;
    unsigned int        focus;
    es_out_id_t         *p_es_video;
    dc1394video_frame_t *frame;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux );
static int Control( demux_t *, int, va_list );
static block_t *GrabVideo( demux_t *p_demux );
static int process_options( demux_t *p_demux);

/*****************************************************************************
 * FindCameras
 *****************************************************************************/
static int FindCamera( demux_sys_t *sys, demux_t *p_demux )
{
    dc1394camera_list_t *list;
    int i_ret = VLC_EGENERIC;

    msg_Dbg( p_demux, "Scanning for ieee1394 ports ..." );

    if( dc1394_camera_enumerate (sys->p_dccontext, &list) != DC1394_SUCCESS )
    {
        msg_Err(p_demux, "Can not ennumerate cameras");
        goto end;
    }

    if( list->num == 0 )
    {
        msg_Err(p_demux, "Can not find cameras");
        goto end;
    }

    sys->num_cameras = list->num;
    msg_Dbg( p_demux, "Found %d dc1394 cameras.", list->num);

    if( sys->selected_uid )
    {
        int found = 0;
        for( unsigned i = 0; i < sys->num_cameras; i++ )
        {
            if( list->ids[i].guid == sys->selected_uid )
            {
                sys->camera = dc1394_camera_new(sys->p_dccontext,
                                                list->ids[i].guid);
                found++;
                break;
            }
        }
        if( !found )
        {
            msg_Err( p_demux, "Can't find camera with uid : 0x%"PRIx64".",
                     sys->selected_uid );
            goto end;
        }
    }
    else if( sys->selected_camera >= (int)list->num )
    {
        msg_Err( p_demux, "There are not this many cameras. (%d/%d)",
                 sys->selected_camera, sys->num_cameras );
        goto end;
    }
    else if( sys->selected_camera >= 0 )
    {
        sys->camera = dc1394_camera_new(sys->p_dccontext,
                    list->ids[sys->selected_camera].guid);
    }
    else
    {
        sys->camera = dc1394_camera_new(sys->p_dccontext,
                                          list->ids[0].guid);
    }

    i_ret = VLC_SUCCESS;

end:
    dc1394_camera_free_list (list);
    return i_ret;
}

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    demux_t      *p_demux = (demux_t*)p_this;
    demux_sys_t  *p_sys;
    es_format_t   fmt;
    dc1394error_t res;

    if( strncmp(p_demux->psz_access, "dc1394", 6) != 0 )
        return VLC_EGENERIC;

    /* Set up p_demux */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->info.i_update = 0;
    p_demux->info.i_title = 0;
    p_demux->info.i_seekpoint = 0;

    p_demux->p_sys = p_sys = calloc( 1, sizeof( demux_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;

    memset( &fmt, 0, sizeof( es_format_t ) );

    /* DEFAULTS */
    p_sys->video_mode      = DC1394_VIDEO_MODE_640x480_YUV422;
    p_sys->width           = 640;
    p_sys->height          = 480;
    p_sys->frame_rate      = DC1394_FRAMERATE_15;
    p_sys->brightness      = 200;
    p_sys->focus           = 0;
    p_sys->p_dccontext     = NULL;
    p_sys->camera          = NULL;
    p_sys->selected_camera = -1;
    p_sys->dma_buffers     = 1;
    p_sys->reset_bus       = 0;

    /* PROCESS INPUT OPTIONS */
    if( process_options(p_demux) != VLC_SUCCESS )
    {
        msg_Err( p_demux, "Bad MRL, please check the option line "
                          "(MRL was: %s)",
                          p_demux->psz_location );
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_sys->p_dccontext = dc1394_new();
    if( !p_sys->p_dccontext )
    {
        msg_Err( p_demux, "Failed to initialise libdc1394");
        free( p_sys );
        return VLC_EGENERIC;
    }

    if( FindCamera( p_sys, p_demux ) != VLC_SUCCESS )
    {
        dc1394_free( p_sys->p_dccontext );
        free( p_sys );
        return VLC_EGENERIC;
    }

    if( !p_sys->camera )
    {
        msg_Err( p_demux, "No camera found !!" );
        dc1394_free( p_sys->p_dccontext );
        free( p_sys );
        return VLC_EGENERIC;
    }

    if( p_sys->reset_bus )
    {
        if( dc1394_reset_bus( p_sys->camera ) != DC1394_SUCCESS )
        {
            msg_Err( p_demux, "Unable to reset IEEE 1394 bus");
            Close( p_this );
            return VLC_EGENERIC;
        }
        else msg_Dbg( p_demux, "Successfully reset IEEE 1394 bus");
    }

    if( dc1394_camera_reset( p_sys->camera ) != DC1394_SUCCESS )
    {
        msg_Err( p_demux, "Unable to reset camera");
        Close( p_this );
        return VLC_EGENERIC;
    }

    if( dc1394_camera_print_info( p_sys->camera,
                  stderr ) != DC1394_SUCCESS )
    {
        msg_Err( p_demux, "Unable to print camera info");
        Close( p_this );
        return VLC_EGENERIC;
    }

    if( dc1394_feature_get_all( p_sys->camera,
                &p_sys->features ) != DC1394_SUCCESS )
    {
        msg_Err( p_demux, "Unable to get feature set");
        Close( p_this );
        return VLC_EGENERIC;
    }
    // TODO: only print features if verbosity increased
    dc1394_feature_print_all(&p_sys->features, stderr);

#if 0 //"Note that you need to execute this function only if you use exotic video1394 device names. /dev/video1394, /dev/video1394/* and /dev/video1394-* are automatically recognized." http://damien.douxchamps.net/ieee1394/libdc1394/v2.x/api/capture/
    if( p_sys->video_device )
    {
        if( dc1394_capture_set_device_filename( p_sys->camera,
                        p_sys->video_device ) != DC1394_SUCCESS )
        {
            msg_Err( p_demux, "Unable to set video device");
            Close( p_this );
            return VLC_EGENERIC;
        }
    }
#endif

    if( p_sys->focus )
    {
        if( dc1394_feature_set_value( p_sys->camera,
                      DC1394_FEATURE_FOCUS,
                      p_sys->focus ) != DC1394_SUCCESS )
        {
            msg_Err( p_demux, "Unable to set initial focus to %u",
                     p_sys->focus );
        }
        else
            msg_Dbg( p_demux, "Initial focus set to %u", p_sys->focus );
    }

    if( dc1394_feature_set_value( p_sys->camera,
                  DC1394_FEATURE_FOCUS,
                  p_sys->brightness ) != DC1394_SUCCESS )
    {
        msg_Err( p_demux, "Unable to set initial brightness to %u",
                 p_sys->brightness );
    }
    else
        msg_Dbg( p_demux, "Initial brightness set to %u", p_sys->brightness );

    if( dc1394_video_set_framerate( p_sys->camera,
                    p_sys->frame_rate ) != DC1394_SUCCESS )
    {
        msg_Err( p_demux, "Unable to set framerate");
        Close( p_this );
        return VLC_EGENERIC;
    }

    if( dc1394_video_set_mode( p_sys->camera,
                   p_sys->video_mode ) != DC1394_SUCCESS )
    {
        msg_Err( p_demux, "Unable to set video mode");
        Close( p_this );
        return VLC_EGENERIC;
    }

    if( dc1394_video_set_iso_speed( p_sys->camera,
                    DC1394_ISO_SPEED_400 ) != DC1394_SUCCESS )
    {
        msg_Err( p_demux, "Unable to set iso speed");
        Close( p_this );
        return VLC_EGENERIC;
    }

    /* and setup capture */
    res = dc1394_capture_setup( p_sys->camera,
                    p_sys->dma_buffers,
                DC1394_CAPTURE_FLAGS_DEFAULT);
    if( res != DC1394_SUCCESS )
    {
        if( res == DC1394_NO_BANDWIDTH )
        {
            msg_Err( p_demux ,"No bandwidth: try adding the "
             "\"resetbus\" option" );
        }
        else
        {
            msg_Err( p_demux ,"Unable to setup capture" );
        }
        Close( p_this );
        return VLC_EGENERIC;
    }

    /* TODO - UYV444 chroma converter is missing, when it will be available
     * fourcc will become variable (and not just a fixed value for UYVY)
     */
    es_format_Init( &fmt, VIDEO_ES, VLC_CODEC_UYVY );

    fmt.video.i_width = p_sys->width;
    fmt.video.i_height = p_sys->height;

    msg_Dbg( p_demux, "Added new video es %4.4s %dx%d",
             (char*)&fmt.i_codec, fmt.video.i_width, fmt.video.i_height );

    p_sys->p_es_video = es_out_Add( p_demux->out, &fmt );

    /* have the camera start sending us data */
    if( dc1394_video_set_transmission( p_sys->camera,
                       DC1394_ON ) != DC1394_SUCCESS )
    {
        msg_Err( p_demux, "Unable to start camera iso transmission" );
        dc1394_capture_stop( p_sys->camera );
        Close( p_this );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_demux, "Set iso transmission" );
    // TODO: reread camera
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    /* Stop data transmission */
    if( dc1394_video_set_transmission( p_sys->camera,
                       DC1394_OFF ) != DC1394_SUCCESS )
        msg_Err( p_demux, "Unable to stop camera iso transmission" );

    /* Close camera */
    dc1394_capture_stop( p_sys->camera );

    dc1394_camera_free(p_sys->camera);
    dc1394_free(p_sys->p_dccontext);

    free( p_sys->video_device );
    free( p_sys );
}

#if 0
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
#endif

/*****************************************************************************
 * Demux:
 *****************************************************************************/
static block_t *GrabVideo( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t     *p_block = NULL;

    if( dc1394_capture_dequeue( p_sys->camera,
                DC1394_CAPTURE_POLICY_WAIT,
                &p_sys->frame ) != DC1394_SUCCESS )
    {
        msg_Err( p_demux, "Unable to capture a frame" );
        return NULL;
    }

    p_block = block_Alloc( p_sys->frame->size[0] * p_sys->frame->size[1] * 2 );
    if( !p_block )
    {
        msg_Err( p_demux, "Can not get block" );
        return NULL;
    }

    if( !p_sys->frame->image )
    {
        msg_Err (p_demux, "Capture buffer empty");
        block_Release( p_block );
        return NULL;
    }

    memcpy( p_block->p_buffer, (const char *)p_sys->frame->image,
            p_sys->width * p_sys->height * 2 );

    p_block->i_pts = p_block->i_dts = mdate();
    dc1394_capture_enqueue( p_sys->camera, p_sys->frame );
    return p_block;
}

static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t *p_blockv = NULL;

    /* Try grabbing video frame */
    p_blockv = GrabVideo( p_demux );

    if( !p_blockv )
    {
        /* Sleep so we do not consume all the cpu, 10ms seems
         * like a good value (100fps)
         */
        msleep( 10000 );
        return 1;
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
    VLC_UNUSED( p_demux );
    switch( i_query )
    {
        /* Special for access_demux */
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_SEEK:
        case DEMUX_SET_PAUSE_STATE:
        case DEMUX_CAN_CONTROL_PACE:
            *va_arg( args, bool * ) = false;
            return VLC_SUCCESS;

        case DEMUX_GET_PTS_DELAY:
            *va_arg( args, int64_t * ) = (int64_t)DEFAULT_PTS_DELAY;
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

static int process_options( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    char *psz_dup;
    char *psz_parser;
    char *token = NULL;
    char *state = NULL;
    const char *in_size = NULL;
    const char *in_fmt = NULL;
    float rate_f;

    psz_dup = strdup( p_demux->psz_location );
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
                    "video size of 160x120 is actually disabled for lack of"
                    "chroma support. It will relased ASAP, until then try "
                    "an higher size (320x240 and 640x480 are fully supported)" );
                free(psz_dup);
                return VLC_EGENERIC;
#if 0
                in_size = "160x120";
                p_sys->width = 160;
                p_sys->height = 120;
#endif
            }
            else if( strncmp( token, "320x240", 7 ) == 0 )
            {
                in_size = "320x240";
                p_sys->width = 320;
                p_sys->height = 240;
            }
            else if( strncmp( token, "640x480", 7 ) == 0 )
            {
                in_size = "640x480";
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
                free(psz_dup);
                return VLC_EGENERIC;
            }
            msg_Dbg( p_demux, "Requested video size : %s",token );
        }
        if( strncmp( token, "format=", strlen("format=") ) == 0 )
        {
            token += strlen("format=");
            if( strncmp( token, "YUV411", 6 ) == 0 )
            {
                in_fmt = "YUV411";
            }
            else if( strncmp( token, "YUV422", 6 ) == 0 )
            {
                in_fmt = "YUV422";
            }
            else if( strncmp( token, "YUV444", 6 ) == 0 )
            {
                in_fmt = "YUV444";
            }
            else if( strncmp( token, "RGB8", 4 ) == 0 )
            {
                in_fmt = "RGB8";
            }
            else if( strncmp( token, "MONO8", 5 ) == 0 )
            {
                in_fmt = "MONO8";
            }
            else if( strncmp( token, "MONO16", 6 ) == 0 )
            {
                in_fmt = "MONO16";
            }
            else
            {
                msg_Err( p_demux, "Invalid format %s.", token );
                free(psz_dup);
                return VLC_EGENERIC;
            }
            msg_Dbg( p_demux, "Requested video format : %s", token );
        }
        else if( strncmp( token, "fps=", strlen( "fps=" ) ) == 0 )
        {
            token += strlen("fps=");
            sscanf( token, "%g", &rate_f );
            if( rate_f == 1.875 )
                p_sys->frame_rate = DC1394_FRAMERATE_1_875;
            else if( rate_f == 3.75 )
                p_sys->frame_rate = DC1394_FRAMERATE_3_75;
            else if( rate_f == 7.5 )
                p_sys->frame_rate = DC1394_FRAMERATE_7_5;
            else if( rate_f == 15 )
                p_sys->frame_rate = DC1394_FRAMERATE_15;
            else if( rate_f == 30 )
                p_sys->frame_rate = DC1394_FRAMERATE_30;
            else if( rate_f == 60 )
                p_sys->frame_rate = DC1394_FRAMERATE_60;
            else
            {
                msg_Err( p_demux ,
                    "This program supports framerates of"
                    " 1.875, 3.75, 7.5, 15, 30, 60. "
                    "Please specify one of them. You have specified %s.",
                    token);
                free(psz_dup);
                return VLC_EGENERIC;
            }
            msg_Dbg( p_demux, "Requested frame rate : %s",token );
        }
        else if( strncmp( token, "resetbus", strlen( "resetbus" ) ) == 0 )
        {
            token += strlen("resetbus");
            p_sys->reset_bus = 1;
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
                free(psz_dup);
                return VLC_EGENERIC;
            }
        }
        else if( strncmp( token, "buffers=", strlen( "buffers=" ) ) == 0 )
        {
            int nr = 0;
            int in_buf = 0;
            token += strlen("buffers=");
            nr = sscanf( token, "%d", &in_buf);
            if( nr != 1 || in_buf < 1 )
            {
                msg_Err( p_demux, "DMA buffers must be 1 or greater." );
                free(psz_dup);
                return VLC_EGENERIC;
            }
            else p_sys->dma_buffers = in_buf;
        }
#if 0
        // NOTE: If controller support is added back, more logic will needed to be added
        //       after the cameras are scanned.
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
                free(psz_dup);
                return VLC_EGENERIC;
            }
        }
        else if( strncmp( token, "vdev=", strlen( "vdev=" ) ) == 0)
        {
            token += strlen("vdev=");
            p_sys->video_device = strdup(token);
            msg_Dbg( p_demux, "Using video device '%s'.", token );
        }
        else if( strncmp( token, "focus=", strlen("focus=" ) ) == 0)
        {
            int nr = 0;
            token += strlen("focus=");
            nr = sscanf( token, "%u", &p_sys->focus );
            if( nr != 1 )
            {
                msg_Err( p_demux, "Bad focus value '%s', "
                                  "must be an unsigned integer.",
                                  token );
                free(psz_dup);
                return VLC_EGENERIC;
            }
        }
        else if( strncmp( token, "uid=", strlen("uid=") ) == 0)
        {
            token += strlen("uid=");
            sscanf( token, "0x%"SCNx64, &p_sys->selected_uid );
        }
    }

    // The mode is a combination of size and format and not every format
    // is supported by every size.
    if( in_size)
    {
        if( strcmp( in_size, "160x120") == 0)
        {
            if( in_fmt && (strcmp( in_fmt, "YUV444") != 0) )
                msg_Err(p_demux, "160x120 only supports YUV444 - forcing");
            p_sys->video_mode = DC1394_VIDEO_MODE_160x120_YUV444;
        }
        else if( strcmp( in_size, "320x240") == 0)
        {
            if( in_fmt && (strcmp( in_fmt, "YUV422") != 0) )
                msg_Err(p_demux, "320x240 only supports YUV422 - forcing");
            p_sys->video_mode = DC1394_VIDEO_MODE_320x240_YUV422;
        }
    }
    else
    { // 640x480 default
        if( in_fmt )
        {
            if( strcmp( in_fmt, "RGB8") == 0)
                p_sys->video_mode = DC1394_VIDEO_MODE_640x480_RGB8;
            else if( strcmp( in_fmt, "MONO8") == 0)
                p_sys->video_mode = DC1394_VIDEO_MODE_640x480_MONO8;
            else if( strcmp( in_fmt, "MONO16") == 0)
                p_sys->video_mode = DC1394_VIDEO_MODE_640x480_MONO16;
            else if( strcmp( in_fmt, "YUV411") == 0)
                p_sys->video_mode = DC1394_VIDEO_MODE_640x480_YUV411;
            else // YUV422 default
                p_sys->video_mode = DC1394_VIDEO_MODE_640x480_YUV422;
        }
        else // YUV422 default
            p_sys->video_mode = DC1394_VIDEO_MODE_640x480_YUV422;
    }

    free( psz_dup );
    return VLC_SUCCESS;
}
