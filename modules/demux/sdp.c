/*****************************************************************************
 * sdp.c: SDP parser and builtin UDP/RTP/RTSP
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: sdp.c,v 1.2 2003/08/03 16:22:48 fenrir Exp $
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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/input.h>

#include <ninput.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open  ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_description( _("SDP demuxer + UDP/RTP/RTSP") );
    set_capability( "demux", 100 );
    add_category_hint( "Stream", NULL, VLC_FALSE );
        add_integer( "sdp-session", 0, NULL,
                     "Session", "Session", VLC_TRUE );

    set_callbacks( Open, Close );
    add_shortcut( "sdp" );
vlc_module_end();


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Demux ( input_thread_t * );

typedef struct
{
    char *psz_name;
    char *psz_value;

} sdp_attribute_t;

typedef struct
{
    char    *psz_media;
    char    *psz_description;
    char    *psz_connection;
    char    *psz_bandwith;
    char    *psz_key;

    int             i_attribute;
    sdp_attribute_t *attribute;

} sdp_media_t;

typedef struct
{
    char    *psz_origin;
    char    *psz_session;
    char    *psz_description;
    char    *psz_uri;
    char    *psz_email;
    char    *psz_phone;
    char    *psz_connection;
    char    *psz_bandwith;
    char    *psz_key;
    char    *psz_timezone;

    int             i_attribute;
    sdp_attribute_t *attribute;

    int         i_media;
    sdp_media_t *media;

} sdp_session_t;

typedef struct
{
    int           i_session;
    sdp_session_t *session;

} sdp_t;

struct demux_sys_t
{
    stream_t *s;

    int      i_session;
    sdp_t    *p_sdp;
};

static sdp_t *sdp_Parse  ( char * );
static void  sdp_Dump    ( input_thread_t *, sdp_t * );
static void   sdp_Release( sdp_t * );

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t    *p_sys;
    uint8_t        *p_peek;

    int            i_sdp;
    int            i_sdp_max;
    char           *psz_sdp;

    vlc_value_t    val;

    /* See if it looks like a SDP
       v, o, s fields are mandatory and in this order */
    if( input_Peek( p_input, &p_peek, 7 ) < 7 )
    {
        msg_Err( p_input, "cannot peek" );
        return VLC_EGENERIC;
    }
    if( strncmp( p_peek, "v=0\r\no=", 7 ) &&
        strncmp( p_peek, "v=0\no=", 6 ) )
    {
        msg_Err( p_input, "SDP module discarded" );
        return VLC_EGENERIC;
    }

    /* Set input_thread_t fields */
    p_input->pf_demux = Demux;
    p_input->p_demux_data = p_sys = malloc( sizeof( demux_sys_t ) );

    if( ( p_sys->s = stream_OpenInput( p_input ) ) == NULL )
    {
        msg_Err( p_input, "cannot create stream" );
        goto error;
    }

    /* Read the complete SDP file */
    i_sdp = 0;
    i_sdp_max = 1024;
    psz_sdp = malloc( i_sdp_max );
    for( ;; )
    {
        int i_read;

        i_read = stream_Read( p_sys->s, &psz_sdp[i_sdp], i_sdp_max - i_sdp -1 );
        if( i_read <= i_sdp_max - i_sdp -1 )
        {
            if( i_read > 0 )
            {
                i_sdp += i_read;
            }
            break;
        }
        i_sdp += i_read;
        i_sdp_max += 1024;
        psz_sdp = realloc( psz_sdp, i_sdp_max );
    }
    psz_sdp[i_sdp] = '\0';

    if( strlen( psz_sdp ) <= 0 )
    {
        msg_Err( p_input, "cannot read SDP file" );
        goto error;

    }

    if( ( p_sys->p_sdp = sdp_Parse( psz_sdp ) ) == NULL )
    {
        msg_Err( p_input, "cannot parse SDP" );
        goto error;
    }
    sdp_Dump( p_input, p_sys->p_sdp );

    /* Get the selected session */
    var_Create( p_input, "sdp-session", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_input, "sdp-session", &val );
    p_sys->i_session = val.i_int;
    if( p_sys->i_session >= p_sys->p_sdp->i_session )
    {
        p_sys->i_session = 0;
    }

    /* Now create a handler for each media */


    return VLC_SUCCESS;

error:
    if( p_sys->s )
    {
        stream_Release( p_sys->s );
    }
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t    *p_sys = p_input->p_demux_data;

    sdp_Release( p_sys->p_sdp );
    free( p_sys );
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux ( input_thread_t *p_input )
{

    return 0;
}

/*****************************************************************************
 *
 *****************************************************************************/

/*****************************************************************************
 *  SDP Parser
 *****************************************************************************/
