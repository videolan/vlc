/*****************************************************************************
 * mms.c: MMS over tcp, udp and http access plug-in
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: mms.c,v 1.34 2003/05/15 22:27:36 massiot Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "mms.h"

/****************************************************************************
 * NOTES:
 *  MMSProtocole documentation found at http://get.to/sdp
 ****************************************************************************/

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

struct access_sys_t
{
    int i_proto;

};

static int  Open        ( vlc_object_t * );
static void Close       ( vlc_object_t * );


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for mms streams. This " \
    "value should be set in miliseconds units." )

vlc_module_begin();
    set_description( _("Microsoft Media Server (MMS) input") );
    set_capability( "access", 0 );
    add_category_hint( "stream", NULL, VLC_TRUE );
        add_integer( "mms-caching", 4 * DEFAULT_PTS_DELAY / 1000, NULL,
                     CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );

        add_bool( "mms-all", 0, NULL,
                  "force selection of all streams",
                  "force selection of all streams", VLC_TRUE );
#if 0
        add_string( "mms-stream", NULL, NULL,
                    "streams selection",
                    "force this stream selection", VLC_TRUE );
#endif
        add_integer( "mms-maxbitrate", 0, NULL,
                     "max bitrate",
                     "set max bitrate for auto streams selections", VLC_FALSE );
    add_shortcut( "mms" );
    add_shortcut( "mmsu" );
    add_shortcut( "mmst" );
    add_shortcut( "mmsh" );
    set_callbacks( Open, Close );
vlc_module_end();


static int Open( vlc_object_t *p_this )
{
    input_thread_t  *p_input = (input_thread_t*)p_this;

    int i_err;


    if( *p_input->psz_access )
    {
        if( !strncmp( p_input->psz_access, "mmsu", 4 ) )
        {
            return E_( MMSTUOpen )( p_input );
        }
        else if( !strncmp( p_input->psz_access, "mmst", 4 ) )
        {
            return E_( MMSTUOpen )( p_input );
        }
        else if( !strncmp( p_input->psz_access, "mmsh", 4 ) )
        {
            return E_( MMSHOpen )( p_input );
        }
    }


    i_err = E_( MMSTUOpen )( p_input );

    if( i_err )
    {
        i_err = E_( MMSHOpen )( p_input );
    }

    return i_err;
}

/*****************************************************************************
 * Close: free unused data structures
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    input_thread_t *  p_input = (input_thread_t *)p_this;
    access_sys_t   *  p_sys   = p_input->p_access_data;

    if( p_sys->i_proto == MMS_PROTO_TCP || p_sys->i_proto == MMS_PROTO_UDP )
    {
        E_( MMSTUClose )( p_input );
    }
    else if( p_sys->i_proto == MMS_PROTO_HTTP )
    {
        E_( MMSHClose )( p_input );
    }
}

/****************************************************************************
 * parse hostname:port/path@username:password
 * FIXME ipv6 ip will be baddly parsed (contain ':' )
 ****************************************************************************/
url_t *E_( url_new )  ( char * psz_url )
{
    url_t *p_url = malloc( sizeof( url_t ) );

    char  *psz_dup    = strdup( psz_url );
    char  *psz_parser = psz_dup;

    char  *psz_tmp;

    /* 1: get hostname:port */
    while( *psz_parser == '/' )
    {
        psz_parser++;
    }

    psz_tmp = psz_parser;

    while( *psz_parser &&
           *psz_parser != ':' &&  *psz_parser != '/' && *psz_parser != '@' )
    {
        psz_parser++;
    }

    p_url->psz_host     = strndup( psz_tmp, psz_parser - psz_tmp );

    if( *psz_parser == ':' )
    {
        psz_parser++;
        psz_tmp = psz_parser;

        while( *psz_parser && *psz_parser != '/' && *psz_parser != '@' )
        {
            psz_parser++;
        }
        p_url->i_port = atoi( psz_tmp );
    }
    else
    {
        p_url->i_port = 0;
    }

    /* 2: get path */
    if( *psz_parser == '/' )
    {
        //psz_parser++;

        psz_tmp = psz_parser;

        while( *psz_parser && *psz_parser != '@' )
        {
            psz_parser++;
        }

        p_url->psz_path = strndup( psz_tmp, psz_parser - psz_tmp );
    }
    else
    {
        p_url->psz_path = strdup( "" );
    }

    /* 3: usrname and password */
    if( *psz_parser == '@' )
    {
        psz_parser++;

        psz_tmp = psz_parser;

        while( *psz_parser && *psz_parser != ':' )
        {
            psz_parser++;
        }

        p_url->psz_username = strndup( psz_tmp, psz_parser - psz_tmp );

        if( *psz_parser == ':' )
        {
            psz_parser++;

            p_url->psz_password = strdup( psz_parser );
        }
        else
        {
            p_url->psz_password = strdup( "" );
        }
    }
    else
    {
        p_url->psz_username = strdup( "" );
        p_url->psz_password = strdup( "" );
    }
#if 0
    fprintf( stderr,
             "host=`%s' port=%d path=`%s' username=`%s' password=`%s'\n",
             p_url->psz_host,
             p_url->i_port,
             p_url->psz_path,
             p_url->psz_username,
             p_url->psz_password );
#endif
    free( psz_dup );
    return p_url;
}

void   E_( url_free ) ( url_t * p_url )
{
    free( p_url->psz_host );
    free( p_url->psz_path );
    free( p_url->psz_username );
    free( p_url->psz_password );
    free( p_url );
}
