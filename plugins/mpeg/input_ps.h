/*****************************************************************************
 * input_ps.h: thread structure of the PS plugin
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: input_ps.h,v 1.9 2001/10/02 16:46:59 massiot Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Cyril Deguet <asmax@via.ecp.fr>
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

#define DATA_CACHE_SIZE 150
#define PES_CACHE_SIZE 150
#define SMALL_CACHE_SIZE 150
#define LARGE_CACHE_SIZE 150
#define MAX_SMALL_SIZE 50     // frontier between small and large packets

typedef struct
{
    data_packet_t **        p_stack;
    long                    l_index;
} data_packet_cache_t;


typedef struct
{
    pes_packet_t **         p_stack;
    long                    l_index;
} pes_packet_cache_t;


typedef struct
{
    byte_t *                p_data;
    long                    l_size;
} packet_buffer_t;


typedef struct
{
    packet_buffer_t *       p_stack;
    long                    l_index;
} small_buffer_cache_t;


typedef struct
{
    packet_buffer_t *       p_stack;
    long                    l_index;
} large_buffer_cache_t;


typedef struct
{
    vlc_mutex_t             lock;
    data_packet_cache_t     data;
    pes_packet_cache_t      pes;
    small_buffer_cache_t    smallbuffer;
    large_buffer_cache_t    largebuffer;
} packet_cache_t;


