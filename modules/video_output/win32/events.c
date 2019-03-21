/*****************************************************************************
 * events.c: Windows video output events handler
 *****************************************************************************
 * Copyright (C) 2001-2009 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Martell Malone <martellmalone@gmail.com>
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
 * Preamble: This file contains the functions related to the creation of
 *             a window and the handling of its messages (events).
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "win32touch.h"

#include <vlc_common.h>
#include <vlc_vout_display.h>

#include <stdatomic.h>
#include <windows.h>
#include <windowsx.h>                                        /* GET_X_LPARAM */
#include <shellapi.h>                                         /* ExtractIcon */

#include "common.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
#define WM_VLC_CHANGE_TEXT  (WM_APP + 1)

struct event_thread_t
{
    vout_display_t *vd;

    /* */
    vlc_thread_t thread;
    vlc_mutex_t  lock;
    vlc_cond_t   wait;
    bool         b_ready;
    bool         b_done;
    bool         b_error;

    /* */
    bool use_desktop;

    /* Mouse */
    bool is_cursor_hidden;
    HCURSOR cursor_arrow;
    HCURSOR cursor_empty;
    unsigned button_pressed;
    int64_t hide_timeout;
    vlc_tick_t last_moved;

    /* Gestures */
    win32_gesture_sys_t *p_gesture;

    /* Sensors */
    void *p_sensors;

    /* Title */
    char *psz_title;

    int i_window_style;
    int x, y;
    unsigned width, height;

    /* */
    vout_window_t *parent_window;
    TCHAR class_main[256];
    TCHAR class_video[256];
    HWND hparent;
    HWND hwnd;
    HWND hvideownd;
    HWND hfswnd;
    video_format_t       source;
    vout_display_place_t place;

    HICON vlc_icon;

    atomic_bool has_moved;
};

/***************************
 * Local Prototypes        *
 ***************************/
/* Window Creation */
static int  Win32VoutCreateWindow( event_thread_t * );
static void Win32VoutCloseWindow ( event_thread_t * );
static long FAR PASCAL WinVoutEventProc( HWND, UINT, WPARAM, LPARAM );
static int  Win32VoutConvertKey( int i_key );

/* Display/Hide Cursor */
static void UpdateCursor( event_thread_t *p_event, bool b_show );
static HCURSOR EmptyCursor( HINSTANCE instance );

/* Mouse events sending functions */
static void MouseReleased( event_thread_t *p_event, unsigned button );
static void MousePressed( event_thread_t *p_event, HWND hwnd, unsigned button );

static void CALLBACK HideMouse(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
    VLC_UNUSED(uMsg); VLC_UNUSED(dwTime);
    if (hwnd)
    {
        event_thread_t *p_event = (event_thread_t *)idEvent;
        UpdateCursor( p_event, false );
    }
}

static void UpdateCursorMoved( event_thread_t *p_event )
{
    UpdateCursor( p_event, true );
    p_event->last_moved = vlc_tick_now();
    if( p_event->hwnd )
        SetTimer( p_event->hwnd, (UINT_PTR)p_event, p_event->hide_timeout, HideMouse );
}

/* Local helpers */
static inline bool isMouseEvent( WPARAM type )
{
    return type >= WM_MOUSEFIRST &&
           type <= WM_MOUSELAST;
}

static inline bool isKeyEvent( WPARAM type )
{
    return type >= WM_KEYFIRST &&
           type <= WM_KEYLAST;
}
/*****************************************************************************
 * EventThread: Create video window & handle its messages
 *****************************************************************************
 * This function creates a video window and then enters an infinite loop
 * that handles the messages sent to that window.
 * The main goal of this thread is to isolate the Win32 PeekMessage function
 * because this one can block for a long time.
 *****************************************************************************/
