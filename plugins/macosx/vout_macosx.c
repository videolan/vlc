/*****************************************************************************
 * vout_macosx.c: MacOS X video output plugin
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 *
 * Authors: Colin Delacroix <colin@zoy.org>
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

#define MODULE_NAME macosx
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "intf_msg.h"

#include "video.h"
#include "video_output.h"

#include "modules.h"
#include "main.h"

#include "macosx_common.h"


/*****************************************************************************
 * Constants & more
 *****************************************************************************/

// Initial Window Constants
enum
{
    kWindowOffset = 100
};

// where is the off screen
enum
{
    kNoWhere = 0,
    kInVRAM,
    kInAGP,
    kInSystem
};


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  vout_Probe     ( probedata_t *p_data );
static int  vout_Create    ( struct vout_thread_s * );
static int  vout_Init      ( struct vout_thread_s * );
static void vout_End       ( struct vout_thread_s * );
static void vout_Destroy   ( struct vout_thread_s * );
static int  vout_Manage    ( struct vout_thread_s * );
static void vout_Display   ( struct vout_thread_s * );

/* OS specific */

static int CreateDisplay	( struct vout_thread_s * );
static int MakeWindow		( struct vout_thread_s * );
static int AllocBuffer		( struct vout_thread_s * , short index );

void BlitToWindow		( struct vout_thread_s * , short index );
GDHandle GetWindowDevice	( struct vout_thread_s * );
void FillOffscreen		( struct vout_thread_s * , short index);

void FindBestMemoryLocation( struct vout_thread_s * );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( vout_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->pf_probe = vout_Probe;
    p_function_list->functions.vout.pf_create     = vout_Create;
    p_function_list->functions.vout.pf_init       = vout_Init;
    p_function_list->functions.vout.pf_end        = vout_End;
    p_function_list->functions.vout.pf_destroy    = vout_Destroy;
    p_function_list->functions.vout.pf_manage     = vout_Manage;
    p_function_list->functions.vout.pf_display    = vout_Display;
    p_function_list->functions.vout.pf_setpalette = NULL;
}

/*****************************************************************************
 * intf_Probe: return a score
 *****************************************************************************/
static int vout_Probe( probedata_t *p_data )
{
    if( TestMethod( VOUT_METHOD_VAR, "macosx" ) )
    {
        return( 999 );
    }

    return( 100 );
}

/*****************************************************************************
 * vout_Create: allocates MacOS X video thread output method
 *****************************************************************************
 * This function allocates and initializes a MacOS X vout method.
 *****************************************************************************/
static int vout_Create( vout_thread_t *p_vout )
{
    //intf_ErrMsg( "vout_Create()" );

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        intf_ErrMsg( "error: %s", strerror( ENOMEM ) );
        return( 1 );
    }

    p_vout->p_sys->gwLocOffscreen = kNoWhere;
    p_vout->p_sys->p_window       = NULL;
    p_vout->p_sys->p_gw[ 0 ]      = NULL;
    p_vout->p_sys->p_gw[ 1 ]      = NULL;
    
    if ( CreateDisplay( p_vout ) )
    {
        intf_ErrMsg( "vout error: can't open display" );
        free( p_vout->p_sys );
        return( 1 );
    }

#if 0
    intf_ErrMsg( "vout p_vout->i_width %d" , p_vout->i_width);
    intf_ErrMsg( "vout p_vout->i_height %d" , p_vout->i_height);
    intf_ErrMsg( "vout p_vout->i_bytes_per_pixel %d" , p_vout->i_bytes_per_pixel);
    intf_ErrMsg( "vout p_vout->i_screen_depth %d" , p_vout->i_screen_depth);
#endif

    return( 0 );
}

/*****************************************************************************
 * Find the best memory (AGP, VRAM, system) location
 *****************************************************************************/
