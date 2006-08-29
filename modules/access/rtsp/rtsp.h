/*
 * Copyright (C) 2002-2003 the xine project
 *
 * This file is part of xine, a free video player.
 *
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id$
 *
 * a minimalistic implementation of rtsp protocol,
 * *not* RFC 2326 compilant yet.
 */
 
#ifndef HAVE_RTSP_H
#define HAVE_RTSP_H

/* some codes returned by rtsp_request_* functions */

#define RTSP_STATUS_SET_PARAMETER  10
#define RTSP_STATUS_OK            200

typedef struct rtsp_s rtsp_t;

typedef struct
{
    void *p_userdata;

    int (*pf_connect)( void *p_userdata, char *p_server, int i_port );
    int (*pf_disconnect)( void *p_userdata );
    int (*pf_read)( void *p_userdata, uint8_t *p_buffer, int i_buffer );
    int (*pf_read_line)( void *p_userdata, uint8_t *p_buffer, int i_buffer );
    int (*pf_write)( void *p_userdata, uint8_t *p_buffer, int i_buffer );

    rtsp_t *p_private;

} rtsp_client_t;

int rtsp_connect( rtsp_client_t *, const char *mrl, const char *user_agent );

int rtsp_request_options( rtsp_client_t *, const char *what );
int rtsp_request_describe( rtsp_client_t *, const char *what );
int rtsp_request_setup( rtsp_client_t *, const char *what );
int rtsp_request_setparameter( rtsp_client_t *, const char *what );
int rtsp_request_play( rtsp_client_t *, const char *what );
int rtsp_request_tearoff( rtsp_client_t *, const char *what );

int rtsp_send_ok( rtsp_client_t * );

int rtsp_read_data( rtsp_client_t *, uint8_t *buffer, unsigned int size );

char* rtsp_search_answers( rtsp_client_t *, const char *tag );
void rtsp_free_answers( rtsp_client_t * );

void rtsp_add_to_payload( char **payload, const char *string );

int rtsp_read( rtsp_client_t *, char *data, int len );
void rtsp_close( rtsp_client_t * );

void  rtsp_set_session( rtsp_client_t *, const char *id );
char *rtsp_get_session( rtsp_client_t * );

char *rtsp_get_mrl( rtsp_client_t * );

/* int rtsp_peek_header( rtsp_client_t *, char *data ); */

void rtsp_schedule_field( rtsp_client_t *, const char *string );
void rtsp_unschedule_field( rtsp_client_t *, const char *string );
void rtsp_unschedule_all( rtsp_client_t * );

#endif

