/*****************************************************************************
 * ntservice.c: Windows NT/2K/XP service interface
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>
#include <vlc/vlc.h>
#include <vlc/intf.h>

#define VLCSERVICENAME "VLC media player"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Activate( vlc_object_t * );
static void Close   ( vlc_object_t * );

#define INSTALL_TEXT N_( "Install Windows Service" )
#define INSTALL_LONGTEXT N_( \
    "If enabled the interface will install the Service and exit." )
#define UNINSTALL_TEXT N_( "Uninstall Windows Service" )
#define UNINSTALL_LONGTEXT N_( \
    "If enabled the interface will uninstall the Service and exit." )
#define NAME_TEXT N_( "Display name of the Service" )
#define NAME_LONGTEXT N_( \
    "This allows you to change the display name of the Service." )
#define OPTIONS_TEXT N_("Configuration options")
#define OPTIONS_LONGTEXT N_( \
    "This option allows you to specify configuration options that will be " \
    "used by the Service (eg. --foo=bar --no-foobar). It should be specified "\
    "at install time so the Service is properly configured.")
#define EXTRAINTF_TEXT N_("Extra interface modules")
#define EXTRAINTF_LONGTEXT N_( \
    "This option allows you to select additional interfaces spawned by the " \
    "Service. It should be specified at install time so the Service is " \
    "properly configured. Use a comma separated list of interface modules. " \
    "(common values are: logger, sap, rc, http)")

vlc_module_begin();
    set_shortname( _("NT Service"));
    set_description( _("Windows Service interface") );
    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_CONTROL );
    add_bool( "ntservice-install", 0, NULL,
              INSTALL_TEXT, INSTALL_LONGTEXT, VLC_TRUE );
    add_bool( "ntservice-uninstall", 0, NULL,
              UNINSTALL_TEXT, UNINSTALL_LONGTEXT, VLC_TRUE );
    add_string ( "ntservice-name", VLCSERVICENAME, NULL,
                 NAME_TEXT, NAME_LONGTEXT, VLC_TRUE );
    add_string ( "ntservice-options", NULL, NULL,
                 OPTIONS_TEXT, OPTIONS_LONGTEXT, VLC_TRUE );
    add_string ( "ntservice-extraintf", NULL, NULL,
                 EXTRAINTF_TEXT, EXTRAINTF_LONGTEXT, VLC_TRUE );

    set_capability( "interface", 0 );
    set_callbacks( Activate, Close );
vlc_module_end();

struct intf_sys_t
{
    SERVICE_STATUS_HANDLE hStatus;
    SERVICE_STATUS status;
    char *psz_service;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void Run( intf_thread_t *p_intf );
static int NTServiceInstall( intf_thread_t *p_intf );
static int NTServiceUninstall( intf_thread_t *p_intf );
static void WINAPI ServiceDispatch( DWORD numArgs, char **args );
static void WINAPI ServiceCtrlHandler( DWORD control );

/* We need this global */
static intf_thread_t *p_global_intf;

/*****************************************************************************
 * Activate: initialize and create stuff
 *****************************************************************************/
static int Activate( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t*)p_this;

    /* Only works on NT/2K/XP */
    if( !IS_WINNT ) return VLC_EGENERIC;

    p_intf->pf_run = Run;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/
void Close( vlc_object_t *p_this )
{
}

/*****************************************************************************
 * Run: interface thread
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    intf_thread_t *p_extraintf;
    SERVICE_TABLE_ENTRY dispatchTable[] =
    {
        { VLCSERVICENAME, &ServiceDispatch },
        { NULL, NULL }
    };

    p_global_intf = p_intf;
    p_intf->p_sys = alloca( sizeof( intf_sys_t ) );
    p_intf->p_sys->psz_service = config_GetPsz( p_intf, "ntservice-name" );
    p_intf->p_sys->psz_service = p_intf->p_sys->psz_service ?
        p_intf->p_sys->psz_service : strdup(VLCSERVICENAME);

    if( config_GetInt( p_intf, "ntservice-install" ) )
    {
        NTServiceInstall( p_intf );
        return;
    }

    if( config_GetInt( p_intf, "ntservice-uninstall" ) )
    {
        NTServiceUninstall( p_intf );
        return;
    }

    if( StartServiceCtrlDispatcher( dispatchTable ) == 0 )
    {
        msg_Err( p_intf, "StartServiceCtrlDispatcher failed" );
    }

    free( p_intf->p_sys->psz_service );

    /* Stop and destroy the interfaces we spawned */
    while( (p_extraintf = vlc_object_find(p_intf, VLC_OBJECT_INTF, FIND_CHILD)))
    {
        intf_StopThread( p_extraintf );
        vlc_object_detach( p_extraintf );
        vlc_object_release( p_extraintf );
        intf_Destroy( p_extraintf );
    }

    /* Make sure we exit (In case other interfaces have been spawned) */
    p_intf->p_vlc->b_die = VLC_TRUE;
}

/*****************************************************************************
 * NT Service utility functions
 *****************************************************************************/
