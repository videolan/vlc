/*****************************************************************************
 * Copyright © 2010 Rémi Denis-Courmont
 * Copyright © 2024 Prince Gupta <prince@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/


#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_services_discovery.h>
#include <vlc_threads.h>

#include <windows.h>
#include <dbt.h>


// max drives allowed because of letter assignment (A-Z)
#define MAX_DRIVES 26

// name and window title used for observer window
static const char WindowClassName[] = "VLCWinVolumesDeviceObserver";

typedef struct volumes_observer_t
{
    // observer thread
    vlc_thread_t thread;

    // signal to end the observer thread
    HANDLE quit_signal;

    input_item_t *items[MAX_DRIVES];
} volumes_observer_t;


/**
 * @brief CreateInput
 * @param letter - drive letter assosited with drive
 * @return input item for the drive
 */
static input_item_t *CreateInput( const char letter )
{
    char mrl[12] = "file:///A:/", name[MAX_PATH] = {0};
    char path[4] = "A:\\";

    path[0] = letter;
    mrl[8] = letter;

    // ignore the error, this information is not important
    GetVolumeInformationA(path, name, sizeof(name),
                          NULL, NULL, NULL, NULL, 0);

    if (GetDriveTypeA (path) == DRIVE_CDROM)
        return input_item_NewDisc( mrl, name, INPUT_DURATION_INDEFINITE );
    else
        return input_item_NewDirectory( mrl, name, ITEM_LOCAL );
}


/**
 * @brief RegisterWindowClass registers window class for window creation
 * @param class_name
 * @param proc - window procedure associated with window class
 * @return ATOM (unique identifier) for the class on success otherwise 0
 */
static ATOM RegisterWindowClass( const char *class_name, WNDPROC proc )
{
    WNDCLASSEXA wcex =
    {
        .cbSize = sizeof(WNDCLASSEXA),
        .lpfnWndProc = proc,
        .hInstance = (HINSTANCE)GetModuleHandle(NULL),
        .hbrBackground = (HBRUSH)(COLOR_WINDOW),
        .lpszClassName = class_name
    };

    return RegisterClassExA(&wcex);
}


/**
 * @brief StartWindow creates a hidden window, can be used to listen on OS events
 * @param class_name name and title for window
 * @param ctx will be passed as lpParam, can be used with CREATESTRUCT (see CreateWindowA docs)
 * @return HANDLE to the created window on success otherwise NULL
 */
static HWND StartWindow(const char *class_name, void *ctx)
{
    HWND window = CreateWindowA(class_name, class_name, WS_ICONIC,
                                0, 0, CW_USEDEFAULT, 0, NULL, NULL,
                                (HINSTANCE)GetModuleHandle(NULL), ctx);

    if (window == NULL)
        return NULL;

    ShowWindow(window, SW_HIDE);
    return window;
}


static int Add( services_discovery_t *p_sd, const uint32_t disk_mask )
{
    volumes_observer_t *p_sys = p_sd->p_sys;

    for (int i = 0; i < MAX_DRIVES; ++i)
    {
        const uint32_t mask = 1 << i;
        if ( !(disk_mask & mask) )
            continue;

        input_item_t *item = CreateInput( 'A' + i );
        if (unlikely( !item )) return VLC_ENOMEM;

        if ( p_sys->items[i] )
        {
            services_discovery_RemoveItem( p_sd, p_sys->items[i] );
            input_item_Release( p_sys->items[i] );
        }

        p_sys->items[i] = item;

        // notify changes
        services_discovery_AddItem( p_sd, item );
    }

    return VLC_SUCCESS;
}


static void Remove(services_discovery_t *p_sd, const uint32_t disk_mask)
{
    volumes_observer_t *p_sys = p_sd->p_sys;

    for (int i = 0; i < MAX_DRIVES; ++i)
    {
        const uint32_t mask = (1 << i);
        if ( !(disk_mask & mask) )
            continue;

        if ( p_sys->items[i] )
        {
            services_discovery_RemoveItem( p_sd, p_sys->items[i] );
            input_item_Release( p_sys->items[i] );
            p_sys->items[i] = NULL;
        }
    }
}


static int AddExistingDrives( services_discovery_t *p_sd )
{
    // find existing drives
    const DWORD drives = GetLogicalDrives();
    return Add( p_sd, drives );
}


