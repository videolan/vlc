/*****************************************************************************
 * stream_output.c : stream output module
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: stream_output.c,v 1.5 2002/11/11 14:39:12 sam Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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
#include <stdlib.h>                                                /* free() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */

#include <vlc/vlc.h>

#include <vlc/sout.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int      InitInstance      ( sout_instance_t * );

/*****************************************************************************
 * sout_NewInstance: creates a new stream output instance
 *****************************************************************************/
sout_instance_t * __sout_NewInstance ( vlc_object_t *p_parent,
                                       char * psz_dest )
{
    sout_instance_t * p_sout;

    /* Allocate descriptor */
    p_sout = vlc_object_create( p_parent, VLC_OBJECT_SOUT );
    if( p_sout == NULL )
    {
        msg_Err( p_parent, "out of memory" );
        return NULL;
    }

    p_sout->psz_dest = psz_dest;
    if ( InitInstance( p_sout ) == -1 )
    {
        vlc_object_destroy( p_sout );
        return NULL;
    }

    return p_sout;
}

/*****************************************************************************
 * InitInstance: opens appropriate modules
 *****************************************************************************/
static int InitInstance( sout_instance_t * p_sout )
{
    /* Parse dest string. Syntax : [[<access>][/<mux>]:][<dest>] */
    /* This code is identical to input.c:InitThread. FIXME : factorize it ? */
    char * psz_parser = p_sout->psz_dest;

    /* Skip the plug-in names */
    while( *psz_parser && *psz_parser != ':' )
    {
        psz_parser++;
    }
#if defined( WIN32 ) || defined( UNDER_CE )
    if( psz_parser - p_sout->psz_dest == 1 )
    {
        msg_Warn( p_sout, "drive letter %c: found in source string",
                           p_sout->psz_dest ) ;
        psz_parser = "";
    }
#endif

    if( !*psz_parser )
    {
        p_sout->psz_access = p_sout->psz_mux = "";
        p_sout->psz_name = p_sout->psz_dest;
    }
    else
    {
        *psz_parser++ = '\0';

        /* let's skip '//' */
        if( psz_parser[0] == '/' && psz_parser[1] == '/' )
        {
            psz_parser += 2 ;
        } 

        p_sout->psz_name = psz_parser ;

        /* Come back to parse the access and mux plug-ins */
        psz_parser = p_sout->psz_dest;

        if( !*psz_parser )
        {
            /* No access */
            p_sout->psz_access = "";
        }
        else if( *psz_parser == '/' )
        {
            /* No access */
            p_sout->psz_access = "";
            psz_parser++;
        }
        else
        {
            p_sout->psz_access = psz_parser;

            while( *psz_parser && *psz_parser != '/' )
            {
                psz_parser++;
            }

            if( *psz_parser == '/' )
            {
                *psz_parser++ = '\0';
            }
        }

        if( !*psz_parser )
        {
            /* No mux */
            p_sout->psz_mux = "";
        }
        else
        {
            p_sout->psz_mux = psz_parser;
        }
    }

    msg_Dbg( p_sout, "access `%s', mux `%s', name `%s'",
             p_sout->psz_access, p_sout->psz_mux, p_sout->psz_name );


    /* Find and open appropriate access module */
    p_sout->p_access =
        module_Need( p_sout, "sout access", p_sout->psz_access );

    if( p_sout->p_access == NULL )
    {
        msg_Err( p_sout, "no suitable sout access module for `%s/%s://%s'",
                 p_sout->psz_access, p_sout->psz_mux, p_sout->psz_name );
        return -1;
    }

    /* Find and open appropriate mux module */
    p_sout->p_mux =
        module_Need( p_sout, "sout mux", p_sout->psz_mux );

    if( p_sout->p_mux == NULL )
    {
        msg_Err( p_sout, "no suitable mux module for `%s/%s://%s'",
                 p_sout->psz_access, p_sout->psz_mux, p_sout->psz_name );
        module_Unneed( p_sout, p_sout->p_access );
        return -1;
    }

    return 0;
}


/*****************************************************************************
 * sout_DeleteInstance: delete a previously allocated instance
 *****************************************************************************/
void sout_DeleteInstance( sout_instance_t * p_sout )
{
    /* Unlink object */
    vlc_object_detach( p_sout );

    /* Free structure */
    vlc_object_destroy( p_sout );
}

