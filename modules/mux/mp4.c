/*****************************************************************************
 * mp4.c
 *****************************************************************************
 * Copyright (C) 2001, 2002, 2003 VideoLAN
 * $Id: mp4.c,v 1.1 2003/04/18 22:43:08 fenrir Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/sout.h>

#include "codecs.h"

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int     Open   ( vlc_object_t * );
static void    Close  ( vlc_object_t * );

static int Capability(sout_mux_t *, int, void *, void * );
static int AddStream( sout_mux_t *, sout_input_t * );
static int DelStream( sout_mux_t *, sout_input_t * );
static int Mux      ( sout_mux_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("MP4/MOV muxer") );
    set_capability( "sout mux", 5 );
    add_shortcut( "mp4" );
    add_shortcut( "mov" );
    set_callbacks( Open, Close );
vlc_module_end();

typedef struct
{
    uint64_t i_pos;
    int      i_size;

    mtime_t  i_pts;
    mtime_t  i_dts;
    mtime_t  i_length;

} mp4_entry_t;

typedef struct
{
    sout_format_t *p_fmt;
    int           i_track_id;

    /* index */
    unsigned int i_entry_count;
    unsigned int i_entry_max;
    mp4_entry_t  *entry;

    /* stats */
    mtime_t      i_duration;
} mp4_stream_t;

struct sout_mux_sys_t
{
    uint64_t i_mdat_pos;
    uint64_t i_pos;

    int          i_nb_streams;
    mp4_stream_t **pp_streams;
};


typedef struct bo_t bo_t;
struct bo_t
{
    vlc_bool_t b_grow;

    int        i_buffer_size;
    int        i_buffer;
    uint8_t    *p_buffer;

};

static void bo_init     ( bo_t *, int , uint8_t *, vlc_bool_t  );
static void bo_add_8    ( bo_t *, uint8_t );
static void bo_add_16be ( bo_t *, uint16_t );
static void bo_add_24be ( bo_t *, uint32_t );
static void bo_add_32be ( bo_t *, uint32_t );
static void bo_add_64be ( bo_t *, uint64_t );
static void bo_add_fourcc(bo_t *, char * );
static void bo_add_bo   ( bo_t *, bo_t * );
static void bo_add_mem  ( bo_t *, int , uint8_t * );

static void bo_fix_32be ( bo_t *, int , uint32_t );

static bo_t *   box_new     ( char *fcc );
static bo_t *   box_full_new( char *fcc, uint8_t v, uint32_t f );
static void     box_fix     ( bo_t *box );
static void     box_free( bo_t *box );
static void     box_gather ( bo_t *box, bo_t *box2 );

static void     box_send( sout_mux_t *p_mux,  bo_t *box );

static sout_buffer_t * bo_to_sout( sout_instance_t *p_sout,  bo_t *box );

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_mux_t      *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t  *p_sys;
    bo_t            *box;

    p_sys = malloc( sizeof( sout_mux_sys_t ) );
    p_sys->i_pos        = 0;
    p_sys->i_nb_streams = 0;
    p_sys->pp_streams   = NULL;


    msg_Info( p_mux, "Open" );

    p_mux->pf_capacity  = Capability;
    p_mux->pf_addstream = AddStream;
    p_mux->pf_delstream = DelStream;
    p_mux->pf_mux       = Mux;
    p_mux->p_sys        = p_sys;
    //p_mux->i_preheader  = 0;

    /* Now add ftyp header */
    box = box_new( "ftyp" );
    bo_add_fourcc( box, "isom" );
    bo_add_32be  ( box, 0 );
    bo_add_fourcc( box, "mp41" );
    box_fix( box );

    p_sys->i_pos += box->i_buffer;
    p_sys->i_mdat_pos = p_sys->i_pos;

    box_send( p_mux, box );

    /* Now add mdat header */
    box = box_new( "mdat" );
    bo_add_64be  ( box, 0 );    // XXX large size

    p_sys->i_pos += box->i_buffer;

    box_send( p_mux, box );

    return VLC_SUCCESS;
}

static uint32_t GetDescriptorLength24b( int i_length )
{
    uint32_t    i_l1, i_l2, i_l3;

    i_l1 = i_length&0x7f;
    i_l2 = ( i_length >> 7 )&0x7f;
    i_l3 = ( i_length >> 14 )&0x7f;

    return( 0x808000 | ( i_l3 << 16 ) | ( i_l2 << 8 ) | i_l1 );
}

