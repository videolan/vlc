/*****************************************************************************
 * sdp.c: SDP parser and builtin UDP/RTP/RTSP
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: sdp.c,v 1.4 2003/08/04 18:50:36 fenrir Exp $
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

#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#if defined( UNDER_CE )
#   include <winsock.h>
#elif defined( WIN32 )
#   include <winsock2.h>
#   include <ws2tcpip.h>
#   ifndef IN_MULTICAST
#       define IN_MULTICAST(a) IN_CLASSD(a)
#   endif
#else
#   include <sys/socket.h>
#endif


#include "network.h"

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

#define FREE( p ) if( p ) { free( p ) ; (p) = NULL; }

#define TAB_APPEND( count, tab, p )             \
    if( (count) > 0 )                           \
    {                                           \
        (tab) = realloc( (tab), sizeof( void ** ) * ( (count) + 1 ) ); \
    }                                           \
    else                                        \
    {                                           \
        (tab) = malloc( sizeof( void ** ) );    \
    }                                           \
    (void**)(tab)[(count)] = (void*)(p);        \
    (count)++

#define TAB_FIND( count, tab, p, index )        \
    {                                           \
        int _i_;                                \
        (index) = -1;                           \
        for( _i_ = 0; _i_ < (count); _i_++ )    \
        {                                       \
            if((void**)(tab)[_i_]==(void*)(p))  \
            {                                   \
                (index) = _i_;                  \
                break;                          \
            }                                   \
        }                                       \
    }

#define TAB_REMOVE( count, tab, p )             \
    {                                           \
        int i_index;                            \
        TAB_FIND( count, tab, p, i_index );     \
        if( i_index >= 0 )                      \
        {                                       \
            if( count > 1 )                     \
            {                                   \
                memmove( ((void**)tab + i_index),    \
                         ((void**)tab + i_index+1),  \
                         ( (count) - i_index - 1 ) * sizeof( void* ) );\
            }                                   \
            else                                \
            {                                   \
                free( tab );                    \
                (tab) = NULL;                   \
            }                                   \
            (count)--;                          \
        }                                       \
    }

/*
 * SDP definitions
 */
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


static sdp_t *sdp_Parse     ( char * );
static void   sdp_Dump      ( input_thread_t *, sdp_t * );
static void   sdp_Release   ( sdp_t * );

#define RTP_PAYLOAD_MAX 10
typedef struct
{
    int     i_cat;          /* AUDIO_ES, VIDEO_ES */

    char    *psz_control;   /* If any, eg rtsp://blabla/... */

    int     i_port;         /* base port */
    int     i_port_count;   /* for hierachical stream */

    int     i_address_count;/* for hierachical stream */
    int     i_address_ttl;
    char    *psz_address;

    char    *psz_transport; /* RTP/AVP, udp, ... */

    int     i_payload_count;
    struct
    {
        int     i_type;
        char    *psz_rtpmap;
        char    *psz_fmtp;
    } payload[RTP_PAYLOAD_MAX];

} sdp_track_t;

static sdp_track_t *sdp_TrackCreate ( sdp_t *, int , int , char * );
static void         sdp_TrackRelease( sdp_track_t * );

static int  NetOpenUDP( vlc_object_t *, char *, int , char *, int );
static void NetClose  ( vlc_object_t *, int );


/*
 * RTP handler
 */
typedef struct rtp_stream_sys_t rtp_stream_sys_t;
typedef struct
{
    struct
    {
        int          i_cat;     /* AUDIO_ES/VIDEO_ES */
        vlc_fourcc_t i_codec;

        struct
        {
            int i_width;
            int i_height;
        } video;

        struct
        {
            int i_channels;
            int i_samplerate;
            int i_samplesize;
        } audio;

        int  i_extra_data;
        void *p_extra_data;
    } es;

    struct
    {
        int  i_data;
        void *p_data;
    } frame;

    /* User private */
    rtp_stream_sys_t *p_sys;

} rtp_stream_t;

typedef struct
{
    vlc_bool_t   b_data;
    rtp_stream_t stream;

    int i_handle;
} rtp_source_t;

typedef struct
{
    VLC_COMMON_MEMBERS

    int          i_rtp;
    rtp_source_t **rtp;

} rtp_t;