static void *EventThread( void *p_this )
{
    event_thread_t *p_event = (event_thread_t *)p_this;
    vout_display_t *vd = p_event->vd;
    MSG msg;
    POINT old_mouse_pos = {0,0}, mouse_pos;
    int canc = vlc_savecancel ();

    bool b_mouse_support = var_InheritBool( p_event->vd, "mouse-events" );
    bool b_key_support = var_InheritBool( p_event->vd, "keyboard-events" );

    vlc_mutex_lock( &p_event->lock );
    /* Create a window for the video */
    /* Creating a window under Windows also initializes the thread's event
     * message queue */
    if( Win32VoutCreateWindow( p_event ) )
        p_event->b_error = true;

    p_event->b_ready = true;
    vlc_cond_signal( &p_event->wait );

    const bool b_error = p_event->b_error;
    vlc_mutex_unlock( &p_event->lock );

    if( b_error )
    {
        vlc_restorecancel( canc );
        return NULL;
    }

    /* Main loop */
    /* GetMessage will sleep if there's no message in the queue */
    for( ;; )
    {
        vout_display_place_t place;

        if( !GetMessage( &msg, 0, 0, 0 ) )
        {
            vlc_mutex_lock( &p_event->lock );
            p_event->b_done = true;
            vlc_mutex_unlock( &p_event->lock );
            break;
        }

        /* Check if we are asked to exit */
        vlc_mutex_lock( &p_event->lock );
        const bool b_done = p_event->b_done;
        vlc_mutex_unlock( &p_event->lock );
        if( b_done )
            break;

        if( !b_mouse_support && isMouseEvent( msg.message ) )
            continue;

        if( !b_key_support && isKeyEvent( msg.message ) )
            continue;

        /* Handle mouse state */
        if( msg.message == WM_MOUSEMOVE ||
            msg.message == WM_NCMOUSEMOVE )
        {
            GetCursorPos( &mouse_pos );
            /* FIXME, why this >2 limits ? */
            if( (abs(mouse_pos.x - old_mouse_pos.x) > 2 ||
                (abs(mouse_pos.y - old_mouse_pos.y)) > 2 ) )
            {
                old_mouse_pos = mouse_pos;
                UpdateCursorMoved( p_event );
            }
        }
        else if( isMouseEvent( msg.message ) )
        {
            UpdateCursorMoved( p_event );
        }

        /* */
        switch( msg.message )
        {
        case WM_MOUSEMOVE:
            vlc_mutex_lock( &p_event->lock );
            place  = p_event->place;
            vlc_mutex_unlock( &p_event->lock );

            if( place.width > 0 && place.height > 0 )
            {
                int x = GET_X_LPARAM(msg.lParam);
                int y = GET_Y_LPARAM(msg.lParam);

                if( msg.hwnd == p_event->hvideownd )
                {
                    /* Child window */
                    x += place.x;
                    y += place.y;
                }
                vout_display_SendMouseMovedDisplayCoordinates(vd, x, y);
            }
            break;
        case WM_NCMOUSEMOVE:
            break;

        case WM_LBUTTONDOWN:
            MousePressed( p_event, msg.hwnd, MOUSE_BUTTON_LEFT );
            break;
        case WM_LBUTTONUP:
            MouseReleased( p_event, MOUSE_BUTTON_LEFT );
            break;
        case WM_LBUTTONDBLCLK:
            vout_display_SendEventMouseDoubleClick(vd);
            break;

        case WM_MBUTTONDOWN:
            MousePressed( p_event, msg.hwnd, MOUSE_BUTTON_CENTER );
            break;
        case WM_MBUTTONUP:
            MouseReleased( p_event, MOUSE_BUTTON_CENTER );
            break;

        case WM_RBUTTONDOWN:
            MousePressed( p_event, msg.hwnd, MOUSE_BUTTON_RIGHT );
            break;
        case WM_RBUTTONUP:
            MouseReleased( p_event, MOUSE_BUTTON_RIGHT );
            break;

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        {
            /* The key events are first processed here and not translated
             * into WM_CHAR events because we need to know the status of the
             * modifier keys. */
            int i_key = Win32VoutConvertKey( msg.wParam );
            if( !i_key )
            {
                /* This appears to be a "normal" (ascii) key */
                i_key = tolower( (unsigned char)MapVirtualKey( msg.wParam, 2 ) );
            }

            if( i_key )
            {
                if( GetKeyState(VK_CONTROL) & 0x8000 )
                {
                    i_key |= KEY_MODIFIER_CTRL;
                }
                if( GetKeyState(VK_SHIFT) & 0x8000 )
                {
                    i_key |= KEY_MODIFIER_SHIFT;
                }
                if( GetKeyState(VK_MENU) & 0x8000 )
                {
                    i_key |= KEY_MODIFIER_ALT;
                }

                vout_window_ReportKeyPress(p_event->parent_window, i_key);
            }
            break;
        }

        case WM_MOUSEWHEEL:
        {
            int i_key;
            if( GET_WHEEL_DELTA_WPARAM( msg.wParam ) > 0 )
            {
                i_key = KEY_MOUSEWHEELUP;
            }
            else
            {
                i_key = KEY_MOUSEWHEELDOWN;
            }
            if( i_key )
            {
                if( GetKeyState(VK_CONTROL) & 0x8000 )
                {
                    i_key |= KEY_MODIFIER_CTRL;
                }
                if( GetKeyState(VK_SHIFT) & 0x8000 )
                {
                    i_key |= KEY_MODIFIER_SHIFT;
                }
                if( GetKeyState(VK_MENU) & 0x8000 )
                {
                    i_key |= KEY_MODIFIER_ALT;
                }
                vout_window_ReportKeyPress(p_event->parent_window, i_key);
            }
            break;
        }

        case WM_VLC_CHANGE_TEXT:
        {
            vlc_mutex_lock( &p_event->lock );
            wchar_t *pwz_title = NULL;
            if( p_event->psz_title )
            {
                const size_t i_length = strlen(p_event->psz_title);
                pwz_title = vlc_alloc( i_length + 1, 2 );
                if( pwz_title )
                {
                    mbstowcs( pwz_title, p_event->psz_title, 2 * i_length );
                    pwz_title[i_length] = 0;
                }
            }
            vlc_mutex_unlock( &p_event->lock );

            if( pwz_title )
            {
                SetWindowTextW( p_event->hwnd, pwz_title );
                if( p_event->hfswnd )
                    SetWindowTextW( p_event->hfswnd, pwz_title );
                free( pwz_title );
            }
            break;
        }

        default:
            /* Messages we don't handle directly are dispatched to the
             * window procedure */
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            break;

        } /* End Switch */

    } /* End Main loop */

    /* Check for WM_QUIT if we created the window */
    if( !p_event->hparent && msg.message == WM_QUIT )
    {
        msg_Warn( vd, "WM_QUIT... should not happen!!" );
        p_event->hwnd = NULL; /* Window already destroyed */
    }

    msg_Dbg( vd, "Win32 Vout EventThread terminating" );

    Win32VoutCloseWindow( p_event );
    vlc_restorecancel(canc);
    return NULL;
}

