/*****************************************************************************
 * vlc_demux.h
 *****************************************************************************
 * Copyright (C) 1999-2004 the VideoLAN team
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

#ifndef _VLC_DEMUX_H
#define _VLC_DEMUX_H 1

/**
 * \defgroup demux Demux
 * @{
 */

struct demux_t
{
    VLC_COMMON_MEMBERS

    /* Module properties */
    module_t    *p_module;

    /* eg informative but needed (we can have access+demux) */
    char        *psz_access;
    char        *psz_demux;
    char        *psz_path;

    /* input stream */
    stream_t    *s;     /* NULL in case of a access+demux in one */

    /* es output */
    es_out_t    *out;   /* ou p_es_out */

    /* set by demuxer */
    int (*pf_demux)  ( demux_t * );   /* demux one frame only */
    int (*pf_control)( demux_t *, int i_query, va_list args);

    /* Demux has to maintain them uptodate
     * when it is responsible of seekpoint/title */
    struct
    {
        unsigned int i_update;  /* Demux sets them on change,
                                   Input removes them once take into account*/
        /* Seekpoint/Title at demux level */
        int          i_title;       /* idem, start from 0 (could be menu) */
        int          i_seekpoint;   /* idem, start from 0 */
    } info;
    demux_sys_t *p_sys;
};

enum demux_query_e
{
    /* I. Common queries to access_demux and demux */
    /* POSITION double between 0.0 and 1.0 */
    DEMUX_GET_POSITION,         /* arg1= double *       res=    */
    DEMUX_SET_POSITION,         /* arg1= double         res=can fail    */

    /* LENGTH/TIME in microsecond, 0 if unknown */
    DEMUX_GET_LENGTH,           /* arg1= int64_t *      res=    */
    DEMUX_GET_TIME,             /* arg1= int64_t *      res=    */
    DEMUX_SET_TIME,             /* arg1= int64_t        res=can fail    */

    /* TITLE_INFO only if more than 1 title or 1 chapter */
    DEMUX_GET_TITLE_INFO,       /* arg1=input_title_t*** arg2=int* can fail */

    /* TITLE/SEEKPOINT, only when TITLE_INFO succeed */
    DEMUX_SET_TITLE,            /* arg1= int            can fail */
    DEMUX_SET_SEEKPOINT,        /* arg1= int            can fail */

    /* DEMUX_SET_GROUP only a hit for demuxer (mainly DVB) to allow not
     * reading everything (you should not use this to call es_out_Control)
     * if you don't know what to do with it, just IGNORE it, it is safe(r)
     * -1 means all group, 0 default group (first es added) */
    DEMUX_SET_GROUP,            /* arg1= int            can fail */

    /* Ask the demux to demux until the given date at the next pf_demux call
     * but not more (and not less, at the precision avaiable of course).
     * XXX: not mandatory (except for subtitle demux) but I will help a lot
     * for multi-input
     */
    DEMUX_SET_NEXT_DEMUX_TIME,  /* arg1= int64_t *      can fail */
    /* FPS for correct subtitles handling */
    DEMUX_GET_FPS,              /* arg1= float *        res=can fail    */
    /* Meta data */
    DEMUX_GET_META,             /* arg1= vlc_meta_t **  res=can fail    */


    /* II. Specific access_demux queries */
    DEMUX_CAN_PAUSE,            /* arg1= vlc_bool_t*    cannot fail */
    DEMUX_CAN_CONTROL_PACE,     /* arg1= vlc_bool_t*    cannot fail */
    DEMUX_GET_PTS_DELAY,        /* arg1= int64_t*       cannot fail */
    DEMUX_SET_PAUSE_STATE, /* arg1= vlc_bool_t     can fail */
};

/* stream_t *s could be null and then it mean a access+demux in one */
#define demux2_New( a, b, c, d, e, f,g ) __demux2_New(VLC_OBJECT(a),b,c,d,e,f,g)
VLC_EXPORT( demux_t *, __demux2_New,  ( vlc_object_t *p_obj, char *psz_access, char *psz_demux, char *psz_path, stream_t *s, es_out_t *out, vlc_bool_t ) );
VLC_EXPORT( void,      demux2_Delete, ( demux_t * ) );
VLC_EXPORT( int,       demux2_vaControlHelper, ( stream_t *, int64_t i_start, int64_t i_end, int i_bitrate, int i_align, int i_query, va_list args ) );

static inline int demux2_Demux( demux_t *p_demux )
{
    return p_demux->pf_demux( p_demux );
}
static inline int demux2_vaControl( demux_t *p_demux, int i_query, va_list args )
{
    return p_demux->pf_control( p_demux, i_query, args );
}
static inline int demux2_Control( demux_t *p_demux, int i_query, ... )
{
    va_list args;
    int     i_result;

    va_start( args, i_query );
    i_result = demux2_vaControl( p_demux, i_query, args );
    va_end( args );
    return i_result;
}
/**
 * @}
 */

#endif
