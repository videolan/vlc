/*****************************************************************************
 * input_ctrl.h: Decodeur control
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
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
 * Requires:
 *  "config.h"
 *  "common.h"
 *  "mtime.h"
 *****************************************************************************/






/*****************************************************************************
 * Prototypes
 *****************************************************************************/
int input_AddPgrmElem( input_thread_t *p_input, int i_current_pid );
int input_DelPgrmElem( input_thread_t *p_input, int i_current_pid );
boolean_t input_IsElemRecv( input_thread_t *p_input, int i_pid );
