/*****************************************************************************
 * langfromtelx.c: dynamic language setting from telx
 *****************************************************************************
 * Copyright © 2009-2011 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
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

#include <ctype.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_block.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define ID_TEXT N_("Elementary Stream ID")
#define ID_LONGTEXT N_( \
    "Specify an identifier integer for this elementary stream to change" )
#define MAGAZINE_TEXT N_("Magazine")
#define MAGAZINE_LONGTEXT N_( \
    "Specify the magazine containing the language page" )
#define PAGE_TEXT N_("Page")
#define PAGE_LONGTEXT N_( \
    "Specify the page containing the language" )
#define ROW_TEXT N_("Row")
#define ROW_LONGTEXT N_( \
    "Specify the row containing the language" )

static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-langfromtelx-"

vlc_module_begin()
    set_shortname( N_("Lang From Telx"))
    set_description( N_("Dynamic language setting from teletext"))
    set_capability( "sout stream", 50 )
    add_shortcut( "langfromtelx" )
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_STREAM )

    set_callbacks( Open, Close )
    add_integer( SOUT_CFG_PREFIX "id", 0, ID_TEXT, ID_LONGTEXT,
                 false )
    add_integer( SOUT_CFG_PREFIX "magazine", 7, MAGAZINE_TEXT,
                 MAGAZINE_LONGTEXT, false )
    add_integer( SOUT_CFG_PREFIX "page", 0x99, PAGE_TEXT, PAGE_LONGTEXT,
                 false )
    add_integer( SOUT_CFG_PREFIX "row", 1, ROW_TEXT, ROW_LONGTEXT,
                 false )
vlc_module_end();


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static const char *ppsz_sout_options[] = {
    "id", "magazine", "page", "row", NULL
};

static sout_stream_id_t *Add   ( sout_stream_t *, es_format_t * );
static int               Del   ( sout_stream_t *, sout_stream_id_t * );
static int               Send  ( sout_stream_t *, sout_stream_id_t *, block_t * );

struct sout_stream_sys_t
{
    int i_id, i_magazine, i_page, i_row;
    char *psz_language, *psz_old_language;
    sout_stream_id_t *p_id, *p_telx;
    int i_current_page;
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys;

    if( !p_stream->p_next )
    {
        msg_Err( p_stream, "cannot create chain" );
        return VLC_EGENERIC;
    }

    p_sys = malloc( sizeof( sout_stream_sys_t ) );
    if( unlikely( !p_sys ) )
        return VLC_ENOMEM;

    config_ChainParse( p_stream, SOUT_CFG_PREFIX, ppsz_sout_options,
                       p_stream->p_cfg );

    p_sys->i_id       = var_GetInteger( p_stream, SOUT_CFG_PREFIX "id" );
    p_sys->i_magazine = var_GetInteger( p_stream, SOUT_CFG_PREFIX "magazine" );
    p_sys->i_page     = var_GetInteger( p_stream, SOUT_CFG_PREFIX "page" );
    p_sys->i_row      = var_GetInteger( p_stream, SOUT_CFG_PREFIX "row" );

    p_stream->pf_add  = Add;
    p_stream->pf_del  = Del;
    p_stream->pf_send = Send;

    p_stream->p_sys   = p_sys;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *)p_stream->p_sys;

    free( p_sys->psz_old_language );
    free( p_sys );
}

static sout_stream_id_t * Add( sout_stream_t *p_stream, es_format_t *p_fmt )
{
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *)p_stream->p_sys;

    if ( p_fmt->i_id == p_sys->i_id )
    {
        p_sys->psz_old_language = p_fmt->psz_language;
        msg_Dbg( p_stream,
                 "changing language of ID %d (magazine %d page %x row %d)",
                 p_sys->i_id, p_sys->i_magazine, p_sys->i_page, p_sys->i_row );
        p_sys->psz_language = p_fmt->psz_language = malloc(4);
        if ( p_sys->psz_old_language != NULL )
            strncpy( p_fmt->psz_language, p_sys->psz_old_language, 3 );
        else
            strcpy( p_fmt->psz_language, "unk" );
        p_fmt->psz_language[3] = '\0';

        p_sys->p_id = p_stream->p_next->pf_add( p_stream->p_next, p_fmt );
        return p_sys->p_id;
    }

    if ( p_fmt->i_codec == VLC_CODEC_TELETEXT )
    {
        p_sys->p_telx = p_stream->p_next->pf_add( p_stream->p_next, p_fmt );
        return p_sys->p_telx;
    }

    return p_stream->p_next->pf_add( p_stream->p_next, p_fmt );
}

static int Del( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *)p_stream->p_sys;

    if ( id == p_sys->p_id ) p_sys->p_id = NULL;
    if ( id == p_sys->p_telx ) p_sys->p_telx = NULL;

    return p_stream->p_next->pf_del( p_stream->p_next, id );
}

static void SetLanguage( sout_stream_t *p_stream, char *psz_language )
{
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *)p_stream->p_sys;

    if ( strncmp( p_sys->psz_language, psz_language, 3 ) )
        msg_Dbg( p_stream, "changing language to %s", psz_language );

    strncpy( p_sys->psz_language, (const char *)psz_language, 3 );
}

/*****************************************************************************
 * Teletext stuff
 *****************************************************************************/
