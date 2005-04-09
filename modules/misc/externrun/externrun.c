/*****************************************************************************
 * externrun.c: Run extern programs (vlc:run:"theprogram":delayinseconds)
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 *
 * Authors: Sylvain Cadilhac <sylv@via.ecp.fr>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <string.h>

#if defined( WIN32 )
#include <windows.h>
#endif


/*****************************************************************************
 * run_command_t: a command and its delay
 *****************************************************************************/
struct run_command_t
{
    int                  i_delay;
    char                 *psz_torun;
    struct run_command_t *p_next;
};
typedef struct run_command_t run_command_t;

/*****************************************************************************
 * intf_sys_t: description and status of FB interface
 *****************************************************************************/
struct intf_sys_t
{
    vlc_mutex_t   change_lock;
    mtime_t       next_check;
    run_command_t *p_first_command;
    run_command_t *p_last_command;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );
static void Run     ( intf_thread_t * );
static int  AddRunCommand( vlc_object_t *, char const *,
                           vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_category( CAT_INTERFACE );
    set_description( _("Execution of extern programs interface function") );
    set_capability( "interface", 0 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: initialize interface
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    
    /* Search for the Playlist object */
    playlist_t *p_playlist;    
    p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        msg_Err( p_intf, "we are not attached to a playlist" );
        return 1;
    }    

    /* Allocate instance and initialize some members */
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        msg_Err( p_intf, "out of memory" );
        return 1;
    }
    vlc_mutex_init( p_intf, &p_intf->p_sys->change_lock );
    
    var_Create( p_playlist, "run-program-command", VLC_VAR_STRING );
    
    p_intf->p_sys->p_first_command = NULL;
    p_intf->p_sys->p_last_command = NULL;
    p_intf->pf_run = Run;
    var_AddCallback( p_playlist, "run-program-command", AddRunCommand, p_intf );
    vlc_object_release( p_playlist );    
    
    return 0;
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    playlist_t *p_playlist;    
    p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        msg_Err( p_intf, "we are not attached to a playlist" );
        return;
    }     
    var_Destroy( p_playlist, "run-program-command" );
    vlc_object_release( p_playlist );  
    
    run_command_t *p_command = p_intf->p_sys->p_first_command;
    run_command_t *p_next;
    
    while( p_command )
    {
        p_next = p_command->p_next;
        free( p_command->psz_torun );
        free( p_command );
        p_command = p_next;
    }

    /* Destroy structure */
    free( p_intf->p_sys );
}

/*****************************************************************************
 * Run: main loop
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    p_intf->p_sys->next_check = mdate() + 1000000;
    run_command_t *p_command;
    run_command_t *p_previous;

    while( !p_intf->b_die )
    {
        vlc_mutex_lock( &p_intf->p_sys->change_lock );
        p_command = p_intf->p_sys->p_first_command;
        p_previous = NULL;
        while( p_command )
        {
            if( p_command->i_delay > 0 )
            {
                p_command->i_delay--;
                p_previous = p_command;
                p_command = p_previous->p_next;
            }
            else
            {    
                msg_Info( p_intf, "running %s", p_command->psz_torun );
#if defined( WIN32 )

                STARTUPINFO si;
                PROCESS_INFORMATION pi;
                ZeroMemory( &si, sizeof(si) );
                si.cb = sizeof(si);
                ZeroMemory( &pi, sizeof(pi) );
                if( !CreateProcess( NULL, p_command->psz_torun, NULL, NULL,
                    FALSE, 0, NULL, NULL, &si, &pi ) ) 
                {
                    msg_Err( p_intf, "can't run \"%s\"", p_command->psz_torun );
                }    
#else
                if ( fork() )
                {
                    execl( p_command->psz_torun, NULL, (char *)NULL );
                }
#endif
                if( p_previous )
                {
                    p_previous->p_next = p_command->p_next;
                    free( p_command->psz_torun );
                    free( p_command );
                    p_command = p_previous->p_next;
                }
                else
                {
                    p_intf->p_sys->p_first_command = p_command->p_next;
                    free( p_command->psz_torun );
                    free( p_command );
                    if ( p_intf->p_sys->p_last_command == p_command )
                    {
                        p_intf->p_sys->p_last_command = p_command->p_next;
                    }
                     p_command = p_intf->p_sys->p_first_command;
                }
            }
        }
        vlc_mutex_unlock( &p_intf->p_sys->change_lock );
        mwait( p_intf->p_sys->next_check );
        p_intf->p_sys->next_check += 1000000;
    }
    return VLC_SUCCESS;
}


/*****************************************************************************
 * 
 *****************************************************************************/
static int AddRunCommand( vlc_object_t *p_this, char const *psz_var,
                     vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_data;
    

    char *psz_command = NULL;
    int i_delay = 0;
    char *psz_delay = strrchr( newval.psz_string, ':' );
    if ( psz_delay )
    {
        char *psz_err;
        i_delay = ( int )strtol( psz_delay + 1, &psz_err, 10 );
        if ( *psz_err == 0 )
        {
            psz_command = malloc( psz_delay - newval.psz_string );
            if ( psz_command == NULL )
            {
                msg_Err( p_intf, "out of memory: can't add run command" );
                return VLC_EGENERIC;
            }
            strncpy( psz_command, newval.psz_string, psz_delay - newval.psz_string );
            psz_command[ psz_delay - newval.psz_string ] = '\0';
        }
    }
    if ( !psz_command )
    {
        psz_command = malloc( strlen( newval.psz_string ) + 1 );
        strcpy( psz_command, newval.psz_string );
    }
    
    run_command_t *p_command = malloc( sizeof( run_command_t ) );
    if ( p_command == NULL )
    {
        msg_Err( p_intf, "out of memory: can't add run command" );
        return VLC_EGENERIC;    
    }
    p_command->i_delay = i_delay;
    p_command->p_next = NULL;
    p_command->psz_torun = psz_command;
    
    msg_Info( p_intf, "will run %s in %d seconds", psz_command, i_delay );
    
    
    vlc_mutex_lock( &p_intf->p_sys->change_lock );
    if ( p_intf->p_sys->p_last_command )
    {
        p_intf->p_sys->p_last_command->p_next = p_command;
        p_intf->p_sys->p_last_command = p_command;
    }
    else
    {
        p_intf->p_sys->p_last_command = p_command;
        p_intf->p_sys->p_first_command = p_command;
    }
    vlc_mutex_unlock( &p_intf->p_sys->change_lock );

    return VLC_SUCCESS;
}


