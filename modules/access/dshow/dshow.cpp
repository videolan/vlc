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

#include "filter.h"

/*****************************************************************************
 * Access: local prototypes
 *****************************************************************************/
static int AccessRead    ( access_t *, byte_t *, int );
static int ReadCompressed( access_t *, byte_t *, int );
static int AccessControl ( access_t *, int, va_list );

static int OpenDevice( access_t *, string, vlc_bool_t );
static IBaseFilter *FindCaptureDevice( vlc_object_t *, string *,
                                       list<string> *, vlc_bool_t );
static size_t EnumDeviceCaps( vlc_object_t *, IBaseFilter *,
                                     int, int, int, int, int, int, AM_MEDIA_TYPE *mt, size_t mt_max);
static bool ConnectFilters( access_t *, IBaseFilter *, CaptureFilter * );

static int FindDevicesCallback( vlc_object_t *, char const *,
                                vlc_value_t, vlc_value_t, void * );
static int ConfigDevicesCallback( vlc_object_t *, char const *,
                                  vlc_value_t, vlc_value_t, void * );

static void PropertiesPage( vlc_object_t *, IBaseFilter *,
                            ICaptureGraphBuilder2 *, vlc_bool_t );

#if 0
    /* Debug only, use this to find out GUIDs */
    unsigned char p_st[];
    UuidToString( (IID *)&IID_IAMBufferNegotiation, &p_st );
    msg_Err( p_access, "BufferNegotiation: %s" , p_st );
#endif

/*
 * header:
 *  fcc  ".dsh"
 *  u32    stream count
 *      fcc "auds"|"vids"       0
 *      fcc codec               4
 *      if vids
 *          u32 width           8
 *          u32 height          12
 *          u32 padding         16
 *      if auds
 *          u32 channels        12
 *          u32 samplerate      8
 *          u32 samplesize      16
 *
 * data:
 *  u32     stream number
 *  u32     data size
 *  u8      data
 */

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static char *ppsz_vdev[] = { "", "none" };
static char *ppsz_vdev_text[] = { N_("Default"), N_("None") };
static char *ppsz_adev[] = { "", "none" };
static char *ppsz_adev_text[] = { N_("Default"), N_("None") };

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
#define CONFIG_TEXT N_("Device properties")
#define CONFIG_LONGTEXT N_( \
    "Show the properties dialog of the selected device before starting the " \
    "stream.")

static int  AccessOpen ( vlc_object_t * );
static void AccessClose( vlc_object_t * );

static int  DemuxOpen  ( vlc_object_t * );
static void DemuxClose ( vlc_object_t * );

vlc_module_begin();
    set_shortname( _("DirectShow") );
    set_description( _("DirectShow input") );
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

    add_bool( "dshow-config", VLC_FALSE, NULL, CONFIG_TEXT, CONFIG_LONGTEXT,
              VLC_FALSE );

    add_shortcut( "dshow" );
    set_capability( "access2", 0 );
    set_callbacks( AccessOpen, AccessClose );

    add_submodule();
    set_description( _("DirectShow demuxer") );
    add_shortcut( "dshow" );
    set_capability( "demux2", 200 );
    set_callbacks( DemuxOpen, DemuxClose );

vlc_module_end();

/****************************************************************************
 * DirectShow elementary stream descriptor
 ****************************************************************************/
typedef struct dshow_stream_t
{
    string          devicename;
    IBaseFilter     *p_device_filter;
    CaptureFilter   *p_capture_filter;
    AM_MEDIA_TYPE   mt;
    int             i_fourcc;
    vlc_bool_t      b_invert;

    union
    {
      VIDEOINFOHEADER video;
      WAVEFORMATEX    audio;

    } header;

    vlc_bool_t      b_pts;

} dshow_stream_t;

/****************************************************************************
 * Access descriptor declaration
 ****************************************************************************/
#define MAX_CROSSBAR_DEPTH 10

typedef struct CrossbarRouteRec {
    IAMCrossbar *pXbar;
    LONG        VideoInputIndex;
    LONG        VideoOutputIndex;
    LONG        AudioInputIndex;
    LONG        AudioOutputIndex;
} CrossbarRoute;

struct access_sys_t
{
    /* These 2 must be left at the beginning */
    vlc_mutex_t lock;
    vlc_cond_t  wait;

    IFilterGraph           *p_graph;
    ICaptureGraphBuilder2  *p_capture_graph_builder2;
    IMediaControl          *p_control;

    int                     i_crossbar_route_depth;
    CrossbarRoute           crossbar_routes[MAX_CROSSBAR_DEPTH];

    /* header */
    int     i_header_size;
    int     i_header_pos;
    uint8_t *p_header;

    /* list of elementary streams */
    dshow_stream_t **pp_streams;
    int            i_streams;
    int            i_current_stream;

    /* misc properties */
    int            i_mtu;
    int            i_width;
    int            i_height;
    int            i_chroma;
    int            b_audio;
};

/****************************************************************************
 * DirectShow utility functions
 ****************************************************************************/
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