static uint8_t bytereverse( int n )
{
    n = (((n >> 1) & 0x55) | ((n << 1) & 0xaa));
    n = (((n >> 2) & 0x33) | ((n << 2) & 0xcc));
    n = (((n >> 4) & 0x0f) | ((n << 4) & 0xf0));
    return n;
}

static int hamming_8_4( int a )
{
    switch (a) {
    case 0xA8:
        return 0;
    case 0x0B:
        return 1;
    case 0x26:
        return 2;
    case 0x85:
        return 3;
    case 0x92:
        return 4;
    case 0x31:
        return 5;
    case 0x1C:
        return 6;
    case 0xBF:
        return 7;
    case 0x40:
        return 8;
    case 0xE3:
        return 9;
    case 0xCE:
        return 10;
    case 0x6D:
        return 11;
    case 0x7A:
        return 12;
    case 0xD9:
        return 13;
    case 0xF4:
        return 14;
    case 0x57:
        return 15;
    default:
        return -1;     // decoding error , not yet corrected
    }
}

static void decode_string( char *res, uint8_t *packet, int len )
{
    for ( int i = 0; i < len; i++ )
        res[i] = bytereverse( packet[i] ) & 0x7f;

    res[len] = '\0';
}

static void HandleTelx( sout_stream_t *p_stream, block_t *p_block )
{
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *)p_stream->p_sys;
    int len = p_block->i_buffer;

    for ( int offset = 1; offset + 46 <= len; offset += 46 )
    {
        uint8_t * packet = (uint8_t *) p_block->p_buffer+offset;
        if ( packet[0] == 0xFF )
            continue;

        int mpag = (hamming_8_4( packet[4] ) << 4) | hamming_8_4( packet[5] );
        int i_row, i_magazine;
        if ( mpag < 0 )
        {
            /* decode error */
            continue;
        }

        i_row = 0xFF & bytereverse(mpag);
        i_magazine = (7 & i_row) == 0 ? 8 : (7 & i_row);
        i_row >>= 3;

        if ( i_magazine != p_sys->i_magazine )
            continue;

        if ( i_row == 0 )
        {
            /* row 0 : flags and header line */
            p_sys->i_current_page =
                (0xF0 & bytereverse( hamming_8_4(packet[7]) )) |
                (0xF & (bytereverse( hamming_8_4(packet[6]) ) >> 4) );
        }
        if ( p_sys->i_current_page != p_sys->i_page ) continue;
        if ( i_row == p_sys->i_row )
        {
            /* row 1-23 : normal lines */
            char psz_line[41];
            decode_string( psz_line, packet + 6, 40 );
            psz_line[0] = tolower(psz_line[0]);
            psz_line[1] = tolower(psz_line[1]);
            psz_line[2] = tolower(psz_line[2]);
            psz_line[3] = '\0';
            SetLanguage( p_stream, psz_line );
        }
    }
}

/*****************************************************************************
 * Send:
 *****************************************************************************/
static int Send( sout_stream_t *p_stream, sout_stream_id_t *id,
                 block_t *p_buffer )
{
    sout_stream_sys_t *p_sys = (sout_stream_sys_t *)p_stream->p_sys;

    if ( id == p_sys->p_telx )
        HandleTelx( p_stream, p_buffer );

    return p_stream->p_next->pf_send( p_stream->p_next, id, p_buffer );
}
