/*****************************************************************************
 * input_ps.h: thread structure of the PS plugin
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: input_ps.h,v 1.2 2001/03/21 13:42:34 sam Exp $
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
 * thread_ps_data_t: extension of input_thread_t
 *****************************************************************************/
typedef struct thread_ps_data_s
{
    /* We're necessarily reading a file. */
    FILE *                  stream;
} thread_ps_data_t;