void EventThreadUpdateTitle( event_thread_t *p_event, const char *psz_fallback )
{
    char *psz_title = var_InheritString( p_event->vd, "video-title" );
    if( !psz_title )
        psz_title = strdup( psz_fallback );
    if( !psz_title )
        return;

    vlc_mutex_lock( &p_event->lock );
    free( p_event->psz_title );
    p_event->psz_title = psz_title;
    vlc_mutex_unlock( &p_event->lock );

    PostMessage( p_event->hwnd, WM_VLC_CHANGE_TEXT, 0, 0 );
}
int EventThreadGetWindowStyle( event_thread_t *p_event )
{
    /* No need to lock, it is serialized by EventThreadStart */
    return p_event->i_window_style;
}

void EventThreadUpdateWindowPosition( event_thread_t *p_event,
                                      bool *pb_moved, bool *pb_resized,
                                      int x, int y, unsigned w, unsigned h )
{
    vlc_mutex_lock( &p_event->lock );
    *pb_moved   = x != p_event->x || y != p_event->y;
    *pb_resized = w != p_event->width || h != p_event->height;

    p_event->x      = x;
    p_event->y      = y;
    p_event->width  = w;
    p_event->height = h;
    vlc_mutex_unlock( &p_event->lock );
}

void EventThreadUpdateSourceAndPlace( event_thread_t *p_event,
                                      const video_format_t *p_source,
                                      const vout_display_place_t *p_place )
{
    vlc_mutex_lock( &p_event->lock );
    p_event->source = *p_source;
    p_event->place  = *p_place;
    vlc_mutex_unlock( &p_event->lock );
}

bool EventThreadGetAndResetHasMoved( event_thread_t *p_event )
{
    return atomic_exchange(&p_event->has_moved, false);
}

event_thread_t *EventThreadCreate( vout_display_t *vd, const vout_display_cfg_t *vdcfg)
{
    if (vdcfg->window->type != VOUT_WINDOW_TYPE_HWND &&
        !(vdcfg->window->type == VOUT_WINDOW_TYPE_DUMMY && vdcfg->window->handle.hwnd == 0))
        return NULL;
     /* Create the Vout EventThread, this thread is created by us to isolate
     * the Win32 PeekMessage function calls. We want to do this because
     * Windows can stay blocked inside this call for a long time, and when
     * this happens it thus blocks vlc's video_output thread.
     * Vout EventThread will take care of the creation of the video
     * window (because PeekMessage has to be called from the same thread which
     * created the window). */
    msg_Dbg( vd, "creating Vout EventThread" );
    event_thread_t *p_event = malloc( sizeof(*p_event) );
    if( !p_event )
        return NULL;

    p_event->vd = vd;
    vlc_mutex_init( &p_event->lock );
    vlc_cond_init( &p_event->wait );

    p_event->parent_window = vdcfg->window;

    p_event->is_cursor_hidden = false;
    p_event->button_pressed = 0;
    p_event->psz_title = NULL;
    p_event->source = vd->source;
    p_event->hwnd = NULL;
    atomic_init(&p_event->has_moved, false);
    vout_display_PlacePicture(&p_event->place, &vd->source, vdcfg);

    _sntprintf( p_event->class_main, sizeof(p_event->class_main)/sizeof(*p_event->class_main),
               _T("VLC video main %p"), (void *)p_event );
    _sntprintf( p_event->class_video, sizeof(p_event->class_video)/sizeof(*p_event->class_video),
               _T("VLC video output %p"), (void *)p_event );
    return p_event;
}

