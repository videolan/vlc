/*******************************************************************************
 * itml.h : iTunes Music Library import functions
 *******************************************************************************
 * Copyright (C) 2007 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Yoann Peronneau <yoann@videolan.org>
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
 *******************************************************************************/
/**
 * \file modules/demux/playlist/itml.h
 * \brief iTunes Music Library import: prototypes, datatypes, defines
 */

/* defines */
#define FREE_VALUE()    FREENULL( psz_value )
#define FREE_KEY()      FREENULL( psz_key )
#define FREE_ATT()      FREE_VALUE()
#define FREE_ATT_KEY()  do{ FREE_VALUE();FREE_KEY();} while(0)

#define UNKNOWN_CONTENT 0
#define SIMPLE_CONTENT 1
#define COMPLEX_CONTENT 2

#define SIMPLE_INTERFACE  (track_elem_t    *p_track,\
                           const char      *psz_name,\
                           char            *psz_value)
#define COMPLEX_INTERFACE (demux_t         *p_demux,\
                           input_item_node_t    *p_input_node,\
                           track_elem_t    *p_track,\
                           xml_reader_t    *p_xml_reader,\
                           const char      *psz_element,\
                           struct xml_elem_hnd  *p_handlers)

/* datatypes */
typedef struct
{
    char *name, *artist, *album, *genre, *trackNum, *location;
    mtime_t duration;
} track_elem_t;

struct xml_elem_hnd
{
    const char *name;
    int type;
    union
    {
        bool (*smpl) SIMPLE_INTERFACE;
        bool (*cmplx) COMPLEX_INTERFACE;
    } pf_handler;
};
typedef struct xml_elem_hnd xml_elem_hnd_t;

/* prototypes */
static bool parse_plist_node COMPLEX_INTERFACE;
static bool skip_element COMPLEX_INTERFACE;
static bool parse_dict COMPLEX_INTERFACE;
static bool parse_plist_dict COMPLEX_INTERFACE;
static bool parse_tracks_dict COMPLEX_INTERFACE;
static bool parse_track_dict COMPLEX_INTERFACE;
static bool save_data SIMPLE_INTERFACE;
static bool add_meta( input_item_t*, track_elem_t* );
static track_elem_t *new_track( void );
static void free_track( track_elem_t* );