static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        // store passed lParam as user data to use as context for handling other events
        CREATESTRUCT *c = (CREATESTRUCT *)lParam;
        SetWindowLongPtr( hWnd, GWLP_USERDATA, (LONG_PTR)c->lpCreateParams );
        return 0;
    }

    case WM_DEVICECHANGE:
    {
        if ((wParam != DBT_DEVICEARRIVAL) && (wParam != DBT_DEVICEREMOVECOMPLETE))
            break;

        DEV_BROADCAST_HDR *header = (DEV_BROADCAST_HDR *) lParam;
        if (header->dbch_devicetype != DBT_DEVTYP_VOLUME)
            break;

        // get associated user data for the context
        LONG_PTR user_data = GetWindowLongPtr( hWnd, GWLP_USERDATA );
        if ( user_data == 0 )
            break;

        services_discovery_t *p_sd = (services_discovery_t *)user_data;
        DEV_BROADCAST_VOLUME *node = (DEV_BROADCAST_VOLUME *)lParam;

        switch (wParam)
        {
        case DBT_DEVICEARRIVAL:
        {
            Add( p_sd, node->dbcv_unitmask );
            break;
        }
        case DBT_DEVICEREMOVECOMPLETE:
        {
            Remove( p_sd, node->dbcv_unitmask );
            break;
        }
        default:
            break;
        }

        break;
    }

    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    }

    default:
        break;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}


static void *Run(void *ctx)
{
    services_discovery_t *p_sd = ctx;
    volumes_observer_t *p_sys = p_sd->p_sys;

    // register window class
    ATOM registered_class = RegisterWindowClass( WindowClassName, WndProc );
    DWORD error = GetLastError();
    if ((registered_class == 0)
            && (error != ERROR_CLASS_ALREADY_EXISTS))
    {
        msg_Err( p_sd, "failed to register window class %lu", error );
        return NULL;
    }

    HWND window = StartWindow( WindowClassName, ctx );
    if ( !window )
    {
        msg_Err( p_sd, "failed to start window %lu", GetLastError() );
        return NULL;
    }

    // window message loop
    for(;;)
    {
        // Wait for either a message or the quit event
        DWORD wait_result = MsgWaitForMultipleObjects(1, &p_sys->quit_signal,
                                                      FALSE, INFINITE, QS_ALLINPUT);

        if (wait_result == WAIT_OBJECT_0)
        {
            // The close event is signaled, break out of the loop
            break;
        }
        else if (wait_result == WAIT_OBJECT_0 + 1)
        {
            MSG msg;

            // There are messages in the queue, process them
            while (PeekMessage(&msg, window, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT)
                {
                    break;
                }
                else
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }
        }
        else if (wait_result == WAIT_FAILED)
        {
            break;
        }
    }

    DestroyWindow( window );
    return NULL;
}


static void ReleaseItems( volumes_observer_t *p_sys )
{
    for (int i = 0; i < MAX_DRIVES; ++i)
    {
        if (p_sys->items[i])
            input_item_Release( p_sys->items[i] );
    }
}


// Module functions -
static int Open (vlc_object_t *obj)
{
    services_discovery_t *p_sd = (services_discovery_t *)obj;

    volumes_observer_t *p_sys = (volumes_observer_t *)calloc( 1, sizeof(volumes_observer_t) );
    if ( !p_sys )
        return VLC_ENOMEM;

    p_sd->p_sys = p_sys;
    p_sd->description = _("Lists volumes on host computer");

    if ( AddExistingDrives( p_sd ) != VLC_SUCCESS)
    {
        msg_Err( p_sd, "failed to find existing drives" );
        goto error;
    }

    p_sys->quit_signal = CreateEvent(NULL, TRUE, FALSE, NULL);
    if ( p_sys->quit_signal == NULL )
    {
        msg_Err( p_sd, "failed to create event, error: %lu", GetLastError());
        goto error;
    }

    // start the thread for window handling
    if ( vlc_clone( &p_sys->thread, Run, p_sd ) )
    {
        msg_Err( p_sd, "failed to start observer thread");
        goto error;
    }

    return VLC_SUCCESS;

error:
    if ( p_sys->quit_signal )
        CloseHandle( p_sys->quit_signal );

    ReleaseItems( p_sys );
    free( p_sys );
    return VLC_EGENERIC;
}


static void Close( vlc_object_t * obj )
{
    services_discovery_t *p_sd = (services_discovery_t *)obj;
    volumes_observer_t *p_sys = (volumes_observer_t *) p_sd->p_sys;

    // signal window thread to quit
    SetEvent( p_sys->quit_signal );

    vlc_join( p_sys->thread, NULL );

    // release resources
    CloseHandle( p_sys->quit_signal );
    ReleaseItems( p_sys );
    free( p_sys );
}


VLC_SD_PROBE_HELPER("volume", N_("Volumes"), SD_CAT_MYCOMPUTER)

/*
 * Module descriptor
 *
 * list volumes on Windows
 */
vlc_module_begin ()
    set_shortname (N_("Volumes"))
    set_description (N_("Lists volumes on host computer"))
    set_capability ("services_discovery", 0)
    set_callbacks(Open, Close)
    add_shortcut ("volume")

    VLC_SD_PROBE_SUBMODULE
vlc_module_end ()
