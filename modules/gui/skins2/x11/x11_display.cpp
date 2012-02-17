/*****************************************************************************
 * x11_display.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef X11_SKINS

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/xpm.h>
#include <X11/extensions/shape.h>

#include "x11_display.hpp"
#include "../src/logger.hpp"

#include "../../../share/icons/32x32/vlc.xpm"

template<class type> type X11Display::putPixel(type r, type g, type b) const
{
    return ( (r >> m_redRightShift)   << m_redLeftShift   ) |
           ( (g >> m_greenRightShift) << m_greenLeftShift ) |
           ( (b >> m_blueRightShift)  << m_blueLeftShift  );
}

template<class type>
type X11Display::blendPixel(type v,type r, type g, type b, type a) const
{
    uint16_t temp;

    temp = ((uint8_t)((v >> m_redLeftShift) << m_redRightShift));
    uint8_t red = r + ( temp * (255 - a) ) / 255;
    temp = ((uint8_t)((v >> m_greenLeftShift) << m_greenRightShift));
    uint8_t green = g + ( temp * (255 - a) ) / 255;
    temp = ((uint8_t)((v >> m_blueLeftShift) << m_blueRightShift));
    uint8_t blue = b + ( temp * (255 - a) ) / 255;

    return putPixel<type>(red,green,blue);
}

X11Display::X11Display( intf_thread_t *pIntf ): SkinObject( pIntf ),
    m_mainWindow( 0 ), m_gc( NULL ), m_colormap( 0 )
{
    // Open a connection to the X Server
    m_pDisplay = XOpenDisplay( NULL );
    if( m_pDisplay == NULL )
    {
        msg_Dbg( pIntf, "Cannot open X display" );
        return;
    }

    // Load the XShape extension
    int event, error;
    XShapeQueryExtension( m_pDisplay, &event, &error );

    // Get the display parameters
    int screen = DefaultScreen( m_pDisplay );
    int depth = DefaultDepth( m_pDisplay, screen );
    int order = ImageByteOrder( m_pDisplay );
    Window root = DefaultRootWindow( m_pDisplay );

    // Template for looking up the XVisualInfo
    XVisualInfo xVInfoTemplate;
    xVInfoTemplate.screen = screen;
    xVInfoTemplate.depth = depth;

    XVisualInfo *pVInfo = NULL;
    int vCount = 0;

    switch( depth )
    {
    case 8:
        xVInfoTemplate.c_class = DirectColor;
        // Get the DirectColor visual
        pVInfo = XGetVisualInfo( m_pDisplay, VisualScreenMask |
                                 VisualClassMask, &xVInfoTemplate,
                                 &vCount );
        if( pVInfo == NULL )
        {
            msg_Err( getIntf(), "no DirectColor visual available" );
            m_pDisplay = NULL;
            break;
        }
        m_pVisual = pVInfo->visual;

        // Compute the color shifts
        getShifts( pVInfo->red_mask, m_redLeftShift, m_redRightShift );
        getShifts( pVInfo->green_mask, m_greenLeftShift,
                   m_greenRightShift );
        getShifts( pVInfo->blue_mask, m_blueLeftShift, m_blueRightShift );

        // Create a color map
        m_colormap = XCreateColormap( m_pDisplay, root,
                DefaultVisual( m_pDisplay, screen ), AllocAll );

        // Create the palette
        XColor pColors[255];
        for( uint16_t i = 0; i < 255; i++ )
        {
            // kludge: colors are indexed reversely because color 255 seems
            // to bereserved for black even if we try to set it to white
            pColors[i].pixel = 254-i;
            pColors[i].pad   = 0;
            pColors[i].flags = DoRed | DoGreen | DoBlue;
            pColors[i].red   =
                (i >> m_redLeftShift) << (m_redRightShift + 8);
            pColors[i].green =
                (i >> m_greenLeftShift) << (m_greenRightShift + 8);
            pColors[i].blue  =
                (i >> m_blueLeftShift) << (m_blueRightShift + 8);
        }
        XStoreColors( m_pDisplay, m_colormap, pColors, 255 );
        blendPixelImpl = &X11Display::blendPixel8;
        putPixelImpl = &X11Display::putPixel8;
        m_pixelSize = 1;
        break;

    case 15:
    case 16:
    case 24:
    case 32:
        // Get the TrueColor visual
        xVInfoTemplate.c_class = TrueColor;
        pVInfo = XGetVisualInfo( m_pDisplay, VisualScreenMask |
                                 VisualDepthMask | VisualClassMask,
                                 &xVInfoTemplate, &vCount );
        if( pVInfo == NULL )
        {
            msg_Err( getIntf(), "No TrueColor visual for depth %d", depth );
            m_pDisplay = NULL;
            break;
        }
        m_pVisual = pVInfo->visual;

        // Compute the color shifts
        getShifts( pVInfo->red_mask,   m_redLeftShift,   m_redRightShift   );
        getShifts( pVInfo->green_mask, m_greenLeftShift, m_greenRightShift );
        getShifts( pVInfo->blue_mask,  m_blueLeftShift,  m_blueRightShift  );

        if( depth == 15 || depth == 16 )
        {
            if( order == MSBFirst )
            {
                blendPixelImpl = &X11Display::blendPixel16MSB;
                putPixelImpl = &X11Display::putPixel16MSB;
            }
            else
            {
                blendPixelImpl = &X11Display::blendPixel16LSB;
                putPixelImpl = &X11Display::putPixel16LSB;
            }
            m_pixelSize = 2;
        }
        else
        {
            if( order == MSBFirst )
            {
                blendPixelImpl = &X11Display::blendPixel32MSB;
                putPixelImpl = &X11Display::putPixel32MSB;
            }
            else
            {
                blendPixelImpl = &X11Display::blendPixel32LSB;
                putPixelImpl = &X11Display::putPixel32LSB;
            }
            m_pixelSize = 4;
        }
        break;

    default:
        msg_Err( getIntf(), "unsupported depth: %d bpp", depth );
        m_pDisplay = NULL;
        break;
    }

    // Free the visual info
    if( pVInfo )
    {
        XFree( pVInfo );
    }

    // Create a graphics context that doesn't generate GraphicsExpose events
    if( m_pDisplay )
    {
        XGCValues xgcvalues;
        xgcvalues.graphics_exposures = False;
        m_gc = XCreateGC( m_pDisplay, root, GCGraphicsExposures, &xgcvalues );

        // Create a parent window to have a single task in the task bar
        XSetWindowAttributes attr;
        m_mainWindow = XCreateWindow( m_pDisplay, root, 0, 0, 1, 1, 0, 0,
                                      InputOutput, CopyFromParent, 0, &attr );

        // initialize WM_CLASS
        XClassHint classhint;
        classhint.res_name = (char*) "vlc";
        classhint.res_class = (char*) "Vlc";
        XSetClassHint( m_pDisplay, m_mainWindow, &classhint );

        // Receive WM_DELETE_WINDOW
        Atom wm_delete = XInternAtom( m_pDisplay, "WM_DELETE_WINDOW", False);
        XSetWMProtocols( m_pDisplay, m_mainWindow, &wm_delete, 1);

        // Changing decorations
        struct {
            unsigned long flags;
            unsigned long functions;
            unsigned long decorations;
            long input_mode;
            unsigned long status;
        } motifWmHints;
        Atom hints_atom = XInternAtom( m_pDisplay, "_MOTIF_WM_HINTS", False );
        motifWmHints.flags = 2;    // MWM_HINTS_DECORATIONS;
        motifWmHints.decorations = 0;
        XChangeProperty( m_pDisplay, m_mainWindow, hints_atom, hints_atom, 32,
                         PropModeReplace, (unsigned char *)&motifWmHints,
                         sizeof( motifWmHints ) / sizeof( long ) );

        // Change the window title
        XStoreName( m_pDisplay, m_mainWindow, "VLC Media Player" );

        // Add an icon
        Pixmap icon_pixmap, shape_pixmap;
        int ret = XpmCreatePixmapFromData( m_pDisplay, root, (char**)vlc_xpm,
                                           &icon_pixmap, &shape_pixmap, NULL );
        if( ret == XpmSuccess )
        {
            XWMHints *wm = XAllocWMHints();
            if( wm )
            {
                wm->icon_pixmap = icon_pixmap;
                wm->icon_mask = shape_pixmap;
                wm->flags = (IconPixmapHint|IconMaskHint);

                XSetWMHints( m_pDisplay, m_mainWindow, wm );
                XFree( wm );
            }
        }
        else
        {
            msg_Err( getIntf(), "icon failed to be loaded (err=%i)", ret );
        }

        // Receive map notify events
        XSelectInput( m_pDisplay, m_mainWindow, StructureNotifyMask );

        // Set an empty mask for the window
        Region mask = XCreateRegion();
        XShapeCombineRegion( m_pDisplay, m_mainWindow, ShapeBounding, 0, 0,
                             mask, ShapeSet );
        XDestroyRegion( mask );

        XMapWindow( m_pDisplay, m_mainWindow);

        // Move it outside the screen to avoid seeing it in workspace selector
        XMoveWindow( m_pDisplay, m_mainWindow, -10, -10 );

        // test EWMH capabilities
        testEWMH();

        // Force _NET_WM_PID whatever the WM _NET_SUPPORTED says
        m_net_wm_pid = XInternAtom( m_pDisplay, "_NET_WM_PID" , False );
    }
}


X11Display::~X11Display()
{
    if( m_mainWindow ) XDestroyWindow( m_pDisplay, m_mainWindow );
    if( m_gc )         XFreeGC( m_pDisplay, m_gc );
    if( m_colormap )   XFreeColormap( m_pDisplay, m_colormap );
    if( m_pDisplay )   XCloseDisplay( m_pDisplay );
}


void X11Display::testEWMH()
{
    int i_ret, i_format;
    unsigned long i_items, i_bytesafter;
    union { Atom *p_atom; unsigned char *p_char; } p_args;
    p_args.p_atom = NULL;

    m_net_wm_supported = XInternAtom( m_pDisplay, "_NET_SUPPORTED", False );
    msg_Dbg( getIntf(), "EWMH: supported %d", m_net_wm_supported != None ? 1 : 0 );
    if( m_net_wm_supported == None )
        return;

    i_ret = XGetWindowProperty( m_pDisplay, DefaultRootWindow( m_pDisplay ),
                                m_net_wm_supported,
                                0, 16384, False, AnyPropertyType,
                                &m_net_wm_supported,
                                &i_format, &i_items, &i_bytesafter,
                                (unsigned char **)&p_args );

    if( i_ret != Success || i_items == 0 )
        return; /* Not supported */