void FindBestMemoryLocation( vout_thread_t *p_vout )
{
    long versionSystem;

    Gestalt( gestaltSystemVersion, &versionSystem );
    if ( 0x00000900 <= ( versionSystem & 0x00000FF00  ) )
    {
        intf_ErrMsg( "FindBestMemoryLocation : gNewNewGWorld = true" );
        p_vout->p_sys->gNewNewGWorld = true;
    }
    else
    {
        // now it is tricky
        // we will try to allocate in VRAM and find out where the allocation really ended up.
        GWorldPtr pgwTest = NULL;
        Rect rectTest = {0, 0, 10, 10};
        short wPixDepth = 
            (**(GetPortPixMap( GetWindowPort( p_vout->p_sys->p_window ) ))).pixelSize;
        GDHandle hgdWindow = GetWindowDevice( p_vout );

        intf_ErrMsg( "FindBestMemoryLocation : gNewNewGWorld = false !" );
#if 0
        p_vout->i_screen_depth = wPixDepth;
        p_vout->i_bytes_per_pixel = wPixDepth;
        p_vout->i_bytes_per_line = (**(**hgdWindow).gdPMap).rowBytes & 0x3FFF ;
#endif
        if(    ( noErr == NewGWorld( &pgwTest, wPixDepth, &rectTest, NULL, hgdWindow,
                                     noNewDevice | useDistantHdwrMem ) ) 
            && ( pgwTest ) )
        {
            p_vout->p_sys->gNewNewGWorld = true;	
        }
        
        if( pgwTest )
        {
            DisposeGWorld( pgwTest );
        }
    }
}

/*****************************************************************************
 * CreateDisplay: setup display params...
 *****************************************************************************/
static int CreateDisplay( vout_thread_t *p_vout )
{
    PixMapHandle hPixmap0, hPixmap1;
    void * hPixmapBaseAddr0, * hPixmapBaseAddr1;

    //intf_ErrMsg( "CreateDisplay()" );

    if( MakeWindow( p_vout ) )
    {
        intf_ErrMsg( "vout error: can't open window display" );
        return( 1 );
    }

    // FindBestMemoryLocation( p_vout );

    //try to allocate @ best location, will eventaully trickle down to worst
    p_vout->p_sys->gwLocOffscreen = kInVRAM;
    if( AllocBuffer( p_vout, 0 ) || AllocBuffer( p_vout, 1 ) )
    {
        intf_ErrMsg( "vout error: can't alloc offscreen buffers" );
        return( 1 );
    }

//FIXME ? - lock this down until the end...
    hPixmap0 = GetGWorldPixMap( p_vout->p_sys->p_gw[0] );
    LockPixels(hPixmap0);
    hPixmap1 = GetGWorldPixMap( p_vout->p_sys->p_gw[1] );
    LockPixels(hPixmap1);
    
    //FIXME hopefully this is the same for all Gworlds & window since they are the same size
    p_vout->i_bytes_per_line = (**hPixmap0).rowBytes & 0x3FFF;

    if ( (hPixmap0 == NULL) || (hPixmap1 == NULL) )
    {
        intf_ErrMsg( "vout error: pixmap problem");
        UnlockPixels(hPixmap0);
        UnlockPixels(hPixmap1);
        return( 1 );
    }

    hPixmapBaseAddr0 = GetPixBaseAddr( hPixmap0 );
    hPixmapBaseAddr1 = GetPixBaseAddr( hPixmap1 );
    if ( (hPixmapBaseAddr0 == NULL) || (hPixmapBaseAddr1 == NULL) )
    {
        intf_ErrMsg( "vout error: pixmap base addr problem");
        return( 1 );
    }

//FIXME - if I ever dispose of the Gworlds and recreate them, i'll have a new address
//and I'll need to tell vout about them...  dunno what problems vout might have if we just updateGworld  
    p_vout->pf_setbuffers( p_vout, hPixmapBaseAddr0, hPixmapBaseAddr1 );

    return 0;
}

/*****************************************************************************
 * MakeWindow: open and set-up a Mac OS main window
 *****************************************************************************/
