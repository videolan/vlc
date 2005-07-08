/*****************************************************************************
 * vlc_access.h
 *****************************************************************************
 * Copyright (C) 1999-2004 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
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

#ifndef _VLC_ACCESS_H
#define _VLC_ACCESS_H 1

/**
 * \defgroup access Access
 * @{
 */

enum access_query_e
{
    /* capabilities */
    ACCESS_CAN_SEEK,        /* arg1= vlc_bool_t*    cannot fail */
    ACCESS_CAN_FASTSEEK,    /* arg1= vlc_bool_t*    cannot fail */
    ACCESS_CAN_PAUSE,       /* arg1= vlc_bool_t*    cannot fail */
    ACCESS_CAN_CONTROL_PACE,/* arg1= vlc_bool_t*    cannot fail */

    /* */
    ACCESS_GET_MTU,         /* arg1= int*           cannot fail(0 if no sense)*/
    ACCESS_GET_PTS_DELAY,   /* arg1= int64_t*       cannot fail */
    /* */
    ACCESS_GET_TITLE_INFO,  /* arg1=input_title_t*** arg2=int* can fail */
    /* Meta data */
    ACCESS_GET_META,        /* arg1= vlc_meta_t **  res=can fail    */

    /* */
    ACCESS_SET_PAUSE_STATE, /* arg1= vlc_bool_t     can fail */

    /* */
    ACCESS_SET_TITLE,       /* arg1= int            can fail */
    ACCESS_SET_SEEKPOINT,   /* arg1= int            can fail */

    /* Special mode for access/demux communication
     * XXX: avoid to use it unless you can't */
    ACCESS_SET_PRIVATE_ID_STATE,    /* arg1= int i_private_data, vlc_bool_t b_selected can fail */
    ACCESS_SET_PRIVATE_ID_CA,    /* arg1= int i_program_number, uint16_t i_vpid, uint16_t i_apid1, uint16_t i_apid2, uint16_t i_apid3, uint8_t i_length, uint8_t *p_data */
    ACCESS_GET_PRIVATE_ID_STATE,    /* arg1=int i_private_data arg2=vlc_bool_t *  res=can fail */
};

struct access_t
{
    VLC_COMMON_MEMBERS

    /* Module properties */
    module_t    *p_module;

    /* Access name (empty if non forced) */
    char        *psz_access;
    /* Access path/url/filename/.... */
    char        *psz_path;
    /* Access source for access_filter (NULL for regular access) */
    access_t    *p_source;

    /* Access can fill this entry to force a demuxer
     * XXX: fill it once you know for sure you will succed
     * (if you fail, this value won't be reseted */
    char        *psz_demux;

    /* pf_read/pf_block is used to read data.
     * XXX A access should set one and only one of them */
    int         (*pf_read) ( access_t *, uint8_t *, int );  /* Return -1 if no data yet, 0 if no more data, else real data read */
    block_t    *(*pf_block)( access_t * );                  /* return a block of data in his 'natural' size, NULL if not yet data or eof */

    /* Called for each seek.
     * XXX can be null */
    int         (*pf_seek) ( access_t *, int64_t );         /* can be null if can't seek */

    /* Used to retreive and configure the access
     * XXX mandatory. look at access_query_e to know what query you *have to* support */
    int         (*pf_control)( access_t *, int i_query, va_list args);

    /* Access has to maintain them uptodate */
    struct
    {
        unsigned int i_update;  /* Access sets them on change,
                                   Input removes them once take into account*/

        int64_t      i_size;    /* Write only for access, read only for input */
        int64_t      i_pos;     /* idem */
        vlc_bool_t   b_eof;     /* idem */

        int          i_title;    /* idem, start from 0 (could be menu) */
        int          i_seekpoint;/* idem, start from 0 */
    } info;
    access_sys_t *p_sys;
};

#define access2_New( a, b, c, d, e ) __access2_New(VLC_OBJECT(a), b, c, d, e )
VLC_EXPORT( access_t *, __access2_New,  ( vlc_object_t *p_obj, char *psz_access, char *psz_demux, char *psz_path, vlc_bool_t b_quick ) );
VLC_EXPORT( access_t *, access2_FilterNew, ( access_t *p_source, char *psz_access_filter ) );
VLC_EXPORT( void,      access2_Delete, ( access_t * ) );

static inline int access2_vaControl( access_t *p_access, int i_query, va_list args )
{
    if( !p_access ) return VLC_EGENERIC;
    return p_access->pf_control( p_access, i_query, args );
}
static inline int access2_Control( access_t *p_access, int i_query, ... )
{
    va_list args;
    int     i_result;

    va_start( args, i_query );
    i_result = access2_vaControl( p_access, i_query, args );
    va_end( args );
    return i_result;
}

#endif
