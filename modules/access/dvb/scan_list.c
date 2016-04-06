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

static void scan_token_strip( const char **ppsz, size_t *pi_len )
{
    const char *p = *ppsz;
    size_t i_len = *pi_len;

    for ( ; *p <= ' ' && *p ; p++ )
        i_len--;

    for( ; i_len > 0; i_len-- )
    {
        const char c = p[ i_len - 1 ];
        if( c > ' ' || c == '\0' )
            break;
    }

    *ppsz = p;
    *pi_len = i_len;
}

static bool scan_list_token_split( const char *psz_line, size_t i_len,
                                   const char **ppsz_key, size_t *pi_keylen,
                                   const char **ppsz_value, size_t *pi_valuelen )
{
    const char *p_split = strchr( psz_line, '=' );
    if( !p_split )
        return false;

    size_t i_keylen = p_split - psz_line;
    p_split++;
    size_t i_valuelen = &psz_line[i_len] - p_split;

    scan_token_strip( &psz_line, &i_keylen );
    scan_token_strip( &p_split, &i_valuelen );

    if( !i_keylen || !i_valuelen )
        return false;

    *ppsz_key = psz_line;
    *pi_keylen = i_keylen;

    *ppsz_value = p_split;
    *pi_valuelen = i_valuelen;

    return true;
}

#define STRING_EQUALS(token, string, stringlen) \
    ((sizeof(token) - 1) == stringlen && !strncasecmp( string, token, stringlen ))

#define KEY_EQUALS(token) \
    ((sizeof(token) - 1) == i_keylen && !strncasecmp( psz_key, token, i_keylen ))

#define VALUE_EQUALS(token) \
    ((sizeof(token) - 1) == i_valuelen && !strncasecmp( psz_value, token, i_valuelen ))

static void scan_list_dvbv5_entry_fill( scan_list_entry_t *p_entry, const char *psz_line, size_t i_len )
{
    const char *psz_key;
    const char *psz_value;
    size_t i_keylen;
    size_t i_valuelen;

    if( !scan_list_token_split( psz_line, i_len, &psz_key, &i_keylen, &psz_value, &i_valuelen ) )
        return;

    char *psz_end = (char *) &psz_value[i_valuelen];

    if ( KEY_EQUALS("SERVICE_ID") )
    {
        p_entry->i_service = strtoll( psz_value, &psz_end, 10 );
    }
    else if( KEY_EQUALS("FREQUENCY") )
    {
        p_entry->i_freq = strtoll( psz_value, &psz_end, 10 );
    }
    else if( KEY_EQUALS("BANDWIDTH_HZ") )
    {
        p_entry->i_bw = strtoll( psz_value, &psz_end, 10 );
    }
    else if( KEY_EQUALS("DELIVERY_SYSTEM") )
    {
        if( VALUE_EQUALS("DVBT") )
            p_entry->delivery = DELIVERY_DVBT;
        else if( VALUE_EQUALS("DVBT2") )
            p_entry->delivery = DELIVERY_DVBT2;
        else if( VALUE_EQUALS("DVBS") )
            p_entry->delivery = DELIVERY_DVBS;
        else if( VALUE_EQUALS("DVBS2") )
            p_entry->delivery = DELIVERY_DVBS2;
        else if( VALUE_EQUALS("DVBC/ANNEX_A") )
            p_entry->delivery = DELIVERY_DVBC;
        else if( VALUE_EQUALS("ISDBT") )
            p_entry->delivery = DELIVERY_ISDBT;
    }
    else if( KEY_EQUALS("POLARIZATION") )
    {
        if( VALUE_EQUALS("VERTICAL") )
            p_entry->polarization = POLARIZATION_VERTICAL;
        else
            p_entry->polarization = POLARIZATION_HORIZONTAL;
    }
    else if( KEY_EQUALS("SYMBOL_RATE") )
    {
        p_entry->i_rate = strtoll( psz_value, &psz_end, 10 );
    }
    else if( KEY_EQUALS("STREAM_ID") )
    {
        p_entry->i_stream_id = strtoll( psz_value, &psz_end, 10 );
    }
    else if( KEY_EQUALS("INNER_FEC") )
    {
        char *psz_val = strndup(psz_value, i_valuelen);
        if( psz_val )
        {
            scan_list_parse_fec( p_entry, psz_val );
            free( psz_val );
        }
    }
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

scan_list_entry_t * scan_list_dvbv5_load( vlc_object_t *p_obj, const char *psz_source, size_t *pi_count )
{
    FILE *p_file = vlc_fopen( psz_source, "r" );
    if( !p_file )
    {
        msg_Err( p_obj, "failed to open dvbv5 file (%s)", psz_source );
        return NULL;
    }

    scan_list_entry_t *p_list = NULL;
    scan_list_entry_t **pp_list_last = &p_list;
    scan_list_entry_t *p_entry = NULL;
    *pi_count = 0;

    char *psz_line = NULL;
    size_t i_len = 0;
    ssize_t i_read;
    bool b_error = false;

    while ( (i_read = getline( &psz_line, &i_len, p_file )) != -1 && !b_error )
    {
        const char *p = psz_line;

        for ( ; *p == ' ' || *p == '\t'; p++ )
            i_read--;

        switch( *p )
        {
            case 0:
            case '\n':
            case '#': /* comment line */
                continue;
            case '[':
            {
                if( p_entry && scan_list_entry_add( &pp_list_last, p_entry ) )
                    (*pi_count)++;
                p_entry = scan_list_entry_New();
                if( !p_entry )
                {
                    b_error = true;
                }
                else
                {
                    char *p_end = strstr( p, "]" );
                    if( !p_end )
                        b_error = true;
                    else
                    {
                        const char *psz_name = p + 1;
                        size_t i_len_name = p_end - p - 1;
                        scan_token_strip( &psz_name, &i_len_name );
                        p_entry->psz_channel = strndup( psz_name, i_len_name );
                    }
                }
                break;
            }

            default:
            {
                if( p_entry )
                    scan_list_dvbv5_entry_fill( p_entry, p, i_read );
                break;
            }
        }

    }

    if( p_entry )
    {
        if( b_error )
            scan_list_entry_Delete( p_entry );
        else if( scan_list_entry_add( &pp_list_last, p_entry ) )
            (*pi_count)++;
    }

    free( psz_line );
    fclose( p_file );

    return p_list;
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