static bo_t *GetESDS( mp4_stream_t *p_stream )
{
    bo_t *esds;
    int  i_stream_type;
    int  i_object_type_indication;
    int  i_decoder_specific_info_size;

    if( p_stream->p_fmt->i_extra_data > 0 )
    {
        i_decoder_specific_info_size = p_stream->p_fmt->i_extra_data + 4;
    }
    else
    {
        i_decoder_specific_info_size = 0;
    }

    esds = box_full_new( "esds", 0, 0 );

    bo_add_8   ( esds, 0x03 );      // ES_DescrTag
    bo_add_24be( esds, GetDescriptorLength24b( 25 + i_decoder_specific_info_size ) );
    bo_add_16be( esds, p_stream->i_track_id );
    bo_add_8   ( esds, 0x1f );      // flags=0|streamPriority=0x1f

    bo_add_8   ( esds, 0x04 );      // DecoderConfigDescrTag
    bo_add_24be( esds, GetDescriptorLength24b( 13 + i_decoder_specific_info_size ) );
    switch( p_stream->p_fmt->i_fourcc )
    {
        case VLC_FOURCC( 'm', 'p', '4', 'v' ):
            i_object_type_indication = 0x20;
            break;
        case VLC_FOURCC( 'm', 'p', 'g', 'v' ):
            i_object_type_indication = 0x60; /* FIXME MPEG-I=0x6b, MPEG-II = 0x60 -> 0x65 */
            break;
        case VLC_FOURCC( 'm', 'p', '4', 'a' ):
            i_object_type_indication = 0x40;        /* FIXME for mpeg2-aac == 0x66->0x68 */
            break;
        case VLC_FOURCC( 'm', 'p', 'g', 'a' ):
            i_object_type_indication = p_stream->p_fmt->i_sample_rate < 32000 ? 0x69 : 0x6b;
            break;
        default:
            i_object_type_indication = 0x00;
            break;
    }
    i_stream_type = p_stream->p_fmt->i_cat == VIDEO_ES ? 0x04 : 0x05;

    bo_add_8   ( esds, i_object_type_indication );
    bo_add_8   ( esds, ( i_stream_type << 2 ) | 1 );
    bo_add_24be( esds, 1024 * 1024 );       // bufferSizeDB
    bo_add_32be( esds, 0x7fffffff );        // maxBitrate
    bo_add_32be( esds, 0 );                 // avgBitrate
    if( p_stream->p_fmt->i_extra_data > 0 )
    {
        int i;
        bo_add_8   ( esds, 0x05 );  // DecoderSpecificInfo
        bo_add_24be( esds, GetDescriptorLength24b( p_stream->p_fmt->i_extra_data ) );

        for( i = 0; i < p_stream->p_fmt->i_extra_data; i++ )
        {
            bo_add_8   ( esds, p_stream->p_fmt->p_extra_data[i] );
        }
    }

    /* SL_Descr mandatory */
    bo_add_8   ( esds, 0x06 );  // SLConfigDescriptorTag
    bo_add_24be( esds, GetDescriptorLength24b( 1 ) );
    bo_add_8   ( esds, 0x02 );  // sl_predefined


    box_fix( esds );


    return esds;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static uint32_t mvhd_matrix[9] = { 0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x40000000 };

static void Close( vlc_object_t * p_this )
{
    sout_mux_t *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t  *p_sys = p_mux->p_sys;
    sout_buffer_t   *p_hdr;
    bo_t            bo;
    bo_t            *moov, *mvhd;
    unsigned int    i;
    int             i_trak, i_index;

    uint32_t        i_movie_timescale = 90000;
    int64_t         i_movie_duration  = 0;
    msg_Info( p_mux, "Close" );

    /* create general info */
    for( i_trak = 0; i_trak < p_sys->i_nb_streams; i_trak++ )
    {
        mp4_stream_t *p_stream;

        p_stream = p_sys->pp_streams[i_trak];

        i_movie_duration = __MAX( i_movie_duration, p_stream->i_duration );
    }
    msg_Dbg( p_mux, "movie duration %ds", (uint32_t)( i_movie_duration / (mtime_t)1000000 ) );

    i_movie_duration = i_movie_duration * (int64_t)i_movie_timescale / (int64_t)1000000;

    /* *** update mdat size *** */
    bo_init      ( &bo, 0, NULL, VLC_TRUE );
    bo_add_32be  ( &bo, 1 );
    bo_add_fourcc( &bo, "mdat" );
    bo_add_64be  ( &bo, p_sys->i_pos - p_sys->i_mdat_pos );
    p_hdr = bo_to_sout( p_mux->p_sout, &bo );
    free( bo.p_buffer );

    /* seek to mdat */
    sout_AccessOutSeek( p_mux->p_access, p_sys->i_mdat_pos );
    sout_AccessOutWrite( p_mux->p_access, p_hdr );

    /* Now create header */
    sout_AccessOutSeek( p_mux->p_access, p_sys->i_pos );

    moov = box_new( "moov" );

    /* *** add /moov/mvhd *** */
    mvhd = box_full_new( "mvhd", 1, 0 );
    bo_add_64be( mvhd, 0    );              // creation time FIXME
    bo_add_64be( mvhd, 0    );              // modification time    FIXME
    bo_add_32be( mvhd, i_movie_timescale);  // timescale
    bo_add_64be( mvhd, i_movie_duration );  // duration
    bo_add_32be( mvhd, 0x10000 );           // rate
    bo_add_16be( mvhd, 0x100 );             // volume
    bo_add_16be( mvhd, 0 );                 // reserved
    for( i = 0; i < 2; i++ )
    {
        bo_add_32be( mvhd, 0 );             // reserved
    }
    for( i = 0; i < 9; i++ )
    {
        bo_add_32be( mvhd, mvhd_matrix[i] );// matrix
    }
    for( i = 0; i < 6; i++ )
    {
        bo_add_32be( mvhd, 0 );             // pre-defined
    }
    bo_add_32be( mvhd, 0xffffffff );        // next-track-id    FIXME
    box_fix( mvhd );
    box_gather( moov, mvhd );

    for( i_trak = 0; i_trak < p_sys->i_nb_streams; i_trak++ )
    {
        mp4_stream_t *p_stream;
        uint32_t     i_timescale;
        uint32_t     i_chunk_count;

        bo_t *trak;
        bo_t *tkhd;
        bo_t *mdia;
        bo_t *mdhd, *hdlr;
        bo_t *minf;
        bo_t *dinf;
        bo_t *dref;
        bo_t *url;
        bo_t *stbl;
        bo_t *stsd;
        bo_t *stts;
        bo_t *stsz;

        p_stream = p_sys->pp_streams[i_trak];

        if( p_stream->p_fmt->i_cat != AUDIO_ES && p_stream->p_fmt->i_cat != VIDEO_ES )
        {
            msg_Err( p_mux, "FIXME ignoring trak (noaudio&&novideo)" );
            continue;
        }
        if( p_stream->p_fmt->i_cat == AUDIO_ES )
        {
            i_timescale = p_stream->p_fmt->i_sample_rate;
        }
        else
        {
            i_timescale = 1001;
        }

        /* *** add /moov/trak *** */
        trak = box_new( "trak" );

        /* *** add /moov/trak/tkhd *** */
        tkhd = box_full_new( "tkhd", 1, 1 );
        bo_add_64be( tkhd, 0    );                  // creation time
        bo_add_64be( tkhd, 0    );                  // modification time
        bo_add_32be( tkhd, p_stream->i_track_id );
        bo_add_32be( tkhd, 0 );                     // reserved 0
        bo_add_64be( tkhd, p_stream->i_duration *
                           (int64_t)i_movie_timescale /
                           (mtime_t)1000000 );      // duration
        for( i = 0; i < 2; i++ )
        {
            bo_add_32be( tkhd, 0 );                 // reserved
        }
        bo_add_16be( tkhd, 0 );                     // layer
        bo_add_16be( tkhd, 0 );                     // pre-defined
        bo_add_16be( tkhd, p_stream->p_fmt->i_cat == AUDIO_ES ? 0x100 : 0 ); // volume
        bo_add_16be( tkhd, 0 );                     // reserved
        for( i = 0; i < 9; i++ )
        {
            bo_add_32be( tkhd, mvhd_matrix[i] );    // matrix
        }
        if( p_stream->p_fmt->i_cat == AUDIO_ES )
        {
            bo_add_32be( tkhd, 0 );                 // width (presentation)
            bo_add_32be( tkhd, 0 );                 // height(presentation)
        }
        else
        {
            bo_add_32be( tkhd, p_stream->p_fmt->i_width  << 16 );     // width (presentation)
            bo_add_32be( tkhd, p_stream->p_fmt->i_height << 16 );     // height(presentation)
        }
        box_fix( tkhd );
        box_gather( trak, tkhd );

        /* *** add /moov/trak/mdia *** */
        mdia = box_new( "mdia" );

        /* */
        mdhd = box_full_new( "mdhd", 1, 0 );
        bo_add_64be( mdhd, 0    );              // creation time
        bo_add_64be( mdhd, 0    );              // modification time
        bo_add_32be( mdhd, i_timescale);        // timescale
        bo_add_64be( mdhd, p_stream->i_duration *
                           (int64_t)i_timescale /
                           (mtime_t)1000000 );  // duration

        bo_add_16be( mdhd, 0    );              // language   FIXME
        bo_add_16be( mdhd, 0    );              // predefined
        box_fix( mdhd );
        box_gather( mdia, mdhd );

        /* */
        hdlr = box_full_new( "hdlr", 0, 0 );
        bo_add_32be( hdlr, 0 );         // predefined
        if( p_stream->p_fmt->i_cat == AUDIO_ES )
        {
            bo_add_fourcc( hdlr, "soun" );
        }
        else
        {
            bo_add_fourcc( hdlr, "vide" );
        }
        for( i = 0; i < 3; i++ )
        {
            bo_add_32be( hdlr, 0 );     // reserved
        }
        bo_add_mem( hdlr, 2, "?" );

        box_fix( hdlr );
        box_gather( mdia, hdlr );

        /* minf*/
        minf = box_new( "minf" );

        /* add smhd|vmhd */
        if( p_stream->p_fmt->i_cat == AUDIO_ES )
        {
            bo_t *smhd;

            smhd = box_full_new( "smhd", 0, 0 );
            bo_add_16be( smhd, 0 );     // balance
            bo_add_16be( smhd, 0 );     // reserved
            box_fix( smhd );

            box_gather( minf, smhd );
        }
        else if( p_stream->p_fmt->i_cat == VIDEO_ES )
        {
            bo_t *vmhd;

            vmhd = box_full_new( "vmhd", 0, 1 );
            bo_add_16be( vmhd, 0 );     // graphicsmode
            for( i = 0; i < 3; i++ )
            {
                bo_add_16be( vmhd, 0 ); // opcolor
            }
            box_fix( vmhd );

            box_gather( minf, vmhd );
        }

        /* dinf */
        dinf = box_new( "dinf" );
        dref = box_full_new( "dref", 0, 0 );
        bo_add_32be( dref, 1 );
        url = box_full_new( "url ", 0, 0x01 );
        box_fix( url );
        box_gather( dref, url );
        box_fix( dref );
        box_gather( dinf, dref );

        /* append dinf to mdia */
        box_fix( dinf );
        box_gather( minf, dinf );

        /* add stbl */
        stbl = box_new( "stbl" );
        stsd = box_full_new( "stsd", 0, 0 );
        bo_add_32be( stsd, 1 );

        if( p_stream->p_fmt->i_cat == AUDIO_ES )
        {
            bo_t *soun;
            char fcc[4] = "    ";
            int  i;
            vlc_bool_t b_mpeg4_hdr;

            switch( p_stream->p_fmt->i_fourcc )
            {
                case VLC_FOURCC( 'm', 'p', '4', 'a' ):
                case VLC_FOURCC( 'm', 'p', 'g', 'a' ):
                    memcpy( fcc, "mp4a", 4 );
                    b_mpeg4_hdr = VLC_TRUE;
                    break;

                default:
                    memcpy( fcc, (char*)&p_stream->p_fmt->i_fourcc, 4 );
                    b_mpeg4_hdr = VLC_FALSE;
                    break;
            }

            soun = box_new( fcc );
            for( i = 0; i < 6; i++ )
            {
                bo_add_8( soun, 0 );        // reserved;
            }
            bo_add_16be( soun, 1 );         // data-reference-index
            bo_add_32be( soun, 0 );         // reserved;
            bo_add_32be( soun, 0 );         // reserved;
            bo_add_16be( soun, p_stream->p_fmt->i_channels );   // channel-count
            bo_add_16be( soun, 16);         // FIXME sample size
            bo_add_16be( soun, 0 );         // predefined
            bo_add_16be( soun, 0 );         // reserved
            bo_add_16be( soun, p_stream->p_fmt->i_sample_rate ); // sampleratehi
            bo_add_16be( soun, 0 );                              // sampleratelo

            /* add an ES Descriptor */
            if( b_mpeg4_hdr )
            {
                bo_t *esds;

                esds = GetESDS( p_stream );

                box_fix( esds );
                box_gather( soun, esds );
            }

            box_fix( soun );
            box_gather( stsd, soun );
        }
        else if( p_stream->p_fmt->i_cat == VIDEO_ES )
        {
            bo_t *vide;
            char fcc[4] = "    ";
            int  i;
            vlc_bool_t b_mpeg4_hdr;

            switch( p_stream->p_fmt->i_fourcc )
            {
                case VLC_FOURCC( 'm', 'p', '4', 'v' ):
                case VLC_FOURCC( 'm', 'p', 'g', 'v' ):
                    memcpy( fcc, "mp4v", 4 );
                    b_mpeg4_hdr = VLC_TRUE;
                    break;

                default:
                    memcpy( fcc, (char*)&p_stream->p_fmt->i_fourcc, 4 );
                    b_mpeg4_hdr = VLC_FALSE;
                    break;
            }

            vide = box_new( fcc );
            for( i = 0; i < 6; i++ )
            {
                bo_add_8( vide, 0 );        // reserved;
            }
            bo_add_16be( vide, 1 );         // data-reference-index

            bo_add_16be( vide, 0 );         // predefined;
            bo_add_16be( vide, 0 );         // reserved;
            for( i = 0; i < 3; i++ )
            {
                bo_add_32be( vide, 0 );     // predefined;
            }

            bo_add_16be( vide, p_stream->p_fmt->i_width );  // i_width
            bo_add_16be( vide, p_stream->p_fmt->i_height ); // i_height

            bo_add_32be( vide, 0x00480000 );                // h 72dpi
            bo_add_32be( vide, 0x00480000 );                // v 72dpi

            bo_add_32be( vide, 0 );         // reserved
            bo_add_16be( vide, 1 );         // predefined
            for( i = 0; i < 32; i++ )
            {
                bo_add_8( vide, 0 );        // compressor name;
            }
            bo_add_16be( vide, 0x18 );      // depth
            bo_add_16be( vide, 0xffff );    // predefined

            /* add an ES Descriptor */
            if( b_mpeg4_hdr )
            {
                bo_t *esds;

                esds = GetESDS( p_stream );

                box_fix( esds );
                box_gather( vide, esds );
            }

            box_fix( vide );
            box_gather( stsd, vide );
        }

        /* append stsd to stbl */
        box_fix( stsd );
        box_gather( stbl, stsd );

        /* we will create chunk table and stsc table FIXME optim stsc table FIXME */
        i_chunk_count = 0;
        for( i = 0; i < p_stream->i_entry_count; )
        {
            while( i < p_stream->i_entry_count )
            {
                if( i + 1 < p_stream->i_entry_count && p_stream->entry[i].i_pos + p_stream->entry[i].i_size != p_stream->entry[i + 1].i_pos )
                {
                    i++;
                    break;
                }

                i++;
            }
            i_chunk_count++;
        }

//        if( p_sys->i_pos >= 0xffffffff )
        {
            bo_t *co64;
            bo_t *stsc;

            unsigned int  i_chunk;

            msg_Dbg( p_mux, "creating %d chunk (co64)", i_chunk_count );

            co64 = box_full_new( "co64", 0, 0 );
            bo_add_32be( co64, i_chunk_count );

            stsc = box_full_new( "stsc", 0, 0 );
            bo_add_32be( stsc, i_chunk_count );     // entry-count
            for( i_chunk = 0, i = 0; i < p_stream->i_entry_count; i_chunk++ )
            {
                int i_first;
                bo_add_64be( co64, p_stream->entry[i].i_pos );

                i_first = i;

                while( i < p_stream->i_entry_count )
                {
                    if( i + 1 < p_stream->i_entry_count && p_stream->entry[i].i_pos + p_stream->entry[i].i_size != p_stream->entry[i + 1].i_pos )
                    {
                        i++;
                        break;
                    }

                    i++;
                }
                bo_add_32be( stsc, 1 + i_chunk );       // first-chunk
                bo_add_32be( stsc, i - i_first ) ;  // samples-per-chunk
                bo_add_32be( stsc, 1 );             // sample-descr-index
            }
            /* append co64 to stbl */
            box_fix( co64 );
            box_gather( stbl, co64 );

            /* append stsc to stbl */
            box_fix( stsc );
            box_gather( stbl, stsc );
        }
//        else
//        {
//              FIXME implement it
//        }


        /* add stts */
        stts = box_full_new( "stts", 0, 0 );
        bo_add_32be( stts, 0 );     // fixed latter
        for( i = 0, i_index = 0; i < p_stream->i_entry_count; i_index++)
        {
            int64_t i_delta;
            int     i_first;

            i_first = i;
            i_delta = p_stream->entry[i].i_length;

            while( i < p_stream->i_entry_count )
            {
                if( i + 1 < p_stream->i_entry_count && p_stream->entry[i + 1].i_length != i_delta )
                {
                    i++;
                    break;
                }

                i++;
            }

            bo_add_32be( stts, i - i_first );           // sample-count
            bo_add_32be( stts, i_delta *
                               (int64_t)i_timescale /
                               (int64_t)1000000 );      // sample-delta
        }
        bo_fix_32be( stts, 12, i_index );

        /* append stts to stbl */
        box_fix( stts );
        box_gather( stbl, stts );

        /* FIXME add ctts ?? FIXME */

        stsz = box_full_new( "stsz", 0, 0 );
        bo_add_32be( stsz, 0 );                             // sample-size
        bo_add_32be( stsz, p_stream->i_entry_count );       // sample-count
        for( i = 0; i < p_stream->i_entry_count; i++ )
        {
            bo_add_32be( stsz, p_stream->entry[i].i_size ); // sample-size
        }
        /* append stsz to stbl */
        box_fix( stsz );
        box_gather( stbl, stsz );

        /* append stbl to minf */
        box_fix( stbl );
        box_gather( minf, stbl );


        /* append minf to mdia */
        box_fix( minf );
        box_gather( mdia, minf );


        /* append mdia to trak */
        box_fix( mdia );
        box_gather( trak, mdia );

        /* append trak to moov */
        box_fix( trak );
        box_gather( moov, trak );
    }

    box_fix( moov );
    box_send( p_mux, moov );
}

static int Capability( sout_mux_t *p_mux, int i_query, void *p_args, void *p_answer )
{
   switch( i_query )
   {
        case SOUT_MUX_CAP_GET_ADD_STREAM_ANY_TIME:
            *(vlc_bool_t*)p_answer = VLC_FALSE;
            return( SOUT_MUX_CAP_ERR_OK );
        default:
            return( SOUT_MUX_CAP_ERR_UNIMPLEMENTED );
   }
}

static int AddStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    sout_mux_sys_t  *p_sys = p_mux->p_sys;
    mp4_stream_t    *p_stream;

    switch( p_input-p_fmt->i_fourcc )
    {
        case VLC_FOURCC( 'm', 'p', '4', 'a' ):
        case VLC_FOURCC( 'm', 'p', '4', 'v' ):
        case VLC_FOURCC( 'm', 'p', 'g', 'a' ):
        case VLC_FOURCC( 'm', 'p', 'g', 'v' ):
            break;
        default:
            msg_Err( p_mux,
                     "unsupported codec %4.4s in mp4",
                     (char*)&p_input-p_fmt->i_fourcc );
            return VLC_EGENERIC;
    }

    p_stream                = malloc( sizeof( mp4_stream_t ) );
    p_stream->p_fmt         = malloc( sizeof( sout_format_t ) );
    memcpy( p_stream->p_fmt, p_input->p_fmt, sizeof( sout_format_t ) );
    if( p_stream->p_fmt->i_extra_data )
    {
        p_stream->p_fmt->p_extra_data = malloc( p_stream->p_fmt->i_extra_data );
        memcpy( p_stream->p_fmt->p_extra_data,
                p_input->p_fmt->p_extra_data,
                p_input->p_fmt->i_extra_data );
    }
    p_stream->i_track_id    = p_sys->i_nb_streams + 1;
    p_stream->i_entry_count = 0;
    p_stream->i_entry_max   = 1000;
    p_stream->entry         = calloc( p_stream->i_entry_max, sizeof( mp4_entry_t ) );
    p_stream->i_duration    = 0;

    p_input->p_sys          = p_stream;

    msg_Dbg( p_mux, "adding input" );

    TAB_APPEND( p_sys->i_nb_streams, p_sys->pp_streams, p_stream );
    return( VLC_SUCCESS );
}

