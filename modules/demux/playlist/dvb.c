/*****************************************************************************
 * dvb.c : DVB channel list import (szap/tzap/czap compatible channel lists)
 *****************************************************************************
 * Copyright (C) 2005-20009 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_demux.h>
#include <vlc_charset.h>

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
int Import_DVB( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    const uint8_t *p_peek;
    int     i_peek;
    bool b_valid = false;

    if( !demux_IsPathExtension( p_demux, ".conf" ) && !p_demux->b_force )
        return VLC_EGENERIC;

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

        if( ParseLine( psz_line, 0, 0, 0 ) ) b_valid = true;
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
void Close_DVB( vlc_object_t *p_this )
{
    VLC_UNUSED(p_this);
}

/*****************************************************************************
 * Demux: The important stuff
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    char       *psz_line;
    input_item_t *p_input;
    input_item_t *p_current_input = GetCurrentItem(p_demux);

    input_item_node_t *p_subitems = input_item_node_Create( p_current_input );

    while( (psz_line = stream_ReadLine( p_demux->s )) )
    {
        char **ppsz_options = NULL;
        int  i_options = 0;
        char *psz_name = NULL;
        char *psz_uri = strdup( "dvb://" );
        int i_optionslen = 0;

        if( !ParseLine( psz_line, &psz_name, &ppsz_options, &i_options ) )
        {
            free( psz_line );
            continue;
        }

        EnsureUTF8( psz_name );
        for( int i = 0; i< i_options; i++ )
        {
            EnsureUTF8( ppsz_options[i] );
            i_optionslen += ( strlen( ppsz_options[i] ) + 2 );
        }

        if ( i_optionslen )
        {
            /* ensure uri is also generated dvb:// :op1 :op2 */
            char *psz_localuri = calloc( i_optionslen + 6 + 1, sizeof(char) );
            if ( psz_localuri )
            {
                char *psz_tmp;
                char *psz_forward;
                psz_forward = strcat( psz_localuri, psz_uri ) + 6;
                for( int i = 0; i< i_options; i++ )
                {
                    psz_tmp = ppsz_options[i]; /* avoid doing i*strcat */
                    *psz_forward++ = ' ';
                    *psz_forward++ = ':';
                    while( *psz_tmp ) *psz_forward++ = *psz_tmp++;
                }
                free( psz_uri );
                psz_uri = psz_localuri;
            }
        }

        p_input = input_item_NewExt( psz_uri, psz_name,
                                     i_options, (const char**)ppsz_options, VLC_INPUT_OPTION_TRUSTED, -1 );
        input_item_node_AppendItem( p_subitems, p_input );
        vlc_gc_decref( p_input );

        while( i_options-- )
            free( ppsz_options[i_options] );
        free( ppsz_options );

        free( psz_uri );
        free( psz_line );
    }

    input_item_node_PostAndDelete( p_subitems );

    vlc_gc_decref(p_current_input);
    return 0; /* Needed for correct operation of go back */
}

static const struct
{
    const char *psz_name;
    const char *psz_option;

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
    { "8VSB", "dvb-modulation=8"  },
    { "16VSB", "dvb-modulation=16"  },

    { "TRANSMISSION_MODE_AUTO", "dvb-transmission=0" },
    { "TRANSMISSION_MODE_2K", "dvb-transmission=2" },
    { "TRANSMISSION_MODE_8K", "dvb-transmission=8" },
    { 0, 0 }

};

static int ParseLine( char *psz_line, char **ppsz_name,
                      char ***pppsz_options, int *pi_options )
{
    char *psz_name = NULL, *psz_parse = psz_line;
    int i_count = 0, i_program = 0, i_frequency = 0, i_symbolrate = 0;
    bool b_valid = false;

    if( pppsz_options ) *pppsz_options = NULL;
    if( pi_options ) *pi_options = 0;
    if( ppsz_name ) *ppsz_name = NULL;

    /* Skip leading tabs and spaces */
    while( *psz_parse == ' ' || *psz_parse == '\t' ||
           *psz_parse == '\n' || *psz_parse == '\r' ) psz_parse++;

    /* Ignore comments */
    if( *psz_parse == '#' ) return false;

    while( psz_parse )
    {
        const char *psz_option = NULL;
        char *psz_option_end = strchr( psz_parse, ':' );
        if( psz_option_end ) { *psz_option_end = 0; psz_option_end++; }

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
                    b_valid = true;
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
                    i_value != LONG_MAX && i_value != LONG_MIN &&
                    !i_symbolrate )
                {
                    i_symbolrate = i_value;
                }
                else if( psz_end != psz_parse &&
                    i_value != LONG_MAX && i_value != LONG_MIN )
                {
                    i_program = i_value;
                }
            }
        }

        if( psz_option && pppsz_options && pi_options )
        {
            char *psz_dup = strdup( psz_option );
            if (psz_dup != NULL)
                INSERT_ELEM( *pppsz_options, (*pi_options), (*pi_options),
                             psz_dup );
        }

        psz_parse = psz_option_end;
        i_count++;
    }

    if( !b_valid && pppsz_options && pi_options )
    {
        /* This isn't a valid channels file, cleanup everything */
        while( (*pi_options)-- ) free( (*pppsz_options)[*pi_options] );
        free( *pppsz_options );
        *pppsz_options = NULL; *pi_options = 0;
    }

    if( i_program && pppsz_options && pi_options )
    {
        char *psz_option;

        if( asprintf( &psz_option, "program=%i", i_program ) != -1 )
            INSERT_ELEM( *pppsz_options, (*pi_options), (*pi_options),
                         psz_option );
    }
    if( i_frequency && pppsz_options && pi_options )
    {
        char *psz_option;

        if( asprintf( &psz_option, "dvb-frequency=%i", i_frequency ) != -1 )
            INSERT_ELEM( *pppsz_options, (*pi_options), (*pi_options),
                         psz_option );
    }
    if( i_symbolrate && pppsz_options && pi_options )
    {
        char *psz_option;

        if( asprintf( &psz_option, "dvb-srate=%i", i_symbolrate ) != -1 )
            INSERT_ELEM( *pppsz_options, (*pi_options), (*pi_options),
                         psz_option );
    }
    if( ppsz_name && psz_name ) *ppsz_name = strdup( psz_name );

    return b_valid;
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    VLC_UNUSED(p_demux); VLC_UNUSED(i_query); VLC_UNUSED(args);
    return VLC_EGENERIC;
}
