/*****************************************************************************
 * dynamicoverlay.h : dynamic overlay plugin for vlc
 *****************************************************************************
 * Copyright (C) 2008 VLC authors and VideoLAN
 *
 * Author: Jean-Paul Saman <jpsaman@videolan.org>
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

#ifndef DYNAMIC_OVERLAY_H
#define DYNAMIC_OVERLAY_H   1

#include <vlc_common.h>
#include <vlc_filter.h>
#include <vlc_text_style.h>

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
char *BufferGetToken( buffer_t *p_buffer );

/*****************************************************************************
 * Command structures
 *****************************************************************************/

/** struct commandparams_t - command params structure */
typedef struct commandparams_t
{
    int32_t i_id;       /*< overlay id */
    int32_t i_shmid;    /*< shared memory identifier */

    vlc_fourcc_t fourcc;/*< chroma */

    int32_t i_x;        /*< x position of overlay */
    int32_t i_y;        /*< y position of overlay */
    int32_t i_width;    /*< width of overlay */
    int32_t i_height;   /*< height of overlay */

    int32_t i_alpha;    /*< alpha value of overlay */

    text_style_t fontstyle; /*< text style */

    bool b_visible; /*< visibility flag of overlay */
} commandparams_t;

typedef int (*parser_func_t)(char *psz_command, char *psz_end, commandparams_t *p_params );
typedef int (*execute_func_t)( filter_t *p_filter, const commandparams_t *p_params, commandparams_t *p_results );
typedef int (*unparse_func_t)( const commandparams_t *p_results, buffer_t *p_output );

typedef struct commanddesc_t
{
    char *psz_command;
    bool b_atomic;
    parser_func_t pf_parser;
    execute_func_t pf_execute;
    unparse_func_t pf_unparse;
} commanddesc_t;

typedef struct commanddesc_static_t
{
    const char *psz_command;
    bool b_atomic;
    parser_func_t pf_parser;
    execute_func_t pf_execute;
    unparse_func_t pf_unparse;
} commanddesc_static_t;


typedef struct command_t
{
    struct commanddesc_t *p_command;
    int i_status;
    commandparams_t params;
    commandparams_t results;
    struct command_t *p_next;
} command_t;

void RegisterCommand( filter_t *p_filter );
void UnregisterCommand( filter_t *p_filter );

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

/*****************************************************************************
 * overlay_t: Overlay descriptor
 *****************************************************************************/

typedef struct overlay_t
{
    int i_x, i_y;
    int i_alpha;
    bool b_active;

    video_format_t format;
    text_style_t *p_fontstyle;
    union {
        picture_t *p_pic;
        char *p_text;
    } data;
} overlay_t;

overlay_t *OverlayCreate( void );
int OverlayDestroy( overlay_t *p_ovl );

/*****************************************************************************
 * list_t: Command queue
 *****************************************************************************/

typedef struct list_t
{
    overlay_t **pp_head, **pp_tail;
} list_t;

int do_ListInit( list_t *p_list );
int do_ListDestroy( list_t *p_list );
ssize_t ListAdd( list_t *p_list, overlay_t *p_new );
int ListRemove( list_t *p_list, size_t i_idx );
overlay_t *ListGet( list_t *p_list, size_t i_idx );
overlay_t *ListWalk( list_t *p_list );

/*****************************************************************************
 * filter_sys_t: adjust filter method descriptor
 *****************************************************************************/

typedef struct
{
    buffer_t input, output;

    int i_inputfd, i_outputfd;
    char *psz_inputfile, *psz_outputfile;

    commanddesc_t **pp_commands; /* array of commands */
    size_t i_commands;

    bool b_updated, b_atomic;
    queue_t atomic, pending, processed;
    list_t overlays;

    vlc_mutex_t lock;   /* lock to protect psz_inputfile and psz_outputfile */
} filter_sys_t;

#endif
