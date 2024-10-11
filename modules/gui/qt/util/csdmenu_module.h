#ifndef QTCSDMENU_MODULE_H
#define QTCSDMENU_MODULE_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_objects.h>

#include <stdint.h>

struct xcb_connection_t;

struct wl_display;
struct wl_seat;
struct xdg_toplevel;

typedef enum {
    QT_CSD_PLATFORM_UNKNOWN,
    QT_CSD_PLATFORM_WAYLAND,
    QT_CSD_PLATFORM_X11,
    QT_CSD_PLATFORM_WINDOWS
} qt_csd_menu_platform;

typedef enum {
    QT_CSD_WINDOW_FULLSCREEN = (1 << 0),
    QT_CSD_WINDOW_MAXIMIZED  = (1 << 1),
    QT_CSD_WINDOW_MINIMIZED  = (1 << 2),
    QT_CSD_WINDOW_FIXED_SIZE = (1 << 3)
} qt_csd_menu_window_state;


typedef void (*notifyMenuVisibleCallback)(void* userData, bool visible);

//information about the window, plateform dependent
typedef struct {
    qt_csd_menu_platform platform;
    union {
        struct {
            struct xcb_connection_t* connection;
            uint32_t rootwindow;
        } x11;
        struct {
            struct wl_display* display;
            struct xdg_toplevel* toplevel;
        } wayland;
        struct {
            void* hwnd;
        } windows;
    } data;

    //is the UI rtl or ltr
    bool isRtl;

    //callback called to notify that the menu is visible/hidden
    notifyMenuVisibleCallback notifyMenuVisible;
    //user data passed to callbacks
    void* userData;
} qt_csd_menu_info;

typedef struct {
    qt_csd_menu_platform platform;
    union {
        struct {
            struct wl_seat* seat;
            uint32_t serial;
        } wayland;
        struct {
            uint32_t window;
        } x11;
        struct {
            void* hwnd;
        } win32;
    } data;
    int x;
    int y;
    qt_csd_menu_window_state windowState;
} qt_csd_menu_event;

typedef struct qt_csd_menu_t {
    struct vlc_object_t obj;
    void* p_sys;

    bool (*popup)(struct qt_csd_menu_t* menu, qt_csd_menu_event* event);
} qt_csd_menu_t;

typedef int (*qt_csd_menu_open)(qt_csd_menu_t* obj, qt_csd_menu_info* info);

#endif // QTCSDMENU_MODULE_H
