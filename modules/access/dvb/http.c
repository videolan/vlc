/*****************************************************************************
 * http.c: HTTP interface
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/


/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef ENABLE_HTTPD
#include <vlc_common.h>
#include <vlc_access.h>
#include <vlc_httpd.h>

#include <sys/types.h>

/* Include dvbpsi headers */
# include <dvbpsi/dvbpsi.h>
# include <dvbpsi/descriptor.h>
# include <dvbpsi/pat.h>
# include <dvbpsi/pmt.h>
# include <dvbpsi/dr.h>
# include <dvbpsi/psi.h>
# include <dvbpsi/demux.h>
# include <dvbpsi/sdt.h>

#include "dvb.h"

struct httpd_file_sys_t
{
    access_t         *p_access;
    httpd_file_t     *p_file;
};

static int HttpCallback( httpd_file_sys_t *p_args,
                         httpd_file_t *p_file,
                         uint8_t *_p_request,
                         uint8_t **_pp_data, int *pi_data );

/*****************************************************************************
 * HTTPOpen: Start the internal HTTP server
 *****************************************************************************/
int HTTPOpen( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;
    char          *psz_address, *psz_user = NULL, *psz_password = NULL;
    int           i_port       = 0;
    char          psz_tmp[10];
    httpd_file_sys_t *f;

    vlc_mutex_init( &p_sys->httpd_mutex );
    vlc_cond_init( &p_sys->httpd_cond );
    p_sys->b_request_frontend_info = p_sys->b_request_mmi_info = false;
    p_sys->i_httpd_timeout = 0;

    psz_address = var_GetNonEmptyString( p_access, "dvb-http-host" );
    if( psz_address != NULL )
    {
        char *psz_parser = strchr( psz_address, ':' );
        if( psz_parser )
        {
            *psz_parser++ = '\0';
            i_port = atoi( psz_parser );
        }
    }
    else
        return VLC_SUCCESS;

    if ( i_port <= 0 )
        i_port= 8082;

    /* Ugly hack to allow to run several HTTP servers on different ports. */
    sprintf( psz_tmp, ":%d", i_port + 1 );
    config_PutPsz( p_access, "dvb-http-host", psz_tmp );

    msg_Dbg( p_access, "base %d", i_port );

    p_sys->p_httpd_host = vlc_http_HostNew( VLC_OBJECT(p_access), i_port );
    if ( p_sys->p_httpd_host == NULL )
    {
        msg_Err( p_access, "cannot listen on port %d", i_port );
        free( psz_address );
        return VLC_EGENERIC;
    }
    free( psz_address );

    psz_user = var_GetNonEmptyString( p_access, "dvb-http-user" );
    psz_password = var_GetNonEmptyString( p_access, "dvb-http-password" );

    /* Declare an index.html file. */
    f = malloc( sizeof(httpd_file_sys_t) );
    f->p_access = p_access;
    f->p_file = httpd_FileNew( p_sys->p_httpd_host, "/index.html",
                               "text/html; charset=UTF-8",
                               psz_user, psz_password, NULL,
                               HttpCallback, f );

    free( psz_user );
    free( psz_password );

    if ( f->p_file == NULL )
    {
        free( f );
        p_sys->p_httpd_file = NULL;
        return VLC_EGENERIC;
    }

    p_sys->p_httpd_file = f;
    p_sys->p_httpd_redir = httpd_RedirectNew( p_sys->p_httpd_host,
                                              "/index.html", "/" );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * HTTPClose: Stop the internal HTTP server
 *****************************************************************************/
void HTTPClose( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;

    if ( p_sys->p_httpd_host != NULL )
    {
        if ( p_sys->p_httpd_file != NULL )
        {
            /* Unlock the thread if it is stuck in HttpCallback */
            vlc_mutex_lock( &p_sys->httpd_mutex );
            if ( p_sys->b_request_frontend_info )
            {
                p_sys->b_request_frontend_info = false;
                p_sys->psz_frontend_info = strdup("");
            }
            if ( p_sys->b_request_mmi_info )
            {
                p_sys->b_request_mmi_info = false;
                p_sys->psz_mmi_info = strdup("");
            }
            vlc_cond_signal( &p_sys->httpd_cond );
            vlc_mutex_unlock( &p_sys->httpd_mutex );

            httpd_FileDelete( p_sys->p_httpd_file->p_file );
            httpd_RedirectDelete( p_sys->p_httpd_redir );
        }

        httpd_HostDelete( p_sys->p_httpd_host );
    }

    vlc_mutex_destroy( &p_sys->httpd_mutex );
    vlc_cond_destroy( &p_sys->httpd_cond );
}


static const char *psz_constant_header =
    "<html>\n"
    "<head><title>VLC DVB monitoring interface</title></head>\n"
    "<body><a href=\"index.html\">Reload this page</a>\n"
    "<h1>CAM info</h1>\n";

static const char *psz_constant_middle =
    "<hr><h1>Frontend Info</h1>\n";

static const char *psz_constant_footer =
    "</body></html>\n";

/****************************************************************************
 * HttpCallback: Return the index.html file
 ****************************************************************************/
static int HttpCallback( httpd_file_sys_t *p_args,
                         httpd_file_t *p_file,
                         uint8_t *_psz_request,
                         uint8_t **_pp_data, int *pi_data )
{
    VLC_UNUSED(p_file);
    access_sys_t *p_sys = p_args->p_access->p_sys;
    char *psz_request = (char *)_psz_request;
    char **pp_data = (char **)_pp_data;

    vlc_mutex_lock( &p_sys->httpd_mutex );

    p_sys->i_httpd_timeout = mdate() + INT64_C(3000000); /* 3 s */
    p_sys->psz_request = psz_request;
    p_sys->b_request_frontend_info = true;
    if ( p_sys->p_cam != NULL )
    {
        p_sys->b_request_mmi_info = true;
    }
    else
    {
        p_sys->psz_mmi_info = strdup( "No available CAM interface\n" );
    }

    do
    {
        vlc_cond_wait( &p_sys->httpd_cond, &p_sys->httpd_mutex );
    }
    while ( p_sys->b_request_frontend_info || p_sys->b_request_mmi_info );

    p_sys->i_httpd_timeout = 0;
    vlc_mutex_unlock( &p_sys->httpd_mutex );

    *pi_data = strlen( psz_constant_header )
                + strlen( p_sys->psz_mmi_info )
                + strlen( psz_constant_middle )
                + strlen( p_sys->psz_frontend_info )
                + strlen( psz_constant_footer ) + 1;
    *pp_data = malloc( *pi_data );

    sprintf( *pp_data, "%s%s%s%s%s", psz_constant_header,
             p_sys->psz_mmi_info, psz_constant_middle,
             p_sys->psz_frontend_info, psz_constant_footer );
    free( p_sys->psz_frontend_info );
    free( p_sys->psz_mmi_info );

    return VLC_SUCCESS;
}

/****************************************************************************
 * HTTPExtractValue: Extract a GET variable from psz_request
 ****************************************************************************/
const char *HTTPExtractValue( const char *psz_uri, const char *psz_name,
                        char *psz_value, int i_value_max )
{
    const char *p = psz_uri;

    while( (p = strstr( p, psz_name )) )
    {
        /* Verify that we are dealing with a post/get argument */
        if( (p == psz_uri || *(p - 1) == '&' || *(p - 1) == '\n')
              && p[strlen(psz_name)] == '=' )
            break;
        p++;
    }

    if( p )
    {
        int i_len;

        p += strlen( psz_name );
        if( *p == '=' ) p++;

        if( strchr( p, '&' ) )
        {
            i_len = strchr( p, '&' ) - p;
        }
        else
        {
            /* for POST method */
            if( strchr( p, '\n' ) )
            {
                i_len = strchr( p, '\n' ) - p;
                if( i_len && *(p+i_len-1) == '\r' ) i_len--;
            }
            else
            {
                i_len = strlen( p );
            }
        }
        i_len = __MIN( i_value_max - 1, i_len );
        if( i_len > 0 )
        {
            strncpy( psz_value, p, i_len );
            psz_value[i_len] = '\0';
        }
        else
        {
            strncpy( psz_value, "", i_value_max );
        }
        p += i_len;
    }
    else
    {
        strncpy( psz_value, "", i_value_max );
    }

    return p;
}

#endif /* ENABLE_HTTPD */
