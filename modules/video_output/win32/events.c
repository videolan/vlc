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

#include "common.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
#define WM_VLC_SET_TOP_STATE (WM_APP + 2)

struct event_thread_t
{
    vlc_object_t *obj;

    /* */
    vlc_thread_t thread;
    vlc_mutex_t  lock;
    vlc_cond_t   wait;
    bool         b_ready;
    atomic_bool  b_done;
    bool         b_error;

    /* */
    bool use_desktop;
    bool is_projected;

    /* Mouse */
    bool is_cursor_hidden;
    HCURSOR cursor_arrow;
    HCURSOR cursor_empty;
    unsigned button_pressed;
    int64_t hide_timeout;
    vlc_tick_t last_moved;

    /* Gestures */
    win32_gesture_sys_t *p_gesture;

    int i_window_style;
    RECT window_area;

    /* */
    vout_window_t *parent_window;
    WCHAR class_main[256];
    WCHAR class_video[256];
    HWND hparent;
    HWND hwnd;
    HWND hvideownd;
    HWND hfswnd;
    vout_display_place_t place;

    atomic_bool size_changed;
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
    MSG msg;
    POINT old_mouse_pos = {0,0}, mouse_pos;
    int canc = vlc_savecancel ();

    bool b_mouse_support = var_InheritBool( p_event->obj, "mouse-events" );
    bool b_key_support = var_InheritBool( p_event->obj, "keyboard-events" );

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
        if( !GetMessage( &msg, 0, 0, 0 ) || atomic_load( &p_event->b_done ) )
        {
            break;
        }

        if( !b_mouse_support && isMouseEvent( msg.message ) )
            continue;

        if( !b_key_support && isKeyEvent( msg.message ) )
            continue;

        /* Handle mouse state */
        if( msg.message == WM_MOUSEMOVE || msg.message == WM_NCMOUSEMOVE )
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
            vout_display_place_t place  = p_event->place;
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
                vout_window_ReportMouseMoved(p_event->parent_window, x, y);
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
            vout_window_ReportMouseDoubleClick(p_event->parent_window, MOUSE_BUTTON_LEFT);
            break;

        case WM_MBUTTONDOWN:
            MousePressed( p_event, msg.hwnd, MOUSE_BUTTON_CENTER );
            break;
        case WM_MBUTTONUP:
            MouseReleased( p_event, MOUSE_BUTTON_CENTER );
            break;
        case WM_MBUTTONDBLCLK:
            vout_window_ReportMouseDoubleClick(p_event->parent_window, MOUSE_BUTTON_CENTER);
            break;

        case WM_RBUTTONDOWN:
            MousePressed( p_event, msg.hwnd, MOUSE_BUTTON_RIGHT );
            break;
        case WM_RBUTTONUP:
            MouseReleased( p_event, MOUSE_BUTTON_RIGHT );
            break;
        case WM_RBUTTONDBLCLK:
            vout_window_ReportMouseDoubleClick(p_event->parent_window, MOUSE_BUTTON_RIGHT);
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
        msg_Warn( p_event->obj, "WM_QUIT... should not happen!!" );
        p_event->hwnd = NULL; /* Window already destroyed */
    }

    msg_Dbg( p_event->obj, "Win32 Vout EventThread terminating" );

    Win32VoutCloseWindow( p_event );
    vlc_restorecancel(canc);
    return NULL;
}

int EventThreadGetWindowStyle( event_thread_t *p_event )
{
    /* No need to lock, it is serialized by EventThreadStart */
    return p_event->i_window_style;
}

void EventThreadUpdatePlace( event_thread_t *p_event,
                                      const vout_display_place_t *p_place )
{
    vlc_mutex_lock( &p_event->lock );
    p_event->place  = *p_place;
    vlc_mutex_unlock( &p_event->lock );
}

bool EventThreadGetAndResetSizeChanged( event_thread_t *p_event )
{
    return atomic_exchange(&p_event->size_changed, false);
}