#define TEST_EWMH( name, value ) \
{\
    name = XInternAtom( m_pDisplay, value, False );\
    unsigned int i;\
    for( i = 0; i < i_items; i++ )\
    {\
        if( p_args.p_atom[i] == name ) break;\
    }\
    if( i == i_items )\
    {\
        msg_Dbg( getIntf(), "%s support: no", value );\
        name = None;\
    }\
    else\
        msg_Dbg( getIntf(), "%s support: yes", value );\
}

    TEST_EWMH( m_net_wm_state, "_NET_WM_STATE" )
    TEST_EWMH( m_net_wm_state_fullscreen, "_NET_WM_STATE_FULLSCREEN" )
    TEST_EWMH( m_net_wm_stays_on_top, "_NET_WM_STATE_STAYS_ON_TOP" )
    TEST_EWMH( m_net_wm_state_above, "_NET_WM_STATE_ABOVE" )

    TEST_EWMH( m_net_wm_window_opacity, "_NET_WM_WINDOW_OPACITY" )

    TEST_EWMH( m_net_wm_pid, "_NET_WM_PID" )

#undef TEST_EWMH

    XFree( p_args.p_atom );

}


void X11Display::getShifts( uint32_t mask, int &rLeftShift, int &rRightShift )
{
    for( rLeftShift = 0; (rLeftShift < 32) && !(mask & 1); rLeftShift++ )
    {
        mask >>= 1;
    }
    for( rRightShift = 8; (mask & 1) ; rRightShift--)
    {
        mask >>= 1;
    }
    if( rRightShift < 0 )
    {
        rLeftShift -= rRightShift;
        rRightShift = 0;
    }
}


