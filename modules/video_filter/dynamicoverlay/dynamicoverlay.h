/*****************************************************************************
 * dynamicoverlay.h : dynamic overlay plugin for vlc
 *****************************************************************************
 * Copyright (C) 2008 the VideoLAN team
 * $Id$
 *
 * Author: Jean-Paul Saman <jpsaman@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef DYNAMIC_OVERLAY_H
#define DYNAMIC_OVERLAY_H   1

/*****************************************************************************
 * buffer_t: Command and response buffer
 *****************************************************************************/

typedef struct buffer_t
{
    size_t i_size;                         /**< Size of the allocated memory */
    size_t i_length;                          /**< Length of the stored data */

    char *p_memory;                       /**< Start of the allocated memory */
    char *p_begin;                             /**< Start of the stored data */
} buffer_t;

int BufferInit( buffer_t *p_buffer );
int BufferDestroy( buffer_t *p_buffer );
int BufferAdd( buffer_t *p_buffer, const char *p_data, size_t i_len );
int BufferPrintf( buffer_t *p_buffer, const char *p_fmt, ... );
int BufferDel( buffer_t *p_buffer, int i_len );

/*****************************************************************************
 * Command structures
 *****************************************************************************/

 #define INT( name ) int name;
#define CHARS( name, count ) char name[count];
#define COMMAND( name, param, ret, atomic, code ) \
struct commandparams##name##_t \
{ \
    param \
};
#include "dynamicoverlay_commands.h"
#undef COMMAND
#undef INT
#undef CHARS

union commandparams_t
{
#define COMMAND( name, param, ret, atomic, code ) struct commandparams##name##_t name;
#include "dynamicoverlay_commands.h"
#undef COMMAND
};
typedef union commandparams_t commandparams_t;

#define INT( name ) int name;
#define CHARS( name, count ) char name[count];
#define COMMAND( name, param, ret, atomic, code ) \
struct commandresults##name##_t \
{ \
    ret \
};
#include "dynamicoverlay_commands.h"
#undef COMMAND
#undef INT
#undef CHARS

union commandresults_t {
#define COMMAND( name, param, ret, atomic, code ) struct commandresults##name##_t name;
#include "dynamicoverlay_commands.h"
#undef COMMAND
};
typedef union commandresults_t commandresults_t;

typedef struct commanddesc_t
{
    const char *psz_command;
    vlc_bool_t b_atomic;
    int ( *pf_parser ) ( const char *psz_command, const char *psz_end,
                         commandparams_t *p_params );
    int ( *pf_execute ) ( filter_t *p_filter, const commandparams_t *p_params,
                          commandresults_t *p_results,
                          struct filter_sys_t *p_sys );
    int ( *pf_unparser ) ( const commandresults_t *p_results,
                           buffer_t *p_output );
} commanddesc_t;

typedef struct command_t
{
    struct commanddesc_t *p_command;
    int i_status;
    commandparams_t params;
    commandresults_t results;

    struct command_t *p_next;
} command_t;

/*****************************************************************************
 * queue_t: Command queue
 *****************************************************************************/

typedef struct queue_t
{
    command_t *p_head;                  /**< Head (first entry) of the queue */
    command_t *p_tail;                   /**< Tail (last entry) of the queue */
} queue_t;

int QueueInit( queue_t *p_queue );
int QueueDestroy( queue_t *p_queue );
int QueueEnqueue( queue_t *p_queue, command_t *p_cmd );
command_t *QueueDequeue( queue_t *p_queue );
int QueueTransfer( queue_t *p_sink, queue_t *p_source );

#endif
