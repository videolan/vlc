/*****************************************************************************
 * specific.c: OS/2 specific features
 *****************************************************************************
 * Copyright (C) 2010 KO Myung-Hun
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
# include "config.h"
#endif

#include <vlc_common.h>
#include "../libvlc.h"
#include <vlc_playlist.h>
#include <vlc_input.h>
#include <vlc_interface.h>
#include <vlc_url.h>

#include <fcntl.h>
#include <io.h>

#define VLC_IPC_PIPE "\\PIPE\\VLC\\IPC\\"VERSION

#define IPC_CMD_GO      0x00
#define IPC_CMD_ENQUEUE 0x01
#define IPC_CMD_QUIT    0xFF

extern int _fmode_bin;

static HPIPE hpipeIPC     = NULLHANDLE;
static int   tidIPCFirst  = -1;
static int   tidIPCHelper = -1;

static void IPCHelperThread( void *arg )
{
    libvlc_int_t *libvlc = arg;

    ULONG  ulCmd;
    int    i_argc;
    char **ppsz_argv;
    size_t i_len;
    ULONG  cbActual;
    int    i_options;

    /* Add files to the playlist */
    playlist_t *p_playlist;

    do
    {
        DosConnectNPipe( hpipeIPC );

        /* Read command */
        DosRead( hpipeIPC, &ulCmd, sizeof( ulCmd ), &cbActual );
        if( ulCmd == IPC_CMD_QUIT )
            continue;

        /* Read a count of arguments */
        DosRead( hpipeIPC, &i_argc, sizeof( i_argc ), &cbActual );

        ppsz_argv = vlc_alloc( i_argc, sizeof( *ppsz_argv ));

        for( int i_opt = 0; i_opt < i_argc; i_opt++ )
        {
            /* Read a length of argv */
            DosRead( hpipeIPC, &i_len, sizeof( i_len ), &cbActual );

            ppsz_argv[ i_opt ] = malloc( i_len );

            /* Read argv */
            DosRead( hpipeIPC, ppsz_argv[ i_opt ], i_len, &cbActual );
        }

        p_playlist = libvlc_priv(libvlc)->playlist;

        for( int i_opt = 0; i_opt < i_argc;)
        {
            i_options = 0;

            /* Count the input options */
            while( i_opt + i_options + 1 < i_argc &&
                   *ppsz_argv[ i_opt + i_options + 1 ] == ':' )
                i_options++;


            if( p_playlist )
            {
                playlist_AddExt( p_playlist, ppsz_argv[ i_opt ], NULL,
                                 i_opt == 0 && ulCmd != IPC_CMD_ENQUEUE,
                                 i_options,
                                 ( char const ** )
                                     ( i_options ? &ppsz_argv[ i_opt + 1 ] :
                                                   NULL ),
                                 VLC_INPUT_OPTION_TRUSTED,
                                 true );
            }

            for( ; i_options >= 0; i_options-- )
                free( ppsz_argv[ i_opt++ ]);
        }

        free( ppsz_argv );
    } while( !DosDisConnectNPipe( hpipeIPC ) && ulCmd != IPC_CMD_QUIT );

    DosClose( hpipeIPC );
    hpipeIPC = NULLHANDLE;

    tidIPCFirst  = -1;
    tidIPCHelper = -1;
}

void system_Init( void )
{
    /* Set the default file-translation mode */
    _fmode_bin = 1;
    setmode( fileno( stdin ), O_BINARY ); /* Needed for pipes */
}