static rtp_t *rtp_New( input_thread_t *p_input );
static int    rtp_Add( rtp_t *rtp, sdp_track_t *tk, rtp_stream_t **pp_stream );
static int    rtp_Read( rtp_t *rtp, rtp_stream_t **pp_stream );
static int    rtp_Control( rtp_t *rtp, int i_query );
static void   rtp_Release( rtp_t *rtp );

/*
 * Module specific
 */

struct demux_sys_t
{
    stream_t *s;

    int      i_session;
    sdp_t    *p_sdp;

    rtp_t    *rtp;
};

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

    int            i;
    sdp_session_t  *p_session;

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

    /* Init private data */
    p_sys->i_session= 0;
    p_sys->p_sdp    = NULL;
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
        if( i_read < i_sdp_max - i_sdp -1 )
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

    /* Parse this SDP */
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
    p_session = &p_sys->p_sdp->session[p_sys->i_session];

    /* Create a RTP handler */
    p_sys->rtp = rtp_New( p_input );

    /* Now create a track for each media */
    for( i = 0; i < p_session->i_media; i++ )
    {
        sdp_track_t     *tk;
        rtp_stream_t    *rs;
        int j;

        tk = sdp_TrackCreate( p_sys->p_sdp, p_sys->i_session, i,
                              p_input->psz_source );
        if( tk == NULL )
        {
            msg_Warn( p_input, "media[%d] invalid", i );
            continue;
        }

        msg_Dbg( p_input, "media[%d] :", i );
        msg_Dbg( p_input, "    - cat : %s",
                 tk->i_cat == AUDIO_ES ? "audio" :
                                ( tk->i_cat == VIDEO_ES ? "video":"unknown") );
        msg_Dbg( p_input, "    - control : %s", tk->psz_control );
        msg_Dbg( p_input, "    - address : %s ttl : %d count : %d",
                 tk->psz_address, tk->i_address_ttl, tk->i_address_count );
        msg_Dbg( p_input, "    - port : %d count : %d",
                 tk->i_port, tk->i_port_count );
        msg_Dbg( p_input, "    - transport : %s", tk->psz_transport );
        for( j = 0; j < tk->i_payload_count; j++ )
        {
            msg_Dbg( p_input,
                     "    - payload[%d] : type : %d rtpmap : %s fmtp : %s",
                     j, tk->payload[j].i_type,
                     tk->payload[j].psz_rtpmap,
                     tk->payload[j].psz_fmtp );
        }

        if( tk->psz_control )
        {
            msg_Err( p_input, " -> Need control : Unsuported" );
            sdp_TrackRelease( tk );
            continue;
        }

        if( rtp_Add( p_sys->rtp, tk, &rs ) )
        {
            msg_Err( p_input, "cannot add media[%d]", i );
        }

        sdp_TrackRelease( tk );
    }
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

    rtp_Release( p_sys->rtp );
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
    demux_sys_t    *p_sys = p_input->p_demux_data;
    int i;

    for( i = 0; i < 10; i++ )
    {
        rtp_stream_t *rs;

        if( rtp_Read( p_sys->rtp, &rs ) )
        {
            /* FIXME */
            return 1;
        }
        if( !rs )
        {
            continue;
        }

        msg_Dbg( p_input, "rs[%p] frame.size=%d", rs, rs->frame.i_data );
    }

    return 1;
}

/*****************************************************************************
 *
 *****************************************************************************/

/*****************************************************************************
 *  SDP Parser
 *****************************************************************************/
/* TODO:
 * - implement parsing of all fields
 * - ?
 */
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
                p_session->attribute      = NULL;

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
                p_media->attribute      = NULL;
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
                    /* FIXME could be multiple addresses ( but only at session
                     * level FIXME */
                    /* For instance
                           c=IN IP4 224.2.1.1/127
                           c=IN IP4 224.2.1.2/127
                           c=IN IP4 224.2.1.3/127
                        is valid */
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
#undef p_media
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

static char * sdp_AttributeValue( int i_attribute, sdp_attribute_t *attribute,
                                  char *name )
{
    int i;

    for( i = 0; i < i_attribute; i++ )
    {
        if( !strcmp( attribute[i].psz_name, name ) )
        {
            return attribute[i].psz_value;
        }
    }
    return NULL;
}