static int MakeWindow( vout_thread_t *p_vout )
{
    int left = 0;
    int top = 0;
    int bottom = p_vout->i_height;
    int right = p_vout->i_width;
    ProcessSerialNumber PSN;

    WindowAttributes windowAttr = kWindowStandardDocumentAttributes | 
                                    kWindowStandardHandlerAttribute |
                                    kWindowInWindowMenuAttribute;
    
    SetRect( &p_vout->p_sys->wrect, left, top, right, bottom );
    OffsetRect( &p_vout->p_sys->wrect, kWindowOffset, kWindowOffset );

    CreateNewWindow( kDocumentWindowClass, windowAttr, &p_vout->p_sys->wrect, &p_vout->p_sys->p_window );
    if ( p_vout->p_sys->p_window == nil )
    {
        return( 1 );
    }

    InstallStandardEventHandler(GetWindowEventTarget(p_vout->p_sys->p_window));
    SetPort( GetWindowPort( p_vout->p_sys->p_window ) );
    SetWindowTitleWithCFString( p_vout->p_sys->p_window, CFSTR("VLC") );
    ShowWindow( p_vout->p_sys->p_window );
    SelectWindow( p_vout->p_sys->p_window );

    //in case we are run from the command line, bring us to front instead of Terminal
    GetCurrentProcess(&PSN);
    SetFrontProcess(&PSN);

{
    short wPixDepth = (**(GetPortPixMap( GetWindowPort( p_vout->p_sys->p_window ) ))).pixelSize;
    p_vout->i_screen_depth = wPixDepth;
    p_vout->i_bytes_per_pixel = p_vout->i_screen_depth / 8;
    p_vout->i_bytes_per_line   = p_vout->i_width * p_vout->i_bytes_per_pixel;

    p_vout->i_bytes_per_line = (**(**GetWindowDevice( p_vout )).gdPMap).rowBytes & 0x3FFF ;

    switch ( p_vout->i_screen_depth )
    {
        case 32:
        case 24:
            p_vout->i_red_mask =   0xff0000;
            p_vout->i_green_mask = 0xff00;
            p_vout->i_blue_mask =  0xff;
            break;
        case 16:
        case 15:
            p_vout->i_red_mask =   0x00007c00;
            p_vout->i_green_mask = 0x000003e0;
            p_vout->i_blue_mask =  0x0000001f;
            break;
        default:
            break;
    }
}

#if 0
    p_vout->i_red_lshift = 0x10;
    p_vout->i_red_rshift = 0x0;
    p_vout->i_green_lshift = 0x8;
    p_vout->i_green_rshift = 0x0;
    p_vout->i_blue_lshift = 0x0;
    p_vout->i_blue_rshift = 0x0;

    p_vout->i_white_pixel = 0xffffff;
    p_vout->i_black_pixel = 0x0;
    p_vout->i_gray_pixel = 0x808080;
    p_vout->i_blue_pixel = 0x32;
#endif

    return( 0 );
}

/*****************************************************************************
 * AllocBuffer: forces offscreen allocation (if different than current) in
 * memory type specified
 *****************************************************************************/
static int AllocBuffer ( vout_thread_t *p_vout, short index )
{
    Rect bounds;
    GDHandle hgdWindow = GetWindowDevice( p_vout );

    switch ( p_vout->p_sys->gwLocOffscreen )
    {
        case kInVRAM:
            if ( noErr == NewGWorld( &p_vout->p_sys->p_gw[index], p_vout->i_screen_depth, 
                GetPortBounds( GetWindowPort( p_vout->p_sys->p_window ), &bounds ), NULL, 
                hgdWindow, noNewDevice | useDistantHdwrMem ) )		
            {
                intf_ErrMsg( "Allocate off screen image in VRAM" );
                break;
            }
            intf_ErrMsg( "Unable to allocate off screen image in VRAM, trying next best AGP" );
            p_vout->p_sys->gwLocOffscreen = kInAGP;
        case kInAGP:
            if (noErr == NewGWorld( &p_vout->p_sys->p_gw[index], p_vout->i_screen_depth, 
                GetPortBounds( GetWindowPort( p_vout->p_sys->p_window ), &bounds ), NULL, 
                hgdWindow, noNewDevice | useLocalHdwrMem ) )
            {
                intf_ErrMsg( "Allocate off screen image in AGP" );
                break;
            }
            intf_ErrMsg( "Unable to allocate off screen image in AGP, trying next best System" );
            p_vout->p_sys->gwLocOffscreen = kInSystem;
        case kInSystem:
        default:
            if ( noErr == NewGWorld( &p_vout->p_sys->p_gw[index], p_vout->i_screen_depth, 
                GetPortBounds( GetWindowPort( p_vout->p_sys->p_window ), &bounds ), NULL, 
                hgdWindow, noNewDevice | keepLocal) )
            {
                intf_ErrMsg( "Allocate off screen image in System" );
                break;
            }
            intf_ErrMsg( "Unable to allocate off screen image in System, no options left - failing" );
            p_vout->p_sys->gwLocOffscreen = kNoWhere;
            return( 1 ); // nothing was allocated
    } 
    return( 0 );
}