event_thread_t *EventThreadCreate( vlc_object_t *obj, vout_window_t *parent_window)
{
    if (parent_window->type != VOUT_WINDOW_TYPE_HWND &&
        !(parent_window->type == VOUT_WINDOW_TYPE_DUMMY && parent_window->handle.hwnd == 0))
        return NULL;
     /* Create the Vout EventThread, this thread is created by us to isolate
     * the Win32 PeekMessage function calls. We want to do this because
     * Windows can stay blocked inside this call for a long time, and when
     * this happens it thus blocks vlc's video_output thread.
     * Vout EventThread will take care of the creation of the video
     * window (because PeekMessage has to be called from the same thread which
     * created the window). */
    msg_Dbg( obj, "creating Vout EventThread" );
    event_thread_t *p_event = malloc( sizeof(*p_event) );
    if( !p_event )
        return NULL;

    p_event->obj = obj;
    vlc_mutex_init( &p_event->lock );
    vlc_cond_init( &p_event->wait );
    atomic_init( &p_event->b_done, false );

    p_event->parent_window = parent_window;

    p_event->is_cursor_hidden = false;
    p_event->button_pressed = 0;
    p_event->hwnd = NULL;
    atomic_init(&p_event->size_changed, false);

    /* initialized to 0 to match the init in the display_win32_area_t */
    p_event->place.x = 0;
    p_event->place.y = 0;
    p_event->place.width = 0;
    p_event->place.height = 0;

    _snwprintf( p_event->class_main, ARRAYSIZE(p_event->class_main),
               TEXT("VLC video main %p"), (void *)p_event );
    _snwprintf( p_event->class_video, ARRAYSIZE(p_event->class_video),
               TEXT("VLC video output %p"), (void *)p_event );
    return p_event;
}

void EventThreadDestroy( event_thread_t *p_event )
{
    vlc_cond_destroy( &p_event->wait );
    vlc_mutex_destroy( &p_event->lock );
    free( p_event );
}

int EventThreadStart( event_thread_t *p_event, event_hwnd_t *p_hwnd, const event_cfg_t *p_cfg )
{
    p_event->use_desktop = p_cfg->use_desktop;
    p_event->is_projected = p_cfg->is_projected;
    p_event->window_area.left   = p_cfg->x;
    p_event->window_area.top    = p_cfg->y;
    p_event->window_area.right  = p_cfg->x + p_cfg->width;
    p_event->window_area.bottom = p_cfg->y + p_cfg->height;

    atomic_store(&p_event->size_changed, false);

    p_event->b_ready = false;
    atomic_store( &p_event->b_done, false);
    p_event->b_error = false;

    if( vlc_clone( &p_event->thread, EventThread, p_event,
                   VLC_THREAD_PRIORITY_LOW ) )
    {
        msg_Err( p_event->obj, "cannot create Vout EventThread" );
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
    msg_Dbg( p_event->obj, "Vout EventThread running" );

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

    atomic_store( &p_event->b_done, true );

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
    vout_window_ReportMousePressed(p_event->parent_window, button);
}

static void MouseReleased( event_thread_t *p_event, unsigned button )
{
    p_event->button_pressed &= ~(1 << button);
    if( !p_event->button_pressed )
        ReleaseCapture();
    vout_window_ReportMouseReleased(p_event->parent_window, button);
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
        hwnd = FindWindowEx( hwnd, NULL, TEXT("SHELLDLL_DefView"), NULL );
        if( hwnd ) hwnd = FindWindowEx( hwnd, NULL, TEXT("SysListView32"), NULL );
        if( hwnd )
        {
            *wnd = hwnd;
            return false;
        }
    }
    return true;
}

static HWND GetDesktopHandle(vlc_object_t *obj)
{
    /* Find Program Manager */
    HWND hwnd = FindWindow( TEXT("Progman"), NULL );
    if( hwnd ) hwnd = FindWindowEx( hwnd, NULL, TEXT("SHELLDLL_DefView"), NULL );
    if( hwnd ) hwnd = FindWindowEx( hwnd, NULL, TEXT("SysListView32"), NULL );
    if( hwnd )
        return hwnd;

    msg_Dbg( obj, "Couldn't find desktop icon window,. Trying the hard way." );

    EnumWindows( enumWindowsProc, (LPARAM)&hwnd );
    return hwnd;
}
#endif

