/*****************************************************************************
 * os2_factory.cpp
 *****************************************************************************
 * Copyright (C) 2003, 2013 the VideoLAN team
 *
 * Authors: Cyril Deguet      <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
 *          KO Myung-Hun      <komh@chollian.net>
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

#ifdef OS2_SKINS

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "os2_factory.hpp"
#include "os2_graphics.hpp"
#include "os2_timer.hpp"
#include "os2_window.hpp"
#include "os2_tooltip.hpp"
#include "os2_popup.hpp"
#include "os2_loop.hpp"
#include "../src/theme.hpp"
#include "../src/window_manager.hpp"
#include "../src/generic_window.hpp"
#include "../commands/cmd_dialogs.hpp"
#include "../commands/cmd_minimize.hpp"


static void MorphToPM( void )
{
    PPIB pib;
    PTIB tib;

    DosGetInfoBlocks(&tib, &pib);

    // Change flag from VIO to PM
    if (pib->pib_ultype==2) pib->pib_ultype = 3;
}


MRESULT EXPENTRY OS2Factory::OS2FrameProc( HWND hwnd, ULONG msg,
                                           MPARAM mp1, MPARAM mp2 )
{
    // Get pointer to thread info: should only work with the parent window
    intf_thread_t *p_intf = (intf_thread_t *)WinQueryWindowPtr( hwnd, 0 );

    OS2Factory *pFactory = (OS2Factory*)OS2Factory::instance( p_intf );

    if( msg == WM_ADJUSTWINDOWPOS )
    {
        PSWP pswp = ( PSWP )PVOIDFROMMP( mp1 );
        if( pswp->fl & ( SWP_MINIMIZE | SWP_RESTORE ))
        {
            // propagate to all the owned windows
            HENUM henum = WinBeginEnumWindows( HWND_DESKTOP );
            HWND  hwndNext;
            while(( hwndNext = WinGetNextWindow( henum )) != NULLHANDLE )
            {
                if( WinQueryWindow( hwndNext, QW_OWNER ) ==
                        pFactory->m_hParentClientWindow )
                    WinSetWindowPos( hwndNext, 0, 0, 0, 0, 0,
                                     pswp->fl &
                                         ( SWP_MINIMIZE | SWP_RESTORE ));
            }
        }
    }
    else if( msg == WM_SYSCOMMAND )
    {
        // If closing parent window
        if( SHORT1FROMMP(mp1) == SC_CLOSE )
        {
            libvlc_Quit( p_intf->p_libvlc );

            return 0;
        }
        else if( SHORT1FROMMP(mp1) == SC_MINIMIZE )
        {
            pFactory->minimize();

            return 0;
        }
        else if( SHORT1FROMMP(mp1) == SC_RESTORE )
        {
            pFactory->restore();

            return 0;
        }
        else
        {
            msg_Dbg( p_intf, "WM_SYSCOMMAND %i", (SHORT1FROMMP(mp1)));
        }
    }

    return pFactory->m_pfnwpOldFrameProc( hwnd, msg, mp1, mp2 );
}

MRESULT EXPENTRY OS2Factory::OS2Proc( HWND hwnd, ULONG msg,
                                      MPARAM mp1, MPARAM mp2 )
{
    // Get pointer to thread info: should only work with the parent window
    intf_thread_t *p_intf = (intf_thread_t *)WinQueryWindowPtr( hwnd, 0 );

    // If doesn't exist, treat windows message normally
    if( p_intf == NULL || p_intf->p_sys->p_osFactory == NULL )
    {
        return WinDefWindowProc( hwnd, msg, mp1, mp2 );
    }

    OS2Factory *pFactory = (OS2Factory*)OS2Factory::instance( p_intf );
    GenericWindow *pWin = pFactory->m_windowMap[hwnd];

    // Subclass a frame window
    if( !pFactory->m_pfnwpOldFrameProc )
    {
        // Store with it a pointer to the interface thread
        WinSetWindowPtr( pFactory->m_hParentWindow, 0, p_intf );

        pFactory->m_pfnwpOldFrameProc =
            WinSubclassWindow( pFactory->m_hParentWindow, OS2FrameProc );
    }

    if( hwnd != pFactory->getParentWindow() && pWin )
    {
        OS2Loop* pLoop =
            (OS2Loop*) OSFactory::instance( p_intf )->getOSLoop();
        if( pLoop )
            return pLoop->processEvent( hwnd, msg, mp1, mp2 );
    }

    // If hwnd does not match any window or message not processed
    return WinDefWindowProc( hwnd, msg, mp1, mp2 );
}


OS2Factory::OS2Factory( intf_thread_t *pIntf ):
    OSFactory( pIntf ), m_hParentWindow( 0 ),
    m_pfnwpOldFrameProc( 0 ), m_dirSep( "\\" )
{
    // see init()
}


bool OS2Factory::init()
{
    PCSZ vlc_name = "VLC Media Player";
    PCSZ vlc_icon = "VLC_ICON";
    PCSZ vlc_class = "SkinWindowClass";

    MorphToPM();

    m_hab = WinInitialize( 0 );
    m_hmq = WinCreateMsgQueue( m_hab, 0 );

    if( !WinRegisterClass( m_hab, vlc_class, OS2Factory::OS2Proc,
                           CS_SIZEREDRAW, sizeof( PVOID )))
    {
        msg_Err( getIntf(), "cannot register window class" );
        return false;
    }

    ULONG flFrame = FCF_SYSMENU | FCF_MINBUTTON | FCF_TASKLIST
                    /* | FCF_ICON */;

    m_hParentWindow = WinCreateStdWindow( HWND_DESKTOP,
                                          0,
                                          &flFrame,
                                          vlc_class,
                                          vlc_name,
                                          0,
                                          NULLHANDLE,
                                          0/* ID_VLC_ICON */,
                                          &m_hParentClientWindow );

    if( !m_hParentWindow )
    {
        msg_Err( getIntf(), "cannot create parent window" );
        return false;
    }

    // Store with it a pointer to the interface thread
    WinSetWindowPtr( m_hParentClientWindow, 0, getIntf());

    WinSetWindowPos( m_hParentWindow, HWND_TOP, 0, 0, 0, 0,
                     SWP_ACTIVATE | SWP_ZORDER | SWP_MOVE | SWP_SIZE |
                     SWP_SHOW );

    // Set the mouse pointer to a default arrow
    changeCursor( kDefaultArrow );

    // Initialize the resource path
    char *datadir = config_GetUserDir( VLC_DATA_DIR );
    m_resourcePath.push_back( (string)datadir + "\\skins" );
    free( datadir );
    datadir = config_GetDataDir();
    m_resourcePath.push_back( (string)datadir + "\\skins" );
    m_resourcePath.push_back( (string)datadir + "\\skins2" );
    m_resourcePath.push_back( (string)datadir + "\\share\\skins" );
    m_resourcePath.push_back( (string)datadir + "\\share\\skins2" );
    m_resourcePath.push_back( (string)datadir + "\\vlc\\skins" );
    m_resourcePath.push_back( (string)datadir + "\\vlc\\skins2" );
    free( datadir );

    // All went well
    return true;
}


