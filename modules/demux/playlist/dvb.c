/*****************************************************************************
 * dvb.c : DVB channel list import (szap/tzap/czap compatible channel lists)
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/intf.h>

#include "playlist.h"

#ifndef LONG_MAX
#   define LONG_MAX 2147483647L
#   define LONG_MIN (-LONG_MAX-1)
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux);
static int Control( demux_t *p_demux, int i_query, va_list args );

static int ParseLine( char *, char **, char ***, int *);

/*****************************************************************************
 * Import_DVB: main import function
 *****************************************************************************/
int E_(Import_DVB)( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    uint8_t *p_peek;
    int     i_peek;
    char    *psz_ext;
    vlc_bool_t b_valid = VLC_FALSE;

    psz_ext = strrchr ( p_demux->psz_path, '.' );

    if( !( psz_ext && !strncasecmp( psz_ext, ".conf", 5 ) ) &&
        !p_demux->b_force ) return VLC_EGENERIC;

    /* Check if this really is a channels file */
    if( (i_peek = stream_Peek( p_demux->s, &p_peek, 1024 )) > 0 )
    {
        char psz_line[1024+1];
        int i;

        for( i = 0; i < i_peek; i++ )
        {
            if( p_peek[i] == '\n' ) break;
            psz_line[i] = p_peek[i];
        }
        psz_line[i] = 0;

        if( ParseLine( psz_line, 0, 0, 0 ) ) b_valid = VLC_TRUE;
    }

    if( !b_valid ) return VLC_EGENERIC;

    msg_Dbg( p_demux, "found valid DVB conf playlist file");

    p_demux->pf_control = Control;
    p_demux->pf_demux = Demux;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
void E_(Close_DVB)( vlc_object_t *p_this )
{
}

/*****************************************************************************
 * Demux: The important stuff
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    playlist_t *p_playlist;
    char       *psz_line;
    playlist_item_t *p_current;
    vlc_bool_t b_play;

    p_playlist = (playlist_t *) vlc_object_find( p_demux, VLC_OBJECT_PLAYLIST,
                                                 FIND_PARENT );
    if( !p_playlist )
    {
        msg_Err( p_demux, "can't find playlist" );
        return -1;
    }

    b_play = E_(FindItem)( p_demux, p_playlist, &p_current );

    playlist_ItemToNode( p_playlist, p_current );
    p_current->input.i_type = ITEM_TYPE_PLAYLIST;

    while( (psz_line = stream_ReadLine( p_demux->s )) )
    {
        playlist_item_t *p_item;
        char **ppsz_options = NULL;
        int  i, i_options = 0;
        char *psz_name = NULL;

        if( !ParseLine( psz_line, &psz_name, &ppsz_options, &i_options ) )
        {
            free( psz_line );
            continue;
        }

        EnsureUTF8( psz_name );

        p_item = playlist_ItemNew( p_playlist, "dvb:", psz_name );
        for( i = 0; i< i_options; i++ )
        {
            EnsureUTF8( ppsz_options[i] );
            playlist_ItemAddOption( p_item, ppsz_options[i] );
        }
        playlist_NodeAddItem( p_playlist, p_item,
                              p_current->pp_parents[0]->i_view,
                              p_current, PLAYLIST_APPEND, PLAYLIST_END );

        /* We need to declare the parents of the node as the
         *                  * same of the parent's ones */
        playlist_CopyParents( p_current, p_item );
        vlc_input_item_CopyOptions( &p_current->input, &p_item->input );

        while( i_options-- ) free( ppsz_options[i_options] );
        if( ppsz_options ) free( ppsz_options );

        free( psz_line );
    }

    /* Go back and play the playlist */
    if( b_play && p_playlist->status.p_item &&
        p_playlist->status.p_item->i_children > 0 )
    {
        playlist_Control( p_playlist, PLAYLIST_VIEWPLAY,
                          p_playlist->status.i_view,
                          p_playlist->status.p_item,
                          p_playlist->status.p_item->pp_children[0] );
    }

    vlc_object_release( p_playlist );
    return VLC_SUCCESS;
}