void X11Display::blendPixel8( uint8_t *pPixel, uint8_t r, uint8_t g,
                              uint8_t b, uint8_t a ) const
{
    *pPixel = 255 - blendPixel<uint8_t>(255 - *pPixel,r,g,b,a);
}


void X11Display::blendPixel16MSB( uint8_t *pPixel, uint8_t r, uint8_t g,
                                  uint8_t b, uint8_t a ) const
{
    uint16_t value = pPixel[1] | pPixel[0] << 8;

    value = blendPixel<uint16_t>(value,r,g,b,a);

    pPixel[1] = value; value >>= 8;
    pPixel[0] = value;
}


void X11Display::blendPixel16LSB( uint8_t *pPixel, uint8_t r, uint8_t g,
                                  uint8_t b, uint8_t a ) const
{
    uint16_t value = pPixel[0] | pPixel[1] << 8;

    value = blendPixel<uint16_t>(value,r,g,b,a);

    pPixel[0] = value; value >>= 8;
    pPixel[1] = value;
}


void X11Display::blendPixel32MSB( uint8_t *pPixel, uint8_t r, uint8_t g,
                                  uint8_t b, uint8_t a ) const
{
    uint32_t value = pPixel[3] | pPixel[2] << 8 |
                     pPixel[1] << 16 | pPixel[0] << 24;

    value = blendPixel<uint32_t>(value,r,g,b,a);

    pPixel[3] = value; value >>= 8;
    pPixel[2] = value; value >>= 8;
    pPixel[1] = value; value >>= 8;
    pPixel[0] = value;
}


