/*****************************************************************************
 * window_os2.c: OS/2 non-embedded video window provider
 *****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
 *
 * Authors: KO Myung-Hun <komh@chollian.net>
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
#include <vlc_charset.h>
#include <vlc_plugin.h>
#include <vlc_vout_window.h>
#include <vlc_mouse.h>
#include <vlc_actions.h>

#include <float.h>
#include <ctype.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int Open( vout_window_t *wnd );

vlc_module_begin ()
    set_shortname( N_("OS/2 window"))
    set_description( N_("OS/2 non-embeded window video"))
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VOUT )
    set_capability ("vout window", 30 )
    set_callback( Open )
vlc_module_end ()

typedef struct vout_window_sys_t
{
    TID tid;
    HEV ack_event;
    int i_result;
    HAB hab;
    HMQ hmq;
    HWND frame;
    HWND client;
    LONG i_screen_width;
    LONG i_screen_height;
    RECTL client_rect;
    unsigned button_pressed;
    bool is_mouse_hidden;
    ULONG cursor_timeout;
} vout_window_sys_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int ConvertKey( USHORT i_pmkey );
static void MousePressed( vout_window_t *wnd, HWND hwnd, unsigned button );
static void MouseReleased( vout_window_t *wnd, unsigned button );
static MRESULT EXPENTRY WndProc( HWND hwnd, ULONG msg,
                                 MPARAM mp1, MPARAM mp2 );
static void MorphToPM( void );
static void PMThread( void *arg );

static int Enable( vout_window_t *wnd, const vout_window_cfg_t *cfg );
static void Disable( struct vout_window_t *wnd );
static void Resize( vout_window_t *wnd, unsigned width, unsigned height );
static void Close(vout_window_t *wnd );
static void SetState( vout_window_t *wnd, unsigned state );
static void UnsetFullscreen( vout_window_t *wnd );
static void SetFullscreen( vout_window_t *wnd, const char *id );
static void SetTitle( vout_window_t *wnd, const char *title );

#define WC_VLC_WINDOW_OS2 "WC_VLC_WINDOW_OS2"

#define WM_VLC_ENABLE       ( WM_USER + 1 )
#define WM_VLC_RESIZE       ( WM_USER + 2 )
#define WM_VLC_SETSTATE     ( WM_USER + 3 )
#define WM_VLC_FULLSCREEN   ( WM_USER + 4 )
#define WM_VLC_SETTITLE     ( WM_USER + 5 )

#define TID_HIDE_MOUSE  0x1010

static const struct
{
    USHORT i_pmkey;
    int    i_vlckey;
} pmkeys_to_vlckeys[] =
{
    { VK_LEFT, KEY_LEFT },
    { VK_RIGHT, KEY_RIGHT },
    { VK_UP, KEY_UP },
    { VK_DOWN, KEY_DOWN },
    { VK_SPACE, ' ' },
    { VK_NEWLINE, KEY_ENTER },
    { VK_ENTER, KEY_ENTER },
    { VK_F1, KEY_F1 },
    { VK_F2, KEY_F2 },
    { VK_F3, KEY_F3 },
    { VK_F4, KEY_F4 },
    { VK_F5, KEY_F5 },
    { VK_F6, KEY_F6 },
    { VK_F7, KEY_F7 },
    { VK_F8, KEY_F8 },
    { VK_F9, KEY_F9 },
    { VK_F10, KEY_F10 },
    { VK_F11, KEY_F11 },
    { VK_F12, KEY_F12 },
    { VK_HOME, KEY_HOME },
    { VK_END, KEY_END },
    { VK_INSERT, KEY_INSERT },
    { VK_DELETE, KEY_DELETE },
/*
    Not supported
    {, KEY_MENU },
*/
    { VK_ESC, KEY_ESC },
    { VK_PAGEUP, KEY_PAGEUP },
    { VK_PAGEDOWN, KEY_PAGEDOWN },
    { VK_TAB, KEY_TAB },
    { VK_BACKSPACE, KEY_BACKSPACE },
/*
    Not supported
    {, KEY_MOUSEWHEELUP },
    {, KEY_MOUSEWHEELDOWN },
    {, KEY_MOUSEWHEELLEFT },
    {, KEY_MOUSEWHEELRIGHT },

    {, KEY_BROWSER_BACK },
    {, KEY_BROWSER_FORWARD },
    {, KEY_BROWSER_REFRESH },
    {, KEY_BROWSER_STOP },
    {, KEY_BROWSER_SEARCH },
    {, KEY_BROWSER_FAVORITES },
    {, KEY_BROWSER_HOME },
    {, KEY_VOLUME_MUTE },
    {, KEY_VOLUME_DOWN },
    {, KEY_VOLUME_UP },
    {, KEY_MEDIA_NEXT_TRACK },
    {, KEY_MEDIA_PREV_TRACK },
    {, KEY_MEDIA_STOP },
    {, KEY_MEDIA_PLAY_PAUSE },
*/

    { 0, 0 }
};

