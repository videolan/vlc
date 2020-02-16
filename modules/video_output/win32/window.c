/**
 * @file window.c
 * @brief Win32 non-embedded video window provider
 */
/*****************************************************************************
 * Copyright © 2007-2010 VLC authors and VideoLAN
 *             2016-2019 VideoLabs, VLC authors and VideoLAN
 *
 * Authors: Filippo Carone <filippo@carone.org>
 *          Pierre d'Herbemont <pdherbemont # videolan.org>
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdatomic.h>
#include <stdarg.h>

#include <vlc_common.h>
#include <vlc_charset.h>
#include <vlc_plugin.h>
#include <vlc_vout_window.h>
#include <vlc_mouse.h>
#include <vlc_actions.h>

#include <shellapi.h>                                         /* ExtractIcon */

#define RECTWidth(r)   (LONG)((r).right - (r).left)
#define RECTHeight(r)  (LONG)((r).bottom - (r).top)

#define WM_VLC_CHANGE_TEXT   (WM_APP + 1)
#define WM_VLC_SET_TOP_STATE (WM_APP + 2)

#define IDM_TOGGLE_ON_TOP  (WM_USER + 1)

typedef struct vout_window_sys_t
{
    vlc_thread_t thread;
    vlc_sem_t    ready;

    HWND hwnd;

    WCHAR class_main[256];
    HICON vlc_icon;

    /* mouse */
    unsigned button_pressed;

    /* cursor */
    bool       is_cursor_hidden;
    HCURSOR    cursor_arrow;
    HCURSOR    cursor_empty;
    int64_t    hide_timeout; /* mouse hiding timeout in ms */
    vlc_tick_t last_moved;

    /* state before fullscreen */
    WINDOWPLACEMENT window_placement;
    LONG            i_window_style;

    /* Title */
    wchar_t *_Atomic pwz_title;
} vout_window_sys_t;


static void Resize(vout_window_t *wnd, unsigned width, unsigned height)
{
    vout_window_sys_t *sys = wnd->sys;

    /* When you create a window you give the dimensions you wish it to
     * have. Unfortunatly these dimensions will include the borders and
     * titlebar. We use the following function to find out the size of
     * the window corresponding to the useable surface we want */
    RECT decorated_window = {
        .right = width,
        .bottom = height,
    };
    LONG i_window_style = GetWindowLong(sys->hwnd, GWL_STYLE);
    AdjustWindowRect( &decorated_window, i_window_style, 0 );
    SetWindowPos(sys->hwnd, 0, 0, 0,
                 RECTWidth(decorated_window), RECTHeight(decorated_window),
                 SWP_NOZORDER|SWP_NOMOVE);
}

static int Enable(vout_window_t *wnd, const vout_window_cfg_t *cfg)
{
    vout_window_sys_t *sys = wnd->sys;

    LONG i_window_style;
    if (cfg->is_decorated)
        i_window_style = WS_OVERLAPPEDWINDOW | WS_SIZEBOX;
    else
        i_window_style = WS_POPUP;
    i_window_style |= WS_CLIPCHILDREN;

    /* allow user to regain control over input events if requested */
    bool b_mouse_support = var_InheritBool( wnd, "mouse-events" );
    bool b_key_support = var_InheritBool( wnd, "keyboard-events" );
    if( !b_mouse_support && !b_key_support )
        i_window_style |= WS_DISABLED;
    SetWindowLong(sys->hwnd, GWL_STYLE, i_window_style);

    if (cfg->x || cfg->y)
        MoveWindow(sys->hwnd, cfg->x, cfg->y, cfg->width, cfg->height, TRUE);

    Resize(wnd, cfg->width, cfg->height);

    ShowWindow( sys->hwnd, SW_SHOW );
    return VLC_SUCCESS;
}

static void Disable(struct vout_window_t *wnd)
{
    vout_window_sys_t *sys = wnd->sys;
    ShowWindow( sys->hwnd, SW_HIDE );
}

static void SetState(vout_window_t *wnd, unsigned state)
{
    vout_window_sys_t *sys = wnd->sys;
    PostMessage( sys->hwnd, WM_VLC_SET_TOP_STATE, state, 0);
}