/*
 * Create a track from an SDP session/media
 */
static sdp_track_t *sdp_TrackCreate( sdp_t *p_sdp, int i_session, int i_media,
                                     char *psz_url_base )
{
    sdp_track_t   *tk = malloc( sizeof( sdp_track_t ) );
    sdp_session_t *p_session = &p_sdp->session[i_session];
    sdp_media_t   *p_media = &p_session->media[i_media];

    char *psz_url = NULL;
    char *p;
    char *psz;

    /* Be sure there is a terminating '/' */
    if( psz_url_base && *psz_url_base != '\0' )
    {
        psz_url = malloc( strlen( psz_url_base ) + 2);
        strcpy( psz_url, psz_url_base );
        if( psz_url[strlen( psz_url ) -1] != '/' )
        {
            strcat( psz_url, "/" );
        }
    }

    /* Get track type */
    if( !strncmp( p_media->psz_media, "audio", 5 ) )
    {
        tk->i_cat = AUDIO_ES;
    }
    else if( !strncmp( p_media->psz_media, "video", 5 ) )
    {
        tk->i_cat = VIDEO_ES;
    }
    else
    {
        free( tk );
        return NULL;
    }
    p = &p_media->psz_media[5];

    /* Get track port base and count */
    tk->i_port = strtol( p, &p, 0 );

    if( *p == '/' )
    {
        p++;
        tk->i_port_count = strtol( p, &p, 0 );
    }
    else
    {
        tk->i_port_count = 0;
    }

    while( *p == ' ' )
    {
        p++;
    }

    /* Get transport */
    tk->psz_transport = strdup( p );
    if( ( psz = strchr( tk->psz_transport, ' ' ) ) )
    {
        *psz = '\0';
    }
    while( *p && *p != ' ' )
    {
        p++;
    }

    /* Get payload type+fmt */
    tk->i_payload_count = 0;
    for( ;; )
    {
        int i;

        tk->payload[tk->i_payload_count].i_type     = strtol( p, &p, 0 );
        tk->payload[tk->i_payload_count].psz_rtpmap = NULL;
        tk->payload[tk->i_payload_count].psz_fmtp   = NULL;

        for( i = 0; i < p_media->i_attribute; i++ )
        {
            if( !strcmp( p_media->attribute[i].psz_name, "rtpmap" ) &&
                p_media->attribute[i].psz_value )
            {
                char *p = p_media->attribute[i].psz_value;
                int i_type = strtol( p, &p, 0 );

                if( i_type == tk->payload[tk->i_payload_count].i_type )
                {
                    tk->payload[tk->i_payload_count].psz_rtpmap = strdup( p );
                }
            }
            else if( !strcmp( p_media->attribute[i].psz_name, "fmtp" ) &&
                     p_media->attribute[i].psz_value )
            {
                char *p = p_media->attribute[i].psz_value;
                int i_type = strtol( p, &p, 0 );

                if( i_type == tk->payload[tk->i_payload_count].i_type )
                {
                    tk->payload[tk->i_payload_count].psz_fmtp = strdup( p );
                }
            }

        }
        tk->i_payload_count++;
        if( *p == '\0' || tk->i_payload_count >= RTP_PAYLOAD_MAX )
        {
            break;
        }
    }

    /* Get control */
    psz = sdp_AttributeValue( p_media->i_attribute, p_media->attribute,
                              "control" );
    if( !psz || *psz == '\0' )
    {
        psz = sdp_AttributeValue( p_session->i_attribute, p_session->attribute,
                                  "control" );
    }

    if( psz && *psz != '\0')
    {
        if( strstr( psz, "://" ) || psz_url == NULL )
        {
            tk->psz_control = strdup( psz );
        }
        else
        {
            tk->psz_control = malloc( strlen( psz_url ) + strlen( psz ) + 1 );
            strcpy( tk->psz_control, psz_url );
            strcat( tk->psz_control, psz );
        }
    }
    else
    {
        tk->psz_control = NULL;
    }

    /* Get address */
    psz = p_media->psz_connection;
    if( psz == NULL )
    {
        psz = p_session->psz_connection;
    }
    if( psz && *psz != '\0' )
    {
        tk->i_address_count = 0;
        tk->i_address_ttl   = 0;
        tk->psz_address     = NULL;

        if( ( p = strstr( psz, "IP4" ) ) == NULL )
        {
            p = strstr( psz, "IP6" );   /* FIXME No idea if it exists ... */
        }
        if( p )
        {
            p += 3;
            while( *p == ' ' )
            {
                p++;
            }

            tk->psz_address = p = strdup( p );
            p = strchr( p, '/' );
            if(p )
            {
                *p++ = '\0';

                tk->i_address_ttl= strtol( p, &p, 0 );
                if( p )
                {
                    tk->i_address_count = strtol( p, &p, 0 );
                }
            }
        }
    }
    else
    {
        tk->i_address_count = 0;
        tk->i_address_ttl   = 0;
        tk->psz_address     = NULL;
    }

    return tk;
}


