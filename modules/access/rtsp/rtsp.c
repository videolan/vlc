/*****************************************************************************
 * rtsp.c: minimalistic implementation of rtsp protocol.
 *         Not RFC 2326 compilant yet and only handle REAL RTSP.
 *****************************************************************************
 * Copyright (C) 2002-2004 the xine project
 * Copyright (C) 2005 VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Adapted from xine which itself adapted it from joschkas real tools.
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

#include "rtsp.h"

#define BUF_SIZE 4096
#define HEADER_SIZE 1024
#define MAX_FIELDS 256

struct rtsp_s {

  int           s;

  char         *host;
  int           port;
  char         *path;
  char         *mrl;
  char         *user_agent;

  char         *server;
  unsigned int  server_state;
  uint32_t      server_caps;

  unsigned int  cseq;
  char         *session;

  char        *answers[MAX_FIELDS];   /* data of last message */
  char        *scheduled[MAX_FIELDS]; /* will be sent with next message */
};

/*
 * constants
 */

const char rtsp_protocol_version[]="RTSP/1.0";

/* server states */
#define RTSP_CONNECTED 1
#define RTSP_INIT      2
#define RTSP_READY     4
#define RTSP_PLAYING   8
#define RTSP_RECORDING 16

/* server capabilities */
#define RTSP_OPTIONS       0x001
#define RTSP_DESCRIBE      0x002
#define RTSP_ANNOUNCE      0x004
#define RTSP_SETUP         0x008
#define RTSP_GET_PARAMETER 0x010
#define RTSP_SET_PARAMETER 0x020
#define RTSP_TEARDOWN      0x040
#define RTSP_PLAY          0x080
#define RTSP_RECORD        0x100

/*
 * rtsp_get gets a line from stream
 * and returns a null terminated string (must be freed).
 */
 
static char *rtsp_get( rtsp_client_t *rtsp )
{
  char *psz_buffer = malloc( BUF_SIZE );
  char *psz_string = NULL;

  if( rtsp->pf_read_line( rtsp->p_userdata, (uint8_t*)psz_buffer, (unsigned int)BUF_SIZE ) >= 0 )
  {
    //printf( "<< '%s'\n", psz_buffer );
      psz_string = strdup( psz_buffer );
  }

  free( psz_buffer );
  return psz_string;
}


/*
 * rtsp_put puts a line on stream
 */

static int rtsp_put( rtsp_client_t *rtsp, const char *psz_string )
{
    unsigned int i_buffer = strlen( psz_string );
    char *psz_buffer = malloc( i_buffer + 3 );
    int i_ret;

    strcpy( psz_buffer, psz_string );
    psz_buffer[i_buffer] = '\r'; psz_buffer[i_buffer+1] = '\n';
    psz_buffer[i_buffer+2] = 0;

    i_ret = rtsp->pf_write( rtsp->p_userdata, (uint8_t*)psz_buffer, i_buffer + 2 );

    free( psz_buffer );
    return i_ret;
}

/*
 * extract server status code
 */

static int rtsp_get_status_code( rtsp_client_t *rtsp, const char *psz_string )
{
    char psz_buffer[4];
    int i_code = 0;

    if( !strncmp( psz_string, "RTSP/1.0", sizeof("RTSP/1.0") - 1 ) )
    {
        memcpy( psz_buffer, psz_string + sizeof("RTSP/1.0"), 3 );
        psz_buffer[3] = 0;
        i_code = atoi( psz_buffer );
    }
    else if( !strncmp( psz_string, "SET_PARAMETER", 8 ) )
    {
        return RTSP_STATUS_SET_PARAMETER;
    }

    if( i_code != 200 )
    {
        //fprintf( stderr, "librtsp: server responds: '%s'\n", psz_string );
    }

    return i_code;
}

/*
 * send a request
 */

static int rtsp_send_request( rtsp_client_t *rtsp, const char *psz_type,
                              const char *psz_what )
{
    char **ppsz_payload = rtsp->p_private->scheduled;
    char *psz_buffer;
    int i_ret;

    psz_buffer = malloc( strlen(psz_type) + strlen(psz_what) +
                         sizeof("RTSP/1.0") + 2 );

    sprintf( psz_buffer, "%s %s %s", psz_type, psz_what, "RTSP/1.0" );
    i_ret = rtsp_put( rtsp, psz_buffer );
    free( psz_buffer );

    if( ppsz_payload )
        while( *ppsz_payload )
        {
            rtsp_put( rtsp, *ppsz_payload );
            ppsz_payload++;
        }
    rtsp_put( rtsp, "" );
    rtsp_unschedule_all( rtsp );

    return i_ret;
}

/*
 * schedule standard fields
 */

