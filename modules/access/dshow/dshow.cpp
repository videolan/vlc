/*****************************************************************************
 * dshow.cpp : DirectShow access module for vlc
 *****************************************************************************
 * Copyright (C) 2002, 2003 VideoLAN
 * $Id: dshow.cpp,v 1.27 2004/01/29 17:04:01 gbazin Exp $
 *
 * Author: Gildas Bazin <gbazin@netcourrier.com>
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
static ssize_t Read          ( input_thread_t *, byte_t *, size_t );
static ssize_t ReadCompressed( input_thread_t *, byte_t *, size_t );

static int OpenDevice( input_thread_t *, string, vlc_bool_t );
static IBaseFilter *FindCaptureDevice( vlc_object_t *, string *,
                                       list<string> *, vlc_bool_t );
static AM_MEDIA_TYPE EnumDeviceCaps( vlc_object_t *, IBaseFilter *,
                                     int, int, int, int, int, int );
static bool ConnectFilters( vlc_object_t *, IFilterGraph *,
                            IBaseFilter *, IPin * );

static int FindDevicesCallback( vlc_object_t *, char const *,
                                vlc_value_t, vlc_value_t, void * );
static int ConfigDevicesCallback( vlc_object_t *, char const *,
                                  vlc_value_t, vlc_value_t, void * );

static void PropertiesPage( vlc_object_t *, IBaseFilter * );

#if 0
    /* Debug only, use this to find out GUIDs */
    unsigned char p_st[];
    UuidToString( (IID *)&IID_IAMBufferNegotiation, &p_st );
    msg_Err( p_input, "BufferNegotiation: %s" , p_st );
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

static void SetDWBE( uint8_t *p, uint32_t dw )
{
    p[0] = (dw >> 24)&0xff;
    p[1] = (dw >> 16)&0xff;
    p[2] = (dw >>  8)&0xff;
    p[3] = (dw      )&0xff;
}

static void SetQWBE( uint8_t *p, uint64_t qw )
{
    SetDWBE( p, (qw >> 32)&0xffffffff );
    SetDWBE( &p[4], qw&0xffffffff );
}

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static char *ppsz_vdev[] = { "", "none" };
static char *ppsz_vdev_text[] = { N_("Default"), N_("None") };
static char *ppsz_adev[] = { "", "none" };
static char *ppsz_adev_text[] = { N_("Default"), N_("None") };

#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for directshow streams. " \
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
    "Show the properties dialog of the selected device.")

static int  AccessOpen ( vlc_object_t * );
static void AccessClose( vlc_object_t * );

static int  DemuxOpen  ( vlc_object_t * );
static void DemuxClose ( vlc_object_t * );

vlc_module_begin();
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
              VLC_TRUE );

    add_shortcut( "dshow" );
    set_capability( "access", 0 );
    set_callbacks( AccessOpen, AccessClose );

    add_submodule();
    set_description( _("DirectShow demuxer") );
    add_shortcut( "dshow" );
    set_capability( "demux", 200 );
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
struct access_sys_t
{
    vlc_mutex_t lock;
    vlc_cond_t  wait;

    IFilterGraph  *p_graph;
    IMediaControl *p_control;

    /* header */
    int     i_header_size;
    int     i_header_pos;
    uint8_t *p_header;

    /* list of elementary streams */
    dshow_stream_t **pp_streams;
    int            i_streams;
    int            i_current_stream;

    /* misc properties */
    int            i_width;
    int            i_height;
    int            i_chroma;
    int            b_audio;
};

/*****************************************************************************
 * Open: open direct show device
 *****************************************************************************/