static void         sdp_TrackRelease( sdp_track_t *tk )
{
    int i;

    FREE( tk->psz_control );
    FREE( tk->psz_address );
    FREE( tk->psz_transport );
    for( i = 0; i < tk->i_payload_count; i++ )
    {
        FREE( tk->payload[i].psz_rtpmap );
        FREE( tk->payload[i].psz_fmtp );
    }
    free( tk );
}

/*****************************************************************************
 * RTP/RTCP/RTSP handler
 *****************************************************************************/

static rtp_t *rtp_New( input_thread_t *p_input )
{
    rtp_t *rtp = vlc_object_create( p_input, sizeof( rtp_t ) );

    rtp->i_rtp = 0;
    rtp->rtp   = NULL;

    return rtp;
}

static int    rtp_Add( rtp_t *rtp, sdp_track_t *tk, rtp_stream_t **pp_stream )
{
    rtp_source_t *rs = malloc( sizeof( rtp_source_t ) );


    if( tk->psz_control )
    {
        msg_Err( rtp, "RTP using control unsupported" );
        free( rs );
        return VLC_EGENERIC;
    }

    *pp_stream = &rs->stream;

    /* no data unread */
    rs->b_data = VLC_FALSE;

    /* Init stream properties */
    rs->stream.es.i_cat   = tk->i_cat;
    rs->stream.es.i_codec = VLC_FOURCC( 'u', 'n', 'd', 'f' );
    if( rs->stream.es.i_cat == AUDIO_ES )
    {
        rs->stream.es.audio.i_channels = 0;
        rs->stream.es.audio.i_samplerate = 0;
        rs->stream.es.audio.i_samplesize = 0;
    }
    else if( rs->stream.es.i_cat == VIDEO_ES )
    {
        rs->stream.es.video.i_width = 0;
        rs->stream.es.video.i_height = 0;
    }
    rs->stream.es.i_extra_data = 0;
    rs->stream.es.p_extra_data = NULL;
    rs->stream.frame.i_data = 0;
    rs->stream.frame.p_data = malloc( 65535 );  /* Max size of a UDP packet */
    rs->stream.p_sys = NULL;

    /* Open the handle */
    rs->i_handle = NetOpenUDP( VLC_OBJECT( rtp ),
                               tk->psz_address, tk->i_port,
                               "", 0 );

    if( rs->i_handle < 0 )
    {
        msg_Err( rtp, "cannot connect at %s:%d", tk->psz_address, tk->i_port );
        free( rs );
    }

    TAB_APPEND( rtp->i_rtp, rtp->rtp, rs );

    return VLC_SUCCESS;
}