/*****************************************************************************
 * vout_Init: initialize video thread output method
 *****************************************************************************/
static int vout_Init( vout_thread_t *p_vout )
{
    //intf_ErrMsg( "vout_Init()" );
    return( 0 );
}

/*****************************************************************************
 * vout_End: terminate video thread output method
 *****************************************************************************/
static void vout_End( vout_thread_t *p_vout )
{
    //intf_ErrMsg( "vout_End()" );
    ;
}

/*****************************************************************************
 * vout_Destroy: destroy video thread output method
 *****************************************************************************/
static void vout_Destroy( vout_thread_t *p_vout )
{
    //intf_ErrMsg( "vout_Destroy()" );

//FIXME Big Lock around Gworlds
    PixMapHandle hPixmap0, hPixmap1;
    hPixmap0 = GetGWorldPixMap( p_vout->p_sys->p_gw[0] );
    hPixmap1 = GetGWorldPixMap( p_vout->p_sys->p_gw[1] );
    UnlockPixels(hPixmap0);
    UnlockPixels(hPixmap1);

    if ( p_vout->p_sys->p_gw[0] )
    {
        DisposeGWorld( p_vout->p_sys->p_gw[0] );
    }
    if ( p_vout->p_sys->p_gw[1] )
    {
        DisposeGWorld( p_vout->p_sys->p_gw[1] );
    }
    if ( p_vout->p_sys->p_window )
    {
        DisposeWindow( p_vout->p_sys->p_window );
    }

    free( p_vout->p_sys );
}

/*****************************************************************************
 * vout_Manage: handle events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * console events. It returns a non null value on error.
 *****************************************************************************/
static int vout_Manage( vout_thread_t *p_vout )
{
//    intf_ErrMsg( "vout_Manage()" );
    return( 0 );
}

/*****************************************************************************
 * vout_Display: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to image, waits until
 * it is displayed and switch the two rendering buffers, preparing next frame.
 *****************************************************************************/
static void vout_Display( vout_thread_t *p_vout )
{
//    intf_ErrMsg( "vout_Display()" );

//we should not be called if we set the status to paused or stopped via the interface
//    if ( p_vout->p_sys->playback_status != PAUSED &&  p_vout->p_sys->playback_status != STOPPED )
        BlitToWindow ( p_vout, p_vout->i_buffer_index );
}


/*****************************************************************************
 * flushQD: flushes buffered window area
 *****************************************************************************/
void flushQD( vout_thread_t *p_vout )
{
    CGrafPtr thePort;

    //intf_ErrMsg( "flushQD()" );
    
    thePort = GetWindowPort( p_vout->p_sys->p_window );
    
    /* flush the entire port */
    if (QDIsPortBuffered(thePort))
        QDFlushPortBuffer(thePort, NULL);

#if 0
    /* flush part of the port */
    if (QDIsPortBuffered(thePort)) {
        RgnHandle theRgn;
        theRgn = NewRgn();
            /* local port coordinates */
        SetRectRgn(theRgn, 10, 10, 100, 30); 
        QDFlushPortBuffer(thePort, theRgn);
        DisposeRgn(theRgn);
    }
#endif

}

/*****************************************************************************
 * BlitToWindow: checks offscreen and blits it to the front
 *****************************************************************************/

void BlitToWindow( vout_thread_t *p_vout, short index )
{
    Rect rectDest, rectSource;
    GrafPtr pCGrafSave, windowPort = GetWindowPort( p_vout->p_sys->p_window );

    //intf_ErrMsg( "BlitToWindow() for %d", index );

    GetPortBounds( p_vout->p_sys->p_gw[index], &rectSource );
    GetPortBounds( windowPort, &rectDest );
    
    GetPort ( &pCGrafSave );
    SetPortWindowPort( p_vout->p_sys->p_window );
//FIXME have global lock - kinda bad but oh well 
//    if ( LockPixels( GetGWorldPixMap( p_vout->p_sys->p_gw[index] ) ) )
//    {

//LockPortBits(GetWindowPort( p_vout->p_sys->p_window ));
//NoPurgePixels( GetGWorldPixMap( p_vout->p_sys->p_gw[index] ) );

        CopyBits( GetPortBitMapForCopyBits( p_vout->p_sys->p_gw[index] ), 
                    GetPortBitMapForCopyBits( GetWindowPort( p_vout->p_sys->p_window ) ), 
                    &rectSource, &rectDest, srcCopy, NULL);

//UnlockPortBits(GetWindowPort( p_vout->p_sys->p_window ));
//AllowPurgePixels( GetGWorldPixMap( p_vout->p_sys->p_gw[index] ) );

//        UnlockPixels( GetGWorldPixMap( p_vout->p_sys->p_gw[index] ) );
        //flushQD( p_vout );
//    }
    SetPort ( pCGrafSave );
}


