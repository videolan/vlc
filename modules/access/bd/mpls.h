/*****************************************************************************
 * mpls.h: BluRay Disc MPLS
 *****************************************************************************
 * Copyright (C) 2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

typedef enum
{
    BD_MPLS_STREAM_TYPE_PLAY_ITEM = 0x01,
    BD_MPLS_STREAM_TYPE_SUB_PATH = 0x02,
    BD_MPLS_STREAM_TYPE_IN_MUX_SUB_PATH = 0x03,
} bd_mpls_stream_type_t;
typedef enum
{
    BD_MPLS_STREAM_CLASS_PRIMARY_VIDEO = 0,
    BD_MPLS_STREAM_CLASS_PRIMARY_AUDIO,
    BD_MPLS_STREAM_CLASS_PG,
    BD_MPLS_STREAM_CLASS_IG,
    BD_MPLS_STREAM_CLASS_SECONDARY_AUDIO,
    BD_MPLS_STREAM_CLASS_SECONDARY_PIP_PG,
} bd_mpls_stream_class_t;
typedef enum
{
    BD_MPLS_STREAM_CHARSET_UNKNOWN = -1,

    BD_MPLS_STREAM_CHARSET_UTF8 = 0x01,
    BD_MPLS_STREAM_CHARSET_UTF16 = 0x02,
    BD_MPLS_STREAM_CHARSET_SHIFT_JIS = 0x03,
    BD_MPLS_STREAM_CHARSET_KSC = 0x04,
    BD_MPLS_STREAM_CHARSET_GB18030 = 0x05,
    BD_MPLS_STREAM_CHARSET_GB2312 = 0x06,
    BD_MPLS_STREAM_CHARSET_BIG5 = 0x7,
} bd_mpls_stream_charset_t;

typedef struct
{
    /* Stream entry */
    int i_type;             /* Type of the stream entry (bd_mpls_stream_type_t) */
    int i_class;            /* Class of the stream entry (bd_mpls_stream_class_t) */
    union
    {
        struct
        {
            int i_pid;              /* PID of the associated stream */
        } play_item;
        struct
        {
            int i_sub_path_id;      /* Index of the sub path entry */
            int i_sub_clip_id;      /* Index of the sub clip entry (?) */
            int i_pid;              /* PID of the associated stream */
        } sub_path;
        struct
        {
            int i_sub_path_id;      /* Index of the sub path entry */
            int i_pid;              /* PID of the associated stream */
        } in_mux_sub_path;
    };

    /* Stream attributes */
    int  i_stream_type;     /* MPEG-2 TS stream_type */
    char psz_language[3+1]; /* ISO-639 code, empty if NA */
    int  i_charset;         /* For text stream only (bd_mpls_stream_attributes_charset_t) */
} bd_mpls_stream_t;

void bd_mpls_stream_Parse( bd_mpls_stream_t *p_stream, bs_t *s, int i_class );

typedef enum
{
    BD_MPLS_PLAY_ITEM_CONNECTION_NOT_SEAMLESS = 0x01,
    BD_MPLS_PLAY_ITEM_CONNECTION_SEAMLESS_5 = 0x05,
    BD_MPLS_PLAY_ITEM_CONNECTION_SEAMLESS_6 = 0x06,
} bd_mpls_play_item_connection_t;

typedef enum
{
    BD_MPLS_PLAY_ITEM_STILL_NONE = 0x00,
    BD_MPLS_PLAY_ITEM_STILL_FINITE = 0x01,
    BD_MPLS_PLAY_ITEM_STILL_INFINITE = 0x02,
} bd_mpls_play_item_still_t;

typedef struct
{
    int     i_id;
    int     i_stc_id;
} bd_mpls_clpi_t;

typedef struct
{
    int     i_connection;   /* Connection with previous play item (bd_mpls_play_item_connection_t) */
    int64_t i_in_time;      /* Start time in 45kHz */
    int64_t i_out_time;     /* Stop time in 45kHz */
    int     i_still;        /* Still mode (bd_mpls_play_item_still_t) */
    int     i_still_time;   /* Still time for BD_MPLS_PLAY_ITEM_STILL_FINITE (second?) */

    /* Default clpi/angle */
    bd_mpls_clpi_t  clpi;

    /* Extra clpi (multiple angles) */
    int             i_clpi;
    bd_mpls_clpi_t *p_clpi;
    bool            b_angle_different_audio;
    bool            b_angle_seamless;


    /* */
    int              i_stream;
    bd_mpls_stream_t *p_stream;

} bd_mpls_play_item_t;
void bd_mpls_play_item_Clean( bd_mpls_play_item_t *p_item );
void bd_mpls_play_item_Parse( bd_mpls_play_item_t *p_item, bs_t *s );

typedef enum
{
    BD_MPLS_SUB_PATH_TYPE_PRIMARY_AUDIO = 0x02,
    BD_MPLS_SUB_PATH_TYPE_IG = 0x03,
    BD_MPLS_SUB_PATH_TYPE_TEXT_SUB = 0x04,
    BD_MPLS_SUB_PATH_TYPE_OUT_OF_MUX_AND_SYNC = 0x05,
    BD_MPLS_SUB_PATH_TYPE_OUT_OF_MUX_AND_ASYNC = 0x06,
    BD_MPLS_SUB_PATH_TYPE_IN_OF_MUX_AND_SYNC = 0x07,
} bd_mpls_sub_path_type_t;
typedef struct
{
    int  i_type;        /* Sub path type (bd_mpls_sub_path_type_t) */
    bool b_repeat;      /* Repeated sub-path */

    int  i_item;
    /* TODO
    bd_mpls_sub_play_item_t *p_item;
    */
} bd_mpls_sub_path_t;
void bd_mpls_sub_path_Parse( bd_mpls_sub_path_t *p_path, bs_t *s );

typedef enum
{
    BD_MPLS_MARK_TYPE_RESUME = 0x00,
    BD_MPLS_MARK_TYPE_BOOKMARK = 0x01,
    BD_MPLS_MARK_TYPE_SKIP = 0x02,
} bd_mpls_mark_type_t;

typedef struct
{
    int     i_type;             /* Type of the mark (bd_mpls_mark_type_t) */
    int     i_play_item_id;     /* Play item ID */
    int64_t i_time;             /* Time of the mark in 45kHz */
    int     i_entry_es_pid;     /* Entry ES PID */
} bd_mpls_mark_t;
void bd_mpls_mark_Parse( bd_mpls_mark_t *p_mark, bs_t *s );

typedef struct
{
    int                 i_id;

    int                 i_play_item;
    bd_mpls_play_item_t *p_play_item;

    int                 i_sub_path;
    bd_mpls_sub_path_t  *p_sub_path;

    int                 i_mark;
    bd_mpls_mark_t      *p_mark;
} bd_mpls_t;
void bd_mpls_Clean( bd_mpls_t *p_mpls );
int bd_mpls_Parse( bd_mpls_t *p_mpls, bs_t *s, int i_id );