static int ConvertKey( USHORT i_pmkey )
{
    int i;
    for( i = 0; pmkeys_to_vlckeys[ i ].i_pmkey != 0; i++ )
    {
        if( pmkeys_to_vlckeys[ i ].i_pmkey == i_pmkey )
            return pmkeys_to_vlckeys[ i ].i_vlckey;
    }
    return 0;
}

static void MousePressed( vout_window_t *wnd, HWND hwnd, unsigned button )
{
    vout_window_sys_t *sys = wnd->sys;

    if( WinQueryFocus( HWND_DESKTOP ) != hwnd )
        WinSetFocus( HWND_DESKTOP, hwnd );

    if( !sys->button_pressed )
        WinSetCapture( HWND_DESKTOP, hwnd );

    sys->button_pressed |= 1 << button;

    vout_window_ReportMousePressed( wnd, button );
}

static void MouseReleased( vout_window_t *wnd, unsigned button )
{
    vout_window_sys_t *sys = wnd->sys;

    sys->button_pressed &= ~( 1 << button );
    if( !sys->button_pressed )
        WinSetCapture( HWND_DESKTOP, NULLHANDLE );

    vout_window_ReportMouseReleased(wnd, button);
}

#define WM_MOUSELEAVE   0x41F

static MRESULT EXPENTRY WndProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
    vout_window_t *wnd = WinQueryWindowPtr( hwnd, 0 );
    MRESULT result = ( MRESULT )TRUE;

    if ( !wnd )
        return WinDefWindowProc( hwnd, msg, mp1, mp2 );

    vout_window_sys_t *sys = wnd->sys;
    RECTL rcl;
    SWP   swp;

    if ( sys->is_mouse_hidden &&
         (( msg >= WM_MOUSEFIRST    && msg <= WM_MOUSELAST ) ||
          ( msg >= WM_EXTMOUSEFIRST && msg <= WM_EXTMOUSELAST ) ||
            msg == WM_MOUSELEAVE ))
    {
        WinShowPointer( HWND_DESKTOP, TRUE );
        sys->is_mouse_hidden = false;

        WinStartTimer( sys->hab, sys->client, TID_HIDE_MOUSE,
                       sys->cursor_timeout );
    }

    switch( msg )
    {
        /* the user wants to close the window */
        case WM_CLOSE:
            vout_window_ReportClose( wnd );
            WinPostMsg( hwnd, WM_QUIT, 0, 0 );
            result = 0;
            break;

        case WM_SIZE:
            vout_window_ReportSize( wnd,
                                    SHORT1FROMMP( mp2 ), SHORT2FROMMP( mp2 ));
            result = 0;
            break;

        case WM_MOUSEMOVE:
        {
            SHORT i_mouse_x = SHORT1FROMMP( mp1 );
            SHORT i_mouse_y = SHORT2FROMMP( mp1 );

            WinQueryWindowRect( hwnd, &rcl );

            // Invert Y
            int h = rcl.yTop - rcl.yBottom;
            i_mouse_y = ( h - i_mouse_y ) - 1;

            vout_window_ReportMouseMoved( wnd, i_mouse_x, i_mouse_y );

            result = WinDefWindowProc( hwnd, msg, mp1, mp2 );
            break;
        }

        case WM_BUTTON1DOWN :
            MousePressed( wnd, hwnd, MOUSE_BUTTON_LEFT );
            break;

        case WM_BUTTON2DOWN :
            MousePressed( wnd, hwnd, MOUSE_BUTTON_RIGHT );
            break;

        case WM_BUTTON3DOWN :
            MousePressed( wnd, hwnd, MOUSE_BUTTON_CENTER );
            break;

        case WM_BUTTON1UP :
            MouseReleased( wnd, MOUSE_BUTTON_LEFT );
            break;

        case WM_BUTTON2UP :
            MouseReleased( wnd, MOUSE_BUTTON_RIGHT );
            break;

        case WM_BUTTON3UP :
            MouseReleased( wnd, MOUSE_BUTTON_CENTER );
            break;

        case WM_BUTTON1DBLCLK :
            vout_window_ReportMouseDoubleClick( wnd, MOUSE_BUTTON_LEFT );
            break;

        case WM_BUTTON2DBLCLK :
            vout_window_ReportMouseDoubleClick( wnd, MOUSE_BUTTON_RIGHT );
            break;

        case WM_BUTTON3DBLCLK :
            vout_window_ReportMouseDoubleClick( wnd, MOUSE_BUTTON_CENTER );
            break;

        case WM_TRANSLATEACCEL :
            /* We have no accelerator table at all */
            result = ( MRESULT )FALSE;
            break;

        case WM_CHAR :
        {
            USHORT i_flags = SHORT1FROMMP( mp1 );
            USHORT i_ch    = SHORT1FROMMP( mp2 );
            USHORT i_vk    = SHORT2FROMMP( mp2 );
            int    i_key   = 0;

            if( !( i_flags & KC_KEYUP ))
            {
                if( i_flags & KC_VIRTUALKEY )
                    /* convert the key if possible */
                    i_key = ConvertKey( i_vk );
                else if(( i_flags & KC_CHAR ) && !HIBYTE( i_ch ))
                    i_key = tolower( i_ch );

                if( i_key )
                {
                    if( i_flags & KC_SHIFT )
                       i_key |= KEY_MODIFIER_SHIFT;

                    if( i_flags & KC_CTRL )
                        i_key |= KEY_MODIFIER_CTRL;

                    if( i_flags & KC_ALT )
                        i_key |= KEY_MODIFIER_ALT;

                    vout_window_ReportKeyPress( wnd, i_key );
                }
            }
            break;
        }

        case WM_TIMER :
            if( !sys->is_mouse_hidden &&
                SHORT1FROMMP( mp1 ) == TID_HIDE_MOUSE )
            {
                POINTL ptl;

                WinQueryPointerPos( HWND_DESKTOP, &ptl );
                if( WinWindowFromPoint( HWND_DESKTOP, &ptl, TRUE )
                        == sys->client )
                {
                    WinShowPointer( HWND_DESKTOP, FALSE );
                    sys->is_mouse_hidden = true;

                    WinStopTimer( sys->hab, sys->client, TID_HIDE_MOUSE );
                }
            }
            break;

        case WM_VLC_ENABLE :
        {
            if( !LONGFROMMP( mp1 ))
            {
                /* Disable */
                WinShowWindow( sys->frame, FALSE );
                break;
            }

            /* Enable */
            vout_window_cfg_t *cfg = PVOIDFROMMP( mp2 );

            if( cfg->is_decorated )
            {
                WinSetParent( WinWindowFromID( sys->frame, FID_SYSMENU ),
                              sys->frame, FALSE );
                WinSetParent( WinWindowFromID( sys->frame, FID_TITLEBAR ),
                              sys->frame, FALSE );
                WinSetParent( WinWindowFromID( sys->frame, FID_MINMAX ),
                              sys->frame, FALSE );

                WinSetWindowBits( sys->frame, QWL_STYLE,
                                  FS_SIZEBORDER, FS_SIZEBORDER );
            }
            else
            {
                WinSetParent( WinWindowFromID( sys->frame, FID_SYSMENU ),
                              HWND_OBJECT, FALSE );
                WinSetParent( WinWindowFromID( sys->frame, FID_TITLEBAR ),
                              HWND_OBJECT, FALSE );
                WinSetParent( WinWindowFromID( sys->frame, FID_MINMAX ),
                              HWND_OBJECT, FALSE );

                WinSetWindowBits( sys->frame, QWL_STYLE, 0, FS_SIZEBORDER );
            }

            /* allow user to regain control over input events if requested */
            bool b_mouse_support = var_InheritBool( wnd, "mouse-events");
            bool b_key_support = var_InheritBool( wnd, "keyboard-events");
            WinEnableWindow( hwnd, b_mouse_support || b_key_support );

            sys->client_rect.xLeft   = ( sys->i_screen_width - cfg->width )
                                       / 2;
            sys->client_rect.yBottom = ( sys->i_screen_height - cfg->height )
                                       / 2;
            sys->client_rect.xRight  = sys->client_rect.xLeft + cfg->width;
            sys->client_rect.yTop    = sys->client_rect.yBottom + cfg->height;

            if( cfg->is_fullscreen )
                WinQueryWindowRect( HWND_DESKTOP, &rcl );
            else
                rcl = sys->client_rect;

            WinCalcFrameRect( sys->frame, &rcl, FALSE );

            WinSetWindowPos( sys->frame, HWND_TOP,
                             rcl.xLeft, rcl.yBottom,
                             rcl.xRight - rcl.xLeft, rcl.yTop - rcl.yBottom,
                             SWP_ZORDER | SWP_MOVE | SWP_SIZE |
                             SWP_SHOW | SWP_ACTIVATE );

            free( cfg );
            break;
        }

        case WM_VLC_RESIZE :
            rcl.xLeft   = 0;
            rcl.yBottom = 0;
            rcl.xRight  = LONGFROMMP( mp1 );
            rcl.yTop    = LONGFROMMP( mp2 );
            WinCalcFrameRect( sys->frame, &rcl, FALSE );

            WinSetWindowPos( sys->frame, NULLHANDLE,
                             0, 0,
                             rcl.xRight - rcl.xLeft, rcl.yTop - rcl.yBottom,
                             SWP_SIZE );

            WinQueryWindowPos( sys->frame, &swp );
            sys->client_rect.xLeft   = swp.x;
            sys->client_rect.yBottom = swp.y;
            sys->client_rect.xRight  = sys->client_rect.xLeft   + swp.cx;
            sys->client_rect.yTop    = sys->client_rect.yBottom + swp.cy;
            WinCalcFrameRect( sys->frame, &sys->client_rect, TRUE );
            break;

        case WM_VLC_SETSTATE:
            /* TODO */
            break;

        case WM_VLC_FULLSCREEN:
            if( LONGFROMMP( mp1 ))
            {
                /* Fullscreen */
                WinQueryWindowPos( sys->frame, &swp );
                sys->client_rect.xLeft   = swp.x;
                sys->client_rect.yBottom = swp.y;
                sys->client_rect.xRight  = sys->client_rect.xLeft   + swp.cx;
                sys->client_rect.yTop    = sys->client_rect.yBottom + swp.cy;
                WinCalcFrameRect( sys->frame, &sys->client_rect, TRUE );

                WinQueryWindowRect( HWND_DESKTOP, &rcl );
            }
            else
            {
                /* Windowed */
                rcl = sys->client_rect;
            }

            WinCalcFrameRect( sys->frame, &rcl, FALSE );

            WinSetWindowPos( sys->frame, HWND_TOP,
                             rcl.xLeft, rcl.yBottom,
                             rcl.xRight - rcl.xLeft, rcl.yTop - rcl.yBottom,
                             SWP_MOVE | SWP_SIZE | SWP_ZORDER | SWP_SHOW |
                             SWP_ACTIVATE );
            break;

        case WM_VLC_SETTITLE :
        {
            char *title = PVOIDFROMMP( mp1 );

            WinSetWindowText( hwnd, title );

            free( title );
            break;
        }

        default :
            return WinDefWindowProc( hwnd, msg, mp1, mp2 );
    }

    return result;
}