static void rtsp_schedule_standard( rtsp_client_t *rtsp )
{
    char tmp[17];

    sprintf( tmp, "Cseq: %u", rtsp->p_private->cseq);
    rtsp_schedule_field( rtsp, tmp );

    if( rtsp->p_private->session )
    {
        char *buf;
        buf = malloc( strlen(rtsp->p_private->session) + 15 );
        sprintf( buf, "Session: %s", rtsp->p_private->session );
        rtsp_schedule_field( rtsp, buf );
        free( buf );
    }
}

/*
 * get the answers, if server responses with something != 200, return NULL
 */

static int rtsp_get_answers( rtsp_client_t *rtsp )
{
    char *answer = NULL;
    unsigned int answer_seq;
    char **answer_ptr = rtsp->p_private->answers;
    int code;
    int ans_count = 0;

    answer = rtsp_get( rtsp );
    if( !answer ) return 0;
    code = rtsp_get_status_code( rtsp, answer );
    free( answer );

    rtsp_free_answers( rtsp );

    do { /* while we get answer lines */

      answer = rtsp_get( rtsp );
      if( !answer ) return 0;

      if( !strncasecmp( answer, "Cseq:", 5 ) )
      {
          sscanf( answer, "%*s %u", &answer_seq );
          if( rtsp->p_private->cseq != answer_seq )
          {
            //fprintf( stderr, "warning: Cseq mismatch. got %u, assumed %u",
            //       answer_seq, rtsp->p_private->cseq );

              rtsp->p_private->cseq = answer_seq;
          }
      }
      if( !strncasecmp( answer, "Server:", 7 ) )
      {
          char *buf = malloc( strlen(answer) );
          sscanf( answer, "%*s %s", buf );
          free( rtsp->p_private->server );
          rtsp->p_private->server = buf;
      }
      if( !strncasecmp( answer, "Session:", 8 ) )
      {
          char *buf = malloc( strlen(answer) );
          sscanf( answer, "%*s %s", buf );
          if( rtsp->p_private->session )
          {
              if( strcmp( buf, rtsp->p_private->session ) )
              {
                  //fprintf( stderr,
                  //         "rtsp: warning: setting NEW session: %s\n", buf );
                  free( rtsp->p_private->session );
                  rtsp->p_private->session = strdup( buf );
              }
          }
          else
          {
              //fprintf( stderr, "setting session id to: %s\n", buf );
              rtsp->p_private->session = strdup( buf );
          }
          free( buf );
      }

      *answer_ptr = answer;
      answer_ptr++;
    } while( (strlen(answer) != 0) && (++ans_count < MAX_FIELDS) );

    rtsp->p_private->cseq++;

    *answer_ptr = NULL;
    rtsp_schedule_standard( rtsp );

    return code;
}

/*
 * send an ok message
 */

int rtsp_send_ok( rtsp_client_t *rtsp )
{
    char cseq[16];

    rtsp_put( rtsp, "RTSP/1.0 200 OK" );
    sprintf( cseq, "CSeq: %u", rtsp->p_private->cseq );
    rtsp_put( rtsp, cseq );
    rtsp_put( rtsp, "" );
    return 0;
}

/*
 * implementation of must-have rtsp requests; functions return
 * server status code.
 */

int rtsp_request_options( rtsp_client_t *rtsp, const char *what )
{
    char *buf;

    if( what ) buf = strdup(what);
    else
    {
        buf = malloc( strlen(rtsp->p_private->host) + 16 );
        sprintf( buf, "rtsp://%s:%i", rtsp->p_private->host,
                 rtsp->p_private->port );
    }
    rtsp_send_request( rtsp, "OPTIONS", buf );
    free( buf );

    return rtsp_get_answers( rtsp );
}

int rtsp_request_describe( rtsp_client_t *rtsp, const char *what )
{
    char *buf;

    if( what )
    {
        buf = strdup(what);
    }
    else
    {
        buf = malloc( strlen(rtsp->p_private->host) +
                      strlen(rtsp->p_private->path) + 16 );
        sprintf( buf, "rtsp://%s:%i/%s", rtsp->p_private->host,
                 rtsp->p_private->port, rtsp->p_private->path );
    }
    rtsp_send_request( rtsp, "DESCRIBE", buf );
    free( buf );

    return rtsp_get_answers( rtsp );
}

int rtsp_request_setup( rtsp_client_t *rtsp, const char *what )
{
    rtsp_send_request( rtsp, "SETUP", what );
    return rtsp_get_answers( rtsp );
}

int rtsp_request_setparameter( rtsp_client_t *rtsp, const char *what )
{
    char *buf;

    if( what )
    {
        buf = strdup(what);
    }
    else
    {
        buf = malloc( strlen(rtsp->p_private->host) +
                      strlen(rtsp->p_private->path) + 16 );
        sprintf( buf, "rtsp://%s:%i/%s", rtsp->p_private->host,
                 rtsp->p_private->port, rtsp->p_private->path );
    }

    rtsp_send_request( rtsp, "SET_PARAMETER", buf );
    free( buf );

    return rtsp_get_answers( rtsp );
}