void EventThreadDestroy( event_thread_t *p_event )
{
    free( p_event->psz_title );
    vlc_cond_destroy( &p_event->wait );
    vlc_mutex_destroy( &p_event->lock );
    free( p_event );
}

int EventThreadStart( event_thread_t *p_event, event_hwnd_t *p_hwnd, const event_cfg_t *p_cfg )
{
    p_event->use_desktop = p_cfg->use_desktop;
    p_event->x           = p_cfg->x;
    p_event->y           = p_cfg->y;
    p_event->width       = p_cfg->width;
    p_event->height      = p_cfg->height;

    atomic_store(&p_event->has_moved, false);

    p_event->b_ready = false;
    p_event->b_done  = false;
    p_event->b_error = false;

    if( vlc_clone( &p_event->thread, EventThread, p_event,
                   VLC_THREAD_PRIORITY_LOW ) )
    {
        msg_Err( p_event->vd, "cannot create Vout EventThread" );
        return VLC_EGENERIC;
    }

    vlc_mutex_lock( &p_event->lock );
    while( !p_event->b_ready )
        vlc_cond_wait( &p_event->wait, &p_event->lock );
    const bool b_error = p_event->b_error;
    vlc_mutex_unlock( &p_event->lock );

    if( b_error )
    {
        vlc_join( p_event->thread, NULL );
        p_event->b_ready = false;
        return VLC_EGENERIC;
    }
    msg_Dbg( p_event->vd, "Vout EventThread running" );

    /* */
    p_hwnd->parent_window = p_event->parent_window;
    p_hwnd->hparent       = p_event->hparent;
    p_hwnd->hwnd          = p_event->hwnd;
    p_hwnd->hvideownd     = p_event->hvideownd;
    p_hwnd->hfswnd        = p_event->hfswnd;
    return VLC_SUCCESS;
}

void EventThreadStop( event_thread_t *p_event )
{
    if( !p_event->b_ready )
        return;

    vlc_mutex_lock( &p_event->lock );
    p_event->b_done = true;
    vlc_mutex_unlock( &p_event->lock );

    /* we need to be sure Vout EventThread won't stay stuck in
     * GetMessage, so we send a fake message */
    if( p_event->hwnd )
        PostMessage( p_event->hwnd, WM_NULL, 0, 0);

    vlc_join( p_event->thread, NULL );
    p_event->b_ready = false;
}


/***********************************
 * Local functions implementations *
 ***********************************/
static void UpdateCursor( event_thread_t *p_event, bool b_show )
{
    if( p_event->is_cursor_hidden == !b_show )
        return;
    p_event->is_cursor_hidden = !b_show;

#if 1
    HCURSOR cursor = b_show ? p_event->cursor_arrow : p_event->cursor_empty;
    if( p_event->hvideownd )
        SetClassLongPtr( p_event->hvideownd, GCLP_HCURSOR, (LONG_PTR)cursor );
    if( p_event->hwnd )
        SetClassLongPtr( p_event->hwnd, GCLP_HCURSOR, (LONG_PTR)cursor );
#endif

    /* FIXME I failed to find a cleaner way to force a redraw of the cursor */
    POINT p;
    GetCursorPos(&p);
    HWND hwnd = WindowFromPoint(p);
    if( hwnd == p_event->hvideownd || hwnd == p_event->hwnd )
    {
        SetCursor( cursor );
    }
}

static HCURSOR EmptyCursor( HINSTANCE instance )
{
    const int cw = GetSystemMetrics(SM_CXCURSOR);
    const int ch = GetSystemMetrics(SM_CYCURSOR);

    HCURSOR cursor = NULL;
    uint8_t *and = malloc(cw * ch);
    uint8_t *xor = malloc(cw * ch);
    if( and && xor )
    {
        memset(and, 0xff, cw * ch );
        memset(xor, 0x00, cw * ch );
        cursor = CreateCursor( instance, 0, 0, cw, ch, and, xor);
    }
    free( and );
    free( xor );

    return cursor;
}

static void MousePressed( event_thread_t *p_event, HWND hwnd, unsigned button )
{
    if( !p_event->button_pressed )
        SetCapture( hwnd );
    p_event->button_pressed |= 1 << button;
    vout_display_SendEventMousePressed( p_event->vd, button );
}