void system_Configure( libvlc_int_t *p_this, int i_argc, const char *const ppsz_argv[] )
{
    if( var_InheritBool( p_this, "high-priority" ) )
    {
        if( !DosSetPriority( PRTYS_PROCESS, PRTYC_REGULAR, PRTYD_MAXIMUM, 0 ) )
        {
            msg_Dbg( p_this, "raised process priority" );
        }
        else
        {
            msg_Dbg( p_this, "could not raise process priority" );
        }
    }

    if( var_InheritBool( p_this, "one-instance" )
        || ( var_InheritBool( p_this, "one-instance-when-started-from-file" )
             && var_InheritBool( p_this, "started-from-file" ) ) )
    {
        HPIPE hpipe;
        ULONG ulAction;
        ULONG rc;

        msg_Info( p_this, "one instance mode ENABLED");

        /* Use a named pipe to check if another instance is already running */
        for(;;)
        {
            rc = DosOpen( VLC_IPC_PIPE, &hpipe, &ulAction, 0, 0,
                          OPEN_ACTION_OPEN_IF_EXISTS,
                          OPEN_ACCESS_READWRITE | OPEN_SHARE_DENYREADWRITE |
                          OPEN_FLAGS_FAIL_ON_ERROR,
                          NULL );

            if( rc == ERROR_PIPE_BUSY )
                DosWaitNPipe( VLC_IPC_PIPE, -1 );
            else
                break;
        }

        if( rc )
        {
            rc = DosCreateNPipe( VLC_IPC_PIPE,  &hpipeIPC,
                                 NP_ACCESS_DUPLEX,
                                 NP_WAIT | NP_TYPE_MESSAGE |
                                 NP_READMODE_MESSAGE | 0x01,
                                 32768, 32768, 0 );
            if( rc )
            {
                /* Failed to create a named pipe. Just ignore the option and
                 * go on as normal. */
                msg_Err( p_this, "one instance mode DISABLED "
                         "(a named pipe couldn't be created)" );
                return;
            }

            /* We are the 1st instance. */

            /* Save the tid of the first instance */
            tidIPCFirst = _gettid();

            /* Run the helper thread */
            tidIPCHelper = _beginthread( IPCHelperThread, NULL, 1024 * 1024,
                                         p_this );
            if( tidIPCHelper == -1 )
            {
                msg_Err( p_this, "one instance mode DISABLED "
                         "(IPC helper thread couldn't be created)");

                tidIPCFirst = -1;
            }
        }
        else
        {
            /* Another instance is running */
            ULONG ulCmd = var_InheritBool( p_this, "playlist-enqueue") ?
                              IPC_CMD_ENQUEUE : IPC_CMD_GO;
            ULONG cbActual;

            /* Write a command */
            DosWrite( hpipe, &ulCmd, sizeof( ulCmd ), &cbActual );

            /* We assume that the remaining parameters are filenames
             * and their input options */

            /* Write a count of arguments */
            DosWrite( hpipe, &i_argc, sizeof( i_argc ), &cbActual );

            for( int i_opt = 0; i_opt < i_argc; i_opt++ )
            {
                /* We need to resolve relative paths in this instance */
                char *mrl;
                if( strstr( ppsz_argv[ i_opt ], "://" ))
                    mrl = strdup( ppsz_argv[ i_opt ] );
                else
                    mrl = vlc_path2uri( ppsz_argv[ i_opt ], NULL );

                if( !mrl )
                    mrl = ( char * )ppsz_argv[ i_opt ];

                size_t i_len = strlen( mrl ) + 1;

                /* Write a length of an argument */
                DosWrite( hpipe, &i_len, sizeof( i_len ), &cbActual );

                /* Write an argument */
                DosWrite( hpipe, mrl, i_len, &cbActual );

                if( mrl != ppsz_argv[ i_opt ])
                    free( mrl );
            }

            /* Close a named pipe of a client side */
            DosClose( hpipe );

            /* Bye bye */
            system_End();
            exit( 0 );
        }
    }
}

/**
 * Cleans up after system_Init() and system_Configure().
 */
void system_End(void)
{
    if( tidIPCFirst == _gettid())
    {
        HPIPE hpipe;
        ULONG ulAction;
        ULONG cbActual;
        ULONG rc;

        do
        {
            rc = DosOpen( VLC_IPC_PIPE, &hpipe, &ulAction, 0, 0,
                          OPEN_ACTION_OPEN_IF_EXISTS,
                          OPEN_ACCESS_READWRITE | OPEN_SHARE_DENYREADWRITE |
                          OPEN_FLAGS_FAIL_ON_ERROR,
                          NULL );

            if( rc == ERROR_PIPE_BUSY )
                DosWaitNPipe( VLC_IPC_PIPE, -1 );
            else if( rc )
                DosSleep( 1 );
        } while( rc );

        /* Ask for IPCHelper to quit */
        ULONG ulCmd = IPC_CMD_QUIT;
        DosWrite( hpipe, &ulCmd, sizeof( ulCmd ), &cbActual );

        DosClose( hpipe );

        TID tid = tidIPCHelper;
        DosWaitThread( &tid, DCWW_WAIT );
    }
}

