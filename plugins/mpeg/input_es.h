/*****************************************************************************
 * input_es.h: thread structure of the ES plugin
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: input_es.h,v 1.1 2001/04/20 15:02:48 sam Exp $
 *
 * Authors: 
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
 * thread_es_data_t: extension of input_thread_t
 *****************************************************************************/
typedef struct thread_es_data_s
{
    /* We're necessarily reading a file. */
    FILE *                  stream;
} thread_es_data_t;

