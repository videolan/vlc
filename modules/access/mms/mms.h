/*****************************************************************************
 * mms.h: MMS access plug-in
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: mms.h,v 1.10 2003/08/17 23:02:51 fenrir Exp $
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

#define MMS_PROTO_AUTO  0
#define MMS_PROTO_TCP   1
#define MMS_PROTO_UDP   2
#define MMS_PROTO_HTTP  3


/* mmst and mmsu */
int  E_( MMSTUOpen )  ( input_thread_t * );
void E_( MMSTUClose ) ( input_thread_t * );

/* mmsh */
int  E_( MMSHOpen )  ( input_thread_t * );
void E_( MMSHClose ) ( input_thread_t * );

#define FREE( p ) if( p ) { free( p ); (p) = NULL; }

/* url: [/]host[:port][/path][@username[:password]] */
typedef struct url_s
{
    char    *psz_host;
    int     i_port;

    char    *psz_path;

    char    *psz_username;
    char    *psz_password;
} url_t;

url_t *E_( url_new )  ( char * );
void   E_( url_free ) ( url_t * );