int rtsp_request_play( rtsp_client_t *rtsp, const char *what )
{
    char *buf;

    if( what )
    {
        buf = strdup( what );
    }
    else
    {
        buf = malloc( strlen(rtsp->p_private->host) +
                      strlen(rtsp->p_private->path) + 16 );
        sprintf( buf, "rtsp://%s:%i/%s", rtsp->p_private->host,
                 rtsp->p_private->port, rtsp->p_private->path );
    }

    rtsp_send_request( rtsp, "PLAY", buf );
    free( buf );

    return rtsp_get_answers( rtsp );
}

int rtsp_request_tearoff( rtsp_client_t *rtsp, const char *what )
{
    rtsp_send_request( rtsp, "TEAROFF", what );
    return rtsp_get_answers( rtsp );
}

/*
 * read opaque data from stream
 */

int rtsp_read_data( rtsp_client_t *rtsp, uint8_t *buffer, unsigned int size )
{
    int i, seq;

    if( size >= 4 )
    {
        i = rtsp->pf_read( rtsp->p_userdata, (uint8_t*)buffer, (unsigned int) 4 );
        if( i < 4 ) return i;

        if( buffer[0]=='S' && buffer[1]=='E' && buffer[2]=='T' &&
            buffer[3]=='_' )
        {
            char *rest = rtsp_get( rtsp );
            if( !rest ) return -1;

            seq = -1;
            do
            {
                free( rest );
                rest = rtsp_get( rtsp );
                if( !rest ) return -1;

                if( !strncasecmp( rest, "Cseq:", 5 ) )
                    sscanf( rest, "%*s %u", &seq );
            } while( *rest );
            free( rest );

            if( seq < 0 )
            {
                //fprintf(stderr, "warning: cseq not recognized!\n");
                seq = 1;
            }

            /* lets make the server happy */
            rtsp_put( rtsp, "RTSP/1.0 451 Parameter Not Understood" );
            rest = malloc(17);
            sprintf( rest,"CSeq: %u", seq );
            rtsp_put( rtsp, rest );
            rtsp_put( rtsp, "" );
            free( rest );
            i = rtsp->pf_read( rtsp->p_userdata, (unsigned char*)buffer, size );
        }
        else
        {
            i = rtsp->pf_read( rtsp->p_userdata, (unsigned char*)buffer + 4, size - 4 );
            i += 4;
        }
    }
    else i = rtsp->pf_read( rtsp->p_userdata, (unsigned char*)buffer, size );

    //fprintf( stderr, "<< %d of %d bytes\n", i, size );

    return i;
}

/*
 * connect to a rtsp server
 */

int rtsp_connect( rtsp_client_t *rtsp, const char *psz_mrl,
                  const char *psz_user_agent )
{
    rtsp_t *s;
    char *mrl_ptr;
    char *slash, *colon;
    unsigned int hostend, pathbegin, i;

    if( !psz_mrl ) return -1;
    s = malloc( sizeof(rtsp_t) );
    rtsp->p_private = s;

    if( !strncmp( psz_mrl, "rtsp://", 7 ) ) psz_mrl += 7;
    mrl_ptr = strdup( psz_mrl );

    for( i=0; i<MAX_FIELDS; i++ )
    {
        s->answers[i]=NULL;
        s->scheduled[i]=NULL;
    }

    s->host = NULL;
    s->port = 554; /* rtsp standard port */
    s->path = NULL;
    s->mrl  = strdup(psz_mrl);

    s->server = NULL;
    s->server_state = 0;
    s->server_caps = 0;

    s->cseq = 0;
    s->session = NULL;

    if( psz_user_agent ) s->user_agent = strdup( psz_user_agent );
    else s->user_agent = strdup( "User-Agent: RealMedia Player Version "
                                 "6.0.9.1235 (linux-2.0-libc6-i386-gcc2.95)" );

    slash = strchr( mrl_ptr, '/' );
    colon = strchr( mrl_ptr, ':' );

    if( !slash ) slash = mrl_ptr + strlen(mrl_ptr) + 1;
    if( !colon ) colon = slash;
    if( colon > slash ) colon = slash;

    pathbegin = slash - mrl_ptr;
    hostend = colon - mrl_ptr;

    s->host = malloc(hostend+1);
    strncpy( s->host, mrl_ptr, hostend );
    s->host[hostend] = 0;

    if( pathbegin < strlen(mrl_ptr) ) s->path = strdup(mrl_ptr+pathbegin+1);
    if( colon != slash )
    {
        char buffer[pathbegin-hostend];

        strncpy( buffer, mrl_ptr+hostend+1, pathbegin-hostend-1 );
        buffer[pathbegin-hostend-1] = 0;
        s->port = atoi(buffer);
        if( s->port < 0 || s->port > 65535 ) s->port = 554;
    }

    free( mrl_ptr );
    //fprintf( stderr, "got mrl: %s %i %s\n", s->host, s->port, s->path );

    s->s = rtsp->pf_connect( rtsp->p_userdata, s->host, s->port );

    if( s->s < 0 )
    {
        //fprintf(stderr, "rtsp: failed to connect to '%s'\n", s->host);
        rtsp_close( rtsp );
        return -1;
    }

    s->server_state = RTSP_CONNECTED;

    /* now lets send an options request. */
    rtsp_schedule_field( rtsp, "CSeq: 1");
    rtsp_schedule_field( rtsp, s->user_agent);
    rtsp_schedule_field( rtsp, "ClientChallenge: "
                               "9e26d33f2984236010ef6253fb1887f7");
    rtsp_schedule_field( rtsp, "PlayerStarttime: [28/03/2003:22:50:23 00:00]");
    rtsp_schedule_field( rtsp, "CompanyID: KnKV4M4I/B2FjJ1TToLycw==" );
    rtsp_schedule_field( rtsp, "GUID: 00000000-0000-0000-0000-000000000000" );
    rtsp_schedule_field( rtsp, "RegionData: 0" );
    rtsp_schedule_field( rtsp, "ClientID: "
                               "Linux_2.4_6.0.9.1235_play32_RN01_EN_586" );
    /*rtsp_schedule_field( rtsp, "Pragma: initiate-session" );*/
    rtsp_request_options( rtsp, NULL );

    return 0;
}