static void MorphToPM( void )
{
    PPIB pib;

    DosGetInfoBlocks(NULL, &pib);

    /* Change flag from VIO to PM */
    if (pib->pib_ultype == 2)
        pib->pib_ultype = 3;
}

static void PMThread( void *arg )
{
    vout_window_t *wnd = arg;
    vout_window_sys_t *sys = wnd->sys;
    ULONG i_frame_flags;
    QMSG qm;

    /* */
    MorphToPM();

    sys->hab = WinInitialize( 0 );
    sys->hmq = WinCreateMsgQueue( sys->hab, 0);

    WinRegisterClass( sys->hab,
                      WC_VLC_WINDOW_OS2,
                      WndProc,
                      CS_SIZEREDRAW | CS_MOVENOTIFY,
                      sizeof( PVOID ));

    i_frame_flags = FCF_SYSMENU | FCF_TITLEBAR | FCF_MINMAX |
                    FCF_SIZEBORDER | FCF_TASKLIST;

    sys->frame =
        WinCreateStdWindow( HWND_DESKTOP,       /* parent window handle */
                            WS_VISIBLE,         /* frame window style */
                            &i_frame_flags,     /* window style */
                            WC_VLC_WINDOW_OS2,  /* class name */
                            "VLC Video Window", /* window title */
                            0L,                 /* default client style */
                            NULLHANDLE,         /* resource in exe file */
                            1,                  /* frame window id */
                            &sys->client );     /* client window handle */

    if( sys->frame == NULLHANDLE )
    {
        msg_Err( wnd, "cannot create a frame window");

        goto exit_error;
    }

    WinSetWindowPtr( sys->client, 0, wnd );

    sys->cursor_timeout =
        ( ULONG )var_InheritInteger( wnd, "mouse-hide-timeout" );
    WinStartTimer( sys->hab, sys->client, TID_HIDE_MOUSE,
                   sys->cursor_timeout );

    sys->i_screen_width  = WinQuerySysValue( HWND_DESKTOP, SV_CXSCREEN );
    sys->i_screen_height = WinQuerySysValue( HWND_DESKTOP, SV_CYSCREEN );

    /* Prevent SIG_FPE */
    _control87( MCW_EM, MCW_EM );

    sys->i_result = VLC_SUCCESS;
    DosPostEventSem( sys->ack_event );

    while( WinGetMsg( sys->hab, &qm, NULLHANDLE, 0, 0 ))
        WinDispatchMsg( sys->hab, &qm );

    WinDestroyWindow( sys->frame );

    if( sys->is_mouse_hidden )
        WinShowPointer( HWND_DESKTOP, TRUE );

exit_error:
    WinDestroyMsgQueue( sys->hmq );
    WinTerminate( sys->hab );

    sys->i_result = VLC_EGENERIC;
    DosPostEventSem( sys->ack_event );
}

