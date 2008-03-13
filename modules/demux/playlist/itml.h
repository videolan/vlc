/*******************************************************************************
 * itml.c : iTunes Music Library import functions
 *******************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Yoann Peronneau <yoann@videolan.org>
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
 * \file modules/demux/playlist/itml.h
 * \brief iTunes Music Library import: prototypes, datatypes, defines
 */

/* defines */
#define FREE(v)        free(v);v=NULL;
#define FREE_NAME()    free(psz_name);psz_name=NULL;
#define FREE_VALUE()   free(psz_value);psz_value=NULL;
#define FREE_KEY()     free(psz_key);psz_key=NULL;
#define FREE_ATT()     FREE_NAME();FREE_VALUE()
#define FREE_ATT_KEY() FREE_NAME();FREE_VALUE();FREE_KEY()

#define UNKNOWN_CONTENT 0
#define SIMPLE_CONTENT 1
#define COMPLEX_CONTENT 2

#define SIMPLE_INTERFACE  (track_elem_t    *p_track,\
                           const char      *psz_name,\
                           char            *psz_value)
#define COMPLEX_INTERFACE (demux_t         *p_demux,\
                           playlist_t      *p_playlist,\
                           input_item_t    *p_input_item,\
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
        vlc_bool_t (*smpl) SIMPLE_INTERFACE;
        vlc_bool_t (*cmplx) COMPLEX_INTERFACE;
    } pf_handler;
};
typedef struct xml_elem_hnd xml_elem_hnd_t;

/* prototypes */
static vlc_bool_t parse_plist_node COMPLEX_INTERFACE;
static vlc_bool_t skip_element COMPLEX_INTERFACE;
static vlc_bool_t parse_dict COMPLEX_INTERFACE;
static vlc_bool_t parse_plist_dict COMPLEX_INTERFACE;
static vlc_bool_t parse_tracks_dict COMPLEX_INTERFACE;
static vlc_bool_t parse_track_dict COMPLEX_INTERFACE;
static vlc_bool_t save_data SIMPLE_INTERFACE;
static vlc_bool_t add_meta( input_item_t*, track_elem_t* );
static track_elem_t *new_track( void );
static void free_track( track_elem_t* );

