/*****************************************************************************
 * demux.c
 *****************************************************************************
 * Copyright (C) 1999-2003 VideoLAN
 * $Id: demux.c,v 1.1 2003/08/02 16:43:59 fenrir Exp $
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

#include <stdlib.h>
#include <vlc/vlc.h>
#include <vlc/input.h>

#include "ninput.h"

int  demux_vaControl( input_thread_t *p_input, int i_query, va_list args )
{
    if( p_input->pf_demux_control )
    {
        return p_input->pf_demux_control( p_input, i_query, args );
    }
    return VLC_EGENERIC;
}

int  demux_Control  ( input_thread_t *p_input, int i_query, ...  )
{
    va_list args;
    int     i_result;

    va_start( args, i_query );
    i_result = demux_vaControl( p_input, i_query, args );
    va_end( args );

    return i_result;
}