OS2Factory::~OS2Factory()
{
    if( m_hParentWindow ) WinDestroyWindow( m_hParentWindow );

    WinDestroyMsgQueue( m_hmq );
    WinTerminate( m_hab );
}


OSGraphics *OS2Factory::createOSGraphics( int width, int height )
{
    return new OS2Graphics( getIntf(), width, height );
}


OSLoop *OS2Factory::getOSLoop()
{
    return OS2Loop::instance( getIntf() );
}


void OS2Factory::destroyOSLoop()
{
    OS2Loop::destroy( getIntf() );
}

void OS2Factory::minimize()
{
    /* Make sure no tooltip is visible first */
    getIntf()->p_sys->p_theme->getWindowManager().hideTooltip();

    WinSetWindowPos( m_hParentWindow, NULLHANDLE, 0, 0, 0, 0, SWP_MINIMIZE );
}

void OS2Factory::restore()
{
    WinSetWindowPos( m_hParentWindow, NULLHANDLE, 0, 0, 0, 0, SWP_RESTORE );
}

void OS2Factory::addInTray()
{
    // TODO
}

void OS2Factory::removeFromTray()
{
    // TODO
}

void OS2Factory::addInTaskBar()
{
    WinSetWindowPos( m_hParentWindow, NULLHANDLE, 0, 0, 0, 0, SWP_HIDE );

    HSWITCH hswitch = WinQuerySwitchHandle( m_hParentWindow, 0 );

    SWCNTRL swctl;
    WinQuerySwitchEntry( hswitch, &swctl );
    swctl.uchVisibility = SWL_VISIBLE;
    WinChangeSwitchEntry( hswitch, &swctl );

    WinSetWindowPos( m_hParentWindow, NULLHANDLE, 0, 0, 0, 0,
                     SWP_ACTIVATE | SWP_SHOW );
}

