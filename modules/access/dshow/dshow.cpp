/*****************************************************************************
 * dshow.cpp : DirectShow access module for vlc
 *****************************************************************************
 * Copyright (C) 2002, 2003 VideoLAN
 * $Id$
 *
 * Author: Gildas Bazin <gbazin@videolan.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/vout.h>

#include "common.h"
#include "filter.h"

/*****************************************************************************
 * Access: local prototypes
 *****************************************************************************/
static block_t *ReadCompressed( access_t * );
static int AccessControl ( access_t *, int, va_list );

static int Demux       ( demux_t * );
static int DemuxControl( demux_t *, int, va_list );

static int OpenDevice( vlc_object_t *, access_sys_t *, string, vlc_bool_t );
static IBaseFilter *FindCaptureDevice( vlc_object_t *, string *,
                                       list<string> *, vlc_bool_t );
static size_t EnumDeviceCaps( vlc_object_t *, IBaseFilter *,
                              int, int, int, int, int, int,
                              AM_MEDIA_TYPE *mt, size_t );
static bool ConnectFilters( vlc_object_t *, access_sys_t *,
                            IBaseFilter *, CaptureFilter * );
static int FindDevicesCallback( vlc_object_t *, char const *,
                                vlc_value_t, vlc_value_t, void * );
static int ConfigDevicesCallback( vlc_object_t *, char const *,
                                  vlc_value_t, vlc_value_t, void * );

static void ShowPropertyPage( IUnknown * );
static void ShowDeviceProperties( vlc_object_t *, ICaptureGraphBuilder2 *, 
                                  IBaseFilter *, vlc_bool_t );
static void ShowTunerProperties( vlc_object_t *, ICaptureGraphBuilder2 *, 
                                 IBaseFilter *, vlc_bool_t );
static void ConfigTuner( vlc_object_t *, ICaptureGraphBuilder2 *,
                         IBaseFilter * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static char *ppsz_vdev[] = { "", "none" };
static char *ppsz_vdev_text[] = { N_("Default"), N_("None") };
static char *ppsz_adev[] = { "", "none" };
static char *ppsz_adev_text[] = { N_("Default"), N_("None") };
static int  pi_tuner_input[] = { 0, 1, 2 };
static char *ppsz_tuner_input_text[] =
    {N_("Default"), N_("Cable"), N_("Antenna")};

#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for DirectShow streams. " \
    "This value should be set in milliseconds units." )
#define VDEV_TEXT N_("Video device name")
#define VDEV_LONGTEXT N_( \
    "You can specify the name of the video device that will be used by the " \
    "DirectShow plugin. If you don't specify anything, the default device " \
    "will be used.")
#define ADEV_TEXT N_("Audio device name")
#define ADEV_LONGTEXT N_( \
    "You can specify the name of the audio device that will be used by the " \
    "DirectShow plugin. If you don't specify anything, the default device " \
    "will be used.")
#define SIZE_TEXT N_("Video size")
#define SIZE_LONGTEXT N_( \
    "You can specify the size of the video that will be displayed by the " \
    "DirectShow plugin. If you don't specify anything the default size for " \
    "your device will be used.")
#define CHROMA_TEXT N_("Video input chroma format")
#define CHROMA_LONGTEXT N_( \
    "Force the DirectShow video input to use a specific chroma format " \
    "(eg. I420 (default), RV24, etc.)")
#define FPS_TEXT N_("Video input frame rate")
#define FPS_LONGTEXT N_( \
    "Force the DirectShow video input to use a specific frame rate" \
    "(eg. 0 means default, 25, 29.97, 50, 59.94, etc.)")
#define CONFIG_TEXT N_("Device properties")
#define CONFIG_LONGTEXT N_( \
    "Show the properties dialog of the selected device before starting the " \
    "stream.")
#define TUNER_TEXT N_("Tuner properties")
#define TUNER_LONGTEXT N_( \
    "Show the tuner properties [channel selection] page." )
#define CHANNEL_TEXT N_("Tuner TV Channel")
#define CHANNEL_LONGTEXT N_( \
    "Allows you to set the TV channel the tuner will set to " \
    "(0 means default)." )
#define COUNTRY_TEXT N_("Tuner country code")
#define COUNTRY_LONGTEXT N_( \
    "Allows you to set the tuner country code that establishes the current " \
    "channel-to-frequency mapping (0 means default)." )
#define TUNER_INPUT_TEXT N_("Tuner input type")
#define TUNER_INPUT_LONGTEXT N_( \
    "Allows you to select the tuner input type (Cable/Antenna)." )

static int  CommonOpen ( vlc_object_t *, access_sys_t *, vlc_bool_t );
static void CommonClose( vlc_object_t *, access_sys_t * );

static int  AccessOpen ( vlc_object_t * );
static void AccessClose( vlc_object_t * );

static int  DemuxOpen  ( vlc_object_t * );
static void DemuxClose ( vlc_object_t * );

vlc_module_begin();
    set_shortname( _("DirectShow") );
    set_description( _("DirectShow input") );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS );
    add_integer( "dshow-caching", (mtime_t)(0.2*CLOCK_FREQ) / 1000, NULL,
                 CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );

    add_string( "dshow-vdev", NULL, NULL, VDEV_TEXT, VDEV_LONGTEXT, VLC_FALSE);
        change_string_list( ppsz_vdev, ppsz_vdev_text, FindDevicesCallback );
        change_action_add( FindDevicesCallback, N_("Refresh list") );
        change_action_add( ConfigDevicesCallback, N_("Configure") );

    add_string( "dshow-adev", NULL, NULL, ADEV_TEXT, ADEV_LONGTEXT, VLC_FALSE);
        change_string_list( ppsz_adev, ppsz_adev_text, FindDevicesCallback );
        change_action_add( FindDevicesCallback, N_("Refresh list") );
        change_action_add( ConfigDevicesCallback, N_("Configure") );

    add_string( "dshow-size", NULL, NULL, SIZE_TEXT, SIZE_LONGTEXT, VLC_FALSE);

    add_string( "dshow-chroma", NULL, NULL, CHROMA_TEXT, CHROMA_LONGTEXT,
                VLC_TRUE );

    add_float( "dshow-fps", 0.0f, NULL, FPS_TEXT, FPS_LONGTEXT,
                VLC_TRUE );

    add_bool( "dshow-config", VLC_FALSE, NULL, CONFIG_TEXT, CONFIG_LONGTEXT,
              VLC_FALSE );

    add_bool( "dshow-tuner", VLC_FALSE, NULL, TUNER_TEXT, TUNER_LONGTEXT,
              VLC_FALSE );

    add_integer( "dshow-tuner-channel", 0, NULL, CHANNEL_TEXT,
                 CHANNEL_LONGTEXT, VLC_TRUE );

    add_integer( "dshow-tuner-country", 0, NULL, COUNTRY_TEXT,
                 COUNTRY_LONGTEXT, VLC_TRUE );

    add_integer( "dshow-tuner-input", 0, NULL, TUNER_INPUT_TEXT,
                 TUNER_INPUT_LONGTEXT, VLC_TRUE );
        change_integer_list( pi_tuner_input, ppsz_tuner_input_text, 0 );

    add_shortcut( "dshow" );
    set_capability( "access_demux", 0 );
    set_callbacks( DemuxOpen, DemuxClose );

    add_submodule();
    set_description( _("DirectShow input") );
    add_shortcut( "dshow" );
    set_capability( "access2", 0 );
    set_callbacks( AccessOpen, AccessClose );

vlc_module_end();

/*****************************************************************************
 * DirectShow elementary stream descriptor
 *****************************************************************************/
typedef struct dshow_stream_t
{
    string          devicename;
    IBaseFilter     *p_device_filter;
    CaptureFilter   *p_capture_filter;
    AM_MEDIA_TYPE   mt;

    union
    {
      VIDEOINFOHEADER video;
      WAVEFORMATEX    audio;

    } header;

    int             i_fourcc;
    es_out_id_t     *p_es;

    vlc_bool_t      b_pts;

} dshow_stream_t;

/*****************************************************************************
 * DirectShow utility functions
 *****************************************************************************/
static void CreateDirectShowGraph( access_sys_t *p_sys )
{
    p_sys->i_crossbar_route_depth = 0;

    /* Create directshow filter graph */
    if( SUCCEEDED( CoCreateInstance( CLSID_FilterGraph, 0, CLSCTX_INPROC,
                       (REFIID)IID_IFilterGraph, (void **)&p_sys->p_graph) ) )
    {
        /* Create directshow capture graph builder if available */
        if( SUCCEEDED( CoCreateInstance( CLSID_CaptureGraphBuilder2, 0,
                         CLSCTX_INPROC, (REFIID)IID_ICaptureGraphBuilder2,
                         (void **)&p_sys->p_capture_graph_builder2 ) ) )
        {
            p_sys->p_capture_graph_builder2->
                SetFiltergraph((IGraphBuilder *)p_sys->p_graph);
        }

        p_sys->p_graph->QueryInterface( IID_IMediaControl,
                                        (void **)&p_sys->p_control );
    }
}