static const struct vout_window_operations ops = {
    .enable  = Enable,
    .disable = Disable,
    .resize = Resize,
    .destroy = Close,
    .set_state = SetState,
    .unset_fullscreen = UnsetFullscreen,
    .set_fullscreen = SetFullscreen,
    .set_title = SetTitle,
};

static int Open( vout_window_t *wnd )
{
    vout_window_sys_t *sys;

    wnd->sys = sys = calloc( 1, sizeof( *sys ));
    if( !sys )
        return VLC_ENOMEM;

    DosCreateEventSem( NULL, &sys->ack_event, 0, FALSE );

    sys->tid = _beginthread( PMThread, NULL, 1024 * 1024, wnd );
    DosWaitEventSem( sys->ack_event, SEM_INDEFINITE_WAIT );

    if( sys->i_result != VLC_SUCCESS )
    {
        DosCloseEventSem( sys->ack_event );

        free( sys );

        return VLC_EGENERIC;
    }

    wnd->type = VOUT_WINDOW_TYPE_HWND;
    wnd->handle.hwnd = ( void * )sys->client;
    wnd->ops = &ops;
    wnd->info.has_double_click = true;

    return VLC_SUCCESS;
}

static int Enable( vout_window_t *wnd, const vout_window_cfg_t *cfg)
{
    vout_window_sys_t *sys = wnd->sys;

    vout_window_cfg_t *config = malloc( sizeof( *config ));
    if( !config )
        return VLC_ENOMEM;

    memcpy( config, cfg, sizeof( *config ));
    WinPostMsg( sys->client, WM_VLC_ENABLE,
                MPFROMLONG( TRUE ), MPFROMP( config ) );

    return VLC_SUCCESS;
}