static void MouseReleased( event_thread_t *p_event, unsigned button )
{
    p_event->button_pressed &= ~(1 << button);
    if( !p_event->button_pressed )
        ReleaseCapture();
    vout_display_SendEventMouseReleased( p_event->vd, button );
}

#if defined(MODULE_NAME_IS_direct3d9) || defined(MODULE_NAME_IS_direct3d11)
static int CALLBACK
enumWindowsProc(HWND hwnd, LPARAM lParam)
{
    HWND *wnd = (HWND *)lParam;

    char name[128];
    name[0] = '\0';
    GetClassNameA( hwnd, name, 128 );

    if( !strcasecmp( name, "WorkerW" ) )
    {
        hwnd = FindWindowEx( hwnd, NULL, _T("SHELLDLL_DefView"), NULL );
        if( hwnd ) hwnd = FindWindowEx( hwnd, NULL, _T("SysListView32"), NULL );
        if( hwnd )
        {
            *wnd = hwnd;
            return false;
        }
    }
    return true;
}

static HWND GetDesktopHandle(vout_display_t *vd)
{
    /* Find Program Manager */
    HWND hwnd = FindWindow( _T("Progman"), NULL );
    if( hwnd ) hwnd = FindWindowEx( hwnd, NULL, _T("SHELLDLL_DefView"), NULL );
    if( hwnd ) hwnd = FindWindowEx( hwnd, NULL, _T("SysListView32"), NULL );
    if( hwnd )
        return hwnd;

    msg_Dbg( vd, "Couldn't find desktop icon window,. Trying the hard way." );

    EnumWindows( enumWindowsProc, (LPARAM)&hwnd );
    return hwnd;
}
#endif

/*****************************************************************************
 * Win32VoutCreateWindow: create a window for the video.
 *****************************************************************************
 * Before creating a direct draw surface, we need to create a window in which
 * the video will be displayed. This window will also allow us to capture the
 * events.
 *****************************************************************************/
