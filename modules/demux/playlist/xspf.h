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
#define FREE_NAME()  free(psz_name);psz_name=NULL;
#define FREE_VALUE() free(psz_value);psz_value=NULL;
#define FREE_ATT()   FREE_NAME();FREE_VALUE()

enum {
    UNKNOWN_CONTENT,
    SIMPLE_CONTENT,
    COMPLEX_CONTENT
};

#define SIMPLE_INTERFACE  (input_item_t    *p_input,\
                           const char      *psz_name,\
                           char            *psz_value)
#define COMPLEX_INTERFACE (demux_t         *p_demux,\
                           input_item_t    *p_input_item,\
                           xml_reader_t    *p_xml_reader,\
                           const char      *psz_element)

/* prototypes */
static bool parse_playlist_node COMPLEX_INTERFACE;
static bool parse_tracklist_node COMPLEX_INTERFACE;
static bool parse_track_node COMPLEX_INTERFACE;
static bool parse_extension_node COMPLEX_INTERFACE;
static bool parse_extitem_node COMPLEX_INTERFACE;
static bool set_item_info SIMPLE_INTERFACE;
static bool set_option SIMPLE_INTERFACE;
static bool skip_element COMPLEX_INTERFACE;

/* datatypes */
typedef struct
{
    const char *name;
    int type;
    union
    {
        bool (*smpl) SIMPLE_INTERFACE;
        bool (*cmplx) COMPLEX_INTERFACE;
    } pf_handler;
} xml_elem_hnd_t;