static int AccessOpen( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    access_sys_t   *p_sys;
    vlc_value_t    val;

    /* Get/parse options and open device(s) */
    string vdevname, adevname;
    int i_width = 0, i_height = 0, i_chroma = 0;

    var_Create( p_input, "dshow-vdev", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    var_Get( p_input, "dshow-vdev", &val );
    if( val.psz_string ) vdevname = string( val.psz_string );
    if( val.psz_string ) free( val.psz_string );

    var_Create( p_input, "dshow-adev", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    var_Get( p_input, "dshow-adev", &val );
    if( val.psz_string ) adevname = string( val.psz_string );
    if( val.psz_string ) free( val.psz_string );

    var_Create( p_input, "dshow-size", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    var_Get( p_input, "dshow-size", &val );
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
            msg_Dbg( p_input, "Width x Height %dx%d", i_width, i_height );
        }
    }
    if( val.psz_string ) free( val.psz_string );

    var_Create( p_input, "dshow-chroma", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    var_Get( p_input, "dshow-chroma", &val );
    if( val.psz_string && strlen( val.psz_string ) >= 4 )
    {
        i_chroma = VLC_FOURCC( val.psz_string[0], val.psz_string[1],
                               val.psz_string[2], val.psz_string[3] );
    }
    if( val.psz_string ) free( val.psz_string );

    p_input->pf_read        = Read;
    p_input->pf_seek        = NULL;
    p_input->pf_set_area    = NULL;
    p_input->pf_set_program = NULL;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.b_pace_control = 0;
    p_input->stream.b_seekable = 0;
    p_input->stream.p_selected_area->i_size = 0;
    p_input->stream.p_selected_area->i_tell = 0;
    p_input->stream.i_method = INPUT_METHOD_FILE;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    var_Create( p_input, "dshow-caching", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);
    var_Get( p_input, "dshow-caching", &val );
    p_input->i_pts_delay = val.i_int * 1000;

    /* Initialize OLE/COM */
    CoInitialize( 0 );

    /* create access private data */
    p_input->p_access_data = p_sys =
        (access_sys_t *)malloc( sizeof( access_sys_t ) );

    /* Initialize some data */
    p_sys->i_streams = 0;
    p_sys->pp_streams = (dshow_stream_t **)malloc( 1 );
    p_sys->i_width = i_width;
    p_sys->i_height = i_height;
    p_sys->i_chroma = i_chroma;
    p_sys->b_audio = VLC_TRUE;

    /* Create header */
    p_sys->i_header_size = 8;
    p_sys->p_header      = (uint8_t *)malloc( p_sys->i_header_size );
    memcpy(  &p_sys->p_header[0], ".dsh", 4 );
    SetDWBE( &p_sys->p_header[4], 1 );
    p_sys->i_header_pos = p_sys->i_header_size;

    /* Build directshow graph */
    CoCreateInstance( CLSID_FilterGraph, 0, CLSCTX_INPROC,
                      (REFIID)IID_IFilterGraph, (void **)&p_sys->p_graph );

    p_sys->p_graph->QueryInterface( IID_IMediaControl,
                                    (void **)&p_sys->p_control );

    if( OpenDevice( p_input, vdevname, 0 ) != VLC_SUCCESS )
    {
        msg_Err( p_input, "can't open video");
    }

    if( p_sys->b_audio && OpenDevice( p_input, adevname, 1 ) != VLC_SUCCESS )
    {
        msg_Err( p_input, "can't open audio");
    }

    if( !p_sys->i_streams )
    {
        /* Release directshow objects */
        if( p_sys->p_control ) p_sys->p_control->Release();
        p_sys->p_graph->Release();

        /* Uninitialize OLE/COM */
        CoUninitialize();

        free( p_sys->p_header );
        free( p_sys->pp_streams );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Initialize some data */
    p_sys->i_current_stream = 0;
    p_input->i_mtu += p_sys->i_header_size + 16 /* data header size */;

    vlc_mutex_init( p_input, &p_sys->lock );
    vlc_cond_init( p_input, &p_sys->wait );

    /* Everything is ready. Let's rock baby */
    p_sys->p_control->Run();

    return VLC_SUCCESS;
}

/*****************************************************************************
 * AccessClose: close device
 *****************************************************************************/
static void AccessClose( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    access_sys_t    *p_sys  = p_input->p_access_data;

    /* Stop capturing stuff */
    //p_sys->p_control->Stop(); /* FIXME?: we get stuck here sometimes */
    p_sys->p_control->Release();

    /* Remove filters from graph */
    for( int i = 0; i < p_sys->i_streams; i++ )
    {
        p_sys->p_graph->RemoveFilter( p_sys->pp_streams[i]->p_capture_filter );
        p_sys->p_graph->RemoveFilter( p_sys->pp_streams[i]->p_device_filter );
        p_sys->pp_streams[i]->p_capture_filter->Release();
        p_sys->pp_streams[i]->p_device_filter->Release();
    }
    p_sys->p_graph->Release();

    /* Uninitialize OLE/COM */
    CoUninitialize();

    free( p_sys->p_header );
    for( int i = 0; i < p_sys->i_streams; i++ ) delete p_sys->pp_streams[i];
    free( p_sys->pp_streams );
    free( p_sys );
}

/****************************************************************************
 * ConnectFilters
 ****************************************************************************/
static bool ConnectFilters( vlc_object_t *p_this, IFilterGraph *p_graph,
                            IBaseFilter *p_filter, IPin *p_input_pin )
{
    IEnumPins *p_enumpins;
    IPin *p_pin;

    if( S_OK != p_filter->EnumPins( &p_enumpins ) ) return false;

    while( S_OK == p_enumpins->Next( 1, &p_pin, NULL ) )
    {
        PIN_DIRECTION pin_dir;
        p_pin->QueryDirection( &pin_dir );

        if( pin_dir == PINDIR_OUTPUT &&
            S_OK == p_graph->ConnectDirect( p_pin, p_input_pin, 0 ) )
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

static int OpenDevice( input_thread_t *p_input, string devicename,
                       vlc_bool_t b_audio )
{
    access_sys_t *p_sys = p_input->p_access_data;
    list<string> list_devices;

    /* Enumerate devices and display their names */
    FindCaptureDevice( (vlc_object_t *)p_input, NULL, &list_devices, b_audio );

    if( !list_devices.size() )
        return VLC_EGENERIC;

    list<string>::iterator iter;
    for( iter = list_devices.begin(); iter != list_devices.end(); iter++ )
        msg_Dbg( p_input, "found device: %s", iter->c_str() );

    /* If no device name was specified, pick the 1st one */
    if( devicename.size() == 0 )
    {
        devicename = *list_devices.begin();
    }

    // Use the system device enumerator and class enumerator to find
    // a capture/preview device, such as a desktop USB video camera.
    IBaseFilter *p_device_filter =
        FindCaptureDevice( (vlc_object_t *)p_input, &devicename,
                           NULL, b_audio );
    if( p_device_filter )
        msg_Dbg( p_input, "using device: %s", devicename.c_str() );
    else
    {
        msg_Err( p_input, "can't use device: %s", devicename.c_str() );
        return VLC_EGENERIC;
    }

    AM_MEDIA_TYPE media_type =
        EnumDeviceCaps( (vlc_object_t *)p_input, p_device_filter,
                        p_sys->i_chroma, p_sys->i_width, p_sys->i_height,
                        0, 0, 0 );

    /* Create and add our capture filter */
    CaptureFilter *p_capture_filter = new CaptureFilter( p_input, media_type );
    p_sys->p_graph->AddFilter( p_capture_filter, 0 );

    /* Add the device filter to the graph (seems necessary with VfW before
     * accessing pin attributes). */
    p_sys->p_graph->AddFilter( p_device_filter, 0 );

    /* Attempt to connect one of this device's capture output pins */
    msg_Dbg( p_input, "connecting filters" );
    if( ConnectFilters( VLC_OBJECT(p_input), p_sys->p_graph, p_device_filter,
                        p_capture_filter->CustomGetPin() ) )
    {
        /* Success */
        dshow_stream_t dshow_stream;
        dshow_stream.b_invert = VLC_FALSE;
        dshow_stream.b_pts = VLC_FALSE;
        dshow_stream.mt =
            p_capture_filter->CustomGetPin()->CustomGetMediaType();

        if( dshow_stream.mt.majortype == MEDIATYPE_Video )
        {
            msg_Dbg( p_input, "MEDIATYPE_Video");

            /* Packed RGB formats */
            if( dshow_stream.mt.subtype == MEDIASUBTYPE_RGB1 )
                dshow_stream.i_fourcc = VLC_FOURCC( 'R', 'G', 'B', '1' );
            if( dshow_stream.mt.subtype == MEDIASUBTYPE_RGB4 )
                dshow_stream.i_fourcc = VLC_FOURCC( 'R', 'G', 'B', '4' );
            if( dshow_stream.mt.subtype == MEDIASUBTYPE_RGB8 )
                dshow_stream.i_fourcc = VLC_FOURCC( 'R', 'G', 'B', '8' );
            else if( dshow_stream.mt.subtype == MEDIASUBTYPE_RGB555 )
                dshow_stream.i_fourcc = VLC_FOURCC( 'R', 'V', '1', '5' );
            else if( dshow_stream.mt.subtype == MEDIASUBTYPE_RGB565 )
                dshow_stream.i_fourcc = VLC_FOURCC( 'R', 'V', '1', '6' );
            else if( dshow_stream.mt.subtype == MEDIASUBTYPE_RGB24 )
                dshow_stream.i_fourcc = VLC_FOURCC( 'R', 'V', '2', '4' );
            else if( dshow_stream.mt.subtype == MEDIASUBTYPE_RGB32 )
                dshow_stream.i_fourcc = VLC_FOURCC( 'R', 'V', '3', '2' );
            else if( dshow_stream.mt.subtype == MEDIASUBTYPE_ARGB32 )
                dshow_stream.i_fourcc = VLC_FOURCC( 'R', 'G', 'B', 'A' );

            /* Packed YUV formats */
            else if( dshow_stream.mt.subtype == MEDIASUBTYPE_YVYU )
                dshow_stream.i_fourcc = VLC_FOURCC( 'Y', 'V', 'Y', 'U' );
            else if( dshow_stream.mt.subtype == MEDIASUBTYPE_YUYV )
                dshow_stream.i_fourcc = VLC_FOURCC( 'Y', 'U', 'Y', 'V' );
            else if( dshow_stream.mt.subtype == MEDIASUBTYPE_Y411 )
                dshow_stream.i_fourcc = VLC_FOURCC( 'I', '4', '1', 'N' );
            else if( dshow_stream.mt.subtype == MEDIASUBTYPE_Y211 )
                dshow_stream.i_fourcc = VLC_FOURCC( 'Y', '2', '1', '1' );
            else if( dshow_stream.mt.subtype == MEDIASUBTYPE_YUY2 ||
                     dshow_stream.mt.subtype == MEDIASUBTYPE_UYVY )
                dshow_stream.i_fourcc = VLC_FOURCC( 'Y', 'U', 'Y', '2' );

            /* Planar YUV formats */
            else if( dshow_stream.mt.subtype == MEDIASUBTYPE_I420 )
                dshow_stream.i_fourcc = VLC_FOURCC( 'I', '4', '2', '0' );
            else if( dshow_stream.mt.subtype == MEDIASUBTYPE_Y41P )
                dshow_stream.i_fourcc = VLC_FOURCC( 'I', '4', '1', '1' );
            else if( dshow_stream.mt.subtype == MEDIASUBTYPE_YV12 ||
                     dshow_stream.mt.subtype == MEDIASUBTYPE_IYUV )
                dshow_stream.i_fourcc = VLC_FOURCC( 'Y', 'V', '1', '2' );
            else if( dshow_stream.mt.subtype == MEDIASUBTYPE_YVU9 )
                dshow_stream.i_fourcc = VLC_FOURCC( 'Y', 'V', 'U', '9' );

            /* DV formats */
            else if( dshow_stream.mt.subtype == MEDIASUBTYPE_dvsl )
                dshow_stream.i_fourcc = VLC_FOURCC( 'd', 'v', 's', 'l' );
            else if( dshow_stream.mt.subtype == MEDIASUBTYPE_dvsd )
                dshow_stream.i_fourcc = VLC_FOURCC( 'd', 'v', 's', 'd' );
            else if( dshow_stream.mt.subtype == MEDIASUBTYPE_dvhd )
                dshow_stream.i_fourcc = VLC_FOURCC( 'd', 'v', 'h', 'd' );

            /* MPEG video formats */
            else if( dshow_stream.mt.subtype == MEDIASUBTYPE_MPEG2_VIDEO )
                dshow_stream.i_fourcc = VLC_FOURCC( 'm', 'p', '2', 'v' );

            else goto fail;

            dshow_stream.header.video =
                *(VIDEOINFOHEADER *)dshow_stream.mt.pbFormat;

            int i_height = dshow_stream.header.video.bmiHeader.biHeight;

            /* Check if the image is inverted (bottom to top) */
            if( dshow_stream.i_fourcc == VLC_FOURCC( 'R', 'G', 'B', '1' ) ||
                dshow_stream.i_fourcc == VLC_FOURCC( 'R', 'G', 'B', '4' ) ||
                dshow_stream.i_fourcc == VLC_FOURCC( 'R', 'G', 'B', '8' ) ||
                dshow_stream.i_fourcc == VLC_FOURCC( 'R', 'V', '1', '5' ) ||
                dshow_stream.i_fourcc == VLC_FOURCC( 'R', 'V', '1', '6' ) ||
                dshow_stream.i_fourcc == VLC_FOURCC( 'R', 'V', '2', '4' ) ||
                dshow_stream.i_fourcc == VLC_FOURCC( 'R', 'V', '3', '2' ) ||
                dshow_stream.i_fourcc == VLC_FOURCC( 'R', 'G', 'B', 'A' ) )
            {
                if( i_height > 0 ) dshow_stream.b_invert = VLC_TRUE;
                else i_height = - i_height;
            }

            /* Check if we are dealing with a DV stream */
            if( dshow_stream.i_fourcc == VLC_FOURCC( 'd', 'v', 's', 'l' ) ||
                dshow_stream.i_fourcc == VLC_FOURCC( 'd', 'v', 's', 'd' ) ||
                dshow_stream.i_fourcc == VLC_FOURCC( 'd', 'v', 'h', 'd' ) )
            {
                p_input->pf_read = ReadCompressed;
                if( !p_input->psz_demux || !*p_input->psz_demux )
                {
                    p_input->psz_demux = "rawdv";
                }
                p_sys->b_audio = VLC_FALSE;
            }

            /* Check if we are dealing with an MPEG video stream */
            if( dshow_stream.i_fourcc == VLC_FOURCC( 'm', 'p', '2', 'v' ) )
            {
                p_input->pf_read = ReadCompressed;
                if( !p_input->psz_demux || !*p_input->psz_demux )
                {
                    p_input->psz_demux = "mpgv";
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
            p_input->i_mtu = __MAX( p_input->i_mtu, (unsigned int)i_mtu );
        }

        else if( dshow_stream.mt.majortype == MEDIATYPE_Audio &&
                 dshow_stream.mt.formattype == FORMAT_WaveFormatEx )
        {
            msg_Dbg( p_input, "MEDIATYPE_Audio");

            if( dshow_stream.mt.subtype == MEDIASUBTYPE_PCM )
                dshow_stream.i_fourcc = VLC_FOURCC( 'a', 'r', 'a', 'w' );
            else if( dshow_stream.mt.subtype == MEDIASUBTYPE_IEEE_FLOAT )
                dshow_stream.i_fourcc = VLC_FOURCC( 'f', 'l', '3', '2' );
            else goto fail;

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
            p_input->i_mtu = __MAX( p_input->i_mtu, (unsigned int)i_mtu );
        }

        else if( dshow_stream.mt.majortype == MEDIATYPE_Stream )
        {
            msg_Dbg( p_input, "MEDIATYPE_Stream" );

            if( dshow_stream.mt.subtype == MEDIASUBTYPE_MPEG2_PROGRAM )
                dshow_stream.i_fourcc = VLC_FOURCC( 'm', 'p', '2', 'p' );
            else if( dshow_stream.mt.subtype == MEDIASUBTYPE_MPEG2_TRANSPORT )
                dshow_stream.i_fourcc = VLC_FOURCC( 'm', 'p', '2', 't' );

            msg_Dbg( p_input, "selected stream pin accepts format: %4.4s",
                     (char *)&dshow_stream.i_fourcc);

            p_sys->b_audio = VLC_FALSE;
            p_sys->i_header_size = 0;
            p_sys->i_header_pos = 0;
            p_input->i_mtu = INPUT_DEFAULT_BUFSIZE;

            p_input->pf_read = ReadCompressed;
            p_input->pf_set_program = input_SetProgram;
        }

        else
        {
            msg_Dbg( p_input, "unknown stream majortype" );
            goto fail;
        }

        /* Show properties */
        vlc_value_t val;
        var_Create( p_input, "dshow-config",
                    VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
        var_Get( p_input, "dshow-config", &val );

        if(val.i_int) PropertiesPage( VLC_OBJECT(p_input), p_device_filter );

        /* Add directshow elementary stream to our list */
        dshow_stream.p_device_filter = p_device_filter;
        dshow_stream.p_capture_filter = p_capture_filter;

        p_sys->pp_streams =
            (dshow_stream_t **)realloc( p_sys->pp_streams,
                                        sizeof(dshow_stream_t *)
                                        * (p_sys->i_streams + 1) );
        p_sys->pp_streams[p_sys->i_streams] = new dshow_stream_t;
        *p_sys->pp_streams[p_sys->i_streams++] = dshow_stream;
        SetDWBE( &p_sys->p_header[4], (uint32_t)p_sys->i_streams );

        return VLC_SUCCESS;
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
        msg_Err( p_this, "failed to create the device enumerator (0x%x)", hr);
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
        msg_Err( p_this, "failed to create the class enumerator (0x%x)", hr );
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
                                 "object (0x%x)", hr );
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

static AM_MEDIA_TYPE EnumDeviceCaps( vlc_object_t *p_this,
                                     IBaseFilter *p_filter,
                                     int i_fourcc, int i_width, int i_height,
                                     int i_channels, int i_samplespersec,
                                     int i_bitspersample )
{
    IEnumPins *p_enumpins;
    IPin *p_output_pin;
    IEnumMediaTypes *p_enummt;
    int i_orig_fourcc = i_fourcc;
    vlc_bool_t b_found = VLC_FALSE;

    AM_MEDIA_TYPE media_type;
    media_type.majortype = GUID_NULL;
    media_type.subtype = GUID_NULL;
    media_type.formattype = GUID_NULL;
    media_type.pUnk = NULL;
    media_type.cbFormat = 0;
    media_type.pbFormat = NULL;

    if( S_OK != p_filter->EnumPins( &p_enumpins ) ) return media_type;

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

    while( !b_found && p_enumpins->Next( 1, &p_output_pin, NULL ) == S_OK )
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

                if( p_mt->majortype == MEDIATYPE_Video )
                {
                    int i_current_fourcc = VLC_FOURCC(' ', ' ', ' ', ' ');

                    /* Packed RGB formats */
                    if( p_mt->subtype == MEDIASUBTYPE_RGB1 )
                        i_current_fourcc = VLC_FOURCC( 'R', 'G', 'B', '1' );
                    if( p_mt->subtype == MEDIASUBTYPE_RGB4 )
                        i_current_fourcc = VLC_FOURCC( 'R', 'G', 'B', '4' );
                    if( p_mt->subtype == MEDIASUBTYPE_RGB8 )
                        i_current_fourcc = VLC_FOURCC( 'R', 'G', 'B', '8' );
                    else if( p_mt->subtype == MEDIASUBTYPE_RGB555 )
                        i_current_fourcc = VLC_FOURCC( 'R', 'V', '1', '5' );
                    else if( p_mt->subtype == MEDIASUBTYPE_RGB565 )
                        i_current_fourcc = VLC_FOURCC( 'R', 'V', '1', '6' );
                    else if( p_mt->subtype == MEDIASUBTYPE_RGB24 )
                        i_current_fourcc = VLC_FOURCC( 'R', 'V', '2', '4' );
                    else if( p_mt->subtype == MEDIASUBTYPE_RGB32 )
                        i_current_fourcc = VLC_FOURCC( 'R', 'V', '3', '2' );
                    else if( p_mt->subtype == MEDIASUBTYPE_ARGB32 )
                        i_current_fourcc = VLC_FOURCC( 'R', 'G', 'B', 'A' );

                    /* MPEG2 video elementary stream */
                    else if( p_mt->subtype == MEDIASUBTYPE_MPEG2_VIDEO )
                        i_current_fourcc = VLC_FOURCC( 'm', 'p', '2', 'v' );

                    /* hauppauge pvr video preview */
                    else if( p_mt->subtype == MEDIASUBTYPE_PREVIEW_VIDEO )
                        i_current_fourcc = VLC_FOURCC( 'P', 'V', 'R', 'V' );

                    else i_current_fourcc = *((int *)&p_mt->subtype);

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

                    if( ( !i_fourcc || i_fourcc == i_current_fourcc ||
                          ( !i_orig_fourcc && i_current_fourcc ==
                            VLC_FOURCC('I','4','2','0') ) ) &&
                        ( !i_width || i_width == i_current_width ) &&
                        ( !i_height || i_height == i_current_height ) )
                    {
                        /* Pick the 1st match */
                        media_type = *p_mt;
                        i_fourcc = i_current_fourcc;
                        i_width = i_current_width;
                        i_height = i_current_height;
                        b_found = VLC_TRUE;
                    }
                    else
                    {
                        FreeMediaType( *p_mt );
                    }
                }
                else if( p_mt->majortype == MEDIATYPE_Audio )
                {
                    int i_current_fourcc;
                    int i_current_channels =
                        ((WAVEFORMATEX *)p_mt->pbFormat)->nChannels;
                    int i_current_samplespersec =
                        ((WAVEFORMATEX *)p_mt->pbFormat)->nSamplesPerSec;
                    int i_current_bitspersample =
                        ((WAVEFORMATEX *)p_mt->pbFormat)->wBitsPerSample;

                    if( p_mt->subtype == MEDIASUBTYPE_PCM )
                        i_current_fourcc = VLC_FOURCC( 'p', 'c', 'm', ' ' );
                    else i_current_fourcc = *((int *)&p_mt->subtype);

                    msg_Dbg( p_this, "EnumDeviceCaps: input pin "
                             "accepts format: %4.4s, channels:%i, "
                             "samples/sec:%i bits/sample:%i",
                             (char *)&i_current_fourcc, i_current_channels,
                             i_current_samplespersec, i_current_bitspersample);

                    if( (!i_channels || i_channels == i_current_channels) &&
                        (!i_samplespersec ||
                         i_samplespersec == i_current_samplespersec) &&
                        (!i_bitspersample ||
                         i_bitspersample == i_current_bitspersample) )
                    {
                        /* Pick the 1st match */
                        media_type = *p_mt;
                        i_channels = i_current_channels;
                        i_samplespersec = i_current_samplespersec;
                        i_bitspersample = i_current_bitspersample;
                        b_found = VLC_TRUE;

                        /* Setup a few properties like the audio latency */
                        IAMBufferNegotiation *p_ambuf;

                        if( SUCCEEDED( p_output_pin->QueryInterface(
                              IID_IAMBufferNegotiation, (void **)&p_ambuf ) ) )
                        {
                            ALLOCATOR_PROPERTIES AllocProp;
                            AllocProp.cbAlign = -1;
                            AllocProp.cbBuffer = i_channels * i_samplespersec *
                              i_bitspersample / 8 / 10 ; /*100 ms of latency*/
                            AllocProp.cbPrefix = -1;
                            AllocProp.cBuffers = -1;
                            p_ambuf->SuggestAllocatorProperties( &AllocProp );
                            p_ambuf->Release();
                        }
                    }
                    else
                    {
                        FreeMediaType( *p_mt );
                    }
                }
                else if( p_mt->majortype == MEDIATYPE_Stream )
                {
                    msg_Dbg( p_this, "EnumDeviceCaps: MEDIATYPE_Stream" );

                    int i_current_fourcc = VLC_FOURCC(' ', ' ', ' ', ' ');

                    if( p_mt->subtype == MEDIASUBTYPE_MPEG2_PROGRAM )
                        i_current_fourcc = VLC_FOURCC( 'm', 'p', '2', 'p' );
                    else if( p_mt->subtype == MEDIASUBTYPE_MPEG2_TRANSPORT )
                        i_current_fourcc = VLC_FOURCC( 'm', 'p', '2', 't' );

                    if( ( !i_fourcc || i_fourcc == i_current_fourcc ) )
                    {
                        /* Pick the 1st match */
                        media_type = *p_mt;
                        i_fourcc = i_current_fourcc;
                        b_found = VLC_TRUE;
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

                CoTaskMemFree( (PVOID)p_mt );
            }
            p_enummt->Release();
        }

        p_output_pin->Release();
    }

    p_enumpins->Release();
    return media_type;
}

/*****************************************************************************
 * Read: reads from the device into PES packets.
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, otherwise the number of
 * bytes.
 *****************************************************************************/
static ssize_t Read( input_thread_t * p_input, byte_t * p_buffer,
                     size_t i_len )
{
    access_sys_t   *p_sys = p_input->p_access_data;
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
        msg_Dbg( p_input, "Read() stream: %i PTS: "I64Fd, i_stream, i_pts );
#endif

        /* Create pseudo header */
        SetDWBE( &p_sys->p_header[0], i_stream );
        SetDWBE( &p_sys->p_header[4], i_data_size );
        SetQWBE( &p_sys->p_header[8], i_pts  * 9 / 1000 );

#if 0
        msg_Info( p_input, "access read %i data_size %i", i_len, i_data_size );
#endif

        /* First copy header */
        memcpy( p_buffer, p_sys->p_header, 16 /* header size */ );
        p_buffer += 16 /* header size */;

        /* Then copy stream data if any */
        if( !p_stream->b_invert )
        {
            p_input->p_vlc->pf_memcpy( p_buffer, p_data, i_data_size );
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
                p_input->p_vlc->pf_memcpy( p_buffer,
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
static ssize_t ReadCompressed( input_thread_t * p_input, byte_t * p_buffer,
                               size_t i_len )
{
    access_sys_t   *p_sys = p_input->p_access_data;
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
        if( p_input->b_die || p_input->b_error ) return 0;

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
        msg_Info( p_input, "access read %i data_size %i", i_len, i_data_size );
#endif
        i_data_size = __MIN( i_data_size, (int)i_len );

        p_input->p_vlc->pf_memcpy( p_buffer, p_data, i_data_size );

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

static int  Demux      ( input_thread_t * );

/****************************************************************************
 * DemuxOpen:
 ****************************************************************************/
static int DemuxOpen( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t    *p_sys;

    uint8_t        *p_peek;
    int            i_es;
    int            i;

    /* a little test to see if it's a dshow stream */
    if( stream_Peek( p_input->s, &p_peek, 8 ) < 8 )
    {
        msg_Warn( p_input, "dshow plugin discarded (cannot peek)" );
        return VLC_EGENERIC;
    }

    if( memcmp( p_peek, ".dsh", 4 ) ||
        ( i_es = GetDWBE( &p_peek[4] ) ) <= 0 )
    {
        msg_Warn( p_input, "dshow plugin discarded (not a valid stream)" );
        return VLC_EGENERIC;
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1)
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot init stream" );
        return VLC_EGENERIC;
    }
    p_input->stream.i_mux_rate =  0 / 50;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    p_input->pf_demux = Demux;
    p_input->pf_demux_control = demux_vaControlDefault;
    p_input->p_demux_data = p_sys =
        (demux_sys_t *)malloc( sizeof( demux_sys_t ) );
    p_sys->i_es = 0;
    p_sys->es   = NULL;

    if( stream_Peek( p_input->s, &p_peek, 8 + 20 * i_es ) < 8 + 20 * i_es )
    {
        msg_Err( p_input, "dshow plugin discarded (cannot peek)" );
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

            msg_Dbg( p_input, "new audio es %d channels %dHz",
                     fmt.audio.i_channels, fmt.audio.i_rate );

            TAB_APPEND( p_sys->i_es, p_sys->es,
                        es_out_Add( p_input->p_es_out, &fmt ) );
        }
        else if( !memcmp( p_peek, "vids", 4 ) )
        {
            es_format_Init( &fmt, VIDEO_ES, VLC_FOURCC( p_peek[4], p_peek[5],
                                                        p_peek[6], p_peek[7] ) );
            fmt.video.i_width  = GetDWBE( &p_peek[8] );
            fmt.video.i_height = GetDWBE( &p_peek[12] );

            msg_Dbg( p_input, "added new video es %4.4s %dx%d",
                     (char*)&fmt.i_codec,
                     fmt.video.i_width, fmt.video.i_height );
            TAB_APPEND( p_sys->i_es, p_sys->es,
                        es_out_Add( p_input->p_es_out, &fmt ) );
        }

        p_peek += 20;
    }

    /* Skip header */
    stream_Read( p_input->s, NULL, 8 + 20 * i_es );

    return VLC_SUCCESS;
}

/****************************************************************************
 * DemuxClose:
 ****************************************************************************/
static void DemuxClose( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t    *p_sys = p_input->p_demux_data;

    if( p_sys->i_es > 0 )
    {
        free( p_sys->es );
    }
    free( p_sys );
}

/****************************************************************************
 * Demux:
 ****************************************************************************/
static int Demux( input_thread_t *p_input )
{
    demux_sys_t *p_sys = p_input->p_demux_data;
    block_t     *p_block;

    int i_es;
    int i_size;

    uint8_t *p_peek;
    mtime_t i_pcr;

    if( stream_Peek( p_input->s, &p_peek, 16 ) < 16 )
    {
        msg_Warn( p_input, "cannot peek (EOF ?)" );
        return 0;
    }

    i_es   = GetDWBE( &p_peek[0] );
    if( i_es < 0 || i_es >= p_sys->i_es )
    {
        msg_Err( p_input, "cannot find ES" );
        return -1;
    }

    i_size = GetDWBE( &p_peek[4] );
    i_pcr  = GetQWBE( &p_peek[8] );

    if( ( p_block = stream_Block( p_input->s, 16 + i_size ) ) == NULL )
    {
        msg_Warn( p_input, "cannot read data" );
        return 0;
    }

    p_block->p_buffer += 16;
    p_block->i_buffer -= 16;

    /* Call the pace control. */
    input_ClockManageRef( p_input, p_input->stream.p_selected_program, i_pcr );

    p_block->i_dts =
    p_block->i_pts = i_pcr <= 0 ? 0 :
        input_ClockGetTS( p_input, p_input->stream.p_selected_program, i_pcr );

    es_out_Send( p_input->p_es_out, p_sys->es[i_es], p_block );

    return 1;
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

    FindCaptureDevice( p_this, NULL, &list_devices, b_audio );

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
        PropertiesPage( p_this, p_device_filter );
    else
    {
        msg_Err( p_this, "didn't find device: %s", devicename.c_str() );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static void PropertiesPage( vlc_object_t *p_this,
                            IBaseFilter *p_device_filter )
{
    ISpecifyPropertyPages *p_spec;
    CAUUID cauuid;
 
    HRESULT hr = p_device_filter->QueryInterface( IID_ISpecifyPropertyPages,
                                                  (void **)&p_spec );
    if( hr == S_OK )
    {
        hr = p_spec->GetPages( &cauuid );
        if( hr == S_OK )
        {
            HWND hwnd_desktop = ::GetDesktopWindow();

            hr = OleCreatePropertyFrame( hwnd_desktop, 30, 30, NULL, 1,
                                         (IUnknown **)&p_device_filter,
                                         cauuid.cElems,
                                         (GUID *)cauuid.pElems, 0, 0, NULL );
            CoTaskMemFree( cauuid.pElems );
        }
        p_spec->Release();
    }
}