static struct
{
    char *psz_name;
    char *psz_option;

} dvb_options[] =
{
    { "INVERSION_OFF", "dvb-inversion=0" },
    { "INVERSION_ON", "dvb-inversion=1" },
    { "INVERSION_AUTO", "dvb-inversion=2" },

    { "BANDWIDTH_AUTO", "dvb-bandwidth=0" },
    { "BANDWIDTH_6_MHZ", "dvb-bandwidth=6" },
    { "BANDWIDTH_7_MHZ", "dvb-bandwidth=7" },
    { "BANDWIDTH_8_MHZ", "dvb-bandwidth=8" },

    { "FEC_NONE", "dvb-fec=0" },
    { "FEC_1_2", "dvb-fec=1" },
    { "FEC_2_3", "dvb-fec=2" },
    { "FEC_3_4", "dvb-fec=3" },
    { "FEC_4_5", "dvb-fec=4" },
    { "FEC_5_6", "dvb-fec=5" },
    { "FEC_6_7", "dvb-fec=6" },
    { "FEC_7_8", "dvb-fec=7" },
    { "FEC_8_9", "dvb-fec=8" },
    { "FEC_AUTO", "dvb-fec=9" },

    { "GUARD_INTERVAL_AUTO", "dvb-guard=0" },
    { "GUARD_INTERVAL_1_4", "dvb-guard=4" },
    { "GUARD_INTERVAL_1_8", "dvb-guard=8" },
    { "GUARD_INTERVAL_1_16", "dvb-guard=16" },
    { "GUARD_INTERVAL_1_32", "dvb-guard=32" },

    { "HIERARCHY_NONE", "dvb-hierarchy=-1" },
    { "HIERARCHY_1", "dvb-hierarchy=1" },
    { "HIERARCHY_2", "dvb-hierarchy=2" },
    { "HIERARCHY_4", "dvb-hierarchy=4" },

    { "QPSK", "dvb-modulation=-1" },
    { "QAM_AUTO", "dvb-modulation=0" },
    { "QAM_16", "dvb-modulation=16" },
    { "QAM_32", "dvb-modulation=32" },
    { "QAM_64", "dvb-modulation=64" },
    { "QAM_128", "dvb-modulation=128" },
    { "QAM_256", "dvb-modulation=256" },

    { "TRANSMISSION_MODE_AUTO", "dvb-transmission=0" },
    { "TRANSMISSION_MODE_2K", "dvb-transmission=2" },
    { "TRANSMISSION_MODE_8K", "dvb-transmission=8" },
    { 0, 0 }

};

static int ParseLine( char *psz_line, char **ppsz_name,
                      char ***pppsz_options, int *pi_options )
{
    char *psz_name = 0, *psz_parse = psz_line;
    int i_count = 0, i_program = 0, i_frequency = 0;
    vlc_bool_t b_valid = VLC_FALSE;

    if( pppsz_options ) *pppsz_options = 0;
    if( pi_options ) *pi_options = 0;
    if( ppsz_name ) *ppsz_name = 0;

    /* Skip leading tabs and spaces */
    while( *psz_parse == ' ' || *psz_parse == '\t' ||
           *psz_parse == '\n' || *psz_parse == '\r' ) psz_parse++;

    /* Ignore comments */
    if( *psz_parse == '#' ) return VLC_FALSE;

    while( psz_parse )
    {
        char *psz_option = 0;
        char *psz_end = strchr( psz_parse, ':' );
        if( psz_end ) { *psz_end = 0; psz_end++; }

        if( i_count == 0 )
        {
            /* Channel name */
            psz_name = psz_parse;
        }
        else if( i_count == 1 )
        {
            /* Frequency */
            char *psz_end;
            long i_value;

            i_value = strtol( psz_parse, &psz_end, 10 );
            if( psz_end == psz_parse ||
                i_value == LONG_MAX || i_value == LONG_MIN ) break;

            i_frequency = i_value;
        }
        else
        {
            int i;

            /* Check option name with our list */
            for( i = 0; dvb_options[i].psz_name; i++ )
            {
                if( !strcmp( psz_parse, dvb_options[i].psz_name ) )
                {
                    psz_option = dvb_options[i].psz_option;

                    /* If we recognize one of the strings, then we are sure
                     * the data is really valid (ie. a channels file). */
                    b_valid = VLC_TRUE;
                    break;
                }
            }

            if( !psz_option )
            {
                /* Option not recognized, test if it is a number */
                char *psz_end;
                long i_value;

                i_value = strtol( psz_parse, &psz_end, 10 );
                if( psz_end != psz_parse &&
                    i_value != LONG_MAX && i_value != LONG_MIN )
                {
                    i_program = i_value;
                }
            }
        }

        if( psz_option && pppsz_options && pi_options )
        {
            psz_option = strdup( psz_option );
            INSERT_ELEM( *pppsz_options, (*pi_options), (*pi_options),
                         psz_option );
        }

        psz_parse = psz_end;
        i_count++;
    }

    if( !b_valid && pppsz_options && pi_options )
    {
        /* This isn't a valid channels file, cleanup everything */
        while( (*pi_options)-- ) free( (*pppsz_options)[*pi_options] );
        if( *pppsz_options ) free( *pppsz_options );
        *pppsz_options = 0; *pi_options = 0;
    }

    if( i_program && pppsz_options && pi_options )
    {
        char *psz_option;

        asprintf( &psz_option, "program=%i", i_program );
        INSERT_ELEM( *pppsz_options, (*pi_options), (*pi_options),
                     psz_option );
    }
    if( i_frequency && pppsz_options && pi_options )
    {
        char *psz_option;

        asprintf( &psz_option, "dvb-frequency=%i", i_frequency );
        INSERT_ELEM( *pppsz_options, (*pi_options), (*pi_options),
                     psz_option );
    }
    if( ppsz_name && psz_name ) *ppsz_name = strdup( psz_name );

    return b_valid;
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    return VLC_EGENERIC;
}
