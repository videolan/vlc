/*****************************************************************************
 * specific.c: OS/2 specific features
 *****************************************************************************
 * Copyright (C) 2010 KO Myung-Hun
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include "../libvlc.h"

#include <fcntl.h>
#include <io.h>

extern int _fmode_bin;

void system_Init( void )
{
    PPIB ppib;
    CHAR psz_path[ CCHMAXPATH ];
    PSZ  psz_dirsep;

    DosGetInfoBlocks( NULL, &ppib );

    DosQueryModuleName( ppib->pib_hmte, sizeof( psz_path ), psz_path );

    /* remove the executable name */
    psz_dirsep = strrchr( psz_path, '\\');
    if( psz_dirsep )
        *psz_dirsep = '\0';

    /* remove the last directory, i.e, \\bin */
    psz_dirsep = strrchr( psz_path, '\\' );
    if( psz_dirsep )
        *psz_dirsep = '\0';

    DosEnterCritSec();

    if( !psz_vlcpath )
        asprintf( &psz_vlcpath, "%s\\lib\\vlc", psz_path );

    DosExitCritSec();

    /* Set the default file-translation mode */
    _fmode_bin = 1;
    setmode( fileno( stdin ), O_BINARY ); /* Needed for pipes */
}

void system_Configure( libvlc_int_t *p_this, int i_argc, const char *const ppsz_argv[] )
{
    VLC_UNUSED( i_argc ); VLC_UNUSED( ppsz_argv );
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
}

void system_End( void )
{
    free( psz_vlcpath );
    psz_vlcpath = NULL;
}