static void DeleteCrossbarRoutes( access_sys_t *p_sys )
{
    /* Remove crossbar filters from graph */
    for( int i = 0; i < p_sys->i_crossbar_route_depth; i++ )
    {
        p_sys->crossbar_routes[i].pXbar->Release();
    }
    p_sys->i_crossbar_route_depth = 0;
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

static void ReleaseDirectShow( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;

    msg_Dbg( p_access, "Releasing DirectShow");

    DeleteDirectShowGraph( p_sys );

    /* Uninitialize OLE/COM */
    CoUninitialize();

    free( p_sys->p_header );
    /* Remove filters from graph */
    for( int i = 0; i < p_sys->i_streams; i++ )
    {
        delete p_sys->pp_streams[i];
    }
    free( p_sys->pp_streams );
    free( p_sys );
}

/*****************************************************************************
 * Open: open direct show device
 *****************************************************************************/
static int AccessOpen( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys;
    vlc_value_t  val;

    /* Get/parse options and open device(s) */
    string vdevname, adevname;
    int i_width = 0, i_height = 0, i_chroma = 0;

    var_Create( p_access, "dshow-config", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);

    var_Create( p_access, "dshow-vdev", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    var_Get( p_access, "dshow-vdev", &val );
    if( val.psz_string ) vdevname = string( val.psz_string );
    if( val.psz_string ) free( val.psz_string );

    var_Create( p_access, "dshow-adev", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    var_Get( p_access, "dshow-adev", &val );
    if( val.psz_string ) adevname = string( val.psz_string );
    if( val.psz_string ) free( val.psz_string );

    var_Create( p_access, "dshow-size", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    var_Get( p_access, "dshow-size", &val );
    if( val.psz_string && *val.psz_string )
    {
        if( !strcmp( val.psz_string, "subqcif" ) )
        {
            i_width  = 128; i_height = 96;
        }
        else if( !strcmp( val.psz_string, "qsif" ) )
        {
            i_width  = 160; i_height = 120;
        }
        else if( !strcmp( val.psz_string, "qcif" ) )
        {
            i_width  = 176; i_height = 144;
        }
        else if( !strcmp( val.psz_string, "sif" ) )
        {
            i_width  = 320; i_height = 240;
        }
        else if( !strcmp( val.psz_string, "cif" ) )
        {
            i_width  = 352; i_height = 288;
        }
        else if( !strcmp( val.psz_string, "vga" ) )
        {
            i_width  = 640; i_height = 480;
        }
        else
        {
            /* Width x Height */
            char *psz_parser;
            i_width = strtol( val.psz_string, &psz_parser, 0 );
            if( *psz_parser == 'x' || *psz_parser == 'X')
            {
                i_height = strtol( psz_parser + 1, &psz_parser, 0 );
            }
            msg_Dbg( p_access, "Width x Height %dx%d", i_width, i_height );
        }
    }
    if( val.psz_string ) free( val.psz_string );

    var_Create( p_access, "dshow-chroma", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    var_Get( p_access, "dshow-chroma", &val );
    if( val.psz_string && strlen( val.psz_string ) >= 4 )
    {
        i_chroma = VLC_FOURCC( val.psz_string[0], val.psz_string[1],
                               val.psz_string[2], val.psz_string[3] );
    }
    if( val.psz_string ) free( val.psz_string );

    /* Setup Access */
    p_access->pf_read = AccessRead;
    p_access->pf_block = NULL;
    p_access->pf_control = AccessControl;
    p_access->pf_seek = NULL;
    p_access->info.i_update = 0;
    p_access->info.i_size = 0;
    p_access->info.i_pos = 0;
    p_access->info.b_eof = VLC_FALSE;
    p_access->info.i_title = 0;
    p_access->info.i_seekpoint = 0;
    p_access->p_sys = p_sys = (access_sys_t *)malloc( sizeof( access_sys_t ) );
    memset( p_sys, 0, sizeof( access_sys_t ) );

    var_Create( p_access, "dshow-caching",
                VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    /* Initialize OLE/COM */
    CoInitialize( 0 );

    /* Initialize some data */
    p_sys->i_streams = 0;
    p_sys->pp_streams = (dshow_stream_t **)malloc( 1 );
    p_sys->i_width = i_width;
    p_sys->i_height = i_height;
    p_sys->i_chroma = i_chroma;
    p_sys->b_audio = VLC_TRUE;
    p_sys->i_mtu = 0;

    /* Create header */
    p_sys->i_header_size = 8;
    p_sys->p_header      = (uint8_t *)malloc( p_sys->i_header_size );
    memcpy(  &p_sys->p_header[0], ".dsh", 4 );
    SetDWBE( &p_sys->p_header[4], 1 );
    p_sys->i_header_pos = p_sys->i_header_size;

    p_sys->p_graph = NULL;
    p_sys->p_capture_graph_builder2 = NULL;
    p_sys->p_control = NULL;

    /* Build directshow graph */
    CreateDirectShowGraph( p_sys );

    if( OpenDevice( p_access, vdevname, 0 ) != VLC_SUCCESS )
    {
        msg_Err( p_access, "can't open video");
    }

    if( p_sys->b_audio && OpenDevice( p_access, adevname, 1 ) != VLC_SUCCESS )
    {
        msg_Err( p_access, "can't open audio");
    }
    
    for( int i = 0; i < p_sys->i_crossbar_route_depth; i++ )
    {
        IAMCrossbar *pXbar = p_sys->crossbar_routes[i].pXbar;
        LONG VideoInputIndex = p_sys->crossbar_routes[i].VideoInputIndex;
        LONG VideoOutputIndex = p_sys->crossbar_routes[i].VideoOutputIndex;
        LONG AudioInputIndex = p_sys->crossbar_routes[i].AudioInputIndex;
        LONG AudioOutputIndex = p_sys->crossbar_routes[i].AudioOutputIndex;

        if( SUCCEEDED(pXbar->Route(VideoOutputIndex, VideoInputIndex)) )
        {
            msg_Dbg( p_access, "Crossbar at depth %d, Routed video "
                     "ouput %ld to video input %ld", i, VideoOutputIndex,
                     VideoInputIndex );

            if( AudioOutputIndex != -1 && AudioInputIndex != -1 )
            {
                if( SUCCEEDED( pXbar->Route(AudioOutputIndex,
                                            AudioInputIndex)) )
                {
                    msg_Dbg(p_access, "Crossbar at depth %d, Routed audio "
                            "ouput %ld to audio input %ld", i,
                            AudioOutputIndex, AudioInputIndex );
                }
            }
        }
    }

    if( !p_sys->i_streams )
    {
        ReleaseDirectShow( p_access );
        return VLC_EGENERIC;
    }

    /* Initialize some data */
    p_sys->i_current_stream = 0;
    p_sys->i_mtu += p_sys->i_header_size + 16 /* data header size */;
    vlc_mutex_init( p_access, &p_sys->lock );
    vlc_cond_init( p_access, &p_sys->wait );

    msg_Dbg( p_access, "Playing...");

    /* Everything is ready. Let's rock baby */
    p_sys->p_control->Run();

    return VLC_SUCCESS;
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

    ReleaseDirectShow( p_access );
}

/*****************************************************************************
 * AccessControl:
 *****************************************************************************/
static int AccessControl( access_t *p_access, int i_query, va_list args )
{
    access_sys_t *p_sys = p_access->p_sys;
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
            *pi_int = p_sys->i_mtu;
            break;

        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = (int64_t)var_GetInteger( p_access, "dshow-caching" ) *
                                              I64C(1000);
            break;

        /* */
        case ACCESS_SET_PAUSE_STATE:
        case ACCESS_GET_TITLE_INFO:
        case ACCESS_SET_TITLE:
        case ACCESS_SET_SEEKPOINT:
            return VLC_EGENERIC;

        default:
            msg_Err( p_access, "unimplemented query in control" );
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}

/****************************************************************************
 * RouteCrossbars (Does not AddRef the returned *Pin)
 ****************************************************************************/
static HRESULT GetCrossbarIPinAtIndex( IAMCrossbar *pXbar, LONG PinIndex,
                                       BOOL IsInputPin, IPin ** ppPin )
{
    LONG         cntInPins, cntOutPins;
    IPin        *pP = 0;
    IBaseFilter *pFilter = NULL;
    IEnumPins   *pins=0;
    ULONG        n;

    if( !pXbar || !ppPin ) return E_POINTER;

    *ppPin = 0;

    if( S_OK != pXbar->get_PinCounts(&cntOutPins, &cntInPins) ) return E_FAIL;

    LONG TrueIndex = IsInputPin ? PinIndex : PinIndex + cntInPins;

    if( pXbar->QueryInterface(IID_IBaseFilter, (void **)&pFilter) == S_OK )
    {
        if( SUCCEEDED(pFilter->EnumPins(&pins)) ) 
        {
            LONG i = 0;
            while( pins->Next(1, &pP, &n) == S_OK ) 
            {
                pP->Release();
                if( i == TrueIndex ) 
                {
                    *ppPin = pP;
                    break;
                }
                i++;
            }
            pins->Release();
        }
        pFilter->Release();
    }

    return *ppPin ? S_OK : E_FAIL; 
}

/****************************************************************************
 * GetCrossbarIndexFromIPin: Find corresponding index of an IPin on a crossbar
 ****************************************************************************/
static HRESULT GetCrossbarIndexFromIPin( IAMCrossbar * pXbar, LONG * PinIndex,
                                         BOOL IsInputPin, IPin * pPin )
{
    LONG         cntInPins, cntOutPins;
    IPin        *pP = 0;
    IBaseFilter *pFilter = NULL;
    IEnumPins   *pins = 0;
    ULONG        n;
    BOOL         fOK = FALSE;

    if(!pXbar || !PinIndex || !pPin )
        return E_POINTER;

    if( S_OK != pXbar->get_PinCounts(&cntOutPins, &cntInPins) )
        return E_FAIL;

    if( pXbar->QueryInterface(IID_IBaseFilter, (void **)&pFilter) == S_OK )
    {
        if( SUCCEEDED(pFilter->EnumPins(&pins)) )
        {
            LONG i=0;

            while( pins->Next(1, &pP, &n) == S_OK )
            {
                pP->Release();
                if( pPin == pP )
                {
                    *PinIndex = IsInputPin ? i : i - cntInPins;
                    fOK = TRUE;
                    break;
                }
                i++;
            }
            pins->Release();
        }
        pFilter->Release();
    }

    return fOK ? S_OK : E_FAIL; 
}

/****************************************************************************
 * FindCrossbarRoutes
 ****************************************************************************/
static HRESULT FindCrossbarRoutes( access_t *p_access, IPin *p_input_pin,
                                   LONG physicalType, int depth = 0 )
{
    access_sys_t *p_sys = p_access->p_sys;
    HRESULT result = S_FALSE;

    IPin *p_output_pin;
    if( FAILED(p_input_pin->ConnectedTo(&p_output_pin)) ) return S_FALSE;

    // It is connected, so now find out if the filter supports IAMCrossbar
    PIN_INFO pinInfo;
    if( FAILED(p_output_pin->QueryPinInfo(&pinInfo)) ||
        PINDIR_OUTPUT != pinInfo.dir )
    {
        p_output_pin->Release ();
        return S_FALSE;
    }

    IAMCrossbar *pXbar=0;
    if( FAILED(pinInfo.pFilter->QueryInterface(IID_IAMCrossbar,
                                               (void **)&pXbar)) )
    {
        pinInfo.pFilter->Release();
        p_output_pin->Release ();
        return S_FALSE;
    }

    LONG inputPinCount, outputPinCount;
    if( FAILED(pXbar->get_PinCounts(&outputPinCount, &inputPinCount)) )
    {
        pXbar->Release();
        pinInfo.pFilter->Release();
        p_output_pin->Release ();
        return S_FALSE;
    }

    LONG inputPinIndexRelated, outputPinIndexRelated;
    LONG inputPinPhysicalType, outputPinPhysicalType;
    LONG inputPinIndex, outputPinIndex;
    if( FAILED(GetCrossbarIndexFromIPin( pXbar, &outputPinIndex,
                                         FALSE, p_output_pin )) ||
        FAILED(pXbar->get_CrossbarPinInfo( FALSE, outputPinIndex,
                                           &outputPinIndexRelated,
                                           &outputPinPhysicalType )) )
    {
        pXbar->Release();
        pinInfo.pFilter->Release();
        p_output_pin->Release ();
        return S_FALSE;
    }

    //
    // for all input pins
    //
    for( inputPinIndex = 0; S_OK != result && inputPinIndex < inputPinCount;
         inputPinIndex++ ) 
    {
        if( FAILED(pXbar->get_CrossbarPinInfo( TRUE,  inputPinIndex,
                &inputPinIndexRelated, &inputPinPhysicalType )) ) continue;
   
        // Is the pin a video pin?
        if( inputPinPhysicalType != physicalType ) continue;

        // Can we route it?
        if( FAILED(pXbar->CanRoute(outputPinIndex, inputPinIndex)) ) continue;

        IPin *pPin;
        if( FAILED(GetCrossbarIPinAtIndex( pXbar, inputPinIndex,
                                           TRUE, &pPin)) ) continue;

        result = FindCrossbarRoutes( p_access, pPin, physicalType, depth+1 );
        if( S_OK == result || (S_FALSE == result &&
              physicalType == inputPinPhysicalType &&
              (p_sys->i_crossbar_route_depth = depth+1) < MAX_CROSSBAR_DEPTH) )
        {
            // hold on crossbar
            pXbar->AddRef();

            // remember crossbar route
            p_sys->crossbar_routes[depth].pXbar = pXbar;
            p_sys->crossbar_routes[depth].VideoInputIndex = inputPinIndex;
            p_sys->crossbar_routes[depth].VideoOutputIndex = outputPinIndex;
            p_sys->crossbar_routes[depth].AudioInputIndex = inputPinIndexRelated;
            p_sys->crossbar_routes[depth].AudioOutputIndex = outputPinIndexRelated;

            msg_Dbg( p_access, "Crossbar at depth %d, Found Route For "
                     "ouput %ld (type %ld) to input %ld (type %ld)", depth,
                     outputPinIndex, outputPinPhysicalType, inputPinIndex,
                     inputPinPhysicalType );

            result = S_OK;
        }
    }

    pXbar->Release();
    pinInfo.pFilter->Release();
    p_output_pin->Release ();

    return result;
}

/****************************************************************************
 * ConnectFilters
 ****************************************************************************/
static bool ConnectFilters( access_t *p_access, IBaseFilter *p_filter,
                            CaptureFilter *p_capture_filter )
{
    access_sys_t *p_sys = p_access->p_sys;
    CapturePin *p_input_pin = p_capture_filter->CustomGetPin();

    AM_MEDIA_TYPE mediaType = p_input_pin->CustomGetMediaType();

    if( p_sys->p_capture_graph_builder2 )
    {
        if( FAILED(p_sys->p_capture_graph_builder2->
                     RenderStream( &PIN_CATEGORY_CAPTURE, &mediaType.majortype,
                                   p_filter, NULL,
                                   (IBaseFilter *)p_capture_filter )) )
        {
            return false;
        }

        // Sort out all the possible video inputs
        // The class needs to be given the capture filters ANALOGVIDEO input pin
        IEnumPins *pins = 0;
        if( mediaType.majortype == MEDIATYPE_Video &&
            SUCCEEDED(p_filter->EnumPins(&pins)) )
        {
            IPin        *pP = 0;
            ULONG        n;
            PIN_INFO     pinInfo;
            BOOL         Found = FALSE;
            IKsPropertySet *pKs=0;
            GUID guid;
            DWORD dw;

            while( !Found && (S_OK == pins->Next(1, &pP, &n)) )
            {
                if(S_OK == pP->QueryPinInfo(&pinInfo))
                {
                    if(pinInfo.dir == PINDIR_INPUT)
                    {
                        // is this pin an ANALOGVIDEOIN input pin?
                        if( pP->QueryInterface(IID_IKsPropertySet,
                                               (void **)&pKs) == S_OK )
                        {
                            if( pKs->Get(AMPROPSETID_Pin,
                                         AMPROPERTY_PIN_CATEGORY, NULL, 0,
                                         &guid, sizeof(GUID), &dw) == S_OK )
                            {
                                if( guid == PIN_CATEGORY_ANALOGVIDEOIN )
                                {
                                    // recursively search crossbar routes
                                    FindCrossbarRoutes( p_access, pP,
                                                        PhysConn_Video_Tuner );
                                    // found it
                                    Found = TRUE;
                                }
                            }
                            pKs->Release();
                        }
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
** get fourcc priority from arbritary preference, the higher the better
*/
static int GetFourCCPriority(int i_fourcc)
{
    switch( i_fourcc )
    {
        case VLC_FOURCC('I','4','2','0'):
        case VLC_FOURCC('f','l','3','2'):
        {
            return 9;
        }

        case VLC_FOURCC('Y','V','1','2'):
        case VLC_FOURCC('a','r','a','w'):
        {
            return 8;
        }

        case VLC_FOURCC('R','V','2','4'):
        {
            return 7;
        }

        case VLC_FOURCC('Y','U','Y','2'):
        case VLC_FOURCC('R','V','3','2'):
        case VLC_FOURCC('R','G','B','A'):
        {
            return 6;
        }
    }
    return 0;
}

#define MAX_MEDIA_TYPES 32

static int OpenDevice( access_t *p_access, string devicename,
                       vlc_bool_t b_audio )
{
    access_sys_t *p_sys = p_access->p_sys;

    /* See if device is already opened */
    for( int i = 0; i < p_sys->i_streams; i++ )
    {
        if( p_sys->pp_streams[i]->devicename == devicename )
        {
            /* Already opened */
            return VLC_SUCCESS;
        }
    }

    list<string> list_devices;

    /* Enumerate devices and display their names */
    FindCaptureDevice( (vlc_object_t *)p_access, NULL, &list_devices, b_audio );

    if( !list_devices.size() )
        return VLC_EGENERIC;

    list<string>::iterator iter;
    for( iter = list_devices.begin(); iter != list_devices.end(); iter++ )
        msg_Dbg( p_access, "found device: %s", iter->c_str() );

    /* If no device name was specified, pick the 1st one */
    if( devicename.size() == 0 )
    {
        devicename = *list_devices.begin();
    }

    // Use the system device enumerator and class enumerator to find
    // a capture/preview device, such as a desktop USB video camera.
    IBaseFilter *p_device_filter =
        FindCaptureDevice( (vlc_object_t *)p_access, &devicename,
                           NULL, b_audio );
    if( p_device_filter )
        msg_Dbg( p_access, "using device: %s", devicename.c_str() );
    else
    {
        msg_Err( p_access, "can't use device: %s, unsupported device type",
                 devicename.c_str() );
        return VLC_EGENERIC;
    }

    AM_MEDIA_TYPE *mt;
    AM_MEDIA_TYPE media_types[MAX_MEDIA_TYPES];

    size_t mt_count = EnumDeviceCaps( (vlc_object_t *)p_access,
                                      p_device_filter, p_sys->i_chroma,
                                      p_sys->i_width, p_sys->i_height,
                                      0, 0, 0, media_types, MAX_MEDIA_TYPES );

    if( mt_count > 0 )
    {
	mt = (AM_MEDIA_TYPE *)malloc( sizeof(AM_MEDIA_TYPE)*mt_count );

	// Order and copy returned media types according to arbitrary
	// fourcc priority
	for( size_t c=0; c<mt_count; c++ )
	{
	    int slot_priority =
		GetFourCCPriority(GetFourCCFromMediaType(media_types[c]));
	    size_t slot_copy = c;
	    for( size_t d=c+1; d<mt_count; d++ )
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
		mt[c] = media_types[slot_copy];
		media_types[slot_copy] = media_types[c];
	    }
	    else
	    {
		mt[c] = media_types[c];
	    }
	}
    }
    else if( ! b_audio ) {
	// Use default video media type
        AM_MEDIA_TYPE mtr;
	VIDEOINFOHEADER vh;

	mtr.majortype            = MEDIATYPE_Video;
	mtr.subtype              = MEDIASUBTYPE_I420;
	mtr.bFixedSizeSamples    = TRUE;
	mtr.bTemporalCompression = FALSE;
	mtr.lSampleSize          = 0;
	mtr.pUnk                 = NULL;
	mtr.formattype           = FORMAT_VideoInfo;
	mtr.cbFormat             = sizeof(vh);
	mtr.pbFormat             = (BYTE *)&vh;

        memset(&vh, 0, sizeof(vh));

	vh.bmiHeader.biSize        = sizeof(vh.bmiHeader);
	vh.bmiHeader.biWidth       = p_sys->i_width > 0 ? p_sys->i_width: 320;
	vh.bmiHeader.biHeight      = p_sys->i_height > 0 ? p_sys->i_height : 240;
	vh.bmiHeader.biPlanes      = 1;
	vh.bmiHeader.biBitCount    = 24;
	vh.bmiHeader.biCompression = VLC_FOURCC('I','4','2','0');
	vh.bmiHeader.biSizeImage   = p_sys->i_width * 24 * p_sys->i_height / 8;

        msg_Warn( p_access, "device %s using built-in video media type",
                 devicename.c_str() );

	mt_count = 1;
	mt = (AM_MEDIA_TYPE *)malloc( sizeof(AM_MEDIA_TYPE)*mt_count );
	CopyMediaType(mt, &mtr);
    }
    else {
	// Use default audio media type
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

        msg_Warn( p_access, "device %s using built-in audio media type",
                 devicename.c_str() );

	mt_count = 1;
	mt = (AM_MEDIA_TYPE *)malloc( sizeof(AM_MEDIA_TYPE)*mt_count );
	CopyMediaType(mt, &mtr);
    }

    /* Create and add our capture filter */
    CaptureFilter *p_capture_filter =
        new CaptureFilter( p_access, mt, mt_count );
    p_sys->p_graph->AddFilter( p_capture_filter, 0 );

    /* Add the device filter to the graph (seems necessary with VfW before
     * accessing pin attributes). */
    p_sys->p_graph->AddFilter( p_device_filter, 0 );

    /* Attempt to connect one of this device's capture output pins */
    msg_Dbg( p_access, "connecting filters" );
    if( ConnectFilters( p_access, p_device_filter, p_capture_filter ) )
    {
        /* Success */
        msg_Dbg( p_access, "filters connected successfully !" );

        dshow_stream_t dshow_stream;
        dshow_stream.b_invert = VLC_FALSE;
        dshow_stream.b_pts = VLC_FALSE;
        dshow_stream.mt =
            p_capture_filter->CustomGetPin()->CustomGetMediaType();

        /* Show properties. Done here so the VLC stream is setup with the
         * proper parameters. */
        vlc_value_t val;
        var_Get( p_access, "dshow-config", &val );
        if( val.i_int )
        {
            PropertiesPage( VLC_OBJECT(p_access), p_device_filter,
                            p_sys->p_capture_graph_builder2,
                            dshow_stream.mt.majortype == MEDIATYPE_Audio );
        }

        dshow_stream.mt =
            p_capture_filter->CustomGetPin()->CustomGetMediaType();

        dshow_stream.i_fourcc = GetFourCCFromMediaType(dshow_stream.mt);
        if( 0 != dshow_stream.i_fourcc )
        {
            if( dshow_stream.mt.majortype == MEDIATYPE_Video )
            {
                dshow_stream.header.video =
                    *(VIDEOINFOHEADER *)dshow_stream.mt.pbFormat;

                int i_height = dshow_stream.header.video.bmiHeader.biHeight;

                /* Check if the image is inverted (bottom to top) */
                if( dshow_stream.i_fourcc == VLC_FOURCC('R','G','B','1') ||
                    dshow_stream.i_fourcc == VLC_FOURCC('R','G','B','4') ||
                    dshow_stream.i_fourcc == VLC_FOURCC('R','G','B','8') ||
                    dshow_stream.i_fourcc == VLC_FOURCC('R','V','1','5') ||
                    dshow_stream.i_fourcc == VLC_FOURCC('R','V','1','6') ||
                    dshow_stream.i_fourcc == VLC_FOURCC('R','V','2','4') ||
                    dshow_stream.i_fourcc == VLC_FOURCC('R','V','3','2') ||
                    dshow_stream.i_fourcc == VLC_FOURCC('R','G','B','A') )
                {
                    if( i_height > 0 ) dshow_stream.b_invert = VLC_TRUE;
                    else i_height = - i_height;
                }

                /* Check if we are dealing with a DV stream */
                if( dshow_stream.i_fourcc == VLC_FOURCC('d','v','s','l') ||
                    dshow_stream.i_fourcc == VLC_FOURCC('d','v','s','d') ||
                    dshow_stream.i_fourcc == VLC_FOURCC('d','v','h','d') )
                {
                    p_access->pf_read = ReadCompressed;
                    if( !p_access->psz_demux || !*p_access->psz_demux )
                    {
                        p_access->psz_demux = strdup( "rawdv" );
                    }
                    p_sys->b_audio = VLC_FALSE;
                }

                /* Check if we are dealing with an MPEG video stream */
                if( dshow_stream.i_fourcc == VLC_FOURCC('m','p','2','v') )
                {
                    p_access->pf_read = ReadCompressed;
                    if( !p_access->psz_demux || !*p_access->psz_demux )
                    {
                        p_access->psz_demux = "mpgv";
                    }
                    p_sys->b_audio = VLC_FALSE;
                }

                /* Add video stream to header */
                p_sys->i_header_size += 20;
                p_sys->p_header = (uint8_t *)realloc( p_sys->p_header,
                                                      p_sys->i_header_size );
                memcpy(  &p_sys->p_header[p_sys->i_header_pos], "vids", 4 );
                memcpy(  &p_sys->p_header[p_sys->i_header_pos + 4],
                         &dshow_stream.i_fourcc, 4 );
                SetDWBE( &p_sys->p_header[p_sys->i_header_pos + 8],
                         dshow_stream.header.video.bmiHeader.biWidth );
                SetDWBE( &p_sys->p_header[p_sys->i_header_pos + 12], i_height );
                SetDWBE( &p_sys->p_header[p_sys->i_header_pos + 16], 0 );
                p_sys->i_header_pos = p_sys->i_header_size;

                /* Greatly simplifies the reading routine */
                int i_mtu = dshow_stream.header.video.bmiHeader.biWidth *
                    i_height * 4;
                p_sys->i_mtu = __MAX( p_sys->i_mtu, i_mtu );
            }

            else if( dshow_stream.mt.majortype == MEDIATYPE_Audio )
            {
                dshow_stream.header.audio =
                    *(WAVEFORMATEX *)dshow_stream.mt.pbFormat;

                /* Add audio stream to header */
                p_sys->i_header_size += 20;
                p_sys->p_header = (uint8_t *)realloc( p_sys->p_header,
                                                      p_sys->i_header_size );
                memcpy(  &p_sys->p_header[p_sys->i_header_pos], "auds", 4 );
                memcpy(  &p_sys->p_header[p_sys->i_header_pos + 4],
                         &dshow_stream.i_fourcc, 4 );
                SetDWBE( &p_sys->p_header[p_sys->i_header_pos + 8],
                         dshow_stream.header.audio.nChannels );
                SetDWBE( &p_sys->p_header[p_sys->i_header_pos + 12],
                         dshow_stream.header.audio.nSamplesPerSec );
                SetDWBE( &p_sys->p_header[p_sys->i_header_pos + 16],
                         dshow_stream.header.audio.wBitsPerSample );
                p_sys->i_header_pos = p_sys->i_header_size;

                /* Greatly simplifies the reading routine */
                IAMBufferNegotiation *p_ambuf;
                IPin *p_pin;
                int i_mtu;

                p_capture_filter->CustomGetPin()->ConnectedTo( &p_pin );
                if( SUCCEEDED( p_pin->QueryInterface(
                        IID_IAMBufferNegotiation, (void **)&p_ambuf ) ) )
                {
                    ALLOCATOR_PROPERTIES AllocProp;
                    memset( &AllocProp, 0, sizeof( ALLOCATOR_PROPERTIES ) );
                    p_ambuf->GetAllocatorProperties( &AllocProp );
                    p_ambuf->Release();
                    i_mtu = AllocProp.cbBuffer;
                }
                else
                {
                    /* Worst case */
                    i_mtu = dshow_stream.header.audio.nSamplesPerSec *
                        dshow_stream.header.audio.nChannels *
                        dshow_stream.header.audio.wBitsPerSample / 8;
                }
                p_pin->Release();
                p_sys->i_mtu = __MAX( p_sys->i_mtu, i_mtu );
            }

            else if( dshow_stream.mt.majortype == MEDIATYPE_Stream )
            {
                msg_Dbg( p_access, "MEDIATYPE_Stream" );

                msg_Dbg( p_access, "selected stream pin accepts format: %4.4s",
                         (char *)&dshow_stream.i_fourcc);

                p_sys->b_audio = VLC_FALSE;
                p_sys->i_header_size = 0;
                p_sys->i_header_pos = 0;
                p_sys->i_mtu = 0;

                p_access->pf_read = ReadCompressed;
            }
            else
            {
                msg_Dbg( p_access, "unknown stream majortype" );
                goto fail;
            }

            /* Add directshow elementary stream to our list */
            dshow_stream.p_device_filter = p_device_filter;
            dshow_stream.p_capture_filter = p_capture_filter;

            p_sys->pp_streams = (dshow_stream_t **)realloc( p_sys->pp_streams,
                sizeof(dshow_stream_t *) * (p_sys->i_streams + 1) );
            p_sys->pp_streams[p_sys->i_streams] = new dshow_stream_t;
            *p_sys->pp_streams[p_sys->i_streams++] = dshow_stream;
            SetDWBE( &p_sys->p_header[4], (uint32_t)p_sys->i_streams );

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
                int i_convert = ( lstrlenW( var.bstrVal ) + 1 ) * 2;
                char *p_buf = (char *)alloca( i_convert ); p_buf[0] = 0;
                WideCharToMultiByte( CP_ACP, 0, var.bstrVal, -1, p_buf,
                                     i_convert, NULL, NULL );
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

        /* Probe pin */
        if( SUCCEEDED( p_output_pin->EnumMediaTypes( &p_enummt ) ) )
        {
            AM_MEDIA_TYPE *p_mt;
            while( p_enummt->Next( 1, &p_mt, NULL ) == S_OK )
            {
                int i_current_fourcc = GetFourCCFromMediaType(*p_mt);
                if( 0 != i_current_fourcc )
                {
                    if( p_mt->majortype == MEDIATYPE_Video )
                    {
                        int i_current_width = p_mt->pbFormat ?
                                ((VIDEOINFOHEADER *)p_mt->pbFormat)->bmiHeader.biWidth : 0;
                        int i_current_height = p_mt->pbFormat ?
                                ((VIDEOINFOHEADER *)p_mt->pbFormat)->bmiHeader.biHeight : 0;
                        if( i_current_height < 0 )
                                i_current_height = -i_current_height; 

                        msg_Dbg( p_this, "EnumDeviceCaps: input pin "
                                         "accepts chroma: %4.4s, width:%i, height:%i",
                                         (char *)&i_current_fourcc, i_current_width,
                                         i_current_height );

                        if( ( !i_fourcc || i_fourcc == i_current_fourcc ) &&
                                ( !i_width || i_width == i_current_width ) &&
                                ( !i_height || i_height == i_current_height ) &&
                                (mt_count < mt_max) )
                        {
                            /* Pick match */
                            mt[mt_count++] = *p_mt;
                        }
                        else
                        {
                            FreeMediaType( *p_mt );
                        }
                    }
                    else if( p_mt->majortype == MEDIATYPE_Audio )
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
                                (mt_count < mt_max) )
                        {
                            /* Pick  match */
                            mt[mt_count++] = *p_mt;

                            /* Pre-Configure the 1st match, Ugly */
                            if( 1 == mt_count ) {
                                /* Setup a few properties like the audio latency */
                                IAMBufferNegotiation *p_ambuf;

                                if( SUCCEEDED( p_output_pin->QueryInterface(
                                          IID_IAMBufferNegotiation, (void **)&p_ambuf ) ) )
                                {
                                    ALLOCATOR_PROPERTIES AllocProp;
                                    AllocProp.cbAlign = -1;
                                    AllocProp.cbBuffer = i_current_channels *
                                      i_current_samplespersec *
                                      i_current_bitspersample / 8 / 10 ; /*100 ms of latency*/
                                    AllocProp.cbPrefix = -1;
                                    AllocProp.cBuffers = -1;
                                    p_ambuf->SuggestAllocatorProperties( &AllocProp );
                                    p_ambuf->Release();
                                }
                            }
                        }
                        else
                        {
                            FreeMediaType( *p_mt );
                        }
                    }
                    else if( p_mt->majortype == MEDIATYPE_Stream )
                    {
                        if( ( !i_fourcc || i_fourcc == i_current_fourcc ) &&
                                (mt_count < mt_max) )
                        {
                                /* Pick match */
                                mt[mt_count++] = *p_mt;
                                i_fourcc = i_current_fourcc;
                        }
                        else
                        {
                                FreeMediaType( *p_mt );
                        }
                    }
                    else
                    {
                        msg_Dbg( p_this,
                                         "EnumDeviceCaps: input pin: unknown format" );
                        FreeMediaType( *p_mt );
                    }
                }
                else
                {
                    msg_Dbg( p_this,
                                     "EnumDeviceCaps: input pin: unknown format" );
                    FreeMediaType( *p_mt );
                }
                CoTaskMemFree( (PVOID)p_mt );
            }
            p_enummt->Release();
        }

        p_output_pin->Release();
    }

    p_enumpins->Release();
    return mt_count;
}

/*****************************************************************************
 * AccessRead: reads from the device.
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, otherwise the number of
 * bytes.
 *****************************************************************************/
static int AccessRead( access_t *p_access, uint8_t *p_buffer, int i_len )
{
    access_sys_t   *p_sys = p_access->p_sys;
    dshow_stream_t *p_stream = NULL;
    byte_t         *p_buf_orig = p_buffer;
    VLCMediaSample  sample;
    int             i_data_size;
    uint8_t         *p_data;

    if( p_sys->i_header_pos )
    {
        /* First header of the stream */
        memcpy( p_buffer, p_sys->p_header, p_sys->i_header_size );
        p_buffer += p_sys->i_header_size;
        p_sys->i_header_pos = 0;
    }

    while( 1 )
    {
        /* Get new sample/frame from next elementary stream.
         * We first loop through all the elementary streams and if all our
         * fifos are empty we block until we are signaled some new data has
         * arrived. */
        vlc_mutex_lock( &p_sys->lock );

        int i_stream;
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
        if( i_stream == p_sys->i_streams )
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
        if( i_stream == p_sys->i_streams )
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

#if 0
        msg_Dbg( p_access, "Read() stream: %i PTS: "I64Fd, i_stream, i_pts );
#endif

        /* Create pseudo header */
        SetDWBE( &p_sys->p_header[0], i_stream );
        SetDWBE( &p_sys->p_header[4], i_data_size );
        SetQWBE( &p_sys->p_header[8], i_pts / 10 );

#if 0
        msg_Info( p_access, "access read %i data_size %i", i_len, i_data_size );
#endif

        /* First copy header */
        memcpy( p_buffer, p_sys->p_header, 16 /* header size */ );
        p_buffer += 16 /* header size */;

        /* Then copy stream data if any */
        if( !p_stream->b_invert )
        {
            p_access->p_vlc->pf_memcpy( p_buffer, p_data, i_data_size );
            p_buffer += i_data_size;
        }
        else
        {
            int i_width = p_stream->header.video.bmiHeader.biWidth;
            int i_height = p_stream->header.video.bmiHeader.biHeight;
            if( i_height < 0 ) i_height = - i_height;

            switch( p_stream->i_fourcc )
            {
            case VLC_FOURCC( 'R', 'V', '1', '5' ):
            case VLC_FOURCC( 'R', 'V', '1', '6' ):
                i_width *= 2;
                break;
            case VLC_FOURCC( 'R', 'V', '2', '4' ):
                i_width *= 3;
                break;
            case VLC_FOURCC( 'R', 'V', '3', '2' ):
            case VLC_FOURCC( 'R', 'G', 'B', 'A' ):
                i_width *= 4;
                break;
            }

            for( int i = i_height - 1; i >= 0; i-- )
            {
                p_access->p_vlc->pf_memcpy( p_buffer,
                     &p_data[i * i_width], i_width );

                p_buffer += i_width;
            }
        }

        sample.p_sample->Release();

        /* The caller got what he wanted */
        return p_buffer - p_buf_orig;
    }

    return 0; /* never reached */
}

/*****************************************************************************
 * ReadCompressed: reads compressed (MPEG/DV) data from the device.
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, otherwise the number of
 * bytes.
 *****************************************************************************/
static int ReadCompressed( access_t *p_access, uint8_t *p_buffer, int i_len )
{
    access_sys_t   *p_sys = p_access->p_sys;
    dshow_stream_t *p_stream = NULL;
    VLCMediaSample  sample;
    int             i_data_size;
    uint8_t         *p_data;

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
        i_data_size = sample.p_sample->GetActualDataLength();
        sample.p_sample->GetPointer( &p_data );

#if 0
        msg_Info( p_access, "access read %i data_size %i", i_len, i_data_size );
#endif
        i_data_size = __MIN( i_data_size, (int)i_len );

        p_access->p_vlc->pf_memcpy( p_buffer, p_data, i_data_size );

        sample.p_sample->Release();

        /* The caller got what he wanted */
        return i_data_size;
    }

    return 0; /* never reached */
}

/*****************************************************************************
 * Demux: local prototypes
 *****************************************************************************/
struct demux_sys_t
{
    int         i_es;
    es_out_id_t **es;
};

static int  Demux      ( demux_t * );
static int DemuxControl( demux_t *, int, va_list );

/****************************************************************************
 * DemuxOpen:
 ****************************************************************************/
static int DemuxOpen( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys;

    uint8_t     *p_peek;
    int         i_es;
    int         i;

    /* a little test to see if it's a dshow stream */
    if( stream_Peek( p_demux->s, &p_peek, 8 ) < 8 )
    {
        msg_Warn( p_demux, "dshow plugin discarded (cannot peek)" );
        return VLC_EGENERIC;
    }

    if( memcmp( p_peek, ".dsh", 4 ) ||
        ( i_es = GetDWBE( &p_peek[4] ) ) <= 0 )
    {
        msg_Warn( p_demux, "dshow plugin discarded (not a valid stream)" );
        return VLC_EGENERIC;
    }

    p_demux->pf_demux   = Demux;
    p_demux->pf_control = DemuxControl;
    p_demux->p_sys = p_sys = (demux_sys_t *)malloc( sizeof( demux_sys_t ) );
    p_sys->i_es = 0;
    p_sys->es   = NULL;

    if( stream_Peek( p_demux->s, &p_peek, 8 + 20 * i_es ) < 8 + 20 * i_es )
    {
        msg_Err( p_demux, "dshow plugin discarded (cannot peek)" );
        return VLC_EGENERIC;
    }
    p_peek += 8;

    for( i = 0; i < i_es; i++ )
    {
        es_format_t fmt;

        if( !memcmp( p_peek, "auds", 4 ) )
        {
            es_format_Init( &fmt, AUDIO_ES, VLC_FOURCC( p_peek[4], p_peek[5],
                                                        p_peek[6], p_peek[7] ) );

            fmt.audio.i_channels = GetDWBE( &p_peek[8] );
            fmt.audio.i_rate = GetDWBE( &p_peek[12] );
            fmt.audio.i_bitspersample = GetDWBE( &p_peek[16] );
            fmt.audio.i_blockalign = fmt.audio.i_channels *
                                     fmt.audio.i_bitspersample / 8;
            fmt.i_bitrate = fmt.audio.i_channels *
                            fmt.audio.i_rate *
                            fmt.audio.i_bitspersample;

            msg_Dbg( p_demux, "new audio es %d channels %dHz",
                     fmt.audio.i_channels, fmt.audio.i_rate );

            p_sys->es = (es_out_id_t **)realloc( p_sys->es,
                          sizeof(es_out_id_t *) * (p_sys->i_es + 1) );
            p_sys->es[p_sys->i_es++] = es_out_Add( p_demux->out, &fmt );
        }
        else if( !memcmp( p_peek, "vids", 4 ) )
        {
            es_format_Init( &fmt, VIDEO_ES, VLC_FOURCC( p_peek[4], p_peek[5],
                                                        p_peek[6], p_peek[7] ) );
            fmt.video.i_width  = GetDWBE( &p_peek[8] );
            fmt.video.i_height = GetDWBE( &p_peek[12] );

            msg_Dbg( p_demux, "added new video es %4.4s %dx%d",
                     (char*)&fmt.i_codec,
                     fmt.video.i_width, fmt.video.i_height );

            p_sys->es = (es_out_id_t **)realloc( p_sys->es,
                          sizeof(es_out_id_t *) * (p_sys->i_es + 1) );
            p_sys->es[p_sys->i_es++] = es_out_Add( p_demux->out, &fmt );
        }

        p_peek += 20;
    }

    /* Skip header */
    stream_Read( p_demux->s, NULL, 8 + 20 * i_es );

    return VLC_SUCCESS;
}

/****************************************************************************
 * DemuxClose:
 ****************************************************************************/
static void DemuxClose( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    if( p_sys->i_es > 0 )
    {
        free( p_sys->es );
    }
    free( p_sys );
}

/****************************************************************************
 * Demux:
 ****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t     *p_block;

    int i_es;
    int i_size;

    uint8_t *p_peek;
    mtime_t i_pts;

    if( stream_Peek( p_demux->s, &p_peek, 16 ) < 16 )
    {
        msg_Warn( p_demux, "cannot peek (EOF ?)" );
        return 0;
    }

    i_es = GetDWBE( &p_peek[0] );
    if( i_es < 0 || i_es >= p_sys->i_es )
    {
        msg_Err( p_demux, "cannot find ES" );
        return -1;
    }

    i_size = GetDWBE( &p_peek[4] );
    i_pts  = GetQWBE( &p_peek[8] );

    if( ( p_block = stream_Block( p_demux->s, 16 + i_size ) ) == NULL )
    {
        msg_Warn( p_demux, "cannot read data" );
        return 0;
    }

    p_block->p_buffer += 16;
    p_block->i_buffer -= 16;

    p_block->i_dts = p_block->i_pts = i_pts;

    es_out_Control( p_demux->out, ES_OUT_SET_PCR, i_pts > 0 ? i_pts : 0 );
    es_out_Send( p_demux->out, p_sys->es[i_es], p_block );

    return 1;
}

/****************************************************************************
 * DemuxControl:
 ****************************************************************************/
static int DemuxControl( demux_t *p_demux, int i_query, va_list args )
{
   return demux2_vaControlHelper( p_demux->s, 0, -1, 0, 1, i_query, args );
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
        PropertiesPage( p_this, p_device_filter, NULL, b_audio );
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

static void ShowPropertyPage( IUnknown *obj, CAUUID *cauuid )
{
    if( cauuid->cElems > 0 )
    {
        HWND hwnd_desktop = ::GetDesktopWindow();

        OleCreatePropertyFrame( hwnd_desktop, 30, 30, NULL, 1, &obj,
                                cauuid->cElems, cauuid->pElems, 0, 0, NULL );

        CoTaskMemFree( cauuid->pElems );
    }
}

static void PropertiesPage( vlc_object_t *p_this, IBaseFilter *p_device_filter,
                            ICaptureGraphBuilder2 *p_capture_graph,
                            vlc_bool_t b_audio )
{
    CAUUID cauuid;

    msg_Dbg( p_this, "Configuring Device Properties" );

    /*
     * Video or audio capture filter page
     */
    ISpecifyPropertyPages *p_spec;

    HRESULT hr = p_device_filter->QueryInterface( IID_ISpecifyPropertyPages,
                                                  (void **)&p_spec );
    if( SUCCEEDED(hr) )
    {
        if( SUCCEEDED(p_spec->GetPages( &cauuid )) )
        {
            ShowPropertyPage( p_device_filter, &cauuid );
        }
        p_spec->Release();
    }

    msg_Dbg( p_this, "looking for WDM Configuration Pages" );

    if( p_capture_graph )
        msg_Dbg( p_this, "got capture graph for WDM Configuration Pages" );

    /*
     * Audio capture pin
     */
    if( p_capture_graph && b_audio )
    {
        IAMStreamConfig *p_SC;

        hr = p_capture_graph->FindInterface( &PIN_CATEGORY_CAPTURE,
                                             &MEDIATYPE_Audio, p_device_filter,
                                             IID_IAMStreamConfig,
                                             (void **)&p_SC );
        if( SUCCEEDED(hr) )
        {
            hr = p_SC->QueryInterface( IID_ISpecifyPropertyPages,
                                       (void **)&p_spec );
            if( SUCCEEDED(hr) )
            {
                hr = p_spec->GetPages( &cauuid );
                if( SUCCEEDED(hr) )
                {
                    for( unsigned int c = 0; c < cauuid.cElems; c++ )
                    {
                        ShowPropertyPage( p_SC, &cauuid );
                    }
                    CoTaskMemFree( cauuid.pElems );
                }
                p_spec->Release();
            }
            p_SC->Release();
        }

        /*
         * TV Audio filter
         */
        IAMTVAudio *p_TVA;
        hr = p_capture_graph->FindInterface( &PIN_CATEGORY_CAPTURE, 
                                             &MEDIATYPE_Audio, p_device_filter,
                                             IID_IAMTVAudio, (void **)&p_TVA );
        if( SUCCEEDED(hr) )
        {
            hr = p_TVA->QueryInterface( IID_ISpecifyPropertyPages,
                                        (void **)&p_spec );
            if( SUCCEEDED(hr) )
            {
                if( SUCCEEDED( p_spec->GetPages( &cauuid ) ) )
                    ShowPropertyPage(p_TVA, &cauuid);

                p_spec->Release();
            }
            p_TVA->Release();
        }
    }

    /*
     * Video capture pin
     */
    if( p_capture_graph && !b_audio )
    {
        IAMStreamConfig *p_SC;

        hr = p_capture_graph->FindInterface( &PIN_CATEGORY_CAPTURE,
                                             &MEDIATYPE_Interleaved,
                                             p_device_filter,
                                             IID_IAMStreamConfig,
                                             (void **)&p_SC );
        if( FAILED(hr) )
        {
            hr = p_capture_graph->FindInterface( &PIN_CATEGORY_CAPTURE,
                                                 &MEDIATYPE_Video,
                                                 p_device_filter,
                                                 IID_IAMStreamConfig,
                                                 (void **)&p_SC );
        }

        if( SUCCEEDED(hr) )
        {
            hr = p_SC->QueryInterface( IID_ISpecifyPropertyPages,
                                       (void **)&p_spec );
            if( SUCCEEDED(hr) )
            {
                if( SUCCEEDED( p_spec->GetPages(&cauuid) ) )
                {
                    ShowPropertyPage(p_SC, &cauuid);
                }
                p_spec->Release();
            }
            p_SC->Release();
        }

        /*
         * Video crossbar, and a possible second crossbar
         */
        IAMCrossbar *p_X, *p_X2;
        IBaseFilter *p_XF;

        hr = p_capture_graph->FindInterface( &PIN_CATEGORY_CAPTURE,
                                             &MEDIATYPE_Interleaved,
                                             p_device_filter,
                                             IID_IAMCrossbar, (void **)&p_X );
        if( FAILED(hr) )
        {
            hr = p_capture_graph->FindInterface( &PIN_CATEGORY_CAPTURE,
                                                 &MEDIATYPE_Video,
                                                 p_device_filter,
                                                 IID_IAMCrossbar,
                                                 (void **)&p_X );
        }

        if( SUCCEEDED(hr) )
        {
            hr = p_X->QueryInterface( IID_IBaseFilter, (void **)&p_XF );
            if( SUCCEEDED(hr) )
            {
                hr = p_X->QueryInterface( IID_ISpecifyPropertyPages,
                                          (void **)&p_spec );
                if( SUCCEEDED(hr) )
                {
                    hr = p_spec->GetPages(&cauuid);
                    if( hr == S_OK && cauuid.cElems > 0 )
                    {
                        ShowPropertyPage( p_X, &cauuid );
                    }
                    p_spec->Release();
                }

                hr = p_capture_graph->FindInterface( &LOOK_UPSTREAM_ONLY, NULL,
                                                     p_XF, IID_IAMCrossbar,
                                                     (void **)&p_X2 );
                if( SUCCEEDED(hr) )
                {
                    hr = p_X2->QueryInterface( IID_ISpecifyPropertyPages,
                                               (void **)&p_spec );
                    if( SUCCEEDED(hr) )
                    {
                        hr = p_spec->GetPages( &cauuid );
                        if( SUCCEEDED(hr) )
                        {
                            ShowPropertyPage( p_X2, &cauuid );
                        }
                        p_spec->Release();
                    }
                    p_X2->Release();
                }

                p_XF->Release();
            }

            p_X->Release();
        }

        /*
         * TV Tuner
         */
        IAMTVTuner *p_TV;
        hr = p_capture_graph->FindInterface( &PIN_CATEGORY_CAPTURE,
                                             &MEDIATYPE_Interleaved,
                                             p_device_filter,
                                             IID_IAMTVTuner, (void **)&p_TV );
        if( FAILED(hr) )
        {
            hr = p_capture_graph->FindInterface( &PIN_CATEGORY_CAPTURE,
                                                 &MEDIATYPE_Video,
                                                 p_device_filter,
                                                 IID_IAMTVTuner,
                                                 (void **)&p_TV );
        }

        if( SUCCEEDED(hr) )
        {
            hr = p_TV->QueryInterface( IID_ISpecifyPropertyPages,
                                       (void **)&p_spec );
            if( SUCCEEDED(hr) )
            {
                hr = p_spec->GetPages(&cauuid);
                if( SUCCEEDED(hr) )
                {
                    ShowPropertyPage(p_TV, &cauuid);
                }
                p_spec->Release();
            }
            p_TV->Release();
        }
    }
}