static int   sdp_GetLine( char **ppsz_sdp, char *p_com, char **pp_arg )
{
    char *p = *ppsz_sdp;
    char *p_end;
    int  i_size;

    if( p[0] < 'a' || p[0] > 'z' || p[1] != '=' )
    {
        return VLC_EGENERIC;
    }

    *p_com = p[0];

    if( ( p_end = strstr( p, "\n" ) ) == NULL )
    {
        p_end = p + strlen( p );
    }
    else
    {
        while( *p_end == '\n' || *p_end == '\r' )
        {
            p_end--;
        }
        p_end++;
    }

    i_size = p_end - &p[2];
    *pp_arg = malloc( i_size + 1 );
    memcpy(  *pp_arg, &p[2], i_size );
    (*pp_arg)[i_size] = '\0';

    while( *p_end == '\r' || *p_end == '\n' )
    {
        p_end++;
    }
    *ppsz_sdp = p_end;
    return VLC_SUCCESS;
}

static sdp_t *sdp_Parse  ( char *psz_sdp )
{
    sdp_t *sdp = malloc( sizeof( sdp_t ) );

    sdp->i_session = -1;
    sdp->session   = NULL;

    for( ;; )
    {
#define p_session (&sdp->session[sdp->i_session])
#define p_media   (&p_session->media[p_session->i_media])
        char com, *psz;

        if( sdp_GetLine( &psz_sdp, &com, &psz ) )
        {
            break;
        }
        fprintf( stderr, "com=%c arg=%s\n", com, psz );
        if( sdp->i_session < 0 && ( com !='v' || strcmp( psz, "0" ) ) )
        {
            break;
        }
        switch( com )
        {
            case 'v':
                fprintf( stderr, "New session added\n" );
                if( sdp->i_session != -1 )
                {
                    p_session->i_media++;
                }
                /* Add a new session */
                sdp->i_session++;
                sdp->session =
                    realloc( sdp->session,
                             (sdp->i_session + 1)*sizeof(sdp_session_t) );
                p_session->psz_origin     = NULL;
                p_session->psz_session    = NULL;
                p_session->psz_description= NULL;
                p_session->psz_uri        = NULL;
                p_session->psz_email      = NULL;
                p_session->psz_phone      = NULL;
                p_session->psz_connection = NULL;
                p_session->psz_bandwith   = NULL;
                p_session->psz_key        = NULL;
                p_session->psz_timezone   = NULL;
                p_session->i_media        = -1;
                p_session->media          = NULL;
                p_session->i_attribute    = 0;
                p_session->attribute      = 0;

                break;
            case 'm':
                fprintf( stderr, "New media added\n" );
                p_session->i_media++;
                p_session->media =
                    realloc( p_session->media,
                             (p_session->i_media + 1)*sizeof( sdp_media_t ) );
                p_media->psz_media      = strdup( psz );
                p_media->psz_description= NULL;
                p_media->psz_connection = NULL;
                p_media->psz_bandwith   = NULL;
                p_media->psz_key        = NULL;
                p_media->i_attribute    = 0;
                p_media->attribute      = 0;
                break;
            case 'o':
                p_session->psz_origin = strdup( psz );
                break;
            case 's':
                p_session->psz_session = strdup( psz );
                break;
            case 'i':
                if( p_session->i_media != -1 )
                {
                    p_media->psz_description = strdup( psz );
                }
                else
                {
                    p_session->psz_description = strdup( psz );
                }
                break;
            case 'u':
                p_session->psz_uri = strdup( psz );
                break;
            case 'e':
                p_session->psz_email = strdup( psz );
                break;
            case 'p':
                p_session->psz_phone = strdup( psz );
                break;
            case 'c':
                if( p_session->i_media != -1 )
                {
                    p_media->psz_connection = strdup( psz );
                }
                else
                {
                    p_session->psz_connection = strdup( psz );
                }
                break;
            case 'b':
                if( p_session->i_media != -1 )
                {
                    p_media->psz_bandwith = strdup( psz );
                }
                else
                {
                    p_session->psz_bandwith = strdup( psz );
                }
                break;
            case 'k':
                if( p_session->i_media != -1 )
                {
                    p_media->psz_key = strdup( psz );
                }
                else
                {
                    p_session->psz_key = strdup( psz );
                }
                break;
            case 'z':
                p_session->psz_timezone   = strdup( psz );
                break;
            case 'a':
            {
                char *p = strchr( psz, ':' );
                char *name = NULL;
                char *value= NULL;

                if( p )
                {
                    *p++ = '\0';
                    value= strdup( p);
                }
                name = strdup( psz );

                if( p_session->i_media != -1 )
                {
                    p_media->attribute
                        = realloc( p_media->attribute,
                                   ( p_media->i_attribute + 1 ) *
                                        sizeof( sdp_attribute_t ) );
                    p_media->attribute[p_media->i_attribute].psz_name = name;
                    p_media->attribute[p_media->i_attribute].psz_value = value;
                    p_media->i_attribute++;
                }
                else
                {
                    p_session->psz_key = strdup( psz );
                    p_session->attribute
                        = realloc( p_session->attribute,
                                   ( p_session->i_attribute + 1 ) *
                                        sizeof( sdp_attribute_t ) );
                    p_session->attribute[p_session->i_attribute].psz_name = name;
                    p_session->attribute[p_session->i_attribute].psz_value = value;
                    p_session->i_attribute++;
                }
                break;
            }

            default:
                fprintf( stderr, "unhandled com=%c\n", com );
                break;
        }

#undef p_session
    }

    if( sdp->i_session < 0 )
    {
        free( sdp );
        return NULL;
    }
    sdp->session[sdp->i_session].i_media++;
    sdp->i_session++;

    return sdp;
}
static void   sdp_Release( sdp_t *p_sdp )
{
#define FREE( p ) if( p ) { free( p ) ; (p) = NULL; }
    int i, j, i_attr;
    for( i = 0; i < p_sdp->i_session; i++ )
    {
        FREE( p_sdp->session[i].psz_origin );
        FREE( p_sdp->session[i].psz_session );
        FREE( p_sdp->session[i].psz_description );
        FREE( p_sdp->session[i].psz_uri );
        FREE( p_sdp->session[i].psz_email );
        FREE( p_sdp->session[i].psz_phone );
        FREE( p_sdp->session[i].psz_connection );
        FREE( p_sdp->session[i].psz_bandwith );
        FREE( p_sdp->session[i].psz_key );
        FREE( p_sdp->session[i].psz_timezone );
        for( i_attr = 0; i_attr < p_sdp->session[i].i_attribute; i_attr++ )
        {
            FREE( p_sdp->session[i].attribute[i].psz_name );
            FREE( p_sdp->session[i].attribute[i].psz_value );
        }
        FREE( p_sdp->session[i].attribute );

        for( j = 0; j < p_sdp->session[i].i_media; j++ )
        {
            FREE( p_sdp->session[i].media[j].psz_media );
            FREE( p_sdp->session[i].media[j].psz_description );
            FREE( p_sdp->session[i].media[j].psz_connection );
            FREE( p_sdp->session[i].media[j].psz_bandwith );
            FREE( p_sdp->session[i].media[j].psz_key );
            for( i_attr = 0; i_attr < p_sdp->session[i].i_attribute; i_attr++ )
            {
                FREE( p_sdp->session[i].media[j].attribute[i_attr].psz_name );
                FREE( p_sdp->session[i].media[j].attribute[i_attr].psz_value );
            }
            FREE( p_sdp->session[i].media[j].attribute );
        }
        FREE( p_sdp->session[i].media);
    }
    FREE( p_sdp->session );
    free( p_sdp );
#undef FREE
}