static int DelStream( sout_mux_t *p_mux, sout_input_t *p_input )
{

    msg_Dbg( p_mux, "removing input" );
    return( 0 );
}


/****************************************************************************/

static int MuxGetStream( sout_mux_t *p_mux,
                         int        *pi_stream,
                         mtime_t    *pi_dts )
{
    mtime_t i_dts;
    int     i_stream;
    int     i;

    for( i = 0, i_dts = 0, i_stream = -1; i < p_mux->i_nb_inputs; i++ )
    {
        sout_fifo_t  *p_fifo;

        p_fifo = p_mux->pp_inputs[i]->p_fifo;

        if( p_fifo->i_depth > 1 )
        {
            sout_buffer_t *p_buf;

            p_buf = sout_FifoShow( p_fifo );
            if( i_stream < 0 || p_buf->i_dts < i_dts )
            {
                i_dts = p_buf->i_dts;
                i_stream = i;
            }
        }
        else
        {
            return( -1 ); // wait that all fifo have at least 2 packets
        }
    }
    if( pi_stream )
    {
        *pi_stream = i_stream;
    }
    if( pi_dts )
    {
        *pi_dts = i_dts;
    }
    return( i_stream );
}


static int Mux      ( sout_mux_t *p_mux )
{
    sout_mux_sys_t  *p_sys = p_mux->p_sys;

    for( ;; )
    {
        sout_input_t    *p_input;
        int             i_stream;
        mp4_stream_t    *p_stream;
        sout_buffer_t   *p_data;
        mtime_t         i_dts;

        if( MuxGetStream( p_mux, &i_stream, &i_dts) < 0 )
        {
            return( VLC_SUCCESS );
        }


        p_input  = p_mux->pp_inputs[i_stream];
        p_stream = (mp4_stream_t*)p_input->p_sys;

        p_data  = sout_FifoGet( p_input->p_fifo );
        //msg_Dbg( p_mux, "stream=%d size=%6d pos=%8lld", i_stream, p_data->i_size, p_sys->i_pos );

        /* add index entry */
        p_stream->entry[p_stream->i_entry_count].i_pos   = p_sys->i_pos;
        p_stream->entry[p_stream->i_entry_count].i_size  = p_data->i_size;
        p_stream->entry[p_stream->i_entry_count].i_pts   = p_data->i_pts;
        p_stream->entry[p_stream->i_entry_count].i_dts   = p_data->i_dts;
        p_stream->entry[p_stream->i_entry_count].i_length= __MAX( p_data->i_length, 0 );

        p_stream->i_entry_count++;
        if( p_stream->i_entry_count >= p_stream->i_entry_max )
        {
            p_stream->i_entry_max += 1000;
            p_stream->entry = realloc( p_stream->entry, p_stream->i_entry_max * sizeof( mp4_entry_t ) );
        }

        /* update */
        p_stream->i_duration += __MAX( p_data->i_length, 0 );
        p_sys->i_pos += p_data->i_size;

        /* write data */
        sout_AccessOutWrite( p_mux->p_access, p_data );
    }

    return( VLC_SUCCESS );
}


