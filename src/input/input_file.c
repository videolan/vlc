/*****************************************************************************
 * input_file.c: functions to read from a file
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <sys/types.h>                        /* on BSD, uio.h needs types.h */
#include <sys/uio.h>                                            /* "input.h" */

#include "threads.h"
#include "common.h"
#include "config.h"
#include "mtime.h"

#include "input.h"
#include "input_file.h"

/*****************************************************************************
 * input_FileOpen : open a file descriptor
 *****************************************************************************/
int input_FileOpen( input_thread_t *p_input )
{
    /* XXX?? */
    return( 1 );
}

/*****************************************************************************
 * input_FileRead : read from a file
 *****************************************************************************/
int input_FileRead( input_thread_t *p_input, const struct iovec *p_vector,
                    size_t i_count )
{
    /* XXX?? */
    return( -1 );
}

/*****************************************************************************
 * input_FileClose : close a file descriptor
 *****************************************************************************/
void input_FileClose( input_thread_t *p_input )
{
    /* XXX?? */
}