static int    rtp_Read( rtp_t *rtp, rtp_stream_t **pp_stream )
{
    int             i;
    struct timeval  timeout;
    fd_set          fds_read;
    int             i_handle_max = 0;
    int             i_ret;

    *pp_stream = NULL;

    /* return already buffered data */
    for( i = 0; i < rtp->i_rtp; i++ )
    {
        if( rtp->rtp[i]->b_data && rtp->rtp[i]->stream.frame.i_data > 0 )
        {
            rtp->rtp[i]->b_data = VLC_FALSE;
            *pp_stream = &rtp->rtp[i]->stream;
            return VLC_SUCCESS;
        }
    }

    /* aquire new data */
    FD_ZERO( &fds_read );
    for( i = 0; i < rtp->i_rtp; i++ )
    {
        if( rtp->rtp[i]->i_handle > 0 )
        {
            FD_SET( rtp->rtp[i]->i_handle, &fds_read );
            i_handle_max = __MAX( i_handle_max, rtp->rtp[i]->i_handle );
        }
    }

    /* we will wait 0.5s */
    timeout.tv_sec = 0;
    timeout.tv_usec = 500*1000;

    i_ret = select( i_handle_max + 1, &fds_read, NULL, NULL, &timeout );
    if( i_ret == -1 && errno != EINTR )
    {
        msg_Warn( rtp, "cannot select sockets" );
        msleep( 1000 );
        return VLC_EGENERIC;
    }
    if( i_ret <= 0 )
    {
        return VLC_EGENERIC;
    }

    for( i = 0; i < rtp->i_rtp; i++ )
    {
        if( rtp->rtp[i]->i_handle > 0 && FD_ISSET( rtp->rtp[i]->i_handle, &fds_read ) )
        {
            int i_recv;
            i_recv = recv( rtp->rtp[i]->i_handle,
                           rtp->rtp[i]->stream.frame.p_data,
                           65535,
                           0 );
#if defined( WIN32 ) || defined( UNDER_CE )
            if( ( i_recv < 0 && WSAGetLastError() != WSAEWOULDBLOCK )||( i_recv == 0 ) )
#else
            if( ( i_recv < 0 && errno != EAGAIN && errno != EINTR )||( i_recv == 0 ) )
#endif
            {
                msg_Warn( rtp, "error reading con[%d] -> closed", i );
                NetClose( VLC_OBJECT( rtp ), rtp->rtp[i]->i_handle );
                rtp->rtp[i]->i_handle = -1;
                continue;
            }

            msg_Dbg( rtp, "con[%d] read %d bytes", i, i_recv );
            rtp->rtp[i]->stream.frame.i_data = i_recv;
            rtp->rtp[i]->b_data = VLC_TRUE;
        }
    }

    /* return buffered data */
    for( i = 0; i < rtp->i_rtp; i++ )
    {
        if( rtp->rtp[i]->b_data && rtp->rtp[i]->stream.frame.i_data > 0 )
        {
            rtp->rtp[i]->b_data = VLC_FALSE;
            *pp_stream = &rtp->rtp[i]->stream;
            return VLC_SUCCESS;
        }
    }

    return VLC_EGENERIC;
}

static int    rtp_Control( rtp_t *rtp, int i_query )
{
    return VLC_EGENERIC;
}

static void   rtp_Release( rtp_t *rtp )
{
    int i;

    for( i = 0; i < rtp->i_rtp; i++ )
    {
        if( rtp->rtp[i]->i_handle > 0 )
        {
            msg_Dbg( rtp, "closing connection[%d]", i );
            NetClose( VLC_OBJECT( rtp ), rtp->rtp[i]->i_handle );
        }
    }
    vlc_object_destroy( rtp );
}


static int  NetOpenUDP( vlc_object_t *p_this,
                        char *psz_local, int i_local_port,
                        char *psz_server, int i_server_port )
{
    char             *psz_network;
    module_t         *p_network;
    network_socket_t socket_desc;

    psz_network = "";
    if( config_GetInt( p_this, "ipv4" ) )
    {
        psz_network = "ipv4";
    }
    else if( config_GetInt( p_this, "ipv6" ) )
    {
        psz_network = "ipv6";
    }

    msg_Dbg( p_this, "waiting for connection..." );

    socket_desc.i_type = NETWORK_UDP;
    socket_desc.psz_server_addr = psz_server;
    socket_desc.i_server_port   = i_server_port;
    socket_desc.psz_bind_addr   = psz_local;
    socket_desc.i_bind_port     = i_local_port;
    socket_desc.i_ttl           = 0;
    p_this->p_private = (void*)&socket_desc;
    if( !( p_network = module_Need( p_this, "network", psz_network ) ) )
    {
        msg_Err( p_this, "failed to connect with server" );
        return -1;
    }
    module_Unneed( p_this, p_network );

    return socket_desc.i_handle;
}


static void NetClose( vlc_object_t *p_this, int i_handle )
{
#if defined( WIN32 ) || defined( UNDER_CE )
    closesocket( i_handle );
#else
    close( i_handle );
#endif
}