static void SetTitle(vout_window_t *wnd, const char *title)
{
    vout_window_sys_t *sys = wnd->sys;
    char *psz_title = var_InheritString( wnd, "video-title" );
    wchar_t *pwz_title = ToWide(psz_title ? psz_title : title);

    free(psz_title);

    if (unlikely(pwz_title == NULL))
        return;

    free(atomic_exchange(&sys->pwz_title, pwz_title));
    PostMessage( sys->hwnd, WM_VLC_CHANGE_TEXT, 0, 0 );
}

static void SetFullscreen(vout_window_t *wnd, const char *id)
{
    VLC_UNUSED(id);
    msg_Dbg(wnd, "entering fullscreen mode");
    vout_window_sys_t *sys = wnd->sys;

    sys->window_placement.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(sys->hwnd, &sys->window_placement);

    sys->i_window_style = GetWindowLong(sys->hwnd, GWL_STYLE);

    /* Change window style, no borders and no title bar */
    SetWindowLong(sys->hwnd, GWL_STYLE, WS_CLIPCHILDREN | WS_VISIBLE);

    /* Retrieve current window position so fullscreen will happen
     * on the right screen */
    HMONITOR hmon = MonitorFromWindow(sys->hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    mi.cbSize = sizeof(MONITORINFO);
    if (GetMonitorInfo(hmon, &mi))
        SetWindowPos(sys->hwnd, 0,
                     mi.rcMonitor.left,
                     mi.rcMonitor.top,
                     RECTWidth(mi.rcMonitor),
                     RECTHeight(mi.rcMonitor),
                     SWP_NOZORDER|SWP_FRAMECHANGED);
}

static void UnsetFullscreen(vout_window_t *wnd)
{
    msg_Dbg(wnd, "leaving fullscreen mode");
    vout_window_sys_t *sys = wnd->sys;

    /* return to normal window for non embedded vout */
    if (sys->window_placement.length)
    {
        SetWindowLong(sys->hwnd, GWL_STYLE, sys->i_window_style);
        SetWindowPlacement(sys->hwnd, &sys->window_placement);
        sys->window_placement.length = 0;
    }
    ShowWindow(sys->hwnd, SW_SHOWNORMAL);
}

static void SetAbove( vout_window_t *wnd, enum vout_window_state state )
{
    vout_window_sys_t *sys = wnd->sys;
    switch (state) {
    case VOUT_WINDOW_STATE_NORMAL:
        if ((GetWindowLong(sys->hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST))
        {
            HMENU hMenu = GetSystemMenu(sys->hwnd, FALSE);
            CheckMenuItem(hMenu, IDM_TOGGLE_ON_TOP, MF_BYCOMMAND | MFS_UNCHECKED);
            SetWindowPos(sys->hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE|SWP_NOMOVE);
        }
        break;
    case VOUT_WINDOW_STATE_ABOVE:
        if (!(GetWindowLong(sys->hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST))
        {
            HMENU hMenu = GetSystemMenu(sys->hwnd, FALSE);
            CheckMenuItem(hMenu, IDM_TOGGLE_ON_TOP, MF_BYCOMMAND | MFS_CHECKED);
            SetWindowPos(sys->hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
        }
        break;
    case VOUT_WINDOW_STATE_BELOW:
        SetWindowPos(sys->hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
        break;

    }
}

static void MousePressed( vout_window_t *wnd, HWND hwnd, unsigned button )
{
    vout_window_sys_t *sys = wnd->sys;
    if( !sys->button_pressed )
        SetCapture( hwnd );
    sys->button_pressed |= 1 << button;
    vout_window_ReportMousePressed(wnd, button);
}

static void MouseReleased( vout_window_t *wnd, unsigned button )
{
    vout_window_sys_t *sys = wnd->sys;
    sys->button_pressed &= ~(1 << button);
    if( !sys->button_pressed )
        ReleaseCapture();
    vout_window_ReportMouseReleased(wnd, button);
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
        /* Store wnd for future use */
        CREATESTRUCT *c = (CREATESTRUCT *)lParam;
        SetWindowLongPtr( hwnd, GWLP_USERDATA, (LONG_PTR)c->lpCreateParams );
        return 0;
    }

    LONG_PTR p_user_data = GetWindowLongPtr( hwnd, GWLP_USERDATA );
    if( p_user_data == 0 )
        return DefWindowProc(hwnd, message, wParam, lParam);
    vout_window_t *wnd = (vout_window_t *)p_user_data;

    switch( message )
    {
    case WM_CLOSE:
        vout_window_ReportClose(wnd);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_SIZE:
        vout_window_ReportSize(wnd, LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_MOUSEMOVE:
        vout_window_ReportMouseMoved(wnd, LOWORD(lParam), HIWORD(lParam));
        break;
    case WM_NCMOUSEMOVE:
        break;

    case WM_CAPTURECHANGED:
    {
        vout_window_sys_t *sys = wnd->sys;
        for( int button = 0; sys->button_pressed; button++ )
        {
            unsigned m = 1 << button;
            if( sys->button_pressed & m )
                vout_window_ReportMouseReleased(wnd, button);
            sys->button_pressed &= ~m;
        }
        sys->button_pressed = 0;
        break;
    }

    case WM_LBUTTONDOWN:
        MousePressed( wnd, hwnd, MOUSE_BUTTON_LEFT );
        return 0;
    case WM_LBUTTONUP:
        MouseReleased( wnd, MOUSE_BUTTON_LEFT );
        return 0;
    case WM_LBUTTONDBLCLK:
        vout_window_ReportMouseDoubleClick(wnd, MOUSE_BUTTON_LEFT);
        return 0;

    case WM_MBUTTONDOWN:
        MousePressed( wnd, hwnd, MOUSE_BUTTON_CENTER );
        return 0;
    case WM_MBUTTONUP:
        MouseReleased( wnd, MOUSE_BUTTON_CENTER );
        return 0;
    case WM_MBUTTONDBLCLK:
        vout_window_ReportMouseDoubleClick(wnd, MOUSE_BUTTON_CENTER);
        return 0;

    case WM_RBUTTONDOWN:
        MousePressed( wnd, hwnd, MOUSE_BUTTON_RIGHT );
        return 0;
    case WM_RBUTTONUP:
        MouseReleased( wnd, MOUSE_BUTTON_RIGHT );
        return 0;
    case WM_RBUTTONDBLCLK:
        vout_window_ReportMouseDoubleClick(wnd, MOUSE_BUTTON_RIGHT);
        return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    {
        /* The key events are first processed here and not translated
         * into WM_CHAR events because we need to know the status of the
         * modifier keys. */
        int i_key = Win32VoutConvertKey( wParam );
        if( !i_key )
        {
            /* This appears to be a "normal" (ascii) key */
            i_key = tolower( (unsigned char)MapVirtualKey( wParam, 2 ) );
        }

        if( i_key )
        {
            if( GetKeyState(VK_CONTROL) & KF_UP )
            {
                i_key |= KEY_MODIFIER_CTRL;
            }
            if( GetKeyState(VK_SHIFT) & KF_UP )
            {
                i_key |= KEY_MODIFIER_SHIFT;
            }
            if( GetKeyState(VK_MENU) & KF_UP )
            {
                i_key |= KEY_MODIFIER_ALT;
            }

            vout_window_ReportKeyPress(wnd, i_key);
        }
        break;
    }

    case WM_SYSCOMMAND:
        switch (wParam)
        {
        case IDM_TOGGLE_ON_TOP:            /* toggle the "on top" status */
        {
            HMENU hMenu = GetSystemMenu(hwnd, FALSE);
            const bool is_on_top = (GetMenuState(hMenu, IDM_TOGGLE_ON_TOP, MF_BYCOMMAND) & MF_CHECKED) == 0;
            SetAbove( wnd, is_on_top ? VOUT_WINDOW_STATE_ABOVE : VOUT_WINDOW_STATE_NORMAL );
            return 0;
        }
        default:
            break;
        }
        break;

    case WM_VLC_SET_TOP_STATE:
        SetAbove( wnd, (enum vout_window_state) wParam);
        return 0;

    case WM_VLC_CHANGE_TEXT:
        {
            vout_window_sys_t *sys = wnd->sys;
            wchar_t *pwz_title = atomic_exchange(&sys->pwz_title, NULL);

            if( pwz_title )
            {
                SetWindowTextW( hwnd, pwz_title );
                free( pwz_title );
            }
            break;
        }
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

static void Close(vout_window_t *wnd)
{
    vout_window_sys_t *sys = wnd->sys;

    if (sys->hwnd)
        PostMessage( sys->hwnd, WM_CLOSE, 0, 0 );
    vlc_join(sys->thread, NULL);
    free(atomic_load(&sys->pwz_title));

    HINSTANCE hInstance = GetModuleHandle(NULL);
    UnregisterClass( sys->class_main, hInstance );
    DestroyCursor( sys->cursor_empty );
    if( sys->vlc_icon )
        DestroyIcon( sys->vlc_icon );
    wnd->sys = NULL;
}

static int CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam)
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

static void UpdateCursor( vout_window_t *wnd, bool b_show )
{
    vout_window_sys_t *sys = wnd->sys;
    if( sys->is_cursor_hidden == !b_show )
        return;
    sys->is_cursor_hidden = !b_show;

    HCURSOR cursor = b_show ? sys->cursor_arrow : sys->cursor_empty;
    SetClassLongPtr( sys->hwnd, GCLP_HCURSOR, (LONG_PTR)cursor );

    /* FIXME I failed to find a cleaner way to force a redraw of the cursor */
    SetCursor( cursor );
}

static void CALLBACK HideMouse(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
    VLC_UNUSED(uMsg); VLC_UNUSED(dwTime);
    if (hwnd)
    {
        vout_window_t *wnd = (vout_window_t *)idEvent;
        UpdateCursor( wnd, false );
    }
}

static void UpdateCursorMoved( vout_window_t *wnd )
{
    vout_window_sys_t *sys = wnd->sys;
    UpdateCursor( wnd, true );
    sys->last_moved = vlc_tick_now();
    SetTimer( sys->hwnd, (UINT_PTR)wnd, sys->hide_timeout, HideMouse );
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

static inline bool isMouseEvent( WPARAM type )
{
    return type >= WM_MOUSEFIRST &&
           type <= WM_MOUSELAST;
}

static void *EventThread( void *p_this )
{
    vout_window_t *wnd = (vout_window_t *)p_this;
    vout_window_sys_t *sys = wnd->sys;

    int canc = vlc_savecancel ();

    HINSTANCE hInstance = GetModuleHandle(NULL);

    LONG i_window_style;
    HWND hwParent;
    if (var_InheritBool( wnd, "video-wallpaper" ))
    {
        hwParent = GetDesktopHandle(p_this);
        i_window_style = WS_CLIPCHILDREN|WS_CHILD;
    }
    else
    {
        hwParent = 0;
        i_window_style = WS_OVERLAPPEDWINDOW | WS_SIZEBOX | WS_CLIPCHILDREN;
    }

    /* allow user to regain control over input events if requested */
    bool b_mouse_support = var_InheritBool( wnd, "mouse-events" );
    bool b_key_support = var_InheritBool( wnd, "keyboard-events" );
    if( !b_mouse_support && !b_key_support )
        i_window_style |= WS_DISABLED;

    /* Create the window */
    sys->hwnd =
        CreateWindowEx( WS_EX_NOPARENTNOTIFY,
                    sys->class_main,                 /* name of window class */
                    TEXT(VOUT_TITLE) TEXT(" (VLC Video Output)"),/* window title */
                    i_window_style,                          /* window style */
                    CW_USEDEFAULT,                   /* default X coordinate */
                    CW_USEDEFAULT,                   /* default Y coordinate */
                    CW_USEDEFAULT,                           /* window width */
                    CW_USEDEFAULT,                          /* window height */
                    hwParent,                               /* parent window */
                    NULL,                          /* no menu in this window */
                    hInstance,            /* handle of this program instance */
                    wnd );                           /* send vd to WM_CREATE */

    vlc_sem_post( &sys->ready );

    if (sys->hwnd == NULL)
    {
        vlc_restorecancel( canc );
        return NULL;
    }

    /* Append a "Always On Top" entry in the system menu */
    HMENU hMenu = GetSystemMenu( sys->hwnd, FALSE );
    AppendMenu( hMenu, MF_SEPARATOR, 0, TEXT("") );
    AppendMenu( hMenu, MF_STRING | MF_UNCHECKED,
                       IDM_TOGGLE_ON_TOP, TEXT("Always on &Top") );

    POINT mouse_pos = {0}, old_mouse_pos = {0};
    for( ;; )
    {
        MSG msg;
        if( !GetMessage( &msg, 0, 0, 0 ) )
        {
            break;
        }

        /* cursor handling */
        if( msg.message == WM_MOUSEMOVE || msg.message == WM_NCMOUSEMOVE )
        {
            GetCursorPos( &mouse_pos );
            /* FIXME, why this >2 limits ? */
            if( (abs(mouse_pos.x - old_mouse_pos.x) > 2 ||
                (abs(mouse_pos.y - old_mouse_pos.y)) > 2 ) )
            {
                old_mouse_pos = mouse_pos;
                UpdateCursorMoved( wnd );
            }
        }
        else if( isMouseEvent( msg.message ) )
        {
            UpdateCursorMoved( wnd );
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    vlc_restorecancel(canc);
    return NULL;
}

static const struct vout_window_operations ops = {
    .enable  = Enable,
    .disable = Disable,
    .resize = Resize,
    .set_title = SetTitle,
    .set_state = SetState,
    .set_fullscreen = SetFullscreen,
    .unset_fullscreen = UnsetFullscreen,
    .destroy = Close,
};

static int Open(vout_window_t *wnd)
{
    vout_window_sys_t *sys = vlc_obj_calloc(VLC_OBJECT(wnd), 1, sizeof(vout_window_sys_t));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    _snwprintf( sys->class_main, ARRAY_SIZE(sys->class_main),
               TEXT("VLC standalone window %p"), (void *)sys );

    HINSTANCE hInstance = GetModuleHandle(NULL);

    WCHAR app_path[MAX_PATH];
    if( GetModuleFileName( NULL, app_path, MAX_PATH ) )
        sys->vlc_icon = ExtractIcon( hInstance, app_path    , 0 );

    sys->button_pressed = 0;
    sys->is_cursor_hidden = false;
    sys->hide_timeout = var_InheritInteger( wnd, "mouse-hide-timeout" );
    sys->cursor_arrow = LoadCursor(NULL, IDC_ARROW);
    sys->cursor_empty = EmptyCursor(hInstance);

    WNDCLASS wc = { 0 };
    /* Fill in the window class structure */
    wc.style         = CS_OWNDC|CS_DBLCLKS;           /* style: dbl click */
    wc.lpfnWndProc   = (WNDPROC)WinVoutEventProc;        /* event handler */
    wc.hInstance     = hInstance;                             /* instance */
    wc.hIcon         = sys->vlc_icon;            /* load the vlc big icon */
    wc.lpszClassName = sys->class_main;            /* use a special class */
    wc.hCursor       = sys->cursor_arrow;

    /* Register the window class */
    if( !RegisterClass(&wc) )
    {
        if( sys->vlc_icon )
            DestroyIcon( sys->vlc_icon );

        msg_Err( wnd, "RegisterClass FAILED (err=%lu)", GetLastError() );
        return VLC_EGENERIC;
    }
    vlc_sem_init( &sys->ready, 0 );

    wnd->sys = sys;
    if( vlc_clone( &sys->thread, EventThread, wnd, VLC_THREAD_PRIORITY_LOW ) )
    {
        Close(wnd);
        return VLC_EGENERIC;
    }

    vlc_sem_wait( &sys->ready );

    if (sys->hwnd == NULL)
    {
        Close(wnd);
        return VLC_EGENERIC;
    }

    wnd->sys = sys;
    wnd->type = VOUT_WINDOW_TYPE_HWND;
    wnd->handle.hwnd = sys->hwnd;
    wnd->ops = &ops;
    wnd->info.has_double_click = true;
    return VLC_SUCCESS;
}

vlc_module_begin()
    set_shortname(N_("Win32 window"))
    set_description(N_("Win32 window"))
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_capability("vout window", 10)
    set_callback(Open)
vlc_module_end()
