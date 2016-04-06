/*****************************************************************************
 * scan_list.c : Scanning parameters and transponders list
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.    See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA    02111, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_fs.h>

#include "scan_list.h"

static scan_list_entry_t * scan_list_entry_New()
{
    scan_list_entry_t *p_entry = calloc(1, sizeof(scan_list_entry_t));
    if( likely(p_entry) )
    {
        p_entry->i_service = -1;
        p_entry->i_stream_id = -1;
    }
    return p_entry;
}

static void scan_list_entry_Delete( scan_list_entry_t *p_entry )
{
    free( p_entry->psz_channel );
    free( p_entry );
}

static bool scan_list_entry_validate( const scan_list_entry_t *p_entry )
{
    switch( p_entry->delivery )
    {
        case DELIVERY_DVBS:
        case DELIVERY_DVBS2:
            return p_entry->i_freq && p_entry->i_rate && p_entry->i_fec;

        case DELIVERY_DVBT:
        case DELIVERY_DVBT2:
        case DELIVERY_ISDBT:
            return p_entry->i_freq && p_entry->i_bw;

        case DELIVERY_DVBC:
            return p_entry->i_freq && p_entry->i_rate;

        case DELIVERY_UNKNOWN:
        default:
            break;
    }
    return false;
}

static bool scan_list_entry_add( scan_list_entry_t ***ppp_last, scan_list_entry_t *p_entry )
{
    if( scan_list_entry_validate( p_entry ) )
    {
         **ppp_last = p_entry;
         *ppp_last = &p_entry->p_next;
        return true;
    }

    scan_list_entry_Delete( p_entry );
    return false;
}

static void scan_list_parse_fec( scan_list_entry_t *p_entry, const char *psz )
{
    const char *psz_fec_list = "1/22/33/44/55/66/77/88/9";
    const char *psz_fec = strstr( psz_fec_list, psz );
    if ( !psz_fec )
        p_entry->i_fec = 9;    /* FEC_AUTO */
    else
        p_entry->i_fec = 1 + ( ( psz_fec - psz_fec_list ) / 3 );
}

void scan_list_entries_release( scan_list_entry_t *p_list )
{
    while( p_list )
    {
        scan_list_entry_t *p_next = p_list->p_next;
        scan_list_entry_Delete( p_list );
        p_list = p_next;
    }
}

scan_list_entry_t * scan_list_dvbv3_load( vlc_object_t *p_obj, const char *psz_source, size_t *pi_count )
{
    FILE *p_file = vlc_fopen( psz_source, "r" );
    if( !p_file )
    {
        msg_Err( p_obj, "failed to open satellite file (%s)", psz_source );
        return NULL;
    }

    scan_list_entry_t *p_list = NULL;
    scan_list_entry_t **pp_list_last = &p_list;
    scan_list_entry_t *p_entry = NULL;
    *pi_count = 0;

    const char *psz_delims = " \t";

    char *psz_line = NULL;
    size_t i_len = 0;
    ssize_t i_read;

    while ( (i_read = getline( &psz_line, &i_len, p_file )) != -1 )
    {
        char *psz_token;
        char *p_save = NULL;

        if( p_entry && scan_list_entry_add( &pp_list_last, p_entry ) )
            (*pi_count)++;

        p_entry = scan_list_entry_New();
        if( unlikely(p_entry == NULL) )
            continue;

        /* DELIVERY */
        if( !(psz_token = strtok_r( psz_line, psz_delims, &p_save )) )
            continue;

        if( !strcmp( psz_token, "S" ) )
        {
            p_entry->delivery = DELIVERY_DVBS;
        }
        else if( !strcmp( psz_token, "S2" ) )
        {
            p_entry->delivery = DELIVERY_DVBS2;
        }

        /* Parse the delivery format */
        if( p_entry->delivery == DELIVERY_DVBS || p_entry->delivery == DELIVERY_DVBS2 )
        {
            /* FREQUENCY */
            if( !(psz_token = strtok_r( NULL, psz_delims, &p_save )) )
                continue;
            p_entry->i_freq = atoi( psz_token );

            /* POLARIZATION */
            if( !(psz_token = strtok_r( NULL, psz_delims, &p_save )) )
                continue;
            p_entry->polarization = !strcasecmp(psz_token, "H") ? POLARIZATION_HORIZONTAL
                                                                : POLARIZATION_VERTICAL;

            /* RATE */
            if( !(psz_token = strtok_r( NULL, psz_delims, &p_save )) )
                continue;
            p_entry->i_rate = atoi( psz_token );

            /* FEC */
            if( !(psz_token = strtok_r( NULL, psz_delims, &p_save )) )
                continue;
            scan_list_parse_fec( p_entry, psz_token );

            /* INVERSION */
            if( !(psz_token = strtok_r( NULL, psz_delims, &p_save )) )
                continue;

            /* MODULATION */
            if( !(psz_token = strtok_r( NULL, psz_delims, &p_save )) )
                continue;

            /* STREAM_ID */
            if( !(psz_token = strtok_r( NULL, psz_delims, &p_save )) )
                continue;
            p_entry->i_stream_id = atoi( psz_token );
        }

    }

    if( p_entry && scan_list_entry_add( &pp_list_last, p_entry ) )
        (*pi_count)++;

    fclose( p_file );

    return p_list;
}
