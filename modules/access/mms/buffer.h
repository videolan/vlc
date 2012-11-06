/*****************************************************************************
 * buffer.h: MMS access plug-in
 *****************************************************************************
 * Copyright (C) 2001, 2002 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef _MMS_BUFFER_H_
#define _MMS_BUFFER_H_ 1

typedef struct
{
    uint8_t *p_data;    // pointer on data
    int     i_data;     // number of bytes set in p_data

    // private
    int    i_size;     // size of p_data memory allocated
} var_buffer_t;

/*****************************************************************************
 * Macro/Function to create/manipulate buffer
 *****************************************************************************/
int  var_buffer_initwrite( var_buffer_t *p_buf, int i_default_size );
int  var_buffer_reinitwrite( var_buffer_t *p_buf, int i_default_size );
void var_buffer_add8 ( var_buffer_t *p_buf, uint8_t  i_byte );
void var_buffer_add16( var_buffer_t *p_buf, uint16_t i_word );
void var_buffer_add32( var_buffer_t *p_buf, uint32_t i_word );
void var_buffer_add64( var_buffer_t *p_buf, uint64_t i_word );
void var_buffer_addmemory( var_buffer_t *p_buf, void *p_mem, int i_mem );
void var_buffer_addUTF16( access_t  *p_access, var_buffer_t *p_buf, const char *p_str );
void var_buffer_free( var_buffer_t *p_buf );


void      var_buffer_initread( var_buffer_t *p_buf, void *p_data, int i_data );
uint8_t   var_buffer_get8 ( var_buffer_t *p_buf );
uint16_t  var_buffer_get16( var_buffer_t *p_buf );
uint32_t  var_buffer_get32( var_buffer_t *p_buf );
uint64_t  var_buffer_get64( var_buffer_t *p_buf );
int       var_buffer_getmemory ( var_buffer_t *p_buf, void *p_mem, int64_t i_mem );
int       var_buffer_readempty( var_buffer_t *p_buf );
void      var_buffer_getguid( var_buffer_t *p_buf, guid_t *p_guid );

#endif