/*
 * closes an rtsp connection
 */

void rtsp_close( rtsp_client_t *rtsp )
{
    if( rtsp->p_private->server_state )
    {
        /* TODO: send a TEAROFF */
        rtsp->pf_disconnect( rtsp->p_userdata );
    }

    free( rtsp->p_private->path );
    free( rtsp->p_private->host );
    free( rtsp->p_private->mrl );
    free( rtsp->p_private->session );
    free( rtsp->p_private->user_agent );
    free( rtsp->p_private->server );
    rtsp_free_answers( rtsp );
    rtsp_unschedule_all( rtsp );
    free( rtsp->p_private );
}

/*
 * search in answers for tags. returns a pointer to the content
 * after the first matched tag. returns NULL if no match found.
 */

char *rtsp_search_answers( rtsp_client_t *rtsp, const char *tag )
{
    char **answer;
    char *ptr;

    if( !rtsp->p_private->answers ) return NULL;
    answer = rtsp->p_private->answers;

    while(*answer)
    {
        if( !strncasecmp( *answer, tag, strlen(tag) ) )
        {
            ptr = strchr(*answer, ':');
            ptr++;
            while( *ptr == ' ' ) ptr++;
            return ptr;
        }
        answer++;
    }

    return NULL;
}

/*
 * session id management
 */

void rtsp_set_session( rtsp_client_t *rtsp, const char *id )
{
    free( rtsp->p_private->session );
    rtsp->p_private->session = strdup(id);
}

char *rtsp_get_session( rtsp_client_t *rtsp )
{
    return rtsp->p_private->session;
}

char *rtsp_get_mrl( rtsp_client_t *rtsp )
{
    return rtsp->p_private->mrl;
}

/*
 * schedules a field for transmission
 */

void rtsp_schedule_field( rtsp_client_t *rtsp, const char *string )
{
    int i = 0;

    if( !string ) return;

    while( rtsp->p_private->scheduled[i] ) i++;

    rtsp->p_private->scheduled[i] = strdup(string);
}

/*
 * removes the first scheduled field which prefix matches string.
 */

void rtsp_unschedule_field( rtsp_client_t *rtsp, const char *string )
{
    char **ptr = rtsp->p_private->scheduled;

    if( !string ) return;

    while( *ptr )
    {
      if( !strncmp(*ptr, string, strlen(string)) ) break;
    }
    free( *ptr );
    ptr++;
    do
    {
        *(ptr-1) = *ptr;
    } while( *ptr );
}

/*
 * unschedule all fields
 */

void rtsp_unschedule_all( rtsp_client_t *rtsp )
{
    char **ptr;

    if( !rtsp->p_private->scheduled ) return;
    ptr = rtsp->p_private->scheduled;

    while( *ptr )
    {
        free( *ptr );
        *ptr = NULL;
        ptr++;
    }
}
/*
 * free answers
 */

void rtsp_free_answers( rtsp_client_t *rtsp )
{
    char **answer;

    if( !rtsp->p_private->answers ) return;
    answer = rtsp->p_private->answers;

    while( *answer )
    {
        free( *answer );
        *answer = NULL;
        answer++;
    }
}
