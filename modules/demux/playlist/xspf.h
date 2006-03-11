/*****************************************************************************
 * Copyright (C) 2006 Daniel Stränger <vlc at schmaller dot de>
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
 *******************************************************************************/
/**
 * \file modules/demux/playlist/xspf.h
 * \brief XSPF playlist import: prototypes, datatypes, defines
 */

/* defines */
#define FREE_NAME()  if (psz_name) {free(psz_name);psz_name=NULL;}
#define FREE_VALUE() if (psz_value) {free(psz_value);psz_value=NULL;}
#define FREE_ATT()   FREE_NAME();FREE_VALUE()

#define UNKNOWN_CONTENT 0
#define SIMPLE_CONTENT 1
#define COMPLEX_CONTENT 2

#define SIMPLE_INTERFACE  (playlist_item_t *p_item,\
                           const char      *psz_name,\
                           char            *psz_value)
#define COMPLEX_INTERFACE (demux_t         *p_demux,\
                           playlist_t      *p_playlist,\
                           playlist_item_t *p_item,\
                           xml_reader_t    *p_xml_reader,\
                           const char      *psz_element)

/* prototypes */
int xspf_import_Demux( demux_t *);
int xspf_import_Control( demux_t *, int, va_list );

static vlc_bool_t parse_playlist_node COMPLEX_INTERFACE;
static vlc_bool_t parse_tracklist_node COMPLEX_INTERFACE;
static vlc_bool_t parse_track_node COMPLEX_INTERFACE;
static vlc_bool_t set_item_info SIMPLE_INTERFACE;
static vlc_bool_t skip_element COMPLEX_INTERFACE;
static vlc_bool_t insert_new_item( playlist_t *, playlist_item_t *, playlist_item_t **, char *);

/* datatypes */
typedef struct
{
    const char *name;
    int type;
    union
    {
        vlc_bool_t (*smpl) SIMPLE_INTERFACE;
        vlc_bool_t (*cmplx) COMPLEX_INTERFACE;
    } pf_handler;
} xml_elem_hnd_t;