static void Disable( struct vout_window_t *wnd )
{
    vout_window_sys_t *sys = wnd->sys;

    WinPostMsg( sys->client, WM_VLC_ENABLE, MPFROMLONG( FALSE ), 0 );
}

static void Resize( vout_window_t *wnd, unsigned width, unsigned height )
{
    vout_window_sys_t *sys = wnd->sys;

    WinPostMsg( sys->client, WM_VLC_RESIZE,
                MPFROMLONG( width ), MPFROMLONG( height ));
}

static void Close( vout_window_t *wnd )
{
    vout_window_sys_t *sys = wnd->sys;

    WinPostQueueMsg( sys->hmq, WM_QUIT, 0, 0 );

    DosWaitThread( &sys->tid, DCWW_WAIT );

    DosCloseEventSem( sys->ack_event );

    free( sys );
}

static void SetState( vout_window_t *wnd, unsigned state )
{
    vout_window_sys_t *sys = wnd->sys;

    WinPostMsg( sys->client, WM_VLC_SETSTATE, MPFROMLONG( state ), 0 );
}

static void UnsetFullscreen(vout_window_t *wnd)
{
    vout_window_sys_t *sys = wnd->sys;

    WinPostMsg( sys->client, WM_VLC_FULLSCREEN, MPFROMLONG( FALSE ), 0 );
}

static void SetFullscreen(vout_window_t *wnd, const char *id)
{
    VLC_UNUSED( id );
    vout_window_sys_t *sys = wnd->sys;

    WinPostMsg( sys->client, WM_VLC_FULLSCREEN, MPFROMLONG( TRUE ), 0 );
}

static void SetTitle(vout_window_t *wnd, const char *title)
{
    vout_window_sys_t *sys = wnd->sys;

    WinPostMsg( sys->client, WM_VLC_SETTITLE, MPFROMP( strdup( title )), 0 );
}
