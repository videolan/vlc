/*****************************************************************************
 * nsc.c: NSC file demux and encoding decoder
 *****************************************************************************
 * Copyright (C) 2005 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Derk-Jan Hartman <hartman at videolan dot org>
 *          based on work from Jon Lech Johansen <jon@nanocrew.net>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_charset.h>

#include <ctype.h>
#define MAX_LINE 16024

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  DemuxOpen  ( vlc_object_t * );
static void DemuxClose ( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("Windows Media NSC metademux") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_capability( "demux", 3 )
    set_callbacks( DemuxOpen, DemuxClose )
vlc_module_end ()

static int Demux ( demux_t *p_demux );
static int Control( demux_t *p_demux, int i_query, va_list args );

static const unsigned char inverse[ 128 ] =
{
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C,
    0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E,
    0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A,
    0x3B, 0x3C, 0x3D, 0x3E, 0xFF, 0x3F, 0xFF, 0xFF
};

static int load_byte( unsigned char encoding_type,
                      unsigned char *output, char **input,
                      unsigned char *j, unsigned char *k )
{
    *output = 0;

    if( encoding_type == 1 )
    {
        if( isxdigit( (unsigned char)**input ) == 0 )
            return -1;

        if( isdigit( (unsigned char)**input ) == 0 )
            *output = (toupper( (unsigned char)**input ) - 7) * 16;
        else
            *output = **input * 16;

        (*input)++;

        if( isxdigit( (unsigned char)**input ) == 0 )
            return -1;

        if( isdigit( (unsigned char)**input ) == 0 )
            *output |= toupper( (unsigned char)**input ) - 0x37;
        else
            *output |= **input - 0x30;

        (*input)++;
    }
    else if( encoding_type == 2 )
    {
        unsigned char **uinput = (unsigned char **)input;

        if( **uinput > 127 || inverse[ **uinput ] == 0xFF )
            return -1;

        if( *k == 0 )
        {
            if( (*uinput)[ 1 ] > 127 || inverse[ (*uinput)[ 1 ] ] == 0xFF )
                return -1;

            *output = (inverse[ (*uinput)[ 0 ] ] * 4) |
                        (inverse[ (*uinput)[ 1 ] ] / 16);

            *j = inverse[ (*uinput)[ 1 ] ] * 16;
            *k = 4;

            (*uinput) += 2;
        }
        else if( *k == 2 )
        {
            *output = *j | inverse[ **uinput ];

            *j = 0;
            *k = 0;

            (*uinput)++;
        }
        else if( *k == 4 )
        {
            *output = (inverse[ **uinput ] / 4) | *j;

            *j = inverse[ **uinput ] * 64;
            *k = 2;

            (*uinput)++;
        }
    }

    return 0;
}

static char *nscdec( vlc_object_t *p_demux, char* p_encoded )
{
    unsigned int i;
    unsigned char tmp;
    unsigned char j, k;
    unsigned int length;
    unsigned char encoding_type;

    unsigned char *buf16;
    char *buf8;

    char *p_input = p_encoded;

    if( strlen( p_input ) < 15 )
    {
        msg_Err( p_demux, "input string less than 15 characters" );
        return NULL;
    }

    if( load_byte( 1, &encoding_type, &p_input, NULL, NULL ) )
    {
        msg_Err( p_demux, "unable to get NSC encoding type" );
        return NULL;
    }

    if( encoding_type != 1 && encoding_type != 2 )
    {
        msg_Err( p_demux, "encoding type %d is not supported",
                 encoding_type );
        return NULL;
    }

    j = k = 0;

    if( load_byte( encoding_type, &tmp, &p_input, &j, &k ) )
    {
        msg_Err( p_demux, "load_byte failed" );
        return NULL;
    }

    for( i = 0; i < 4; i++ )
    {
        if( load_byte( encoding_type, &tmp, &p_input, &j, &k ) )
        {
            msg_Err( p_demux, "load_byte failed" );
            return NULL;
        }
    }

    length = 0;
    for( i = 4; i; i-- )
    {
        if( load_byte( encoding_type, &tmp, &p_input, &j, &k ) )
        {
            msg_Err( p_demux, "load_byte failed" );
            return NULL;
        }
        length |= tmp << ((i - 1) * 8);
    }

    if( length == 0 )
    {
        msg_Err( p_demux, "Length is 0" );
        return NULL;
    }

    buf16 = malloc( length );
    if( buf16 == NULL )
        return NULL;

    for( i = 0; i < length; i++ )
    {
        if( load_byte( encoding_type, &buf16[ i ], &p_input, &j, &k ) )
        {
            msg_Err( p_demux, "load_byte failed" );
            free( buf16 );
            return NULL;
        }
    }

    buf8 = FromCharset( "UTF-16LE", buf16, length );
    free( buf16 );
    if( buf8 == NULL )
    {
        msg_Err( p_demux, "iconv failed" );
        return NULL;
    }
    return buf8;
}

static int DemuxOpen( vlc_object_t * p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    const uint8_t *p_peek;
    int i_size;

    /* Lets check the content to see if this is a NSC file */
    i_size = stream_Peek( p_demux->s, &p_peek, MAX_LINE );
    i_size -= sizeof("NSC Format Version=") - 1;

    if ( i_size > 0 )
    {
        while ( i_size && strncasecmp( (char *)p_peek, "NSC Format Version=",
                                       (int) sizeof("NSC Format Version=") - 1 ) )
        {
            p_peek++;
            i_size--;
        }
        if ( !strncasecmp( (char *)p_peek, "NSC Format Version=",
                           (int) sizeof("NSC Format Version=") -1 ) )
        {
            p_demux->pf_demux = Demux;
            p_demux->pf_control = Control;
            return VLC_SUCCESS;
        }
    }
    return VLC_EGENERIC;
}


/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
static void DemuxClose( vlc_object_t *p_this )
{
    VLC_UNUSED( p_this );
    return;
}

static int ParseLine ( demux_t *p_demux, char *psz_line )
{
    char        *psz_bol;
    char        *psz_value;

    psz_bol = psz_line;
    /* Remove unnecessary tabs or spaces at the beginning of line */
    while( *psz_bol == ' ' || *psz_bol == '\t' ||
           *psz_bol == '\n' || *psz_bol == '\r' )
    {
        psz_bol++;
    }
    psz_value = strchr( psz_bol, '=' );
    if( psz_value == NULL )
    {
        return 0; /* a [Address] or [Formats] line or something else we will ignore */
    }
    *psz_value = '\0';
    psz_value++;
 
    if( !strncasecmp( psz_value, "0x", 2 ) )
    {
        int i_value;
        sscanf( psz_value, "%x", &i_value );
        msg_Dbg( p_demux, "%s = %d", psz_bol, i_value );
    }
    else if( !strncasecmp( psz_bol, "Format", 6 ) )
    {
        msg_Dbg( p_demux, "%s = asf header", psz_bol );
    }
    else
    {
        /* This should be NSC encoded strings in the values */
        char *psz_out;
        psz_out = nscdec( (vlc_object_t *)p_demux, psz_value );
        if( psz_out )
        {
            msg_Dbg( p_demux, "%s = %s", psz_bol, psz_out );
            free( psz_out );
        }
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux ( demux_t *p_demux )
{
    char            *psz_line;

    while( ( psz_line = stream_ReadLine( p_demux->s ) ) )
    {
        ParseLine( p_demux, psz_line );
        free( psz_line );
    }
    return VLC_SUCCESS;
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    VLC_UNUSED( p_demux ); VLC_UNUSED( i_query ); VLC_UNUSED( args );
    //FIXME
    return VLC_EGENERIC;
}