static int NTServiceInstall( intf_thread_t *p_intf )
{
    intf_sys_t *p_sys  = p_intf->p_sys;
    char psz_path[10*MAX_PATH], psz_pathtmp[MAX_PATH], *psz_extra;
    SC_HANDLE handle = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );
    if( handle == NULL )
    {
        msg_Err( p_intf, "could not connect to SCM database" );
        return VLC_EGENERIC;
    }

    /* Find out the filename of ourselves so we can install it to the
     * service control manager */
    GetModuleFileName( NULL, psz_pathtmp, MAX_PATH );
    sprintf( psz_path, "\"%s\" -I "MODULE_STRING, psz_pathtmp );

    psz_extra = config_GetPsz( p_intf, "ntservice-extraintf" );
    if( psz_extra && *psz_extra )
    {
        strcat( psz_path, " --ntservice-extraintf " );
        strcat( psz_path, psz_extra );
    }
    if( psz_extra ) free( psz_extra );

    psz_extra = config_GetPsz( p_intf, "ntservice-options" );
    if( psz_extra && *psz_extra )
    {
        strcat( psz_path, " " );
        strcat( psz_path, psz_extra );
    }
    if( psz_extra ) free( psz_extra );

    SC_HANDLE service =
        CreateService( handle, p_sys->psz_service, p_sys->psz_service,
                       GENERIC_READ | GENERIC_EXECUTE,
                       SERVICE_WIN32_OWN_PROCESS,
                       SERVICE_AUTO_START, SERVICE_ERROR_IGNORE,
                       psz_path, NULL, NULL, NULL, NULL, NULL );
    if( service == NULL )
    {
        if( GetLastError() != ERROR_SERVICE_EXISTS )
        {
            msg_Err( p_intf, "could not create new service: \"%s\" (%s)",
                     p_sys->psz_service ,psz_path );
            CloseServiceHandle( handle );
            return VLC_EGENERIC;
        }
        else
        {
            msg_Warn( p_intf, "service \"%s\" already exists",
                      p_sys->psz_service );
        }
    }
    else
    {
        msg_Warn( p_intf, "service successfuly created" );
    }

    if( service ) CloseServiceHandle( service );
    CloseServiceHandle( handle );

    return VLC_SUCCESS;
}

static int NTServiceUninstall( intf_thread_t *p_intf )
{
    intf_sys_t *p_sys  = p_intf->p_sys;

    SC_HANDLE handle = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );
    if( handle == NULL )
    {
        msg_Err( p_intf, "could not connect to SCM database" );
        return VLC_EGENERIC;
    }

    /* First, open a handle to the service */
    SC_HANDLE service = OpenService( handle, p_sys->psz_service, DELETE );
    if( service == NULL )
    {
        msg_Err( p_intf, "could not open service" );
        CloseServiceHandle( handle );
        return VLC_EGENERIC;
    }

    /* Remove the service */
    if( !DeleteService( service ) )
    {
        msg_Err( p_intf, "could not delete service \"%s\"",
                 p_sys->psz_service );
    }
    else
    {
        msg_Dbg( p_intf, "service deleted successfuly" );
    }

    CloseServiceHandle( service );
    CloseServiceHandle( handle );

    return VLC_SUCCESS;
}

static void WINAPI ServiceDispatch( DWORD numArgs, char **args )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_global_intf;
    intf_sys_t    *p_sys  = p_intf->p_sys;
    char *psz_modules, *psz_parser;

    /* We have to initialize the service-specific stuff */
    memset( &p_sys->status, 0, sizeof(SERVICE_STATUS) );
    p_sys->status.dwServiceType = SERVICE_WIN32;
    p_sys->status.dwCurrentState = SERVICE_START_PENDING;
    p_sys->status.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    p_sys->hStatus =
        RegisterServiceCtrlHandler( p_sys->psz_service, &ServiceCtrlHandler );
    if( p_sys->hStatus == (SERVICE_STATUS_HANDLE)0 )
    {
        msg_Err( p_intf, "failed to register service control handler" );
        return;
    }

    /*
     * Load background interfaces
     */
    psz_modules = config_GetPsz( p_intf, "ntservice-extraintf" );
    psz_parser = psz_modules;
    while( psz_parser && *psz_parser )
    {
        char *psz_module, *psz_temp;
        psz_module = psz_parser;
        psz_parser = strchr( psz_module, ',' );
        if( psz_parser )
        {
            *psz_parser = '\0';
            psz_parser++;
        }
        psz_temp = (char *)malloc( strlen(psz_module) + sizeof(",none") );
        if( psz_temp )
        {
            intf_thread_t *p_new_intf;
            sprintf( psz_temp, "%s,none", psz_module );

            /* Try to create the interface */
            p_new_intf = intf_Create( p_intf, psz_temp, 0, NULL );
            if( p_new_intf == NULL )
            {
                msg_Err( p_intf, "interface \"%s\" initialization failed",
                         psz_temp );
                free( psz_temp );
                continue;
            }

            /* Try to run the interface */
            p_new_intf->b_block = VLC_FALSE;
            if( intf_RunThread( p_new_intf ) )
            {
                vlc_object_detach( p_new_intf );
                intf_Destroy( p_new_intf );
                msg_Err( p_intf, "interface \"%s\" cannot run", psz_temp );
            }

            free( psz_temp );
        }
    }
    if( psz_modules )
    {
        free( psz_modules );
    }

    /* Initialization complete - report running status */
    p_sys->status.dwCurrentState = SERVICE_RUNNING;
    p_sys->status.dwCheckPoint   = 0;
    p_sys->status.dwWaitHint     = 0;

    SetServiceStatus( p_sys->hStatus, &p_sys->status );
}

static void WINAPI ServiceCtrlHandler( DWORD control )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_global_intf;
    intf_sys_t    *p_sys  = p_intf->p_sys;

    switch( control )
    {
    case SERVICE_CONTROL_SHUTDOWN:
    case SERVICE_CONTROL_STOP:
        p_sys->status.dwCurrentState = SERVICE_STOPPED;
        p_sys->status.dwWin32ExitCode = 0;
        p_sys->status.dwCheckPoint = 0;
        p_sys->status.dwWaitHint = 0;
        break;
    case SERVICE_CONTROL_INTERROGATE:
        /* just set the current state to whatever it is... */
        break;
    }

    SetServiceStatus( p_sys->hStatus, &p_sys->status );
}
