/*****************************************************************************
 * httpd.h
 *****************************************************************************
 * Copyright (C) 2001-2003 VideoLAN
 * $Id: httpd.h,v 1.5 2003/06/25 15:50:52 fenrir Exp $
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

typedef struct httpd_t          httpd_t;

typedef struct httpd_host_t     httpd_host_t;

typedef struct httpd_file_t     httpd_file_t;
//typedef struct httpd_stream_t   httpd_stream_t;
typedef httpd_file_t httpd_stream_t;

typedef struct httpd_file_callback_args_t httpd_file_callback_args_t;
typedef int (*httpd_file_callback)( httpd_file_callback_args_t *p_args, uint8_t *p_request, int i_request, uint8_t **pp_data, int *pi_data );

typedef struct httpd_sys_t httpd_sys_t;

struct httpd_t
{
    VLC_COMMON_MEMBERS

    module_t        *p_module;
    httpd_sys_t     *p_sys;

    httpd_host_t   *(*pf_register_host)     ( httpd_t *, char *, int );
    void            (*pf_unregister_host)   ( httpd_t *, httpd_host_t * );

    httpd_file_t   *(*pf_register_file)     ( httpd_t *,
                                              char *psz_file, char *psz_mime,
                                              char *psz_user, char *psz_password,
                                              httpd_file_callback pf_get,
                                              httpd_file_callback pf_post,
                                              httpd_file_callback_args_t *p_args );
    void            (*pf_unregister_file)   ( httpd_t *, httpd_file_t * );

    httpd_stream_t *(*pf_register_stream)   ( httpd_t *,
                                              char *psz_file, char *psz_mime,
                                              char *psz_user, char *psz_password );
    int             (*pf_send_stream)       ( httpd_t *,
                                              httpd_stream_t *,
                                              uint8_t *, int );
    int             (*pf_header_stream)     ( httpd_t *,
                                              httpd_stream_t *,
                                              uint8_t *, int );
    void            (*pf_unregister_stream) ( httpd_t *, httpd_stream_t * );
};

/*
 * httpd_Find:
 *
 *      Return the running httpd instance
 *      (if none and b_create then a new one is created)
 */
static inline httpd_t* httpd_Find( vlc_object_t *p_this, vlc_bool_t b_create )
{
    httpd_t *p_httpd = NULL;

    p_httpd = vlc_object_find( p_this, VLC_OBJECT_HTTPD, FIND_ANYWHERE );
    if( !p_httpd )
    {
        msg_Info(p_this, "creating new http daemon" );
        if( !b_create )
        {
            return( NULL );
        }

        p_httpd = vlc_object_create( p_this, VLC_OBJECT_HTTPD );
        if( !p_httpd )
        {
            msg_Err( p_this, "out of memory" );
            return( NULL );
        }

        p_httpd->p_module = module_Need( p_httpd, "httpd", "" );

        if( !p_httpd->p_module )
        {
            msg_Err( p_this, "no suitable httpd module" );
            vlc_object_destroy( p_httpd );
            return( NULL );
        }

        vlc_object_yield( p_httpd );
        vlc_object_attach( p_httpd, p_this->p_vlc );
    }

    return( p_httpd );
}

static inline void  httpd_Release( httpd_t *p_httpd )
{
    vlc_object_release( p_httpd );

    if( p_httpd->i_refcount <= 0 )
    {
        msg_Info( p_httpd, "destroying unused httpd" );
        vlc_object_detach( p_httpd );
        module_Unneed( p_httpd, p_httpd->p_module );
        vlc_object_destroy( p_httpd );
    }
}