void OS2Factory::removeFromTaskBar()
{
    WinSetWindowPos( m_hParentWindow, NULLHANDLE, 0, 0, 0, 0, SWP_HIDE );

    HSWITCH hswitch = WinQuerySwitchHandle( m_hParentWindow, 0 );

    SWCNTRL swctl;
    WinQuerySwitchEntry( hswitch, &swctl );
    swctl.uchVisibility = SWL_INVISIBLE;
    WinChangeSwitchEntry( hswitch, &swctl );

    WinSetWindowPos( m_hParentWindow, NULLHANDLE, 0, 0, 0, 0,
                     SWP_ACTIVATE | SWP_SHOW );
}

OSTimer *OS2Factory::createOSTimer( CmdGeneric &rCmd )
{
    return new OS2Timer( getIntf(), rCmd, m_hParentClientWindow );
}


OSWindow *OS2Factory::createOSWindow( GenericWindow &rWindow, bool dragDrop,
                                      bool playOnDrop, OSWindow *pParent,
                                      GenericWindow::WindowType_t type )
{
    return new OS2Window( getIntf(), rWindow, m_hInst, m_hParentClientWindow,
                          dragDrop, playOnDrop, (OS2Window*)pParent, type );
}


OSTooltip *OS2Factory::createOSTooltip()
{
    return new OS2Tooltip( getIntf(), m_hInst, m_hParentClientWindow );
}


OSPopup *OS2Factory::createOSPopup()
{
    // XXX FIXME: this way of getting the handle really sucks!
    // In fact, the clean way would be to have in Builder::addPopup() a call
    // to pPopup->associateToWindow() (to be written)... but the problem is
    // that there is no way to access the OS-dependent window handle from a
    // GenericWindow (we cannot even access the OSWindow).
    if( m_windowMap.begin() == m_windowMap.end() )
    {
        msg_Err( getIntf(), "no window has been created before the popup!" );
        return NULL;
    }

    return new OS2Popup( getIntf(), m_windowMap.begin()->first );
}


int OS2Factory::getScreenWidth() const
{
    return WinQuerySysValue( HWND_DESKTOP, SV_CXSCREEN );
}


int OS2Factory::getScreenHeight() const
{
    return WinQuerySysValue( HWND_DESKTOP, SV_CYSCREEN );
}


void OS2Factory::getMonitorInfo( const GenericWindow &rWindow,
                                 int* p_x, int* p_y,
                                 int* p_width, int* p_height ) const
{
    *p_x = 0;
    *p_y = 0;
    *p_width = getScreenWidth();
    *p_height = getScreenHeight();
}


void OS2Factory::getMonitorInfo( int numScreen, int* p_x, int* p_y,
                                   int* p_width, int* p_height ) const
{
    *p_x = 0;
    *p_y = 0;
    *p_width = getScreenWidth();
    *p_height = getScreenHeight();
}


SkinsRect OS2Factory::getWorkArea() const
{
    // TODO : calculate Desktop Workarea excluding WarpCenter

    // Fill a Rect object
    return  SkinsRect( 0, 0, getScreenWidth(), getScreenHeight());
}


void OS2Factory::getMousePos( int &rXPos, int &rYPos ) const
{
    POINTL ptl;

    WinQueryPointerPos( HWND_DESKTOP, &ptl );

    rXPos = ptl.x;
    rYPos = ( getScreenHeight() - 1 ) - ptl.y;   // Invert Y
}


void OS2Factory::changeCursor( CursorType_t type ) const
{
    LONG id;
    switch( type )
    {
    default:
    case kDefaultArrow: id = SPTR_ARROW;    break;
    case kResizeNWSE:   id = SPTR_SIZENWSE; break;
    case kResizeNS:     id = SPTR_SIZENS;   break;
    case kResizeWE:     id = SPTR_SIZEWE;   break;
    case kResizeNESW:   id = SPTR_SIZENESW; break;
    }

    HPOINTER hptr = WinQuerySysPointer( HWND_DESKTOP, id, FALSE );
    WinSetPointer( HWND_DESKTOP, hptr );

    m_cursorType = type;
}


void OS2Factory::rmDir( const string &rPath )
{
    struct dirent *file;
    DIR *dir;

    dir = opendir( rPath.c_str() );
    if( !dir ) return;

    // Parse the directory and remove everything it contains
    while( (file = readdir( dir )) )
    {
        struct stat statbuf;
        string filename = file->d_name;

        // Skip "." and ".."
        if( filename == "." || filename == ".." )
        {
            continue;
        }

        filename = rPath + "\\" + filename;

        if( !stat( filename.c_str(), &statbuf ) && statbuf.st_mode & S_IFDIR )
        {
            rmDir( filename );
        }
        else
        {
            unlink( filename.c_str() );
        }
    }

    // Close the directory
    closedir( dir );

    // And delete it
    rmdir( rPath.c_str() );
}

#endif
