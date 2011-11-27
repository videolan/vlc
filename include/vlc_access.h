/*****************************************************************************
 * vlc_access.h: Access descriptor, queries and methods
 *****************************************************************************
 * Copyright (C) 1999-2006 VLC authors and VideoLAN
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

#ifndef VLC_ACCESS_H
#define VLC_ACCESS_H 1

/**
 * \file
 * This file defines functions and definitions for access object
 */

#include <vlc_block.h>

/**
 * \defgroup access Access
 * @{
 */

enum access_query_e
{
    /* capabilities */
    ACCESS_CAN_SEEK,        /* arg1= bool*    cannot fail */
    ACCESS_CAN_FASTSEEK,    /* arg1= bool*    cannot fail */
    ACCESS_CAN_PAUSE,       /* arg1= bool*    cannot fail */
    ACCESS_CAN_CONTROL_PACE,/* arg1= bool*    cannot fail */

    /* */
    ACCESS_GET_PTS_DELAY = 0x101,/* arg1= int64_t*       cannot fail */
    /* */
    ACCESS_GET_TITLE_INFO,  /* arg1=input_title_t*** arg2=int*      res=can fail */
    /* Meta data */
    ACCESS_GET_META,        /* arg1= vlc_meta_t **                  res=can fail */

    /* */
    ACCESS_GET_CONTENT_TYPE,/* arg1=char **ppsz_content_type                       res=can fail */

    /* */
    ACCESS_GET_SIGNAL,      /* arg1=double *pf_quality, arg2=double *pf_strength   res=can fail */

    /* */
    ACCESS_SET_PAUSE_STATE = 0x200, /* arg1= bool           can fail */

    /* */
    ACCESS_SET_TITLE,       /* arg1= int            can fail */
    ACCESS_SET_SEEKPOINT,   /* arg1= int            can fail */

    /* Special mode for access/demux communication
     * XXX: avoid to use it unless you can't */
    ACCESS_SET_PRIVATE_ID_STATE = 0x1000, /* arg1= int i_private_data, bool b_selected    res=can fail */
    ACCESS_SET_PRIVATE_ID_CA,             /* arg1= int i_program_number, uint16_t i_vpid, uint16_t i_apid1, uint16_t i_apid2, uint16_t i_apid3, uint8_t i_length, uint8_t *p_data */
    ACCESS_GET_PRIVATE_ID_STATE,          /* arg1=int i_private_data arg2=bool *          res=can fail */
};

struct access_t
{
    VLC_COMMON_MEMBERS

    /* Module properties */
    module_t    *p_module;

    /* Access name (empty if non forced) */
    char        *psz_access;
    char        *psz_location; /**< Location (URL with the scheme stripped) */
    char        *psz_filepath; /**< Local file path (if applicable) */

    /* Access can fill this entry to force a demuxer
     * XXX: fill it once you know for sure you will succeed
     * (if you fail, this value won't be reseted */
    char        *psz_demux;

    /* pf_read/pf_block is used to read data.
     * XXX A access should set one and only one of them */
    ssize_t     (*pf_read) ( access_t *, uint8_t *, size_t );  /* Return -1 if no data yet, 0 if no more data, else real data read */
    block_t    *(*pf_block)( access_t * );                  /* return a block of data in his 'natural' size, NULL if not yet data or eof */

    /* Called for each seek.
     * XXX can be null */
    int         (*pf_seek) ( access_t *, uint64_t );         /* can be null if can't seek */

    /* Used to retreive and configure the access
     * XXX mandatory. look at access_query_e to know what query you *have to* support */
    int         (*pf_control)( access_t *, int i_query, va_list args);

    /* Access has to maintain them uptodate */
    struct
    {
        unsigned int i_update;  /* Access sets them on change,
                                   Input removes them once take into account*/

        uint64_t     i_size;    /* Write only for access, read only for input */
        uint64_t     i_pos;     /* idem */
        bool         b_eof;     /* idem */

        int          i_title;    /* idem, start from 0 (could be menu) */
        int          i_seekpoint;/* idem, start from 0 */
    } info;
    access_sys_t *p_sys;

    /* Weak link to parent input */
    input_thread_t *p_input;
};

static inline int access_vaControl( access_t *p_access, int i_query, va_list args )
{
    if( !p_access ) return VLC_EGENERIC;
    return p_access->pf_control( p_access, i_query, args );
}

static inline int access_Control( access_t *p_access, int i_query, ... )
{
    va_list args;
    int     i_result;

    va_start( args, i_query );
    i_result = access_vaControl( p_access, i_query, args );
    va_end( args );
    return i_result;
}

static inline void access_InitFields( access_t *p_a )
{
    p_a->info.i_update = 0;
    p_a->info.i_size = 0;
    p_a->info.i_pos = 0;
    p_a->info.b_eof = false;
    p_a->info.i_title = 0;
    p_a->info.i_seekpoint = 0;
}

/**
 * This function will return the parent input of this access.
 * It is retained. It can return NULL.
 */
VLC_API input_thread_t * access_GetParentInput( access_t *p_access ) VLC_USED;

#define ACCESS_SET_CALLBACKS( read, block, control, seek ) \
    do { \
        p_access->pf_read = (read); \
        p_access->pf_block = (block); \
        p_access->pf_control = (control); \
        p_access->pf_seek = (seek); \
    } while(0)

#define STANDARD_READ_ACCESS_INIT \
    do { \
        access_InitFields( p_access ); \
        ACCESS_SET_CALLBACKS( Read, NULL, Control, Seek ); \
        p_sys = p_access->p_sys = calloc( 1, sizeof( access_sys_t ) ); \
        if( !p_sys ) return VLC_ENOMEM;\
    } while(0);

#define STANDARD_BLOCK_ACCESS_INIT \
    do { \
        access_InitFields( p_access ); \
        ACCESS_SET_CALLBACKS( NULL, Block, Control, Seek ); \
        p_sys = p_access->p_sys = calloc( 1, sizeof( access_sys_t ) ); \
        if( !p_sys ) return VLC_ENOMEM; \
    } while(0);

/**
 * @}
 */

#endif