/****************************************************************************/


static void bo_init( bo_t *p_bo, int i_size, uint8_t *p_buffer, vlc_bool_t b_grow )
{
    if( !p_buffer )
    {
        p_bo->i_buffer_size = __MAX( i_size, 1024 );
        p_bo->p_buffer = malloc( p_bo->i_buffer_size );
    }
    else
    {
        p_bo->i_buffer_size = i_size;
        p_bo->p_buffer = p_buffer;
    }

    p_bo->b_grow = b_grow;
    p_bo->i_buffer = 0;
}

static void bo_add_8( bo_t *p_bo, uint8_t i )
{
    if( p_bo->i_buffer < p_bo->i_buffer_size )
    {
        p_bo->p_buffer[p_bo->i_buffer] = i;
    }
    else if( p_bo->b_grow )
    {
        p_bo->i_buffer_size += 1024;
        p_bo->p_buffer = realloc( p_bo->p_buffer, p_bo->i_buffer_size );

        p_bo->p_buffer[p_bo->i_buffer] = i;
    }

    p_bo->i_buffer++;
}

static void bo_add_16be( bo_t *p_bo, uint16_t i )
{
    bo_add_8( p_bo, ( ( i >> 8) &0xff ) );
    bo_add_8( p_bo, i &0xff );
}