static void  sdp_Dump   ( input_thread_t *p_input, sdp_t *p_sdp )
{
    int i, j, i_attr;
#define PRINTS( var, fmt ) \
    if( var ) { msg_Dbg( p_input, "    - " fmt " : %s", var ); }
#define PRINTM( var, fmt ) \
    if( var ) { msg_Dbg( p_input, "        - " fmt " : %s", var ); }

    for( i = 0; i < p_sdp->i_session; i++ )
    {
        msg_Dbg( p_input, "session[%d]", i );
        PRINTS( p_sdp->session[i].psz_origin, "Origin" );
        PRINTS( p_sdp->session[i].psz_session, "Session" );
        PRINTS( p_sdp->session[i].psz_description, "Description" );
        PRINTS( p_sdp->session[i].psz_uri, "URI" );
        PRINTS( p_sdp->session[i].psz_email, "e-mail" );
        PRINTS( p_sdp->session[i].psz_phone, "Phone" );
        PRINTS( p_sdp->session[i].psz_connection, "Connection" );
        PRINTS( p_sdp->session[i].psz_bandwith, "Bandwith" );
        PRINTS( p_sdp->session[i].psz_key, "Key" );
        PRINTS( p_sdp->session[i].psz_timezone, "TimeZone" );
        for( i_attr = 0; i_attr < p_sdp->session[i].i_attribute; i_attr++ )
        {
            msg_Dbg( p_input, "    - attribute[%d] name:'%s' value:'%s'",
                    i_attr,
                    p_sdp->session[i].attribute[i_attr].psz_name,
                    p_sdp->session[i].attribute[i_attr].psz_value );
        }

        for( j = 0; j < p_sdp->session[i].i_media; j++ )
        {
            msg_Dbg( p_input, "    - media[%d]", j );
            PRINTM( p_sdp->session[i].media[j].psz_media, "Name" );
            PRINTM( p_sdp->session[i].media[j].psz_description, "Description" );
            PRINTM( p_sdp->session[i].media[j].psz_connection, "Connection" );
            PRINTM( p_sdp->session[i].media[j].psz_bandwith, "Bandwith" );
            PRINTM( p_sdp->session[i].media[j].psz_key, "Key" );

            for( i_attr = 0; i_attr < p_sdp->session[i].media[j].i_attribute; i_attr++ )
            {
                msg_Dbg( p_input, "        - attribute[%d] name:'%s' value:'%s'",
                        i_attr,
                        p_sdp->session[i].media[j].attribute[i_attr].psz_name,
                        p_sdp->session[i].media[j].attribute[i_attr].psz_value );
            }
        }
    }
#undef PRINTS
}