/*****************************************************************************
 * GetWindowDevice: returns GDHandle that window resides on (most of it anyway)
 *****************************************************************************/
GDHandle GetWindowDevice( vout_thread_t *p_vout )
{
    GrafPtr pgpSave;
    Rect rectWind, rectSect;
    long greatestArea, sectArea;
    GDHandle hgdNthDevice, hgdZoomOnThisDevice = NULL;
    
    //intf_ErrMsg( "GetWindowDevice()" );

    GetPort( &pgpSave );
    SetPortWindowPort( p_vout->p_sys->p_window );
    GetPortBounds( GetWindowPort( p_vout->p_sys->p_window ), &rectWind );
    LocalToGlobal( ( Point* ) &rectWind.top );
    LocalToGlobal( ( Point* ) &rectWind.bottom );
    hgdNthDevice = GetDeviceList();
    greatestArea = 0;
    // check window against all gdRects in gDevice list and remember 
    //  which gdRect contains largest area of window}
    while ( hgdNthDevice )
    {
        if ( TestDeviceAttribute( hgdNthDevice, screenDevice ) )
        {
            if ( TestDeviceAttribute( hgdNthDevice, screenActive ) )
            {
                // The SectRect routine calculates the intersection 
                //  of the window rectangle and this gDevice 
                //  rectangle and returns TRUE if the rectangles intersect, 
                //  FALSE if they don't.
                SectRect( &rectWind, &( **hgdNthDevice ).gdRect, &rectSect );
                // determine which screen holds greatest window area
                //  first, calculate area of rectangle on current device
                sectArea = ( long )( rectSect.right - rectSect.left ) * ( rectSect.bottom - rectSect.top );
                if ( sectArea > greatestArea )
                {
                    greatestArea = sectArea;	// set greatest area so far
                    hgdZoomOnThisDevice = hgdNthDevice;	// set zoom device
                }
                hgdNthDevice = GetNextDevice( hgdNthDevice );
            }
        }
    } 	// of WHILE
    SetPort( pgpSave );
    return hgdZoomOnThisDevice;
}

/*****************************************************************************
 * FillOffScreen: fills offscreen buffer with random bright color
 *****************************************************************************/

void FillOffscreen( vout_thread_t *p_vout, short index )
{
    static RGBColor rgbColorOld;
    GDHandle hGDSave;
    CGrafPtr pCGrafSave;
    Rect rectSource;
    RGBColor rgbColor;
    
    //intf_ErrMsg( "FillOffscreen" );

    GetPortBounds( p_vout->p_sys->p_gw[index], &rectSource );
    
    do 
        rgbColor.red = ( Random () + 32767) / 2 + 32767;
    while ( abs ( rgbColor.red - rgbColorOld.red ) <  3000 );	
    do 
        rgbColor.green = (Random () + 32767) / 2 + 32767;
    while ( abs ( rgbColor.green - rgbColorOld.green ) <  3000);
    do 
        rgbColor.blue = (Random () + 32767) / 2 + 32767;
    while ( abs ( rgbColor.blue - rgbColorOld.blue ) <  3000);
    
    rgbColorOld = rgbColor;

    GetGWorld( &pCGrafSave, &hGDSave );
    SetGWorld( p_vout->p_sys->p_gw[index], NULL );
//FIXME have global lock - kinda bad but oh well 
//    if ( LockPixels( GetGWorldPixMap( p_vout->p_sys->p_gw[index] ) ) )
//    {
        // draw some background
        EraseRect( &rectSource );
        RGBForeColor( &rgbColor );
        PaintRect( &rectSource );
//        UnlockPixels( GetGWorldPixMap( p_vout->p_sys->p_gw[index] ) );
//    }
    SetGWorld( pCGrafSave, hGDSave );
}
