/*****************************************************************************
 * stream_output.h : stream output module
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: stream_output.h,v 1.1 2002/08/12 22:12:50 massiot Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * sout_instance_t: stream output thread descriptor
 *****************************************************************************/
struct sout_instance_t
{
    VLC_COMMON_MEMBERS

    char * psz_dest;
    char * psz_access;
    char * psz_mux;
    char * psz_name;

    module_t * p_access;
    module_t * p_mux;
};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
#define sout_NewInstance(a,b) __sout_NewInstance(VLC_OBJECT(a),b)
VLC_EXPORT( sout_instance_t *, __sout_NewInstance,  ( vlc_object_t *, char * ) );
VLC_EXPORT( void,              sout_DeleteInstance, ( sout_instance_t * ) );

sout_fifo_t *   sout_CreateFifo     ( void );
void            sout_DestroyFifo    ( sout_fifo_t * );
void            sout_FreeFifo       ( sout_fifo_t * );