static int Win32VoutCreateWindow( event_thread_t *p_event )
{
    vout_display_t *vd = p_event->vd;
    HINSTANCE  hInstance;
    HMENU      hMenu;
    RECT       rect_window;
    WNDCLASS   wc;                            /* window class components */
    TCHAR      vlc_path[MAX_PATH+1];
    int        i_style;

    msg_Dbg( vd, "Win32VoutCreateWindow" );

    /* Get this module's instance */
    hInstance = GetModuleHandle(NULL);

    #if defined(MODULE_NAME_IS_direct3d9) || defined(MODULE_NAME_IS_direct3d11)
    if( !p_event->use_desktop )
    #endif
    {
        /* If an external window was specified, we'll draw in it. */
        p_event->hparent = p_event->parent_window->handle.hwnd;
    }
    #if defined(MODULE_NAME_IS_direct3d9) || defined(MODULE_NAME_IS_direct3d11)
    else
    {
        p_event->parent_window = NULL;
        p_event->hparent = GetDesktopHandle(vd);
    }
    #endif
    p_event->cursor_arrow = LoadCursor(NULL, IDC_ARROW);
    p_event->cursor_empty = EmptyCursor(hInstance);

    /* Get the Icon from the main app */
    p_event->vlc_icon = NULL;
    if( GetModuleFileName( NULL, vlc_path, MAX_PATH ) )
    {
        p_event->vlc_icon = ExtractIcon( hInstance, vlc_path, 0 );
    }
    p_event->hide_timeout = var_InheritInteger( p_event->vd, "mouse-hide-timeout" );
    UpdateCursorMoved( p_event );

    /* Fill in the window class structure */
    wc.style         = CS_OWNDC|CS_DBLCLKS;          /* style: dbl click */
    wc.lpfnWndProc   = (WNDPROC)WinVoutEventProc;       /* event handler */
    wc.cbClsExtra    = 0;                         /* no extra class data */
    wc.cbWndExtra    = 0;                        /* no extra window data */
    wc.hInstance     = hInstance;                            /* instance */
    wc.hIcon         = p_event->vlc_icon;       /* load the vlc big icon */
    wc.hCursor       = p_event->is_cursor_hidden ? p_event->cursor_empty :
                                                   p_event->cursor_arrow;
#if !VLC_WINSTORE_APP
    wc.hbrBackground = GetStockObject(BLACK_BRUSH);  /* background color */
#else
    wc.hbrBackground = NULL;
#endif
    wc.lpszMenuName  = NULL;                                  /* no menu */
    wc.lpszClassName = p_event->class_main;       /* use a special class */

    /* Register the window class */
    if( !RegisterClass(&wc) )
    {
        if( p_event->vlc_icon )
            DestroyIcon( p_event->vlc_icon );

        msg_Err( vd, "Win32VoutCreateWindow RegisterClass FAILED (err=%lu)", GetLastError() );
        return VLC_EGENERIC;
    }

    /* Register the video sub-window class */
    wc.lpszClassName = p_event->class_video;
    wc.hIcon = 0;
    wc.hbrBackground = NULL; /* no background color */
    if( !RegisterClass(&wc) )
    {
        msg_Err( vd, "Win32VoutCreateWindow RegisterClass FAILED (err=%lu)", GetLastError() );
        return VLC_EGENERIC;
    }

    /* When you create a window you give the dimensions you wish it to
     * have. Unfortunatly these dimensions will include the borders and
     * titlebar. We use the following function to find out the size of
     * the window corresponding to the useable surface we want */
    rect_window.left   = 10;
    rect_window.top    = 10;
    rect_window.right  = rect_window.left + p_event->width;
    rect_window.bottom = rect_window.top  + p_event->height;

    i_style = var_GetBool( vd, "video-deco" )
        /* Open with window decoration */
        ? WS_OVERLAPPEDWINDOW|WS_SIZEBOX
        /* No window decoration */
        : WS_POPUP;
    AdjustWindowRect( &rect_window, i_style, 0 );
    i_style |= WS_VISIBLE|WS_CLIPCHILDREN;

    if( p_event->hparent )
    {
        i_style = WS_VISIBLE|WS_CLIPCHILDREN|WS_CHILD;

        /* allow user to regain control over input events if requested */
        bool b_mouse_support = var_InheritBool( vd, "mouse-events" );
        bool b_key_support = var_InheritBool( vd, "keyboard-events" );
        if( !b_mouse_support && !b_key_support )
            i_style |= WS_DISABLED;
    }

    p_event->i_window_style = i_style;

    /* Create the window */
    p_event->hwnd =
        CreateWindowEx( WS_EX_NOPARENTNOTIFY,
                    p_event->class_main,             /* name of window class */
                    _T(VOUT_TITLE) _T(" (VLC Video Output)"),/* window title */
                    i_style,                                 /* window style */
                    (!p_event->x) ? (UINT)CW_USEDEFAULT :
                        (UINT)p_event->x,            /* default X coordinate */
                    (!p_event->y) ? (UINT)CW_USEDEFAULT :
                        (UINT)p_event->y,            /* default Y coordinate */
                    RECTWidth(rect_window),                  /* window width */
                    RECTHeight(rect_window),                /* window height */
                    p_event->hparent,                       /* parent window */
                    NULL,                          /* no menu in this window */
                    hInstance,            /* handle of this program instance */
                    (LPVOID)p_event );           /* send vd to WM_CREATE */

    if( !p_event->hwnd )
    {
        msg_Warn( vd, "Win32VoutCreateWindow create window FAILED (err=%lu)", GetLastError() );
        return VLC_EGENERIC;
    }

    bool b_isProjected  = (vd->fmt.projection_mode != PROJECTION_MODE_RECTANGULAR);
    InitGestures( p_event->hwnd, &p_event->p_gesture, b_isProjected );

    p_event->p_sensors = HookWindowsSensors(vd, p_event->hwnd);

    if( p_event->hparent )
    {
        /* We don't want the window owner to overwrite our client area */
        LONG  parent_style = GetWindowLong( p_event->hparent, GWL_STYLE );
        if( !(parent_style & WS_CLIPCHILDREN) )
            /* Hmmm, apparently this is a blocking call... */
            SetWindowLong( p_event->hparent, GWL_STYLE,
                           parent_style | WS_CLIPCHILDREN );

        /* Create our fullscreen window */
        p_event->hfswnd =
            CreateWindowEx( WS_EX_APPWINDOW, p_event->class_main,
                            _T(VOUT_TITLE) _T(" (VLC Fullscreen Video Output)"),
                            WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN|WS_SIZEBOX,
                            CW_USEDEFAULT, CW_USEDEFAULT,
                            CW_USEDEFAULT, CW_USEDEFAULT,
                            NULL, NULL, hInstance, NULL );
    }
    else
    {
        p_event->hfswnd = NULL;
    }

    /* Append a "Always On Top" entry in the system menu */
    hMenu = GetSystemMenu( p_event->hwnd, FALSE );
    AppendMenu( hMenu, MF_SEPARATOR, 0, _T("") );
    AppendMenu( hMenu, MF_STRING | MF_UNCHECKED,
                       IDM_TOGGLE_ON_TOP, _T("Always on &Top") );

    /* Create video sub-window. This sub window will always exactly match
     * the size of the video, which allows us to use crazy overlay colorkeys
     * without having them shown outside of the video area. */
    /* FIXME vd->source.i_width/i_height seems wrong */
    p_event->hvideownd =
    CreateWindow( p_event->class_video, _T(""),   /* window class */
        WS_CHILD,                   /* window style, not visible initially */
        0, 0,
        vd->source.i_width,          /* default width */
        vd->source.i_height,        /* default height */
        p_event->hwnd,               /* parent window */
        NULL, hInstance,
        (LPVOID)p_event );    /* send vd to WM_CREATE */

    if( !p_event->hvideownd )
        msg_Warn( vd, "can't create video sub-window" );
    else
        msg_Dbg( vd, "created video sub-window" );

    /* Now display the window */
    ShowWindow( p_event->hwnd, SW_SHOW );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Win32VoutCloseWindow: close the window created by Win32VoutCreateWindow
 *****************************************************************************
 * This function returns all resources allocated by Win32VoutCreateWindow.
 *****************************************************************************/
static void Win32VoutCloseWindow( event_thread_t *p_event )
{
    vout_display_t *vd = p_event->vd;
    msg_Dbg( vd, "Win32VoutCloseWindow" );

    #if defined(MODULE_NAME_IS_direct3d9) || defined(MODULE_NAME_IS_direct3d11)
    DestroyWindow( p_event->hvideownd );
    #endif
    DestroyWindow( p_event->hwnd );
    if( p_event->hfswnd )
        DestroyWindow( p_event->hfswnd );

    p_event->hwnd = NULL;

    HINSTANCE hInstance = GetModuleHandle(NULL);
    UnregisterClass( p_event->class_video, hInstance );
    UnregisterClass( p_event->class_main, hInstance );

    if( p_event->vlc_icon )
        DestroyIcon( p_event->vlc_icon );

    DestroyCursor( p_event->cursor_empty );

    UnhookWindowsSensors(p_event->p_sensors);

    CloseGestures( p_event->p_gesture);
}

/*****************************************************************************
 * WinVoutEventProc: This is the window event processing function.
 *****************************************************************************
 * On Windows, when you create a window you have to attach an event processing
 * function to it. The aim of this function is to manage "Queued Messages" and
 * "Nonqueued Messages".
 * Queued Messages are those picked up and retransmitted by vout_Manage
 * (using the GetMessage and DispatchMessage functions).
 * Nonqueued Messages are those that Windows will send directly to this
 * procedure (like WM_DESTROY, WM_WINDOWPOSCHANGED...)
 *****************************************************************************/
static long FAR PASCAL WinVoutEventProc( HWND hwnd, UINT message,
                                         WPARAM wParam, LPARAM lParam )
{
    event_thread_t *p_event;

    if( message == WM_CREATE )
    {
        /* Store vd for future use */
        p_event = (event_thread_t *)((CREATESTRUCT *)lParam)->lpCreateParams;
        SetWindowLongPtr( hwnd, GWLP_USERDATA, (LONG_PTR)p_event );
        return TRUE;
    }
    else
    {
        LONG_PTR p_user_data = GetWindowLongPtr( hwnd, GWLP_USERDATA );
        p_event = (event_thread_t *)p_user_data;
        if( !p_event )
        {
            /* Hmmm mozilla does manage somehow to save the pointer to our
             * windowproc and still calls it after the vout has been closed. */
            return DefWindowProc(hwnd, message, wParam, lParam);
        }
    }
    vout_display_t *vd = p_event->vd;

#if 0
    if( message == WM_SETCURSOR )
    {
        msg_Err(vd, "WM_SETCURSOR: %d (t2)", p_event->is_cursor_hidden);
        SetCursor( p_event->is_cursor_hidden ? p_event->cursor_empty : p_event->cursor_arrow );
        return 1;
    }
#endif
    if( message == WM_CAPTURECHANGED )
    {
        for( int button = 0; p_event->button_pressed; button++ )
        {
            unsigned m = 1 << button;
            if( p_event->button_pressed & m )
                vout_display_SendEventMouseReleased( p_event->vd, button );
            p_event->button_pressed &= ~m;
        }
        p_event->button_pressed = 0;
        return 0;
    }

    if( hwnd == p_event->hvideownd )
    {
        switch( message )
        {
        /*
        ** For OpenGL and Direct3D, vout will update the whole
        ** window at regular interval, therefore dirty region
        ** can be ignored to minimize repaint.
        */
        case WM_ERASEBKGND:
            /* nothing to erase */
            return 1;
        case WM_PAINT:
            /* nothing to repaint */
            ValidateRect(hwnd, NULL);
            // fall through
        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
        }
    }

    switch( message )
    {

    case WM_WINDOWPOSCHANGED:
        atomic_store(&p_event->has_moved, true);
        return 0;

    /* the user wants to close the window */
    case WM_CLOSE:
        vout_window_ReportClose(p_event->parent_window);
        return 0;

    /* the window has been closed so shut down everything now */
    case WM_DESTROY:
        msg_Dbg( vd, "WinProc WM_DESTROY" );
        /* just destroy the window */
        PostQuitMessage( 0 );
        return 0;

    case WM_SYSCOMMAND:
        switch (wParam)
        {
        case IDM_TOGGLE_ON_TOP:            /* toggle the "on top" status */
        {
            msg_Dbg(vd, "WinProc WM_SYSCOMMAND: IDM_TOGGLE_ON_TOP");
            HMENU hMenu = GetSystemMenu(p_event->hwnd, FALSE);
            vout_display_SendWindowState(vd, (GetMenuState(hMenu, IDM_TOGGLE_ON_TOP, MF_BYCOMMAND) & MF_CHECKED) ?
                    VOUT_WINDOW_STATE_NORMAL : VOUT_WINDOW_STATE_ABOVE);
            return 0;
        }
        default:
            break;
        }
        break;

    case WM_PAINT:
    case WM_NCPAINT:
    case WM_ERASEBKGND:
        return DefWindowProc(hwnd, message, wParam, lParam);

    case WM_KILLFOCUS:
        return 0;

    case WM_SETFOCUS:
        return 0;

    case WM_GESTURE:
        return DecodeGesture( VLC_OBJECT(vd), p_event->p_gesture, hwnd, message, wParam, lParam );

    default:
        //msg_Dbg( vd, "WinProc WM Default %i", message );
        break;
    }

    /* Let windows handle the message */
    return DefWindowProc(hwnd, message, wParam, lParam);
}

static struct
{
    int i_dxkey;
    int i_vlckey;

} dxkeys_to_vlckeys[] =
{
    { VK_F1, KEY_F1 }, { VK_F2, KEY_F2 }, { VK_F3, KEY_F3 }, { VK_F4, KEY_F4 },
    { VK_F5, KEY_F5 }, { VK_F6, KEY_F6 }, { VK_F7, KEY_F7 }, { VK_F8, KEY_F8 },
    { VK_F9, KEY_F9 }, { VK_F10, KEY_F10 }, { VK_F11, KEY_F11 },
    { VK_F12, KEY_F12 },

    { VK_RETURN, KEY_ENTER },
    { VK_SPACE, ' ' },
    { VK_ESCAPE, KEY_ESC },

    { VK_LEFT, KEY_LEFT },
    { VK_RIGHT, KEY_RIGHT },
    { VK_UP, KEY_UP },
    { VK_DOWN, KEY_DOWN },

    { VK_HOME, KEY_HOME },
    { VK_END, KEY_END },
    { VK_PRIOR, KEY_PAGEUP },
    { VK_NEXT, KEY_PAGEDOWN },

    { VK_INSERT, KEY_INSERT },
    { VK_DELETE, KEY_DELETE },

    { VK_CONTROL, 0 },
    { VK_SHIFT, 0 },
    { VK_MENU, 0 },

    { VK_BROWSER_BACK, KEY_BROWSER_BACK },
    { VK_BROWSER_FORWARD, KEY_BROWSER_FORWARD },
    { VK_BROWSER_REFRESH, KEY_BROWSER_REFRESH },
    { VK_BROWSER_STOP, KEY_BROWSER_STOP },
    { VK_BROWSER_SEARCH, KEY_BROWSER_SEARCH },
    { VK_BROWSER_FAVORITES, KEY_BROWSER_FAVORITES },
    { VK_BROWSER_HOME, KEY_BROWSER_HOME },
    { VK_VOLUME_MUTE, KEY_VOLUME_MUTE },
    { VK_VOLUME_DOWN, KEY_VOLUME_DOWN },
    { VK_VOLUME_UP, KEY_VOLUME_UP },
    { VK_MEDIA_NEXT_TRACK, KEY_MEDIA_NEXT_TRACK },
    { VK_MEDIA_PREV_TRACK, KEY_MEDIA_PREV_TRACK },
    { VK_MEDIA_STOP, KEY_MEDIA_STOP },
    { VK_MEDIA_PLAY_PAUSE, KEY_MEDIA_PLAY_PAUSE },

    { 0, 0 }
};

static int Win32VoutConvertKey( int i_key )
{
    for( int i = 0; dxkeys_to_vlckeys[i].i_dxkey != 0; i++ )
    {
        if( dxkeys_to_vlckeys[i].i_dxkey == i_key )
        {
            return dxkeys_to_vlckeys[i].i_vlckey;
        }
    }

    return 0;
}