void X11Display::blendPixel32LSB( uint8_t *pPixel, uint8_t r, uint8_t g,
                                  uint8_t b, uint8_t a ) const
{
    uint32_t value = pPixel[0] | pPixel[1] << 8 |
                     pPixel[2] << 16 | pPixel[3] << 24;

    value = blendPixel<uint32_t>(value,r,g,b,a);

    pPixel[0] = value; value >>= 8;
    pPixel[1] = value; value >>= 8;
    pPixel[2] = value; value >>= 8;
    pPixel[3] = value;
}


void X11Display::putPixel8( uint8_t *pPixel, uint8_t r, uint8_t g,
                            uint8_t b, uint8_t a ) const
{
    (void)a;
    *pPixel = 255 - putPixel<uint8_t>(r,g,b);
}


void X11Display::putPixel16MSB( uint8_t *pPixel, uint8_t r, uint8_t g,
                                uint8_t b, uint8_t a ) const
{
    (void)a;
    uint16_t value = putPixel<uint16_t>(r, g, b);

    pPixel[1] = value; value >>= 8;
    pPixel[0] = value;
}


void X11Display::putPixel16LSB( uint8_t *pPixel, uint8_t r, uint8_t g,
                                uint8_t b, uint8_t a ) const
{
    (void)a;
    uint16_t value = putPixel<uint16_t>(r,g,b);
    pPixel[0] = value; value >>= 8;
    pPixel[1] = value;
}


void X11Display::putPixel32MSB( uint8_t *pPixel, uint8_t r, uint8_t g,
                                uint8_t b, uint8_t a ) const
{
    (void)a;
    uint32_t value = putPixel<uint32_t>(r,g,b);

    pPixel[3] = value; value >>= 8;
    pPixel[2] = value; value >>= 8;
    pPixel[1] = value; value >>= 8;
    pPixel[0] = value;
}


void X11Display::putPixel32LSB( uint8_t *pPixel, uint8_t r, uint8_t g,
                                uint8_t b, uint8_t a ) const
{
    (void)a;
    uint32_t value = putPixel<uint32_t>(r,g,b);

    pPixel[0] = value; value >>= 8;
    pPixel[1] = value; value >>= 8;
    pPixel[2] = value; value >>= 8;
    pPixel[3] = value;
}


unsigned long X11Display::getPixelValue( uint8_t r, uint8_t g, uint8_t b )
    const
{
    unsigned long value = putPixel<unsigned long>(r,g,b);

    return m_pixelSize==1 ? 255 - value : value;
}

#endif