static void bo_add_24be( bo_t *p_bo, uint32_t i )
{
    bo_add_8( p_bo, ( ( i >> 16) &0xff ) );
    bo_add_8( p_bo, ( ( i >> 8) &0xff ) );
    bo_add_8( p_bo, (   i &0xff ) );
}
static void bo_add_32be( bo_t *p_bo, uint32_t i )
{
    bo_add_16be( p_bo, ( ( i >> 16) &0xffff ) );
    bo_add_16be( p_bo, i &0xffff );
}

static void bo_fix_32be ( bo_t *p_bo, int i_pos, uint32_t i)
{
    p_bo->p_buffer[i_pos    ] = ( i >> 24 )&0xff;
    p_bo->p_buffer[i_pos + 1] = ( i >> 16 )&0xff;
    p_bo->p_buffer[i_pos + 2] = ( i >>  8 )&0xff;
    p_bo->p_buffer[i_pos + 3] = ( i       )&0xff;
}

static void bo_add_64be( bo_t *p_bo, uint64_t i )
{
    bo_add_32be( p_bo, ( ( i >> 32) &0xffffffff ) );
    bo_add_32be( p_bo, i &0xffffffff );
}

static void bo_add_fourcc( bo_t *p_bo, char *fcc )
{
    bo_add_8( p_bo, fcc[0] );
    bo_add_8( p_bo, fcc[1] );
    bo_add_8( p_bo, fcc[2] );
    bo_add_8( p_bo, fcc[3] );
}