static long FAR PASCAL VideoEventProc( HWND hwnd, UINT message,
                                       WPARAM wParam, LPARAM lParam )
{
    if( message == WM_CREATE )
    {
        /* Store p_event for future use */
        CREATESTRUCT *c = (CREATESTRUCT *)lParam;
        SetWindowLongPtr( hwnd, GWLP_USERDATA, (LONG_PTR)c->lpCreateParams );
        return 0;
    }

    LONG_PTR p_user_data = GetWindowLongPtr( hwnd, GWLP_USERDATA );
    if( p_user_data == 0 ) /* messages before WM_CREATE */
        return DefWindowProc(hwnd, message, wParam, lParam);
    event_thread_t *p_event = (event_thread_t *)p_user_data;

    switch( message )
    {
    case WM_CAPTURECHANGED:
        for( int button = 0; p_event->button_pressed; button++ )
        {
            unsigned m = 1 << button;
            if( p_event->button_pressed & m )
                vout_window_ReportMouseReleased(p_event->parent_window, button);
            p_event->button_pressed &= ~m;
        }
        p_event->button_pressed = 0;
        return 0;

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

static int CreateVideoWindow( event_thread_t *p_event )
{
    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASS   wc;                            /* window class components */

    /* Register the video sub-window class */
    wc.style         = CS_OWNDC|CS_DBLCLKS;          /* style: dbl click */
    wc.lpfnWndProc   = (WNDPROC)VideoEventProc;         /* event handler */
    wc.cbClsExtra    = 0;                         /* no extra class data */
    wc.cbWndExtra    = 0;                        /* no extra window data */
    wc.hInstance     = hInstance;                            /* instance */
    wc.hIcon         = 0;
    wc.hCursor       = p_event->is_cursor_hidden ? p_event->cursor_empty :
                                                   p_event->cursor_arrow;
    wc.hbrBackground = NULL;
    wc.lpszMenuName  = NULL;                                  /* no menu */
    wc.lpszClassName = p_event->class_video;
    if( !RegisterClass(&wc) )
    {
        msg_Err( p_event->obj, "CreateVideoWindow RegisterClass FAILED (err=%lu)", GetLastError() );
        return VLC_EGENERIC;
    }

    /* Create video sub-window. This sub window will always exactly match
     * the size of the video, which allows us to use crazy overlay colorkeys
     * without having them shown outside of the video area. */
    p_event->hvideownd =
        CreateWindow( p_event->class_video, TEXT(""),   /* window class */
            WS_CHILD,                   /* window style, not visible initially */
            p_event->place.x, p_event->place.y,
            p_event->place.width,          /* default width */
            p_event->place.height,        /* default height */
            p_event->hwnd,               /* parent window */
            NULL, hInstance,
            (LPVOID)p_event );    /* send vd to WM_CREATE */

    if( !p_event->hvideownd )
    {
        msg_Err( p_event->obj, "can't create video sub-window" );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_event->obj, "created video sub-window" );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Win32VoutCreateWindow: create a window for the video.
 *****************************************************************************
 * Before creating a direct draw surface, we need to create a window in which
 * the video will be displayed. This window will also allow us to capture the
 * events.
 *****************************************************************************/
static int Win32VoutCreateWindow( event_thread_t *p_event )
{
    HINSTANCE  hInstance;
    WNDCLASS   wc;                            /* window class components */
    int        i_style;

    msg_Dbg( p_event->obj, "Win32VoutCreateWindow" );

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
        p_event->hparent = GetDesktopHandle(p_event->obj);
    }
    #endif
    p_event->cursor_arrow = LoadCursor(NULL, IDC_ARROW);
    p_event->cursor_empty = EmptyCursor(hInstance);

    p_event->hide_timeout = var_InheritInteger( p_event->obj, "mouse-hide-timeout" );
    UpdateCursorMoved( p_event );

    /* Fill in the window class structure */
    wc.style         = CS_OWNDC|CS_DBLCLKS;          /* style: dbl click */
    wc.lpfnWndProc   = (WNDPROC)WinVoutEventProc;       /* event handler */
    wc.cbClsExtra    = 0;                         /* no extra class data */
    wc.cbWndExtra    = 0;                        /* no extra window data */
    wc.hInstance     = hInstance;                            /* instance */
    wc.hIcon         = 0;
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
        msg_Err( p_event->obj, "Win32VoutCreateWindow RegisterClass FAILED (err=%lu)", GetLastError() );
        return VLC_EGENERIC;
    }

    /* When you create a window you give the dimensions you wish it to
     * have. Unfortunatly these dimensions will include the borders and
     * titlebar. We use the following function to find out the size of
     * the window corresponding to the useable surface we want */
    RECT decorated_window = p_event->window_area;
    i_style = var_GetBool( p_event->obj, "video-deco" )
        /* Open with window decoration */
        ? WS_OVERLAPPEDWINDOW|WS_SIZEBOX
        /* No window decoration */
        : WS_POPUP;
    AdjustWindowRect( &decorated_window, i_style, 0 );
    i_style |= WS_VISIBLE|WS_CLIPCHILDREN;

    if( p_event->hparent )
    {
        i_style = WS_VISIBLE|WS_CLIPCHILDREN|WS_CHILD;

        /* allow user to regain control over input events if requested */
        bool b_mouse_support = var_InheritBool( p_event->obj, "mouse-events" );
        bool b_key_support = var_InheritBool( p_event->obj, "keyboard-events" );
        if( !b_mouse_support && !b_key_support )
            i_style |= WS_DISABLED;
    }

    p_event->i_window_style = i_style;

    /* Create the window */
    p_event->hwnd =
        CreateWindowEx( WS_EX_NOPARENTNOTIFY,
                    p_event->class_main,             /* name of window class */
                    TEXT(VOUT_TITLE) TEXT(" (VLC Video Output)"),/* window title */
                    i_style,                                 /* window style */
                    (!p_event->window_area.left) ? CW_USEDEFAULT :
                        p_event->window_area.left,   /* default X coordinate */
                    (!p_event->window_area.top) ? CW_USEDEFAULT :
                        p_event->window_area.top,    /* default Y coordinate */
                    RECTWidth(decorated_window),             /* window width */
                    RECTHeight(decorated_window),           /* window height */
                    p_event->hparent,                       /* parent window */
                    NULL,                          /* no menu in this window */
                    hInstance,            /* handle of this program instance */
                    (LPVOID)p_event );           /* send vd to WM_CREATE */

    if( !p_event->hwnd )
    {
        msg_Warn( p_event->obj, "Win32VoutCreateWindow create window FAILED (err=%lu)", GetLastError() );
        return VLC_EGENERIC;
    }

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
                            TEXT(VOUT_TITLE) TEXT(" (VLC Fullscreen Video Output)"),
                            WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN|WS_SIZEBOX,
                            CW_USEDEFAULT, CW_USEDEFAULT,
                            CW_USEDEFAULT, CW_USEDEFAULT,
                            NULL, NULL, hInstance, NULL );
    }
    else
    {
        p_event->hfswnd = NULL;
    }

    int err = CreateVideoWindow( p_event );
    if ( err != VLC_SUCCESS )
        return err;

    InitGestures( p_event->hwnd, &p_event->p_gesture, p_event->is_projected );

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
    msg_Dbg( p_event->obj, "Win32VoutCloseWindow" );

    DestroyWindow( p_event->hvideownd );
    DestroyWindow( p_event->hwnd );
    if( p_event->hfswnd )
        DestroyWindow( p_event->hfswnd );

    p_event->hwnd = NULL;

    HINSTANCE hInstance = GetModuleHandle(NULL);
    UnregisterClass( p_event->class_video, hInstance );
    UnregisterClass( p_event->class_main, hInstance );

    DestroyCursor( p_event->cursor_empty );

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
    if( message == WM_CREATE )
    {
        /* Store p_event for future use */
        CREATESTRUCT *c = (CREATESTRUCT *)lParam;
        SetWindowLongPtr( hwnd, GWLP_USERDATA, (LONG_PTR)c->lpCreateParams );
        return 0;
    }

    LONG_PTR p_user_data = GetWindowLongPtr( hwnd, GWLP_USERDATA );
    if( p_user_data == 0 ) /* messages before WM_CREATE */
        return DefWindowProc(hwnd, message, wParam, lParam);
    event_thread_t *p_event = (event_thread_t *)p_user_data;

    switch( message )
    {

    case WM_SIZE:
        if (!p_event->hparent)
            atomic_store(&p_event->size_changed, true);
        return 0;

    case WM_CAPTURECHANGED:
        for( int button = 0; p_event->button_pressed; button++ )
        {
            unsigned m = 1 << button;
            if( p_event->button_pressed & m )
                vout_window_ReportMouseReleased(p_event->parent_window, button);
            p_event->button_pressed &= ~m;
        }
        p_event->button_pressed = 0;
        return 0;

    /* the user wants to close the window */
    case WM_CLOSE:
        vout_window_ReportClose(p_event->parent_window);
        return 0;

    /* the window has been closed so shut down everything now */
    case WM_DESTROY:
        msg_Dbg( p_event->obj, "WinProc WM_DESTROY" );
        /* just destroy the window */
        PostQuitMessage( 0 );
        return 0;

    case WM_VLC_SET_TOP_STATE:
        SetAbove( p_event, wParam != 0);
        return 0;

    case WM_KILLFOCUS:
        return 0;

    case WM_SETFOCUS:
        return 0;

    case WM_GESTURE:
        return DecodeGesture( p_event->obj, p_event->p_gesture, hwnd, message, wParam, lParam );

    default:
        break;
    }

    /* Let windows handle the message */
    return DefWindowProc(hwnd, message, wParam, lParam);
}

void EventThreadSetAbove( event_thread_t *p_event, bool is_on_top )
{
    PostMessage( p_event->hwnd, WM_VLC_SET_TOP_STATE, is_on_top != 0, 0);
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

