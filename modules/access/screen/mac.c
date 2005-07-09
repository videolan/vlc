/*****************************************************************************
 * mac.c: Screen capture module for the Mac.
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id: x11.c 8290 2004-07-26 20:29:24Z gbazin $
 *
 * Authors: Derk-Jan Hartman <hartman at videolan dot org>
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

#include <vlc/vlc.h>
#include <vlc/input.h>

#include <Carbon/Carbon.h>
#include <ApplicationServices/ApplicationServices.h>

typedef int CGSConnectionRef;
extern CGError CGSNewConnection(void* unknown, CGSConnectionRef* newConnection);
extern CGError CGSReleaseConnection(CGSConnectionRef connection);

#include "screen.h"

struct screen_data_t
{
    RGBColor          oldForeColor, oldBackColor;
    PenState          oldState;
    CGDirectDisplayID displayID;
    CGSConnectionRef  gConnection;
    GDHandle          gMainDevice;
    char              gDeviceState;
    PixMapHandle      gDevicePix;
    GWorldPtr         LocalBufferGW;
    PixMapHandle      LocalBufferPix;
};

int screen_InitCapture( demux_t *p_demux )
{
    demux_sys_t   *p_sys = p_demux->p_sys;
    screen_data_t *p_data;
    int            i_chroma, i_bbp, i_offset;

    i_chroma = i_bbp = i_offset = 0;

    p_sys->p_data = p_data =
        (screen_data_t *)malloc( sizeof( screen_data_t ) );

    p_data->gConnection = NULL;
    p_data->gMainDevice = NULL;
    p_data->gDevicePix = NULL;
    p_data->gDeviceState = NULL;
    p_data->LocalBufferGW = NULL;
    p_data->LocalBufferPix = NULL;

    p_data->displayID = CGMainDisplayID();
    (void) GetMainDevice();

    if( CGDisplaySamplesPerPixel(p_data->displayID) != 3 )
    {
        msg_Err( p_demux, "screenformat not supported" );
    } 
    
    switch( CGDisplaySamplesPerPixel(p_data->displayID) * CGDisplayBitsPerSample(p_data->displayID) )
    {
    /* TODO figure out 256 colors (who uses it anyways) */
    case 15: /* TODO this is not RV16, but BGR16 */
        i_chroma = VLC_FOURCC('R','V','1','6');
        i_bbp = 16;
        i_offset = 8;
        break;
    case 24:
    case 32:
        i_chroma = VLC_FOURCC('R','V','3','2');
        i_bbp = 32;
        i_offset = 4;
        break;
    default:
        msg_Err( p_demux, "unknown screen depth: %d", CGDisplaySamplesPerPixel(p_data->displayID) * CGDisplayBitsPerSample(p_data->displayID) );
        return VLC_EGENERIC;
    }

    GetBackColor(&p_data->oldBackColor);
    GetPenState(&p_data->oldState);
    ForeColor(blackColor);
    BackColor(whiteColor);
    
    p_data->gMainDevice = GetMainDevice();
    p_data->gDeviceState = HGetState((Handle)p_data->gMainDevice);
    HLock((Handle)p_data->gMainDevice);
    p_data->gDevicePix = (**p_data->gMainDevice).gdPMap;

    NewGWorld(&p_data->LocalBufferGW, (**p_data->gDevicePix).pixelSize, &(**p_data->gDevicePix).bounds, (**p_data->gDevicePix).pmTable, NULL, 0);
    p_data->LocalBufferPix = GetGWorldPixMap(p_data->LocalBufferGW);
    LockPixels(p_data->LocalBufferPix);
    
    es_format_Init( &p_sys->fmt, VIDEO_ES, i_chroma );
    p_sys->fmt.video.i_width  = CGDisplayPixelsWide(p_data->displayID) + i_offset;
    p_sys->fmt.video.i_visible_width  = CGDisplayPixelsWide(p_data->displayID);
    p_sys->fmt.video.i_height = CGDisplayPixelsHigh(p_data->displayID);
    p_sys->fmt.video.i_bits_per_pixel = i_bbp;

    GetForeColor(&p_data->oldForeColor);

    HSetState( (Handle)p_data->gMainDevice, p_data->gDeviceState );
    SetPenState( &p_data->oldState);
    RGBForeColor( &p_data->oldForeColor );
    RGBBackColor( &p_data->oldBackColor );

    return VLC_SUCCESS;
}

int screen_CloseCapture( demux_t *p_demux )
{
    screen_data_t *p_data = (screen_data_t *)p_demux->p_sys->p_data;

    if(p_data->LocalBufferPix) UnlockPixels(p_data->LocalBufferPix); p_data->LocalBufferPix = NULL;
    if(p_data->LocalBufferGW) DisposeGWorld(p_data->LocalBufferGW); p_data->LocalBufferGW = NULL;

    return VLC_SUCCESS;
}

block_t *screen_Capture( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    screen_data_t *p_data = (screen_data_t *)p_sys->p_data;
    block_t *p_block;
    int i_size;
 
    i_size = p_sys->fmt.video.i_height * p_sys->fmt.video.i_width * 32 / 8; 

    if( !( p_block = block_New( p_demux, i_size ) ) )
    {
        msg_Warn( p_demux, "cannot get block" );
        return 0;
    }

    GetForeColor(&p_data->oldForeColor);
    GetBackColor(&p_data->oldBackColor);
    GetPenState(&p_data->oldState);
    ForeColor(blackColor);
    BackColor(whiteColor);

    assert(CGSNewConnection(NULL, &p_data->gConnection) == kCGErrorSuccess);
    p_data->gMainDevice = GetMainDevice();
    p_data->gDeviceState = HGetState((Handle)p_data->gMainDevice);
    HLock((Handle)p_data->gMainDevice);
    p_data->gDevicePix = (**p_data->gMainDevice).gdPMap;

    CopyBits(( BitMap*)*p_data->gDevicePix, (BitMap*)*p_data->LocalBufferPix,
             &(**p_data->gDevicePix).bounds, &(**p_data->gDevicePix).bounds,
             srcCopy, NULL );

    HSetState( (Handle)p_data->gMainDevice, p_data->gDeviceState );
    SetPenState( &p_data->oldState );
    RGBForeColor( &p_data->oldForeColor );
    RGBBackColor( &p_data->oldBackColor );

    assert(CGSReleaseConnection(p_data->gConnection) == kCGErrorSuccess);
    memcpy( p_block->p_buffer, (**p_data->LocalBufferPix).baseAddr, i_size );

    return p_block;
}