static void bo_add_mem( bo_t *p_bo, int i_size, uint8_t *p_mem )
{
    int i;

    for( i = 0; i < i_size; i++ )
    {
        bo_add_8( p_bo, p_mem[i] );
    }
}
static void bo_add_bo( bo_t *p_bo, bo_t *p_bo2 )
{
    int i;

    for( i = 0; i < p_bo2->i_buffer; i++ )
    {
        bo_add_8( p_bo, p_bo2->p_buffer[i] );
    }
}

static bo_t * box_new( char *fcc )
{
    bo_t *box;

    if( ( box = malloc( sizeof( bo_t ) ) ) )
    {
        bo_init( box, 0, NULL, VLC_TRUE );

        bo_add_32be  ( box, 0 );
        bo_add_fourcc( box, fcc );
    }

    return box;
}

static bo_t * box_full_new( char *fcc, uint8_t v, uint32_t f )
{
    bo_t *box;

    if( ( box = malloc( sizeof( bo_t ) ) ) )
    {
        bo_init( box, 0, NULL, VLC_TRUE );

        bo_add_32be  ( box, 0 );
        bo_add_fourcc( box, fcc );
        bo_add_8     ( box, v );
        bo_add_24be  ( box, f );
    }

    return box;
}

static void box_fix( bo_t *box )
{
    bo_t box_tmp;

    memcpy( &box_tmp, box, sizeof( bo_t ) );

    box_tmp.i_buffer = 0;
    bo_add_32be( &box_tmp, box->i_buffer );
}

static void box_free( bo_t *box )
{
    if( box->p_buffer )
    {
        free( box->p_buffer );
    }

    free( box );
}

static void box_gather ( bo_t *box, bo_t *box2 )
{
    bo_add_bo( box, box2 );
    box_free( box2 );
}


static sout_buffer_t * bo_to_sout( sout_instance_t *p_sout,  bo_t *box )
{
    sout_buffer_t *p_buf;

    p_buf = sout_BufferNew( p_sout, box->i_buffer );
    if( box->i_buffer > 0 )
    {
        memcpy( p_buf->p_buffer, box->p_buffer, box->i_buffer );
    }

    p_buf->i_size = box->i_buffer;

    return p_buf;
}


static void box_send( sout_mux_t *p_mux,  bo_t *box )
{
    sout_buffer_t *p_buf;

    p_buf = bo_to_sout( p_mux->p_sout, box );
    box_free( box );

    sout_AccessOutWrite( p_mux->p_access, p_buf );
}



