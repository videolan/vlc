/*****************************************************************************
 * flac.c : FLAC demux module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2003 VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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
#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc_codec.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open  ( vlc_object_t * );
static void Close ( vlc_object_t * );

vlc_module_begin();
    set_description( _("FLAC demuxer") );
    set_capability( "demux2", 155 );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    set_callbacks( Open, Close );
    add_shortcut( "flac" );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux  ( demux_t * );
static int Control( demux_t *, int, va_list );

struct demux_sys_t
{
    vlc_bool_t  b_start;
    es_out_id_t *p_es;

    /* Packetizer */
    decoder_t *p_packetizer;
    vlc_meta_t *p_meta;
};

#define STREAMINFO_SIZE 38
#define FLAC_PACKET_SIZE 16384

/*****************************************************************************
 * Open: initializes ES structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    module_t    *p_id3;
    demux_sys_t *p_sys;
    int          i_peek;
    byte_t      *p_peek;
    es_format_t  fmt;
    vlc_meta_t  *p_meta = NULL;

    /* Skip/parse possible id3 header */
    if( ( p_id3 = module_Need( p_demux, "id3", NULL, 0 ) ) )
    {
        p_meta = (vlc_meta_t *)p_demux->p_private;
        p_demux->p_private = NULL;
        module_Unneed( p_demux, p_id3 );
    }
    
    /* Have a peep at the show. */
    if( stream_Peek( p_demux->s, &p_peek, 4 ) < 4 )
    {
        if( p_meta ) vlc_meta_Delete( p_meta );
        return VLC_EGENERIC;
    }

    if( p_peek[0]!='f' || p_peek[1]!='L' || p_peek[2]!='a' || p_peek[3]!='C' )
    {
        if( strncmp( p_demux->psz_demux, "flac", 4 ) )
        {
            if( p_meta ) vlc_meta_Delete( p_meta );
            return VLC_EGENERIC;
        }
        /* User forced */
        msg_Err( p_demux, "this doesn't look like a flac stream, "
                 "continuing anyway" );
    }

    p_demux->pf_demux   = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys      = p_sys = malloc( sizeof( demux_sys_t ) );
    es_format_Init( &fmt, AUDIO_ES, VLC_FOURCC( 'f', 'l', 'a', 'c' ) );
    p_sys->b_start = VLC_TRUE;
    p_sys->p_meta = p_meta;

    /* We need to read and store the STREAMINFO metadata */
    i_peek = stream_Peek( p_demux->s, &p_peek, 8 );
    if( p_peek[4] & 0x7F )
    {
        msg_Err( p_demux, "this isn't a STREAMINFO metadata block" );
        if( p_meta ) vlc_meta_Delete( p_meta );
        return VLC_EGENERIC;
    }

    if( ((p_peek[5]<<16)+(p_peek[6]<<8)+p_peek[7]) != (STREAMINFO_SIZE - 4) )
    {
        msg_Err( p_demux, "invalid size for a STREAMINFO metadata block" );
        if( p_meta ) vlc_meta_Delete( p_meta );
        return VLC_EGENERIC;
    }

    /*
     * Load the FLAC packetizer
     */
    p_sys->p_packetizer = vlc_object_create( p_demux, VLC_OBJECT_DECODER );
    p_sys->p_packetizer->pf_decode_audio = 0;
    p_sys->p_packetizer->pf_decode_video = 0;
    p_sys->p_packetizer->pf_decode_sub = 0;
    p_sys->p_packetizer->pf_packetize = 0;

    /* Initialization of decoder structure */
    es_format_Init( &p_sys->p_packetizer->fmt_in, AUDIO_ES,
                    VLC_FOURCC( 'f', 'l', 'a', 'c' ) );

    /* Store STREAMINFO for the decoder and packetizer */
    p_sys->p_packetizer->fmt_in.i_extra = fmt.i_extra = STREAMINFO_SIZE + 4;
    p_sys->p_packetizer->fmt_in.p_extra = malloc( STREAMINFO_SIZE + 4 );
    stream_Read( p_demux->s, p_sys->p_packetizer->fmt_in.p_extra,
                 STREAMINFO_SIZE + 4 );

    /* Fake this as the last metadata block */
    ((uint8_t*)p_sys->p_packetizer->fmt_in.p_extra)[4] |= 0x80;
    fmt.p_extra = malloc( STREAMINFO_SIZE + 4 );
    memcpy( fmt.p_extra, p_sys->p_packetizer->fmt_in.p_extra,
            STREAMINFO_SIZE + 4 );

    p_sys->p_packetizer->p_module =
        module_Need( p_sys->p_packetizer, "packetizer", NULL, 0 );
    if( !p_sys->p_packetizer->p_module )
    {
        if( p_sys->p_packetizer->fmt_in.p_extra )
            free( p_sys->p_packetizer->fmt_in.p_extra );

        vlc_object_destroy( p_sys->p_packetizer );
        msg_Err( p_demux, "cannot find flac packetizer" );
        if( p_meta ) vlc_meta_Delete( p_meta );
        return VLC_EGENERIC;
    }

    p_sys->p_es = es_out_Add( p_demux->out, &fmt );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    /* Unneed module */
    module_Unneed( p_sys->p_packetizer, p_sys->p_packetizer->p_module );

    if( p_sys->p_packetizer->fmt_in.p_extra )
        free( p_sys->p_packetizer->fmt_in.p_extra );

    /* Delete the decoder */
    vlc_object_destroy( p_sys->p_packetizer );
    if( p_sys->p_meta ) vlc_meta_Delete( p_sys->p_meta );
    free( p_sys );
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t     *p_block_in, *p_block_out;

    if( !( p_block_in = stream_Block( p_demux->s, FLAC_PACKET_SIZE ) ) )
    {
        return 0;
    }

    if( p_sys->b_start )
    {
        p_block_in->i_pts = p_block_in->i_dts = 1;
        p_sys->b_start = VLC_FALSE;
    }
    else
    {
        p_block_in->i_pts = p_block_in->i_dts = 0;
    }

    while( (p_block_out = p_sys->p_packetizer->pf_packetize(
                p_sys->p_packetizer, &p_block_in )) )
    {
        while( p_block_out )
        {
            block_t *p_next = p_block_out->p_next;

            /* set PCR */
            es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_block_out->i_dts );

            es_out_Send( p_demux->out, p_sys->p_es, p_block_out );

            p_block_out = p_next;
        }
    }

    return 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    /* demux_sys_t *p_sys  = p_demux->p_sys; */
    /* FIXME bitrate */
    if( i_query == DEMUX_SET_TIME )
        return VLC_EGENERIC;
    else if( i_query == DEMUX_GET_META )
    {        
        vlc_meta_t **pp_meta = (vlc_meta_t **)va_arg( args, vlc_meta_t** );
        if( p_demux->p_sys->p_meta ) *pp_meta = vlc_meta_Duplicate( p_demux->p_sys->p_meta );
        else *pp_meta = NULL;
        return VLC_SUCCESS;
    }
    else
        return demux2_vaControlHelper( p_demux->s,
                                       0, -1,
                                       8*0, 1, i_query, args );
}