static void DeleteDirectShowGraph( access_sys_t *p_sys )
{
    DeleteCrossbarRoutes( p_sys );

    /* Remove filters from graph */
    for( int i = 0; i < p_sys->i_streams; i++ )
    {
        p_sys->p_graph->RemoveFilter( p_sys->pp_streams[i]->p_capture_filter );
        p_sys->p_graph->RemoveFilter( p_sys->pp_streams[i]->p_device_filter );
        p_sys->pp_streams[i]->p_capture_filter->Release();
        p_sys->pp_streams[i]->p_device_filter->Release();
    }

    /* Release directshow objects */
    if( p_sys->p_control )
    {
        p_sys->p_control->Release();
        p_sys->p_control = NULL;
    }
    if( p_sys->p_capture_graph_builder2 )
    {
        p_sys->p_capture_graph_builder2->Release();
        p_sys->p_capture_graph_builder2 = NULL;
    }

    if( p_sys->p_graph )
    {
        p_sys->p_graph->Release();
        p_sys->p_graph = NULL;
    }
}

/*****************************************************************************
 * CommonOpen: open direct show device
 *****************************************************************************/
static int CommonOpen( vlc_object_t *p_this, access_sys_t *p_sys,
                       vlc_bool_t b_access_demux )
{
    vlc_value_t  val;
    int i;

    /* Get/parse options and open device(s) */
    string vdevname, adevname;
    int i_width = 0, i_height = 0, i_chroma = 0;
    vlc_bool_t b_audio = VLC_TRUE;

    var_Create( p_this, "dshow-config", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_this, "dshow-tuner", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    var_Create( p_this, "dshow-vdev", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Get( p_this, "dshow-vdev", &val );
    if( val.psz_string ) vdevname = string( val.psz_string );
    if( val.psz_string ) free( val.psz_string );

    var_Create( p_this, "dshow-adev", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Get( p_this, "dshow-adev", &val );
    if( val.psz_string ) adevname = string( val.psz_string );
    if( val.psz_string ) free( val.psz_string );

    static struct {char *psz_size; int  i_width; int  i_height;} size_table[] =
    { { "subqcif", 128, 96 }, { "qsif", 160, 120 }, { "qcif", 176, 144 },
      { "sif", 320, 240 }, { "cif", 352, 288 }, { "cif", 640, 480 },
      { 0, 0, 0 },
    };

    var_Create( p_this, "dshow-size", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Get( p_this, "dshow-size", &val );
    if( val.psz_string && *val.psz_string )
    {
        for( i = 0; size_table[i].psz_size; i++ )
        {
            if( !strcmp( val.psz_string, size_table[i].psz_size ) )
            {
                i_width = size_table[i].i_width;
                i_height = size_table[i].i_height;
                break;
            }
        }
        if( !size_table[i].psz_size ) /* Try to parse "WidthxHeight" */
        {
            char *psz_parser;
            i_width = strtol( val.psz_string, &psz_parser, 0 );
            if( *psz_parser == 'x' || *psz_parser == 'X')
            {
                i_height = strtol( psz_parser + 1, &psz_parser, 0 );
            }
            msg_Dbg( p_this, "Width x Height %dx%d", i_width, i_height );
        }
    }
    if( val.psz_string ) free( val.psz_string );

    var_Create( p_this, "dshow-chroma", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Get( p_this, "dshow-chroma", &val );
    if( val.psz_string && strlen( val.psz_string ) >= 4 )
    {
        i_chroma = VLC_FOURCC( val.psz_string[0], val.psz_string[1],
                               val.psz_string[2], val.psz_string[3] );
    }
    if( val.psz_string ) free( val.psz_string );

    var_Create( p_this, "dshow-fps", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT );
    var_Create( p_this, "dshow-tuner-channel",
                VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_this, "dshow-tuner-country",
                VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_this, "dshow-tuner-input",
                VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    var_Create( p_this, "dshow-caching", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    /* Initialize OLE/COM */
    CoInitialize( 0 );

    /* Initialize some data */
    p_sys->i_streams = 0;
    p_sys->pp_streams = 0;
    p_sys->i_width = i_width;
    p_sys->i_height = i_height;
    p_sys->i_chroma = i_chroma;

    p_sys->p_graph = NULL;
    p_sys->p_capture_graph_builder2 = NULL;
    p_sys->p_control = NULL;

    vlc_mutex_init( p_this, &p_sys->lock );
    vlc_cond_init( p_this, &p_sys->wait );

    /* Build directshow graph */
    CreateDirectShowGraph( p_sys );

    if( OpenDevice( p_this, p_sys, vdevname, 0 ) != VLC_SUCCESS )
    {
        msg_Err( p_this, "can't open video");
    }
    else
    {
        /* Check if we can handle the demuxing ourselves or need to spawn
         * a demuxer module */
        dshow_stream_t *p_stream = p_sys->pp_streams[p_sys->i_streams-1];

        if( p_stream->mt.majortype == MEDIATYPE_Video )
        {
            if( /* Raw DV stream */
                p_stream->i_fourcc == VLC_FOURCC('d','v','s','l') ||
                p_stream->i_fourcc == VLC_FOURCC('d','v','s','d') ||
                p_stream->i_fourcc == VLC_FOURCC('d','v','h','d') ||
                /* Raw MPEG video stream */
                p_stream->i_fourcc == VLC_FOURCC('m','p','2','v') )
            {
                b_audio = VLC_FALSE;

                if( b_access_demux )
                {
                    /* Let the access (only) take care of that */
                    return VLC_EGENERIC;
                }
            }
        }

        if( p_stream->mt.majortype == MEDIATYPE_Stream )
        {
            b_audio = VLC_FALSE;

            if( b_access_demux )
            {
                /* Let the access (only) take care of that */
                return VLC_EGENERIC;
            }

            var_Get( p_this, "dshow-tuner", &val );
            if( val.b_bool )
            {
                /* FIXME: we do MEDIATYPE_Stream here so we don't do
                 * it twice. */
                ShowTunerProperties( p_this, p_sys->p_capture_graph_builder2,
                                     p_stream->p_device_filter, 0 );
            }
        }
    }

    if( b_audio && OpenDevice( p_this, p_sys, adevname, 1 ) != VLC_SUCCESS )
    {
        msg_Err( p_this, "can't open audio");
    }

    for( i = p_sys->i_crossbar_route_depth-1; i >= 0 ; --i )
    {
        IAMCrossbar *pXbar = p_sys->crossbar_routes[i].pXbar;
        LONG VideoInputIndex = p_sys->crossbar_routes[i].VideoInputIndex;
        LONG VideoOutputIndex = p_sys->crossbar_routes[i].VideoOutputIndex;
        LONG AudioInputIndex = p_sys->crossbar_routes[i].AudioInputIndex;
        LONG AudioOutputIndex = p_sys->crossbar_routes[i].AudioOutputIndex;

        if( SUCCEEDED(pXbar->Route(VideoOutputIndex, VideoInputIndex)) )
        {
            msg_Dbg( p_this, "Crossbar at depth %d, Routed video "
                     "ouput %ld to video input %ld", i, VideoOutputIndex,
                     VideoInputIndex );

            if( AudioOutputIndex != -1 && AudioInputIndex != -1 )
            {
                if( SUCCEEDED( pXbar->Route(AudioOutputIndex,
                                            AudioInputIndex)) )
                {
                    msg_Dbg(p_this, "Crossbar at depth %d, Routed audio "
                            "ouput %ld to audio input %ld", i,
                            AudioOutputIndex, AudioInputIndex );
                }
            }
        }
    }

    /*
    ** Show properties pages from other filters in graph
    */
    var_Get( p_this, "dshow-config", &val );
    if( val.b_bool )
    {
        for( i = p_sys->i_crossbar_route_depth-1; i >= 0 ; --i )
        {
            IAMCrossbar *pXbar = p_sys->crossbar_routes[i].pXbar;
            IBaseFilter *p_XF;

            if( SUCCEEDED( pXbar->QueryInterface( IID_IBaseFilter,
                                                  (void **)&p_XF ) ) )
            {
                ShowPropertyPage( p_XF );
                p_XF->Release();
            }
        }
    }

    /* Initialize some data */
    p_sys->i_current_stream = 0;

    if( !p_sys->i_streams ) return VLC_EGENERIC;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DemuxOpen: open direct show device as an access_demux module
 *****************************************************************************/
static int DemuxOpen( vlc_object_t *p_this )
{
    demux_t      *p_demux = (demux_t *)p_this;
    access_sys_t *p_sys;
    int i;

    p_sys = (access_sys_t *)malloc( sizeof( access_sys_t ) );
    memset( p_sys, 0, sizeof( access_sys_t ) );
    p_demux->p_sys = (demux_sys_t *)p_sys;

    if( CommonOpen( p_this, p_sys, VLC_TRUE ) != VLC_SUCCESS )
    {
        CommonClose( p_this, p_sys );
        return VLC_EGENERIC;
    }

    /* Everything is ready. Let's rock baby */
    msg_Dbg( p_this, "Playing...");
    p_sys->p_control->Run();

    p_demux->pf_demux   = Demux;
    p_demux->pf_control = DemuxControl;
    p_demux->info.i_update = 0;
    p_demux->info.i_title = 0;
    p_demux->info.i_seekpoint = 0;

    for( i = 0; i < p_sys->i_streams; i++ )
    {
        dshow_stream_t *p_stream = p_sys->pp_streams[i];
        es_format_t fmt;

        if( p_stream->mt.majortype == MEDIATYPE_Video )
        {
            es_format_Init( &fmt, VIDEO_ES, p_stream->i_fourcc );

            fmt.video.i_width  = p_stream->header.video.bmiHeader.biWidth;
            fmt.video.i_height = p_stream->header.video.bmiHeader.biHeight;
            fmt.video.i_aspect = 4 * VOUT_ASPECT_FACTOR / 3;

            if( !p_stream->header.video.bmiHeader.biCompression )
            {
                /* RGB DIB are coded from bottom to top */
                fmt.video.i_height = (unsigned int)(-(int)fmt.video.i_height);
            }

            /* Setup rgb mask for RGB formats */
            if( p_stream->i_fourcc == VLC_FOURCC('R','V','2','4') )
            {
                /* This is in BGR format */
                fmt.video.i_bmask = 0x00ff0000;
                fmt.video.i_gmask = 0x0000ff00;
                fmt.video.i_rmask = 0x000000ff;
            }

            if( p_stream->header.video.AvgTimePerFrame )
            {
                fmt.video.i_frame_rate = 10000000;
                fmt.video.i_frame_rate_base =
                    p_stream->header.video.AvgTimePerFrame;
            }
        }
        else if( p_stream->mt.majortype == MEDIATYPE_Audio )
        {
            es_format_Init( &fmt, AUDIO_ES, p_stream->i_fourcc );

            fmt.audio.i_channels = p_stream->header.audio.nChannels;
            fmt.audio.i_rate = p_stream->header.audio.nSamplesPerSec;
            fmt.audio.i_bitspersample = p_stream->header.audio.wBitsPerSample;
            fmt.audio.i_blockalign = fmt.audio.i_channels *
                fmt.audio.i_bitspersample / 8;
            fmt.i_bitrate = fmt.audio.i_channels * fmt.audio.i_rate *
                fmt.audio.i_bitspersample;
        }

        p_stream->p_es = es_out_Add( p_demux->out, &fmt );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * AccessOpen: open direct show device as an access module
 *****************************************************************************/
static int AccessOpen( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys;

    p_access->p_sys = p_sys = (access_sys_t *)malloc( sizeof( access_sys_t ) );
    memset( p_sys, 0, sizeof( access_sys_t ) );

    if( CommonOpen( p_this, p_sys, VLC_FALSE ) != VLC_SUCCESS )
    {
        CommonClose( p_this, p_sys );
        return VLC_EGENERIC;
    }

    dshow_stream_t *p_stream = p_sys->pp_streams[0];

    /* Check if we need to force demuxers */
    if( !p_access->psz_demux || !*p_access->psz_demux )
    {
        if( p_stream->i_fourcc == VLC_FOURCC('d','v','s','l') ||
            p_stream->i_fourcc == VLC_FOURCC('d','v','s','d') ||
            p_stream->i_fourcc == VLC_FOURCC('d','v','h','d') )
        {
            p_access->psz_demux = strdup( "rawdv" );
        }
        else if( p_stream->i_fourcc == VLC_FOURCC('m','p','2','v') )
        {
            p_access->psz_demux = "mpgv";
        }
    }

    /* Setup Access */
    p_access->pf_read = NULL;
    p_access->pf_block = ReadCompressed;
    p_access->pf_control = AccessControl;
    p_access->pf_seek = NULL;
    p_access->info.i_update = 0;
    p_access->info.i_size = 0;
    p_access->info.i_pos = 0;
    p_access->info.b_eof = VLC_FALSE;
    p_access->info.i_title = 0;
    p_access->info.i_seekpoint = 0;
    p_access->p_sys = p_sys;

    /* Everything is ready. Let's rock baby */
    msg_Dbg( p_this, "Playing...");
    p_sys->p_control->Run();

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CommonClose: close device
 *****************************************************************************/
static void CommonClose( vlc_object_t *p_this, access_sys_t *p_sys )
{
    msg_Dbg( p_this, "Releasing DirectShow");

    DeleteDirectShowGraph( p_sys );

    /* Uninitialize OLE/COM */
    CoUninitialize();

    for( int i = 0; i < p_sys->i_streams; i++ ) delete p_sys->pp_streams[i];
    if( p_sys->i_streams ) free( p_sys->pp_streams );

    vlc_mutex_destroy( &p_sys->lock );
    vlc_cond_destroy( &p_sys->wait );

    free( p_sys );
}

/*****************************************************************************
 * AccessClose: close device
 *****************************************************************************/
static void AccessClose( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t *)p_this;
    access_sys_t *p_sys    = p_access->p_sys;

    /* Stop capturing stuff */
    p_sys->p_control->Stop();

    CommonClose( p_this, p_sys );
}

/*****************************************************************************
 * DemuxClose: close device
 *****************************************************************************/
static void DemuxClose( vlc_object_t *p_this )
{
    demux_t      *p_demux = (demux_t *)p_this;
    access_sys_t *p_sys   = (access_sys_t *)p_demux->p_sys;

    /* Stop capturing stuff */
    p_sys->p_control->Stop();

    CommonClose( p_this, p_sys );
}

/****************************************************************************
 * ConnectFilters
 ****************************************************************************/
static bool ConnectFilters( vlc_object_t *p_this, access_sys_t *p_sys,
                            IBaseFilter *p_filter,
                            CaptureFilter *p_capture_filter )
{
    CapturePin *p_input_pin = p_capture_filter->CustomGetPin();

    AM_MEDIA_TYPE mediaType = p_input_pin->CustomGetMediaType();

    if( p_sys->p_capture_graph_builder2 )
    {
        if( FAILED(p_sys->p_capture_graph_builder2->
                RenderStream( &PIN_CATEGORY_CAPTURE, &mediaType.majortype,
                              p_filter, 0, (IBaseFilter *)p_capture_filter )) )
        {
            return false;
        }

        // Sort out all the possible video inputs
        // The class needs to be given the capture filters ANALOGVIDEO input pin
        IEnumPins *pins = 0;
        if( ( mediaType.majortype == MEDIATYPE_Video ||
              mediaType.majortype == MEDIATYPE_Stream ) &&
            SUCCEEDED(p_filter->EnumPins(&pins)) )
        {
            IPin        *pP = 0;
            ULONG        n;
            PIN_INFO     pinInfo;
            BOOL         Found = FALSE;
            IKsPropertySet *pKs=0;
            GUID guid;
            DWORD dw;

            while( !Found && ( S_OK == pins->Next(1, &pP, &n) ) )
            {
                if( S_OK == pP->QueryPinInfo(&pinInfo) )
                {
                    // is this pin an ANALOGVIDEOIN input pin?
                    if( pinInfo.dir == PINDIR_INPUT &&
                        pP->QueryInterface( IID_IKsPropertySet,
                                            (void **)&pKs ) == S_OK )
                    {
                        if( pKs->Get( AMPROPSETID_Pin,
                                      AMPROPERTY_PIN_CATEGORY, NULL, 0,
                                      &guid, sizeof(GUID), &dw ) == S_OK )
                        {
                            if( guid == PIN_CATEGORY_ANALOGVIDEOIN )
                            {
                                // recursively search crossbar routes
                                FindCrossbarRoutes( p_this, p_sys, pP, 0 );
                                // found it
                                Found = TRUE;
                            }
                        }
                        pKs->Release();
                    }
                    pinInfo.pFilter->Release();
                }
                pP->Release();
            }
            pins->Release();
        }
        return true;
    }
    else
    {
        IEnumPins *p_enumpins;
        IPin *p_pin;

        if( S_OK != p_filter->EnumPins( &p_enumpins ) ) return false;

        while( S_OK == p_enumpins->Next( 1, &p_pin, NULL ) )
        {
            PIN_DIRECTION pin_dir;
            p_pin->QueryDirection( &pin_dir );

            if( pin_dir == PINDIR_OUTPUT &&
                p_sys->p_graph->ConnectDirect( p_pin, (IPin *)p_input_pin,
                                               0 ) == S_OK )
            {
                p_pin->Release();
                p_enumpins->Release();
                return true;
            }
            p_pin->Release();
        }

        p_enumpins->Release();
        return false;
    }
}

/*
 * get fourcc priority from arbritary preference, the higher the better
 */
static int GetFourCCPriority( int i_fourcc )
{
    switch( i_fourcc )
    {
    case VLC_FOURCC('I','4','2','0'):
    case VLC_FOURCC('f','l','3','2'):
        return 9;
    case VLC_FOURCC('Y','V','1','2'):
    case VLC_FOURCC('a','r','a','w'):
        return 8;
    case VLC_FOURCC('R','V','2','4'):
        return 7;
    case VLC_FOURCC('Y','U','Y','2'):
    case VLC_FOURCC('R','V','3','2'):
    case VLC_FOURCC('R','G','B','A'):
        return 6;
    }

    return 0;
}

#define MAX_MEDIA_TYPES 32

static int OpenDevice( vlc_object_t *p_this, access_sys_t *p_sys,
                       string devicename, vlc_bool_t b_audio )
{
    /* See if device is already opened */
    for( int i = 0; i < p_sys->i_streams; i++ )
    {
        if( devicename.size() &&
            p_sys->pp_streams[i]->devicename == devicename )
        {
            /* Already opened */
            return VLC_SUCCESS;
        }
    }

    list<string> list_devices;

    /* Enumerate devices and display their names */
    FindCaptureDevice( p_this, NULL, &list_devices, b_audio );

    if( !list_devices.size() )
        return VLC_EGENERIC;

    list<string>::iterator iter;
    for( iter = list_devices.begin(); iter != list_devices.end(); iter++ )
        msg_Dbg( p_this, "found device: %s", iter->c_str() );

    /* If no device name was specified, pick the 1st one */
    if( devicename.size() == 0 )
    {
        devicename = *list_devices.begin();
    }

    // Use the system device enumerator and class enumerator to find
    // a capture/preview device, such as a desktop USB video camera.
    IBaseFilter *p_device_filter =
        FindCaptureDevice( p_this, &devicename, 0, b_audio );
    if( p_device_filter )
        msg_Dbg( p_this, "using device: %s", devicename.c_str() );
    else
    {
        msg_Err( p_this, "can't use device: %s, unsupported device type",
                 devicename.c_str() );
        return VLC_EGENERIC;
    }

    // Retreive acceptable media types supported by device
    AM_MEDIA_TYPE media_types[MAX_MEDIA_TYPES];
    size_t media_count =
        EnumDeviceCaps( p_this, p_device_filter, p_sys->i_chroma,
                        p_sys->i_width, p_sys->i_height,
                        0, 0, 0, media_types, MAX_MEDIA_TYPES );

    /* Find out if the pin handles MEDIATYPE_Stream, in which case we
     * won't add a prefered media type as this doesn't seem to work well
     * -- to investigate. */
    vlc_bool_t b_stream_type = VLC_FALSE;
    for( size_t i = 0; i < media_count; i++ )
    {
        if( media_types[i].majortype == MEDIATYPE_Stream )
        {
            b_stream_type = VLC_TRUE;
            break;
        }
    }

    size_t mt_count = 0;
    AM_MEDIA_TYPE *mt = NULL;

    if( !b_stream_type && !b_audio )
    {
        // Insert prefered video media type
        AM_MEDIA_TYPE mtr;
        VIDEOINFOHEADER vh;

        mtr.majortype            = MEDIATYPE_Video;
        mtr.subtype              = MEDIASUBTYPE_I420;
        mtr.bFixedSizeSamples    = TRUE;
        mtr.bTemporalCompression = FALSE;
        mtr.pUnk                 = NULL;
        mtr.formattype           = FORMAT_VideoInfo;
        mtr.cbFormat             = sizeof(vh);
        mtr.pbFormat             = (BYTE *)&vh;

        memset(&vh, 0, sizeof(vh));

        vh.bmiHeader.biSize   = sizeof(vh.bmiHeader);
        vh.bmiHeader.biWidth  = p_sys->i_width > 0 ? p_sys->i_width : 320;
        vh.bmiHeader.biHeight = p_sys->i_height > 0 ? p_sys->i_height : 240;
        vh.bmiHeader.biPlanes      = 3;
        vh.bmiHeader.biBitCount    = 12;
        vh.bmiHeader.biCompression = VLC_FOURCC('I','4','2','0');
        vh.bmiHeader.biSizeImage   = vh.bmiHeader.biWidth * 12 *
            vh.bmiHeader.biHeight / 8;
        mtr.lSampleSize            = vh.bmiHeader.biSizeImage;

        mt_count = 1;
        mt = (AM_MEDIA_TYPE *)malloc( sizeof(AM_MEDIA_TYPE)*mt_count );
        CopyMediaType(mt, &mtr);
    }
    else if( !b_stream_type )
    {
        // Insert prefered audio media type
        AM_MEDIA_TYPE mtr;
        WAVEFORMATEX wf;

        mtr.majortype            = MEDIATYPE_Audio;
        mtr.subtype              = MEDIASUBTYPE_PCM;
        mtr.bFixedSizeSamples    = TRUE;
        mtr.bTemporalCompression = FALSE;
        mtr.lSampleSize          = 0;
        mtr.pUnk                 = NULL;
        mtr.formattype           = FORMAT_WaveFormatEx;
        mtr.cbFormat             = sizeof(wf);
        mtr.pbFormat             = (BYTE *)&wf;

        memset(&wf, 0, sizeof(wf));

        wf.wFormatTag = WAVE_FORMAT_PCM;
        wf.nChannels = 2;
        wf.nSamplesPerSec = 44100;
        wf.wBitsPerSample = 16;
        wf.nBlockAlign = wf.nSamplesPerSec * wf.wBitsPerSample / 8;
        wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
        wf.cbSize = 0;

        mt_count = 1;
        mt = (AM_MEDIA_TYPE *)malloc( sizeof(AM_MEDIA_TYPE)*mt_count );
        CopyMediaType(mt, &mtr);
    }

    if( media_count > 0 )
    {
        mt = (AM_MEDIA_TYPE *)realloc( mt, sizeof(AM_MEDIA_TYPE) *
                                       (mt_count + media_count) );

        // Order and copy returned media types according to arbitrary
        // fourcc priority
        for( size_t c = 0; c < media_count; c++ )
        {
            int slot_priority =
                GetFourCCPriority(GetFourCCFromMediaType(media_types[c]));
            size_t slot_copy = c;
            for( size_t d = c+1; d < media_count; d++ )
            {
                int priority =
                    GetFourCCPriority(GetFourCCFromMediaType(media_types[d]));
                if( priority > slot_priority )
                {
                    slot_priority = priority;
                    slot_copy = d;
                }
            }
            if( slot_copy != c )
            {
                mt[c+mt_count] = media_types[slot_copy];
                media_types[slot_copy] = media_types[c];
            }
            else
            {
                mt[c+mt_count] = media_types[c];
            }
        }
        mt_count += media_count;
    }

    /* Create and add our capture filter */
    CaptureFilter *p_capture_filter =
        new CaptureFilter( p_this, p_sys, mt, mt_count );
    p_sys->p_graph->AddFilter( p_capture_filter, 0 );

    /* Add the device filter to the graph (seems necessary with VfW before
     * accessing pin attributes). */
    p_sys->p_graph->AddFilter( p_device_filter, 0 );

    /* Attempt to connect one of this device's capture output pins */
    msg_Dbg( p_this, "connecting filters" );
    if( ConnectFilters( p_this, p_sys, p_device_filter, p_capture_filter ) )
    {
        /* Success */
        msg_Dbg( p_this, "filters connected successfully !" );

        dshow_stream_t dshow_stream;
        dshow_stream.b_pts = VLC_FALSE;
        dshow_stream.p_es = 0;
        dshow_stream.mt =
            p_capture_filter->CustomGetPin()->CustomGetMediaType();

        /* Show Device properties. Done here so the VLC stream is setup with
         * the proper parameters. */
        vlc_value_t val;
        var_Get( p_this, "dshow-config", &val );
        if( val.b_bool )
        {
            ShowDeviceProperties( p_this, p_sys->p_capture_graph_builder2,
                                  p_device_filter, b_audio );
        }

        ConfigTuner( p_this, p_sys->p_capture_graph_builder2,
                     p_device_filter );

        var_Get( p_this, "dshow-tuner", &val );
        if( val.b_bool && dshow_stream.mt.majortype != MEDIATYPE_Stream )
        {
            /* FIXME: we do MEDIATYPE_Stream later so we don't do it twice. */
            ShowTunerProperties( p_this, p_sys->p_capture_graph_builder2,
                                 p_device_filter, b_audio );
        }

        dshow_stream.mt =
            p_capture_filter->CustomGetPin()->CustomGetMediaType();

        dshow_stream.i_fourcc = GetFourCCFromMediaType( dshow_stream.mt );
        if( dshow_stream.i_fourcc )
        {
            if( dshow_stream.mt.majortype == MEDIATYPE_Video )
            {
                dshow_stream.header.video =
                    *(VIDEOINFOHEADER *)dshow_stream.mt.pbFormat;
                msg_Dbg( p_this, "MEDIATYPE_Video" );
                msg_Dbg( p_this, "selected video pin accepts format: %4.4s",
                         (char *)&dshow_stream.i_fourcc);
            }
            else if( dshow_stream.mt.majortype == MEDIATYPE_Audio )
            {
                dshow_stream.header.audio =
                    *(WAVEFORMATEX *)dshow_stream.mt.pbFormat;
                msg_Dbg( p_this, "MEDIATYPE_Audio" );
                msg_Dbg( p_this, "selected audio pin accepts format: %4.4s",
                         (char *)&dshow_stream.i_fourcc);
            }
            else if( dshow_stream.mt.majortype == MEDIATYPE_Stream )
            {
                msg_Dbg( p_this, "MEDIATYPE_Stream" );
                msg_Dbg( p_this, "selected stream pin accepts format: %4.4s",
                         (char *)&dshow_stream.i_fourcc);
            }
            else
            {
                msg_Dbg( p_this, "unknown stream majortype" );
                goto fail;
            }

            /* Add directshow elementary stream to our list */
            dshow_stream.p_device_filter = p_device_filter;
            dshow_stream.p_capture_filter = p_capture_filter;

            p_sys->pp_streams = (dshow_stream_t **)realloc( p_sys->pp_streams,
                sizeof(dshow_stream_t *) * (p_sys->i_streams + 1) );
            p_sys->pp_streams[p_sys->i_streams] = new dshow_stream_t;
            *p_sys->pp_streams[p_sys->i_streams++] = dshow_stream;

            return VLC_SUCCESS;
        }
    }

 fail:
    /* Remove filters from graph */
    p_sys->p_graph->RemoveFilter( p_device_filter );
    p_sys->p_graph->RemoveFilter( p_capture_filter );

    /* Release objects */
    p_device_filter->Release();
    p_capture_filter->Release();

    return VLC_EGENERIC;
}

static IBaseFilter *
FindCaptureDevice( vlc_object_t *p_this, string *p_devicename,
                   list<string> *p_listdevices, vlc_bool_t b_audio )
{
    IBaseFilter *p_base_filter = NULL;
    IMoniker *p_moniker = NULL;
    ULONG i_fetched;
    HRESULT hr;

    /* Create the system device enumerator */
    ICreateDevEnum *p_dev_enum = NULL;

    hr = CoCreateInstance( CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC,
                           IID_ICreateDevEnum, (void **)&p_dev_enum );
    if( FAILED(hr) )
    {
        msg_Err( p_this, "failed to create the device enumerator (0x%lx)", hr);
        return NULL;
    }

    /* Create an enumerator for the video capture devices */
    IEnumMoniker *p_class_enum = NULL;
    if( !b_audio )
        hr = p_dev_enum->CreateClassEnumerator( CLSID_VideoInputDeviceCategory,
                                                &p_class_enum, 0 );
    else
        hr = p_dev_enum->CreateClassEnumerator( CLSID_AudioInputDeviceCategory,
                                                &p_class_enum, 0 );
    p_dev_enum->Release();
    if( FAILED(hr) )
    {
        msg_Err( p_this, "failed to create the class enumerator (0x%lx)", hr );
        return NULL;
    }

    /* If there are no enumerators for the requested type, then
     * CreateClassEnumerator will succeed, but p_class_enum will be NULL */
    if( p_class_enum == NULL )
    {
        msg_Err( p_this, "no capture device was detected" );
        return NULL;
    }

    /* Enumerate the devices */

    /* Note that if the Next() call succeeds but there are no monikers,
     * it will return S_FALSE (which is not a failure). Therefore, we check
     * that the return code is S_OK instead of using SUCCEEDED() macro. */

    while( p_class_enum->Next( 1, &p_moniker, &i_fetched ) == S_OK )
    {
        /* Getting the property page to get the device name */
        IPropertyBag *p_bag;
        hr = p_moniker->BindToStorage( 0, 0, IID_IPropertyBag,
                                       (void **)&p_bag );
        if( SUCCEEDED(hr) )
        {
            VARIANT var;
            var.vt = VT_BSTR;
            hr = p_bag->Read( L"FriendlyName", &var, NULL );
            p_bag->Release();
            if( SUCCEEDED(hr) )
            {
                int i_convert = WideCharToMultiByte(CP_ACP, 0, var.bstrVal,
                        SysStringLen(var.bstrVal), NULL, 0, NULL, NULL);
                char *p_buf = (char *)alloca( i_convert ); p_buf[0] = 0;
                WideCharToMultiByte( CP_ACP, 0, var.bstrVal,
                        SysStringLen(var.bstrVal), p_buf, i_convert, NULL, NULL );
                SysFreeString(var.bstrVal);

                if( p_listdevices ) p_listdevices->push_back( p_buf );

                if( p_devicename && *p_devicename == string(p_buf) )
                {
                    /* Bind Moniker to a filter object */
                    hr = p_moniker->BindToObject( 0, 0, IID_IBaseFilter,
                                                  (void **)&p_base_filter );
                    if( FAILED(hr) )
                    {
                        msg_Err( p_this, "couldn't bind moniker to filter "
                                 "object (0x%lx)", hr );
                        p_moniker->Release();
                        p_class_enum->Release();
                        return NULL;
                    }
                    p_moniker->Release();
                    p_class_enum->Release();
                    return p_base_filter;
                }
            }
        }

        p_moniker->Release();
    }

    p_class_enum->Release();
    return NULL;
}

static size_t EnumDeviceCaps( vlc_object_t *p_this, IBaseFilter *p_filter,
                              int i_fourcc, int i_width, int i_height,
                              int i_channels, int i_samplespersec,
                              int i_bitspersample, AM_MEDIA_TYPE *mt,
                              size_t mt_max )
{
    IEnumPins *p_enumpins;
    IPin *p_output_pin;
    IEnumMediaTypes *p_enummt;
    size_t mt_count = 0;

    LONGLONG i_AvgTimePerFrame = 0;
    float r_fps = var_GetFloat( p_this, "dshow-fps" );
    if( r_fps )
        i_AvgTimePerFrame = 10000000000LL/(LONGLONG)(r_fps*1000.0f);

    if( FAILED(p_filter->EnumPins( &p_enumpins )) )
    {
        msg_Dbg( p_this, "EnumDeviceCaps failed: no pin enumeration !");
        return 0;
    }

    while( S_OK == p_enumpins->Next( 1, &p_output_pin, NULL ) )
    {
        PIN_INFO info;

        if( S_OK == p_output_pin->QueryPinInfo( &info ) )
        {
            msg_Dbg( p_this, "EnumDeviceCaps: %s pin: %S",
                     info.dir == PINDIR_INPUT ? "input" : "output",
                     info.achName );
            if( info.pFilter ) info.pFilter->Release();
        }

        p_output_pin->Release();
    }

    p_enumpins->Reset();

    while( !mt_count && p_enumpins->Next( 1, &p_output_pin, NULL ) == S_OK )
    {
        PIN_INFO info;

        if( S_OK == p_output_pin->QueryPinInfo( &info ) )
        {
            if( info.pFilter ) info.pFilter->Release();
            if( info.dir == PINDIR_INPUT )
            {
                p_output_pin->Release();
                continue;
            }
            msg_Dbg( p_this, "EnumDeviceCaps: trying pin %S", info.achName );
        }

        AM_MEDIA_TYPE *p_mt;

        /*
        ** Configure pin with a default compatible media if possible
        */

        IAMStreamConfig *pSC;
        if( SUCCEEDED(p_output_pin->QueryInterface( IID_IAMStreamConfig,
                                            (void **)&pSC )) )
        {
            int piCount, piSize;
            if( SUCCEEDED(pSC->GetNumberOfCapabilities(&piCount, &piSize)) )
            {
                BYTE *pSCC= (BYTE *)CoTaskMemAlloc(piSize);
                if( NULL != pSCC )
                {
                    for( int i=0; i<piCount; ++i )
                    {
                        if( SUCCEEDED(pSC->GetStreamCaps(i, &p_mt, pSCC)) )
                        {
                            int i_current_fourcc = GetFourCCFromMediaType( *p_mt );

                            if( !i_current_fourcc || (i_fourcc && (i_current_fourcc != i_fourcc)) )
                            {
                                // incompatible or unrecognized chroma, try next media type
                                FreeMediaType( *p_mt );
                                CoTaskMemFree( (PVOID)p_mt );
                                continue;
                            }

                            if( MEDIATYPE_Video == p_mt->majortype
                                    && FORMAT_VideoInfo == p_mt->formattype )
                            {
                                VIDEO_STREAM_CONFIG_CAPS *pVSCC = reinterpret_cast<VIDEO_STREAM_CONFIG_CAPS*>(pSCC);
                                VIDEOINFOHEADER *pVih = reinterpret_cast<VIDEOINFOHEADER*>(p_mt->pbFormat);

                                if( i_AvgTimePerFrame )
                                {
                                    if( pVSCC->MinFrameInterval > i_AvgTimePerFrame
                                      || i_AvgTimePerFrame > pVSCC->MaxFrameInterval )
                                    {
                                        // required frame rate not compatible, try next media type
                                        FreeMediaType( *p_mt );
                                        CoTaskMemFree( (PVOID)p_mt );
                                        continue;
                                    }
                                    pVih->AvgTimePerFrame = i_AvgTimePerFrame;
                                }

                                if( i_width )
                                {
                                    if( i_width % pVSCC->OutputGranularityX
                                     || pVSCC->MinOutputSize.cx > i_width
                                     || i_width > pVSCC->MaxOutputSize.cx )
                                    {
                                        // required width not compatible, try next media type
                                        FreeMediaType( *p_mt );
                                        CoTaskMemFree( (PVOID)p_mt );
                                        continue;
                                    }
                                    pVih->bmiHeader.biWidth = i_width;
                                }

                                if( i_height )
                                {
                                    if( i_height % pVSCC->OutputGranularityY
                                     || pVSCC->MinOutputSize.cy > i_height 
                                     || i_height > pVSCC->MaxOutputSize.cy )
                                    {
                                        // required height not compatible, try next media type
                                        FreeMediaType( *p_mt );
                                        CoTaskMemFree( (PVOID)p_mt );
                                        continue;
                                    }
                                    pVih->bmiHeader.biHeight = i_height;
                                }

                                // Set the sample size and image size.
                                // (Round the image width up to a DWORD boundary.)
                                p_mt->lSampleSize = pVih->bmiHeader.biSizeImage = 
                                    ((pVih->bmiHeader.biWidth + 3) & ~3) *
                                    pVih->bmiHeader.biHeight * (pVih->bmiHeader.biBitCount>>3);

                                // no cropping, use full video input buffer
                                memset(&(pVih->rcSource), 0, sizeof(RECT));
                                memset(&(pVih->rcTarget), 0, sizeof(RECT));

                                // select this format as default
                                if( SUCCEEDED( pSC->SetFormat(p_mt) ) )
                                {
                                    msg_Dbg( p_this, "EnumDeviceCaps: input pin video format configured");
                                    // no need to check any more media types 
                                    i = piCount;
                                }
                                else FreeMediaType( *p_mt );
                            }
                            else if( p_mt->majortype == MEDIATYPE_Audio
                                    && p_mt->formattype == FORMAT_WaveFormatEx )
                            {
                                AUDIO_STREAM_CONFIG_CAPS *pASCC = reinterpret_cast<AUDIO_STREAM_CONFIG_CAPS*>(pSCC);
                                WAVEFORMATEX *pWfx = reinterpret_cast<WAVEFORMATEX*>(p_mt->pbFormat);

                                if( i_channels )
                                {
                                    if( i_channels % pASCC->ChannelsGranularity
                                     || (unsigned int)i_channels < pASCC->MinimumChannels
                                     || (unsigned int)i_channels > pASCC->MaximumChannels )
                                    {
                                        // required channels not compatible, try next media type
                                        FreeMediaType( *p_mt );
                                        CoTaskMemFree( (PVOID)p_mt );
                                        continue;
                                    }
                                    pWfx->nChannels = i_channels;
                                }

                                if( i_samplespersec )
                                {
                                    if( i_samplespersec % pASCC->BitsPerSampleGranularity
                                     || (unsigned int)i_samplespersec < pASCC->MinimumSampleFrequency
                                     || (unsigned int)i_samplespersec > pASCC->MaximumSampleFrequency )
                                    {
                                        // required sampling rate not compatible, try next media type
                                        FreeMediaType( *p_mt );
                                        CoTaskMemFree( (PVOID)p_mt );
                                        continue;
                                    }
                                    pWfx->nSamplesPerSec = i_samplespersec;
                                }

                                if( i_bitspersample )
                                {
                                    if( i_bitspersample % pASCC->BitsPerSampleGranularity
                                     || (unsigned int)i_bitspersample < pASCC->MinimumBitsPerSample
                                     || (unsigned int)i_bitspersample > pASCC->MaximumBitsPerSample )
                                    {
                                        // required sample size not compatible, try next media type
                                        FreeMediaType( *p_mt );
                                        CoTaskMemFree( (PVOID)p_mt );
                                        continue;
                                    }
                                    pWfx->wBitsPerSample = i_bitspersample;
                                }

                                // select this format as default
                                if( SUCCEEDED( pSC->SetFormat(p_mt) ) )
                                {
                                    msg_Dbg( p_this, "EnumDeviceCaps: input pin default format configured");
                                    // no need to check any more media types 
                                    i = piCount;
                                }
                            }
                            FreeMediaType( *p_mt );
                            CoTaskMemFree( (PVOID)p_mt );
                        }
                    }
                    CoTaskMemFree( (LPVOID)pSCC );
                }
            }
            pSC->Release();
        }

        /*
        ** Probe pin for available medias (may be a previously configured one)
        */

        if( FAILED( p_output_pin->EnumMediaTypes( &p_enummt ) ) )
        {
            p_output_pin->Release();
            continue;
        }


        while( p_enummt->Next( 1, &p_mt, NULL ) == S_OK )
        {
            int i_current_fourcc = GetFourCCFromMediaType( *p_mt );
            if( i_current_fourcc && p_mt->majortype == MEDIATYPE_Video
                && p_mt->formattype == FORMAT_VideoInfo )
            {
                int i_current_width = ((VIDEOINFOHEADER *)p_mt->pbFormat)->bmiHeader.biWidth;
                int i_current_height = ((VIDEOINFOHEADER *)p_mt->pbFormat)->bmiHeader.biHeight;
                LONGLONG i_current_atpf = ((VIDEOINFOHEADER *)p_mt->pbFormat)->AvgTimePerFrame;

                if( i_current_height < 0 )
                    i_current_height = -i_current_height; 

                msg_Dbg( p_this, "EnumDeviceCaps: input pin "
                         "accepts chroma: %4.4s, width:%i, height:%i, fps:%f",
                         (char *)&i_current_fourcc, i_current_width,
                         i_current_height, (10000000.0f/((float)i_current_atpf)) );

                if( ( !i_fourcc || i_fourcc == i_current_fourcc ) &&
                    ( !i_width || i_width == i_current_width ) &&
                    ( !i_height || i_height == i_current_height ) &&
                    ( !i_AvgTimePerFrame || i_AvgTimePerFrame == i_current_atpf ) &&
                    mt_count < mt_max )
                {
                    /* Pick match */
                    mt[mt_count++] = *p_mt;
                }
                else FreeMediaType( *p_mt );
            }
            else if( i_current_fourcc && p_mt->majortype == MEDIATYPE_Audio 
                    && p_mt->formattype == FORMAT_WaveFormatEx)
            {
                int i_current_channels =
                    ((WAVEFORMATEX *)p_mt->pbFormat)->nChannels;
                int i_current_samplespersec =
                    ((WAVEFORMATEX *)p_mt->pbFormat)->nSamplesPerSec;
                int i_current_bitspersample =
                    ((WAVEFORMATEX *)p_mt->pbFormat)->wBitsPerSample;

                msg_Dbg( p_this, "EnumDeviceCaps: input pin "
                         "accepts format: %4.4s, channels:%i, "
                         "samples/sec:%i bits/sample:%i",
                         (char *)&i_current_fourcc, i_current_channels,
                         i_current_samplespersec, i_current_bitspersample);

                if( (!i_channels || i_channels == i_current_channels) &&
                    (!i_samplespersec ||
                     i_samplespersec == i_current_samplespersec) &&
                    (!i_bitspersample ||
                     i_bitspersample == i_current_bitspersample) &&
                    mt_count < mt_max )
                {
                    /* Pick  match */
                    mt[mt_count++] = *p_mt;

                    /* Setup a few properties like the audio latency */
                    IAMBufferNegotiation *p_ambuf;
                    if( SUCCEEDED( p_output_pin->QueryInterface(
                          IID_IAMBufferNegotiation, (void **)&p_ambuf ) ) )
                    {
                        ALLOCATOR_PROPERTIES AllocProp;
                        AllocProp.cbAlign = -1;

                        /* 100 ms of latency */
                        AllocProp.cbBuffer = i_current_channels *
                          i_current_samplespersec *
                          i_current_bitspersample / 8 / 10;

                        AllocProp.cbPrefix = -1;
                        AllocProp.cBuffers = -1;
                        p_ambuf->SuggestAllocatorProperties( &AllocProp );
                        p_ambuf->Release();
                    }
                }
                else FreeMediaType( *p_mt );
            }
            else if( i_current_fourcc && p_mt->majortype == MEDIATYPE_Stream )
            {
                msg_Dbg( p_this, "EnumDeviceCaps: input pin "
                         "accepts stream format: %4.4s",
                         (char *)&i_current_fourcc );

                if( ( !i_fourcc || i_fourcc == i_current_fourcc ) &&
                    mt_count < mt_max )
                {
                    /* Pick match */
                    mt[mt_count++] = *p_mt;
                    i_fourcc = i_current_fourcc;
                }
                else FreeMediaType( *p_mt );
            }
            else
            {
                char *psz_type = "unknown";
                if( p_mt->majortype == MEDIATYPE_Video ) psz_type = "video";
                if( p_mt->majortype == MEDIATYPE_Audio ) psz_type = "audio";
                if( p_mt->majortype == MEDIATYPE_Stream ) psz_type = "stream";
                msg_Dbg( p_this, "EnumDeviceCaps: input pin media: unknown format "
                         "(%s %4.4s)", psz_type, (char *)&p_mt->subtype );
                FreeMediaType( *p_mt );
            }
            CoTaskMemFree( (PVOID)p_mt );
        }

        p_enummt->Release();
        p_output_pin->Release();
    }

    p_enumpins->Release();
    return mt_count;
}

/*****************************************************************************
 * ReadCompressed: reads compressed (MPEG/DV) data from the device.
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, otherwise the number of
 * bytes.
 *****************************************************************************/
static block_t *ReadCompressed( access_t *p_access )
{
    access_sys_t   *p_sys = p_access->p_sys;
    dshow_stream_t *p_stream = NULL;
    VLCMediaSample sample;

    /* Read 1 DV/MPEG frame (they contain the video and audio data) */

    /* There must be only 1 elementary stream to produce a valid stream
     * of MPEG or DV data */
    p_stream = p_sys->pp_streams[0];

    while( 1 )
    {
        if( p_access->b_die || p_access->b_error ) return 0;

        /* Get new sample/frame from the elementary stream (blocking). */
        vlc_mutex_lock( &p_sys->lock );

        if( p_stream->p_capture_filter->CustomGetPin()
              ->CustomGetSample( &sample ) != S_OK )
        {
            /* No data available. Wait until some data has arrived */
            vlc_cond_wait( &p_sys->wait, &p_sys->lock );
            vlc_mutex_unlock( &p_sys->lock );
            continue;
        }

        vlc_mutex_unlock( &p_sys->lock );

        /*
         * We got our sample
         */
        block_t *p_block;
        uint8_t *p_data;
        int i_data_size = sample.p_sample->GetActualDataLength();

        if( !i_data_size || !(p_block = block_New( p_access, i_data_size )) )
        {
            sample.p_sample->Release();
            continue;
        }

        sample.p_sample->GetPointer( &p_data );
        p_access->p_vlc->pf_memcpy( p_block->p_buffer, p_data, i_data_size );
        sample.p_sample->Release();

        /* The caller got what he wanted */
        return p_block;
    }

    return 0; /* never reached */
}

/****************************************************************************
 * Demux:
 ****************************************************************************/
static int Demux( demux_t *p_demux )
{
    access_sys_t *p_sys = (access_sys_t *)p_demux->p_sys;
    dshow_stream_t *p_stream = NULL;
    VLCMediaSample sample;
    int i_data_size, i_stream;
    uint8_t *p_data;
    block_t *p_block;

    vlc_mutex_lock( &p_sys->lock );

    /* Try to grab an audio sample (audio has a higher priority) */
    for( i_stream = 0; i_stream < p_sys->i_streams; i_stream++ )
    {
        p_stream = p_sys->pp_streams[i_stream];
        if( p_stream->mt.majortype == MEDIATYPE_Audio &&
            p_stream->p_capture_filter &&
            p_stream->p_capture_filter->CustomGetPin()
              ->CustomGetSample( &sample ) == S_OK )
        {
            break;
        }
    }
    /* Try to grab a video sample */
    if( i_stream == p_sys->i_streams )
    {
        for( i_stream = 0; i_stream < p_sys->i_streams; i_stream++ )
        {
            p_stream = p_sys->pp_streams[i_stream];
            if( p_stream->p_capture_filter &&
                p_stream->p_capture_filter->CustomGetPin()
                    ->CustomGetSample( &sample ) == S_OK )
            {
                break;
            }
        }
    }

    vlc_mutex_unlock( &p_sys->lock );

    if( i_stream == p_sys->i_streams )
    {
        /* Sleep so we do not consume all the cpu, 10ms seems
         * like a good value (100fps) */
        msleep( 10000 );
        return 1;
    }

    /*
     * We got our sample
     */
    i_data_size = sample.p_sample->GetActualDataLength();
    sample.p_sample->GetPointer( &p_data );

    REFERENCE_TIME i_pts, i_end_date;
    HRESULT hr = sample.p_sample->GetTime( &i_pts, &i_end_date );
    if( hr != VFW_S_NO_STOP_TIME && hr != S_OK ) i_pts = 0;

    if( !i_pts )
    {
        if( p_stream->mt.majortype == MEDIATYPE_Video || !p_stream->b_pts )
        {
            /* Use our data timestamp */
            i_pts = sample.i_timestamp;
            p_stream->b_pts = VLC_TRUE;
        }
    }

    i_pts /= 10; /* Dshow works with 100 nano-seconds resolution */

#if 0
    msg_Dbg( p_demux, "Read() stream: %i, size: %i, PTS: "I64Fd,
             i_stream, i_data_size, i_pts );
#endif

    p_block = block_New( p_demux, i_data_size );
    p_demux->p_vlc->pf_memcpy( p_block->p_buffer, p_data, i_data_size );
    p_block->i_pts = p_block->i_dts = i_pts;
    sample.p_sample->Release();

    es_out_Control( p_demux->out, ES_OUT_SET_PCR, i_pts > 0 ? i_pts : 0 );
    es_out_Send( p_demux->out, p_stream->p_es, p_block );

    return 1;
}

/*****************************************************************************
 * AccessControl:
 *****************************************************************************/
static int AccessControl( access_t *p_access, int i_query, va_list args )
{
    vlc_bool_t   *pb_bool;
    int          *pi_int;
    int64_t      *pi_64;

    switch( i_query )
    {
    /* */
    case ACCESS_CAN_SEEK:
    case ACCESS_CAN_FASTSEEK:
    case ACCESS_CAN_PAUSE:
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
        *pi_64 = (int64_t)var_GetInteger( p_access, "dshow-caching" ) * 1000;
        break;

    /* */
    case ACCESS_SET_PAUSE_STATE:
    case ACCESS_GET_TITLE_INFO:
    case ACCESS_SET_TITLE:
    case ACCESS_SET_SEEKPOINT:
    case ACCESS_SET_PRIVATE_ID_STATE:
        return VLC_EGENERIC;

    default:
        msg_Warn( p_access, "unimplemented query in control" );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/****************************************************************************
 * DemuxControl:
 ****************************************************************************/
static int DemuxControl( demux_t *p_demux, int i_query, va_list args )
{
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
        *pi64 = (int64_t)var_GetInteger( p_demux, "dshow-caching" ) * 1000;
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
 * config variable callback
 *****************************************************************************/
static int FindDevicesCallback( vlc_object_t *p_this, char const *psz_name,
                               vlc_value_t newval, vlc_value_t oldval, void * )
{
    module_config_t *p_item;
    vlc_bool_t b_audio = VLC_FALSE;
    int i;

    p_item = config_FindConfig( p_this, psz_name );
    if( !p_item ) return VLC_SUCCESS;

    if( !strcmp( psz_name, "dshow-adev" ) ) b_audio = VLC_TRUE;

    /* Clear-up the current list */
    if( p_item->i_list )
    {
        /* Keep the 2 first entries */
        for( i = 2; i < p_item->i_list; i++ )
        {
            free( p_item->ppsz_list[i] );
            free( p_item->ppsz_list_text[i] );
        }
        /* TODO: Remove when no more needed */
        p_item->ppsz_list[i] = NULL;
        p_item->ppsz_list_text[i] = NULL;
    }
    p_item->i_list = 2;

    /* Find list of devices */
    list<string> list_devices;

    /* Initialize OLE/COM */
    CoInitialize( 0 );

    FindCaptureDevice( p_this, NULL, &list_devices, b_audio );

    /* Uninitialize OLE/COM */
    CoUninitialize();

    if( !list_devices.size() ) return VLC_SUCCESS;

    p_item->ppsz_list =
        (char **)realloc( p_item->ppsz_list,
                          (list_devices.size()+3) * sizeof(char *) );
    p_item->ppsz_list_text =
        (char **)realloc( p_item->ppsz_list_text,
                          (list_devices.size()+3) * sizeof(char *) );

    list<string>::iterator iter;
    for( iter = list_devices.begin(), i = 2; iter != list_devices.end();
         iter++, i++ )
    {
        p_item->ppsz_list[i] = strdup( iter->c_str() );
        p_item->ppsz_list_text[i] = NULL;
        p_item->i_list++;
    }
    p_item->ppsz_list[i] = NULL;
    p_item->ppsz_list_text[i] = NULL;

    /* Signal change to the interface */
    p_item->b_dirty = VLC_TRUE;

    return VLC_SUCCESS;
}

static int ConfigDevicesCallback( vlc_object_t *p_this, char const *psz_name,
                               vlc_value_t newval, vlc_value_t oldval, void * )
{
    module_config_t *p_item;
    vlc_bool_t b_audio = VLC_FALSE;

    /* Initialize OLE/COM */
    CoInitialize( 0 );

    p_item = config_FindConfig( p_this, psz_name );
    if( !p_item ) return VLC_SUCCESS;

    if( !strcmp( psz_name, "dshow-adev" ) ) b_audio = VLC_TRUE;

    string devicename;

    if( newval.psz_string && *newval.psz_string )
    {
        devicename = newval.psz_string;
    }
    else
    {
        /* If no device name was specified, pick the 1st one */
        list<string> list_devices;

        /* Enumerate devices */
        FindCaptureDevice( p_this, NULL, &list_devices, b_audio );
        if( !list_devices.size() ) return VLC_EGENERIC;
        devicename = *list_devices.begin();
    }

    IBaseFilter *p_device_filter =
        FindCaptureDevice( p_this, &devicename, NULL, b_audio );
    if( p_device_filter )
    {
        ShowPropertyPage( p_device_filter );
    }
    else
    {
        /* Uninitialize OLE/COM */
        CoUninitialize();

        msg_Err( p_this, "didn't find device: %s", devicename.c_str() );
        return VLC_EGENERIC;
    }

    /* Uninitialize OLE/COM */
    CoUninitialize();

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Properties
 *****************************************************************************/
static void ShowPropertyPage( IUnknown *obj )
{
    ISpecifyPropertyPages *p_spec;
    CAUUID cauuid;

    HRESULT hr = obj->QueryInterface( IID_ISpecifyPropertyPages,
                                      (void **)&p_spec );
    if( FAILED(hr) ) return;

    if( SUCCEEDED(p_spec->GetPages( &cauuid )) )
    {
        if( cauuid.cElems > 0 )
        {
            HWND hwnd_desktop = ::GetDesktopWindow();

            OleCreatePropertyFrame( hwnd_desktop, 30, 30, NULL, 1, &obj,
                                    cauuid.cElems, cauuid.pElems, 0, 0, NULL );

            CoTaskMemFree( cauuid.pElems );
        }
        p_spec->Release();
    }
}

static void ShowDeviceProperties( vlc_object_t *p_this,
                                  ICaptureGraphBuilder2 *p_graph,
                                  IBaseFilter *p_device_filter,
                                  vlc_bool_t b_audio )
{
    HRESULT hr;
    msg_Dbg( p_this, "Configuring Device Properties" );

    /*
     * Video or audio capture filter page
     */
    ShowPropertyPage( p_device_filter );

    /*
     * Audio capture pin
     */
    if( p_graph && b_audio )
    {
        IAMStreamConfig *p_SC;

        msg_Dbg( p_this, "Showing WDM Audio Configuration Pages" );

        hr = p_graph->FindInterface( &PIN_CATEGORY_CAPTURE,
                                     &MEDIATYPE_Audio, p_device_filter,
                                     IID_IAMStreamConfig, (void **)&p_SC );
        if( SUCCEEDED(hr) )
        {
            ShowPropertyPage(p_SC);
            p_SC->Release();
        }

        /*
         * TV Audio filter
         */
        IAMTVAudio *p_TVA;
        HRESULT hr = p_graph->FindInterface( &PIN_CATEGORY_CAPTURE,
                                             &MEDIATYPE_Audio, p_device_filter,
                                             IID_IAMTVAudio, (void **)&p_TVA );
        if( SUCCEEDED(hr) )
        {
            ShowPropertyPage(p_TVA);
            p_TVA->Release();
        }
    }

    /*
     * Video capture pin
     */
    if( p_graph && !b_audio )
    {
        IAMStreamConfig *p_SC;

        msg_Dbg( p_this, "Showing WDM Video Configuration Pages" );

        hr = p_graph->FindInterface( &PIN_CATEGORY_CAPTURE,
                                     &MEDIATYPE_Interleaved, p_device_filter,
                                     IID_IAMStreamConfig, (void **)&p_SC );
        if( FAILED(hr) )
        {
            hr = p_graph->FindInterface( &PIN_CATEGORY_CAPTURE,
                                         &MEDIATYPE_Video, p_device_filter,
                                         IID_IAMStreamConfig, (void **)&p_SC );
        }

        if( FAILED(hr) )
        {
            hr = p_graph->FindInterface( &PIN_CATEGORY_CAPTURE,
                                         &MEDIATYPE_Stream, p_device_filter,
                                         IID_IAMStreamConfig, (void **)&p_SC );
        }

        if( SUCCEEDED(hr) )
        {
            ShowPropertyPage(p_SC);
            p_SC->Release();
        }
    }
}

static void ShowTunerProperties( vlc_object_t *p_this,
                                 ICaptureGraphBuilder2 *p_graph,
                                 IBaseFilter *p_device_filter,
                                 vlc_bool_t b_audio )
{
    HRESULT hr;
    msg_Dbg( p_this, "Configuring Tuner Properties" );

    if( !p_graph || b_audio ) return;

    IAMTVTuner *p_TV;
    hr = p_graph->FindInterface( &PIN_CATEGORY_CAPTURE,
                                 &MEDIATYPE_Interleaved, p_device_filter,
                                 IID_IAMTVTuner, (void **)&p_TV );
    if( FAILED(hr) )
    {
        hr = p_graph->FindInterface( &PIN_CATEGORY_CAPTURE,
                                     &MEDIATYPE_Video, p_device_filter,
                                     IID_IAMTVTuner, (void **)&p_TV );
    }

    if( FAILED(hr) )
    {
        hr = p_graph->FindInterface( &PIN_CATEGORY_CAPTURE,
                                     &MEDIATYPE_Stream, p_device_filter,
                                     IID_IAMTVTuner, (void **)&p_TV );
    }

    if( SUCCEEDED(hr) )
    {
        ShowPropertyPage(p_TV);
        p_TV->Release();
    }
}

static void ConfigTuner( vlc_object_t *p_this, ICaptureGraphBuilder2 *p_graph,
                         IBaseFilter *p_device_filter )
{
    int i_channel, i_country, i_input;
    long l_modes = 0;
    IAMTVTuner *p_TV;
    HRESULT hr;

    if( !p_graph ) return;

    i_channel = var_GetInteger( p_this, "dshow-tuner-channel" );
    i_country = var_GetInteger( p_this, "dshow-tuner-country" );
    i_input = var_GetInteger( p_this, "dshow-tuner-input" );

    if( !i_channel && !i_country && !i_input ) return; /* Nothing to do */

    msg_Dbg( p_this, "tuner config: channel %i, country %i, input type %i",
             i_channel, i_country, i_input );

    hr = p_graph->FindInterface( &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Interleaved,
                                 p_device_filter, IID_IAMTVTuner,
                                 (void **)&p_TV );
    if( FAILED(hr) )
    {
        hr = p_graph->FindInterface( &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video,
                                     p_device_filter, IID_IAMTVTuner,
                                     (void **)&p_TV );
    }

    if( FAILED(hr) )
    {
        hr = p_graph->FindInterface( &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Stream,
                                     p_device_filter, IID_IAMTVTuner,
                                     (void **)&p_TV );
    }

    if( FAILED(hr) )
    {
        msg_Dbg( p_this, "couldn't find tuner interface" );
        return;
    }

    hr = p_TV->GetAvailableModes( &l_modes );
    if( SUCCEEDED(hr) && (l_modes & AMTUNER_MODE_TV) )
    {
        hr = p_TV->put_Mode( AMTUNER_MODE_TV );
    }

    if( i_input == 1 ) p_TV->put_InputType( 0, TunerInputCable );
    else if( i_input == 2 ) p_TV->put_InputType( 0, TunerInputAntenna );

    p_TV->put_CountryCode( i_country );
    p_TV->put_Channel( i_channel, AMTUNER_SUBCHAN_NO_TUNE,
                       AMTUNER_SUBCHAN_NO_TUNE );
    p_TV->Release();
}
