/*****************************************************************************
 * mp4.c: mp4/mov muxer
 *****************************************************************************
 * Copyright (C) 2001, 2002, 2003 VideoLAN
 * $Id: mp4.c,v 1.11 2004/01/24 11:56:16 gbazin Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin at videolan dot org>
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

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include "codecs.h"

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int  Open   ( vlc_object_t * );
static void Close  ( vlc_object_t * );

static int Capability(sout_mux_t *, int, void *, void * );
static int AddStream( sout_mux_t *, sout_input_t * );
static int DelStream( sout_mux_t *, sout_input_t * );
static int Mux      ( sout_mux_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define FASTSTART_TEXT N_("Create \"Fast start\" files")
#define FASTSTART_LONGTEXT N_( \
    "When this option is turned on, \"Fast start\" files will be created. " \
    "(\"Fast start\" files are optimized for download, allowing the user " \
    "to start previewing the file while it is downloading).")

vlc_module_begin();
    set_description( _("MP4/MOV muxer") );

    add_category_hint( "MP4/MOV muxer", NULL, VLC_TRUE );
        add_bool( "mp4-faststart", 1, NULL, FASTSTART_TEXT, FASTSTART_LONGTEXT, VLC_TRUE );

    set_capability( "sout mux", 5 );
    add_shortcut( "mp4" );
    add_shortcut( "mov" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
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
    es_format_t   *p_fmt;
    int           i_track_id;

    /* index */
    unsigned int i_entry_count;
    unsigned int i_entry_max;
    mp4_entry_t  *entry;

    /* stats */
    mtime_t      i_duration;

    /* for later stco fix-up (fast start files) */
    uint64_t i_stco_pos;
    vlc_bool_t b_stco64;

} mp4_stream_t;

struct sout_mux_sys_t
{
    vlc_bool_t b_mov;
    vlc_bool_t b_64_ext;
    vlc_bool_t b_fast_start;

    uint64_t i_mdat_pos;
    uint64_t i_pos;

    mtime_t  i_start_dts;

    int          i_nb_streams;
    mp4_stream_t **pp_streams;
};

typedef struct bo_t
{
    vlc_bool_t b_grow;

    int        i_buffer_size;
    int        i_buffer;
    uint8_t    *p_buffer;

} bo_t;

static void bo_init     ( bo_t *, int , uint8_t *, vlc_bool_t  );
static void bo_add_8    ( bo_t *, uint8_t );
static void bo_add_16be ( bo_t *, uint16_t );
static void bo_add_24be ( bo_t *, uint32_t );
static void bo_add_32be ( bo_t *, uint32_t );
static void bo_add_64be ( bo_t *, uint64_t );
static void bo_add_fourcc(bo_t *, char * );
static void bo_add_bo   ( bo_t *, bo_t * );
static void bo_add_mem  ( bo_t *, int , uint8_t * );
static void bo_add_descr( bo_t *, uint8_t , uint32_t );

static void bo_fix_32be ( bo_t *, int , uint32_t );

static bo_t *box_new     ( char *fcc );
static bo_t *box_full_new( char *fcc, uint8_t v, uint32_t f );
static void  box_fix     ( bo_t *box );
static void  box_free    ( bo_t *box );
static void  box_gather  ( bo_t *box, bo_t *box2 );

static void box_send( sout_mux_t *p_mux,  bo_t *box );

static int64_t get_timestamp();

static sout_buffer_t *bo_to_sout( sout_instance_t *p_sout,  bo_t *box );

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
    p_sys->i_mdat_pos   = 0;
    p_sys->b_mov        = p_mux->psz_mux && !strcmp( p_mux->psz_mux, "mov" );
    p_sys->i_start_dts  = 0;

    msg_Dbg( p_mux, "Open" );

    p_mux->pf_capacity  = Capability;
    p_mux->pf_addstream = AddStream;
    p_mux->pf_delstream = DelStream;
    p_mux->pf_mux       = Mux;
    p_mux->p_sys        = p_sys;

    if( !p_sys->b_mov )
    {
        /* Now add ftyp header */
        box = box_new( "ftyp" );
        bo_add_fourcc( box, "isom" );
        bo_add_32be  ( box, 0 );
        bo_add_fourcc( box, "mp41" );
        box_fix( box );

        p_sys->i_pos += box->i_buffer;
        p_sys->i_mdat_pos = p_sys->i_pos;

        box_send( p_mux, box );
    }

    /* FIXME FIXME
     * Quicktime actually doesn't like the 64 bits extensions !!! */
    p_sys->b_64_ext = VLC_FALSE;

    /* Now add mdat header */
    box = box_new( "mdat" );
    bo_add_64be  ( box, 0 ); // enough to store an extended size

    p_sys->i_pos += box->i_buffer;

    box_send( p_mux, box );

    return VLC_SUCCESS;
}

static int GetDescrLength( int i_size )
{
    if( i_size < 0x00000080 )
        return 2 + i_size;
    else if( i_size < 0x00004000 )
        return 3 + i_size;
    else if( i_size < 0x00200000 )
        return 4 + i_size;
    else
        return 5 + i_size;
}

static bo_t *GetESDS( mp4_stream_t *p_stream )
{
    bo_t *esds;
    int  i_stream_type;
    int  i_object_type_indication;
    int  i_decoder_specific_info_size;

    if( p_stream->p_fmt->i_extra > 0 )
    {
        i_decoder_specific_info_size =
            GetDescrLength( p_stream->p_fmt->i_extra );
    }
    else
    {
        i_decoder_specific_info_size = 0;
    }

    esds = box_full_new( "esds", 0, 0 );

    /* ES_Descr */
    bo_add_descr( esds, 0x03, 3 +
                  GetDescrLength( 13 + i_decoder_specific_info_size ) +
                  GetDescrLength( 1 ) );
    bo_add_16be( esds, p_stream->i_track_id );
    bo_add_8   ( esds, 0x1f );      // flags=0|streamPriority=0x1f

    /* DecoderConfigDescr */
    bo_add_descr( esds, 0x04, 13 + i_decoder_specific_info_size );

    switch( p_stream->p_fmt->i_codec )
    {
        case VLC_FOURCC( 'm', 'p', '4', 'v' ):
            i_object_type_indication = 0x20;
            break;
        case VLC_FOURCC( 'm', 'p', 'g', 'v' ):
            /* FIXME MPEG-I=0x6b, MPEG-II = 0x60 -> 0x65 */
            i_object_type_indication = 0x60;
            break;
        case VLC_FOURCC( 'm', 'p', '4', 'a' ):
            /* FIXME for mpeg2-aac == 0x66->0x68 */
            i_object_type_indication = 0x40;
            break;
        case VLC_FOURCC( 'm', 'p', 'g', 'a' ):
            i_object_type_indication =
                p_stream->p_fmt->audio.i_rate < 32000 ? 0x69 : 0x6b;
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

    if( p_stream->p_fmt->i_extra > 0 )
    {
        int i;

        /* DecoderSpecificInfo */
        bo_add_descr( esds, 0x05, p_stream->p_fmt->i_extra );

        for( i = 0; i < p_stream->p_fmt->i_extra; i++ )
        {
            bo_add_8( esds, ((uint8_t*)p_stream->p_fmt->p_extra)[i] );
        }
    }

    /* SL_Descr mandatory */
    bo_add_descr( esds, 0x06, 1 );
    bo_add_8    ( esds, 0x02 );  // sl_predefined

    box_fix( esds );

    return esds;
}

static bo_t *GetWaveTag( mp4_stream_t *p_stream )
{
    bo_t *wave;
    bo_t *box;

    wave = box_new( "wave" );

    box = box_new( "frma" );
    bo_add_fourcc( box, "mp4a" );
    box_fix( box );
    box_gather( wave, box );

    box = box_new( "mp4a" );
    bo_add_32be( box, 0 );
    box_fix( box );
    box_gather( wave, box );

    box = GetESDS( p_stream );
    box_fix( box );
    box_gather( wave, box );

    box = box_new( "srcq" );
    bo_add_32be( box, 0x40 );
    box_fix( box );
    box_gather( wave, box );

    /* wazza ? */
    bo_add_32be( wave, 8 ); /* new empty box */
    bo_add_32be( wave, 0 ); /* box label */

    box_fix( wave );

    return wave;
}

/* TODO: No idea about these values */
static bo_t *GetSVQ3Tag( mp4_stream_t *p_stream )
{
    bo_t *smi = box_new( "SMI " );

    bo_add_fourcc( smi, "SEQH" );
    bo_add_32be( smi, 0x5 );
    bo_add_32be( smi, 0xe2c0211d );
    bo_add_8( smi, 0xc0 );
    box_fix( smi );

    return smi;
}

static bo_t *GetUdtaTag( sout_mux_t *p_mux )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    bo_t *udta = box_new( "udta" );
    int i_track;

    /* Requirements */
    for( i_track = 0; i_track < p_sys->i_nb_streams; i_track++ )
    {
        mp4_stream_t *p_stream = p_sys->pp_streams[i_track];

        if( p_stream->p_fmt->i_codec == VLC_FOURCC('m','p','4','v') ||
            p_stream->p_fmt->i_codec == VLC_FOURCC('m','p','4','a') )
        {
            bo_t *box = box_new( "\251req" );
            /* String length */
            bo_add_16be( box, sizeof("QuickTime 6.0 or greater") - 1);
            bo_add_16be( box, 0 );
            bo_add_mem( box, sizeof("QuickTime 6.0 or greater") - 1,
                        "QuickTime 6.0 or greater" );
            box_fix( box );
            box_gather( udta, box );
            break;
        }
    }

    /* Encoder */
    {
        bo_t *box = box_new( "\251enc" );
        /* String length */
        bo_add_16be( box, sizeof(PACKAGE_STRING " stream output") - 1);
        bo_add_16be( box, 0 );
        bo_add_mem( box, sizeof(PACKAGE_STRING " stream output") - 1,
                    PACKAGE_STRING " stream output" );
        box_fix( box );
        box_gather( udta, box );
    }

    box_fix( udta );
    return udta;
}

static bo_t *GetSounBox( sout_mux_t *p_mux, mp4_stream_t *p_stream )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    vlc_bool_t b_descr = VLC_FALSE;
    bo_t *soun;
    char fcc[4] = "    ";
    int  i;

    switch( p_stream->p_fmt->i_codec )
    {
    case VLC_FOURCC('m','p','4','a'):
        memcpy( fcc, "mp4a", 4 );
        b_descr = VLC_TRUE;
        break;

    case VLC_FOURCC('m','p','g','a'):
        if( p_sys->b_mov )
            memcpy( fcc, ".mp3", 4 );
        else
            memcpy( fcc, "mp4a", 4 );
        break;

    default:
        memcpy( fcc, (char*)&p_stream->p_fmt->i_codec, 4 );
        break;
    }

    soun = box_new( fcc );
    for( i = 0; i < 6; i++ )
    {
        bo_add_8( soun, 0 );        // reserved;
    }
    bo_add_16be( soun, 1 );         // data-reference-index

    /* SoundDescription */
    if( p_sys->b_mov &&
        p_stream->p_fmt->i_codec == VLC_FOURCC('m','p','4','a') )
    {
        bo_add_16be( soun, 1 );     // version 1;
    }
    else
    {
        bo_add_16be( soun, 0 );     // version 0;
    }
    bo_add_16be( soun, 0 );         // revision level (0)
    bo_add_32be( soun, 0 );         // vendor
    // channel-count
    bo_add_16be( soun, p_stream->p_fmt->audio.i_channels );
    // sample size
    bo_add_16be( soun, p_stream->p_fmt->audio.i_bitspersample ?
                 p_stream->p_fmt->audio.i_bitspersample : 16 );
    bo_add_16be( soun, -2 );        // compression id
    bo_add_16be( soun, 0 );         // packet size (0)
    bo_add_16be( soun, p_stream->p_fmt->audio.i_rate ); // sampleratehi
    bo_add_16be( soun, 0 );                             // sampleratelo

    /* Extended data for SoundDescription V1 */
    if( p_sys->b_mov &&
        p_stream->p_fmt->i_codec == VLC_FOURCC('m','p','4','a') )
    {
        /* samples per packet */
        bo_add_32be( soun, p_stream->p_fmt->audio.i_frame_length );
        bo_add_32be( soun, 1536 ); /* bytes per packet */
        bo_add_32be( soun, 2 );    /* bytes per frame */
        /* bytes per sample */
        bo_add_32be( soun, 2 /*p_stream->p_fmt->audio.i_bitspersample/8 */);
    }

    /* Add an ES Descriptor */
    if( b_descr )
    {
        bo_t *box;

        if( p_sys->b_mov &&
            p_stream->p_fmt->i_codec == VLC_FOURCC('m','p','4','a') )
        {
            box = GetWaveTag( p_stream );
        }
        else
        {
            box = GetESDS( p_stream );
        }
        box_fix( box );
        box_gather( soun, box );
    }

    box_fix( soun );

    return soun;
}

static bo_t *GetVideBox( sout_mux_t *p_mux, mp4_stream_t *p_stream )
{

    bo_t *vide;
    char fcc[4] = "    ";
    int  i;

    switch( p_stream->p_fmt->i_codec )
    {
    case VLC_FOURCC('m','p','4','v'):
    case VLC_FOURCC('m','p','g','v'):
        memcpy( fcc, "mp4v", 4 );
        break;

    case VLC_FOURCC('M','J','P','G'):
        memcpy( fcc, "mjpa", 4 );
        break;

    case VLC_FOURCC('S','V','Q','3'):
        memcpy( fcc, "SVQ3", 4 );
        break;

    default:
        memcpy( fcc, (char*)&p_stream->p_fmt->i_codec, 4 );
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

    bo_add_16be( vide, p_stream->p_fmt->video.i_width );  // i_width
    bo_add_16be( vide, p_stream->p_fmt->video.i_height ); // i_height

    bo_add_32be( vide, 0x00480000 );                // h 72dpi
    bo_add_32be( vide, 0x00480000 );                // v 72dpi

    bo_add_32be( vide, 0 );         // data size, always 0
    bo_add_16be( vide, 1 );         // frames count per sample

    // compressor name;
    for( i = 0; i < 32; i++ )
    {
        bo_add_8( vide, 0 );
    }

    bo_add_16be( vide, 0x18 );      // depth
    bo_add_16be( vide, 0xffff );    // predefined

    /* add an ES Descriptor */
    switch( p_stream->p_fmt->i_codec )
    {
    case VLC_FOURCC('m','p','4','v'):
    case VLC_FOURCC('m','p','g','v'):
        {
            bo_t *esds = GetESDS( p_stream );

            box_fix( esds );
            box_gather( vide, esds );
        }
        break;

    case VLC_FOURCC('S','V','Q','3'):
        {
            bo_t *esds = GetSVQ3Tag( p_stream );

            box_fix( esds );
            box_gather( vide, esds );
        }
        break;

    default:
        break;
    }

    box_fix( vide );

    return vide;
}

static bo_t *GetStblBox( sout_mux_t *p_mux, mp4_stream_t *p_stream )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    unsigned int i_chunk, i_stsc_last_val, i_stsc_entries, i, i_index;
    bo_t *stbl, *stsd, *stts, *stco, *stsc, *stsz;
    uint32_t i_timescale;

    stbl = box_new( "stbl" );
    stsd = box_full_new( "stsd", 0, 0 );
    bo_add_32be( stsd, 1 );

    if( p_stream->p_fmt->i_cat == AUDIO_ES )
    {
        bo_t *soun = GetSounBox( p_mux, p_stream );
        box_gather( stsd, soun );
    }
    else if( p_stream->p_fmt->i_cat == VIDEO_ES )
    {
        bo_t *vide = GetVideBox( p_mux, p_stream );
        box_gather( stsd, vide );
    }

    /* append stsd to stbl */
    box_fix( stsd );
    box_gather( stbl, stsd );

    /* chunk offset table */
    p_stream->i_stco_pos = stbl->i_buffer + 16;
    if( p_sys->i_pos >= (((uint64_t)0x1) << 32) )
    {
        /* 64 bits version */
        p_stream->b_stco64 = VLC_TRUE;
        stco = box_full_new( "co64", 0, 0 );
    }
    else
    {
        /* 32 bits version */
        p_stream->b_stco64 = VLC_FALSE;
        stco = box_full_new( "stco", 0, 0 );
    }
    bo_add_32be( stco, 0 );     // entry-count (fixed latter)

    /* sample to chunk table */
    stsc = box_full_new( "stsc", 0, 0 );
    bo_add_32be( stsc, 0 );     // entry-count (fixed latter)

    for( i_chunk = 0, i_stsc_last_val = 0, i_stsc_entries = 0, i = 0;
         i < p_stream->i_entry_count; i_chunk++ )
    {
        int i_first = i;

        if( p_stream->b_stco64 )
            bo_add_64be( stco, p_stream->entry[i].i_pos );
        else
            bo_add_32be( stco, p_stream->entry[i].i_pos );

        while( i < p_stream->i_entry_count )
        {
            if( i + 1 < p_stream->i_entry_count &&
                p_stream->entry[i].i_pos + p_stream->entry[i].i_size
                != p_stream->entry[i + 1].i_pos )
            {
                i++;
                break;
            }

            i++;
        }

        /* Add entry to the stsc table */
        if( i_stsc_last_val != i - i_first )
        {
            bo_add_32be( stsc, 1 + i_chunk );   // first-chunk
            bo_add_32be( stsc, i - i_first ) ;  // samples-per-chunk
            bo_add_32be( stsc, 1 );             // sample-descr-index
            i_stsc_last_val = i - i_first;
            i_stsc_entries++;
        }
    }

    /* Fix stco entry count */
    bo_fix_32be( stco, 12, i_chunk );
    msg_Dbg( p_mux, "created %d chunks (stco)", i_chunk );

    /* append stco to stbl */
    box_fix( stco );
    box_gather( stbl, stco );

    /* Fix stsc entry count */
    bo_fix_32be( stsc, 12, i_stsc_entries  );

    /* append stsc to stbl */
    box_fix( stsc );
    box_gather( stbl, stsc );

    /* add stts */
    stts = box_full_new( "stts", 0, 0 );
    bo_add_32be( stts, 0 );     // entry-count (fixed latter)

    if( p_stream->p_fmt->i_cat == AUDIO_ES )
        i_timescale = p_stream->p_fmt->audio.i_rate;
    else
        i_timescale = 1001;

    for( i = 0, i_index = 0; i < p_stream->i_entry_count; i_index++)
    {
        int64_t i_delta;
        int     i_first;

        i_first = i;
        i_delta = p_stream->entry[i].i_length;

        while( i < p_stream->i_entry_count )
        {
            if( i + 1 < p_stream->i_entry_count &&
                p_stream->entry[i + 1].i_length != i_delta )
            {
                i++;
                break;
            }

            i++;
        }

        bo_add_32be( stts, i - i_first );           // sample-count
        bo_add_32be( stts, i_delta * (int64_t)i_timescale /
                     (int64_t)1000000 );            // sample-delta
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

    box_fix( stbl );

    return stbl;
}

static uint32_t mvhd_matrix[9] =
    { 0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x40000000 };

static bo_t *GetMoovBox( sout_mux_t *p_mux )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;

    bo_t            *moov, *mvhd;
    int             i_trak, i;

    uint32_t        i_movie_timescale = 90000;
    int64_t         i_movie_duration  = 0;

    moov = box_new( "moov" );

    /* *** add /moov/mvhd *** */
    if( !p_sys->b_64_ext )
    {
        mvhd = box_full_new( "mvhd", 0, 0 );
        bo_add_32be( mvhd, get_timestamp() );   // creation time
        bo_add_32be( mvhd, get_timestamp() );   // modification time
        bo_add_32be( mvhd, i_movie_timescale);  // timescale
        bo_add_32be( mvhd, i_movie_duration );  // duration
    }
    else
    {
        mvhd = box_full_new( "mvhd", 1, 0 );
        bo_add_64be( mvhd, get_timestamp() );   // creation time
        bo_add_64be( mvhd, get_timestamp() );   // modification time
        bo_add_32be( mvhd, i_movie_timescale);  // timescale
        bo_add_64be( mvhd, i_movie_duration );  // duration
    }
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

    /* Find the 1st track id */
    for( i_trak = 0; i_trak < p_sys->i_nb_streams; i_trak++ )
    {
        mp4_stream_t *p_stream = p_sys->pp_streams[i_trak];

        if( p_stream->p_fmt->i_cat == AUDIO_ES ||
            p_stream->p_fmt->i_cat == VIDEO_ES )
        {
            /* Found it */
            bo_add_32be( mvhd, p_stream->i_track_id ); // next-track-id
            break;
        }
    }
    if( i_trak == p_sys->i_nb_streams ) /* Just for sanity reasons */
        bo_add_32be( mvhd, 0xffffffff );

    box_fix( mvhd );
    box_gather( moov, mvhd );

    for( i_trak = 0; i_trak < p_sys->i_nb_streams; i_trak++ )
    {
        mp4_stream_t *p_stream;
        uint32_t     i_timescale;

        bo_t *trak, *tkhd, *mdia, *mdhd, *hdlr;
        bo_t *minf, *dinf, *dref, *url, *stbl;

        p_stream = p_sys->pp_streams[i_trak];

        if( p_stream->p_fmt->i_cat != AUDIO_ES &&
            p_stream->p_fmt->i_cat != VIDEO_ES )
        {
            msg_Err( p_mux, "FIXME ignoring trak (noaudio&&novideo)" );
            continue;
        }

        if( p_stream->p_fmt->i_cat == AUDIO_ES )
            i_timescale = p_stream->p_fmt->audio.i_rate;
        else
            i_timescale = 1001;

        /* *** add /moov/trak *** */
        trak = box_new( "trak" );

        /* *** add /moov/trak/tkhd *** */
        if( !p_sys->b_64_ext )
        {
            if( p_sys->b_mov )
                tkhd = box_full_new( "tkhd", 0, 0x0f );
            else
                tkhd = box_full_new( "tkhd", 0, 1 );

            bo_add_32be( tkhd, get_timestamp() );       // creation time
            bo_add_32be( tkhd, get_timestamp() );       // modification time
            bo_add_32be( tkhd, p_stream->i_track_id );
            bo_add_32be( tkhd, 0 );                     // reserved 0
            bo_add_32be( tkhd, p_stream->i_duration *
                         (int64_t)i_movie_timescale /
                         (mtime_t)1000000 );            // duration
        }
        else
        {
            if( p_sys->b_mov )
                tkhd = box_full_new( "tkhd", 1, 0x0f );
            else
                tkhd = box_full_new( "tkhd", 1, 1 );

            bo_add_64be( tkhd, get_timestamp() );       // creation time
            bo_add_64be( tkhd, get_timestamp() );       // modification time
            bo_add_32be( tkhd, p_stream->i_track_id );
            bo_add_32be( tkhd, 0 );                     // reserved 0
            bo_add_64be( tkhd, p_stream->i_duration *
                         (int64_t)i_movie_timescale /
                         (mtime_t)1000000 );            // duration
        }

        for( i = 0; i < 2; i++ )
        {
            bo_add_32be( tkhd, 0 );                 // reserved
        }
        bo_add_16be( tkhd, 0 );                     // layer
        bo_add_16be( tkhd, 0 );                     // pre-defined
        // volume
        bo_add_16be( tkhd, p_stream->p_fmt->i_cat == AUDIO_ES ? 0x100 : 0 );
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
            // width (presentation)
            bo_add_32be( tkhd, p_stream->p_fmt->video.i_aspect *
                         p_stream->p_fmt->video.i_height /
                         VOUT_ASPECT_FACTOR << 16 );
            // height(presentation)
            bo_add_32be( tkhd, p_stream->p_fmt->video.i_height << 16 );
        }
        box_fix( tkhd );
        box_gather( trak, tkhd );

        /* *** add /moov/trak/mdia *** */
        mdia = box_new( "mdia" );

        /* media header */
        if( !p_sys->b_64_ext )
        {
            mdhd = box_full_new( "mdhd", 0, 0 );
            bo_add_32be( mdhd, get_timestamp() );   // creation time
            bo_add_32be( mdhd, get_timestamp() );   // modification time
            bo_add_32be( mdhd, i_timescale);        // timescale
            bo_add_32be( mdhd, p_stream->i_duration * (int64_t)i_timescale /
                               (mtime_t)1000000 );  // duration
        }
        else
        {
            mdhd = box_full_new( "mdhd", 1, 0 );
            bo_add_64be( mdhd, get_timestamp() );   // creation time
            bo_add_64be( mdhd, get_timestamp() );   // modification time
            bo_add_32be( mdhd, i_timescale);        // timescale
            bo_add_64be( mdhd, p_stream->i_duration * (int64_t)i_timescale /
                               (mtime_t)1000000 );  // duration
        }

        bo_add_16be( mdhd, 0    );              // language   FIXME
        bo_add_16be( mdhd, 0    );              // predefined
        box_fix( mdhd );
        box_gather( mdia, mdhd );

        /* handler reference */
        hdlr = box_full_new( "hdlr", 0, 0 );

        bo_add_fourcc( hdlr, "mhlr" );         // media handler
        if( p_stream->p_fmt->i_cat == AUDIO_ES )
        {
            bo_add_fourcc( hdlr, "soun" );
        }
        else
        {
            bo_add_fourcc( hdlr, "vide" );
        }

        bo_add_32be( hdlr, 0 );         // reserved
        bo_add_32be( hdlr, 0 );         // reserved
        bo_add_32be( hdlr, 0 );         // reserved

        if( p_stream->p_fmt->i_cat == AUDIO_ES )
        {
            bo_add_8( hdlr, 13 );
            bo_add_mem( hdlr, 13, "SoundHandler" );
        }
        else
        {
            bo_add_8( hdlr, 13 );
            bo_add_mem( hdlr, 13, "VideoHandler" );
        }

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
        stbl = GetStblBox( p_mux, p_stream );

        /* append stbl to minf */
        p_stream->i_stco_pos += minf->i_buffer;
        box_gather( minf, stbl );

        /* append minf to mdia */
        box_fix( minf );
        p_stream->i_stco_pos += mdia->i_buffer;
        box_gather( mdia, minf );

        /* append mdia to trak */
        box_fix( mdia );
        p_stream->i_stco_pos += trak->i_buffer;
        box_gather( trak, mdia );

        /* append trak to moov */
        box_fix( trak );
        p_stream->i_stco_pos += moov->i_buffer;
        box_gather( moov, trak );
    }

    /* Add user data tags */
    box_gather( moov, GetUdtaTag( p_mux ) );

    box_fix( moov );
    return moov;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_mux_t      *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t  *p_sys = p_mux->p_sys;
    sout_buffer_t   *p_hdr;
    bo_t            bo, *moov;
    vlc_value_t     val;

    int             i_trak;
    uint64_t        i_moov_pos;

    uint32_t        i_movie_timescale = 90000;
    int64_t         i_movie_duration  = 0;

    msg_Dbg( p_mux, "Close" );

    /* Create general info */
    for( i_trak = 0; i_trak < p_sys->i_nb_streams; i_trak++ )
    {
        mp4_stream_t *p_stream = p_sys->pp_streams[i_trak];
        i_movie_duration = __MAX( i_movie_duration, p_stream->i_duration );
    }
    msg_Dbg( p_mux, "movie duration %ds",
             (uint32_t)( i_movie_duration / (mtime_t)1000000 ) );

    i_movie_duration = i_movie_duration * i_movie_timescale / 1000000;

    /* Update mdat size */
    bo_init( &bo, 0, NULL, VLC_TRUE );
    if( p_sys->i_pos - p_sys->i_mdat_pos >= (((uint64_t)1)<<32) )
    {
        /* Extended size */
        bo_add_32be  ( &bo, 1 );
        bo_add_fourcc( &bo, "mdat" );
        bo_add_64be  ( &bo, p_sys->i_pos - p_sys->i_mdat_pos );
    }
    else
    {
        bo_add_32be  ( &bo, 8 );
        bo_add_fourcc( &bo, "wide" );
        bo_add_32be  ( &bo, p_sys->i_pos - p_sys->i_mdat_pos - 8 );
        bo_add_fourcc( &bo, "mdat" );
    }
    p_hdr = bo_to_sout( p_mux->p_sout, &bo );
    free( bo.p_buffer );

    sout_AccessOutSeek( p_mux->p_access, p_sys->i_mdat_pos );
    sout_AccessOutWrite( p_mux->p_access, p_hdr );

    /* Create MOOV header */
    i_moov_pos = p_sys->i_pos;
    moov = GetMoovBox( p_mux );

    /* Check we need to create "fast start" files */
    var_Create( p_this, "mp4-faststart", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Get( p_this, "mp4-faststart", &val );
    p_sys->b_fast_start = val.b_bool;
    while( p_sys->b_fast_start )
    {
        /* Move data to the end of the file so we can fit the moov header
         * at the start */
        sout_buffer_t *p_buf;
        int64_t i_chunk, i_size = p_sys->i_pos - p_sys->i_mdat_pos;
        int i_moov_size = moov->i_buffer;

        while( i_size > 0 )
        {
            i_chunk = __MIN( 32768, i_size );
            p_buf = sout_BufferNew( p_mux->p_sout, i_chunk );
            sout_AccessOutSeek( p_mux->p_access,
                                p_sys->i_mdat_pos + i_size - i_chunk );
            if( sout_AccessOutRead( p_mux->p_access, p_buf ) < i_chunk )
            {
                msg_Warn( p_this, "read() not supported by acces output, "
                          "won't create a fast start file" );
                p_sys->b_fast_start = VLC_FALSE;
                break;
            }
            sout_AccessOutSeek( p_mux->p_access, p_sys->i_mdat_pos + i_size +
                                i_moov_size - i_chunk );
            sout_AccessOutWrite( p_mux->p_access, p_buf );
            i_size -= i_chunk;
        }

        if( !p_sys->b_fast_start ) break;

        /* Fix-up samples to chunks table in MOOV header */
        for( i_trak = 0; i_trak < p_sys->i_nb_streams; i_trak++ )
        {
            mp4_stream_t *p_stream = p_sys->pp_streams[i_trak];
            unsigned int i;
            int i_chunk;

            moov->i_buffer = p_stream->i_stco_pos;
            for( i_chunk = 0, i = 0; i < p_stream->i_entry_count; i_chunk++ )
            {
                if( p_stream->b_stco64 )
                    bo_add_64be( moov, p_stream->entry[i].i_pos + i_moov_size);
                else
                    bo_add_32be( moov, p_stream->entry[i].i_pos + i_moov_size);

                while( i < p_stream->i_entry_count )
                {
                    if( i + 1 < p_stream->i_entry_count &&
                        p_stream->entry[i].i_pos + p_stream->entry[i].i_size
                        != p_stream->entry[i + 1].i_pos )
                    {
                        i++;
                        break;
                    }

                    i++;
                }
            }
        }

        moov->i_buffer = i_moov_size;
        i_moov_pos = p_sys->i_mdat_pos;
        p_sys->b_fast_start = VLC_FALSE;
    }

    /* Write MOOV header */
    sout_AccessOutSeek( p_mux->p_access, i_moov_pos );
    box_send( p_mux, moov );

    /* Clean-up */
    for( i_trak = 0; i_trak < p_sys->i_nb_streams; i_trak++ )
    {
        mp4_stream_t *p_stream = p_sys->pp_streams[i_trak];

        if( p_stream->p_fmt->p_extra )
        {
            free( p_stream->p_fmt->p_extra );
        }
        free( p_stream->p_fmt );
        free( p_stream->entry );
        free( p_stream );
    }
    free( p_sys );
}

static int Capability( sout_mux_t *p_mux, int i_query, void *p_args,
                       void *p_answer )
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

    switch( p_input->p_fmt->i_codec )
    {
        case VLC_FOURCC( 'm', 'p', '4', 'a' ):
        case VLC_FOURCC( 'm', 'p', '4', 'v' ):
        case VLC_FOURCC( 'm', 'p', 'g', 'a' ):
        case VLC_FOURCC( 'm', 'p', 'g', 'v' ):
        case VLC_FOURCC( 'M', 'J', 'P', 'G' ):
        case VLC_FOURCC( 'm', 'j', 'p', 'b' ):
        case VLC_FOURCC( 'S', 'V', 'Q', '3' ):
            break;
        default:
            msg_Err( p_mux, "unsupported codec %4.4s in mp4",
                     (char*)&p_input->p_fmt->i_codec );
            return VLC_EGENERIC;
    }

    p_stream                = malloc( sizeof( mp4_stream_t ) );
    p_stream->p_fmt         = malloc( sizeof( es_format_t ) );
    memcpy( p_stream->p_fmt, p_input->p_fmt, sizeof( es_format_t ) );
    if( p_stream->p_fmt->i_extra )
    {
        p_stream->p_fmt->p_extra =
            malloc( p_stream->p_fmt->i_extra );
        memcpy( p_stream->p_fmt->p_extra,
                p_input->p_fmt->p_extra,
                p_input->p_fmt->i_extra );
    }
    p_stream->i_track_id    = p_sys->i_nb_streams + 1;
    p_stream->i_entry_count = 0;
    p_stream->i_entry_max   = 1000;
    p_stream->entry         =
        calloc( p_stream->i_entry_max, sizeof( mp4_entry_t ) );
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

static int MuxGetStream( sout_mux_t *p_mux, int *pi_stream, mtime_t *pi_dts )
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

static int Mux( sout_mux_t *p_mux )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;

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

        if( !p_sys->i_start_dts )
            p_sys->i_start_dts = i_dts;

        p_input  = p_mux->pp_inputs[i_stream];
        p_stream = (mp4_stream_t*)p_input->p_sys;

        p_data  = sout_FifoGet( p_input->p_fifo );

        /* add index entry */
        p_stream->entry[p_stream->i_entry_count].i_pos   = p_sys->i_pos;
        p_stream->entry[p_stream->i_entry_count].i_size  = p_data->i_size;
        p_stream->entry[p_stream->i_entry_count].i_pts   = p_data->i_pts;
        p_stream->entry[p_stream->i_entry_count].i_dts   = p_data->i_dts;
        p_stream->entry[p_stream->i_entry_count].i_length=
            __MAX( p_data->i_length, 0 );

        if( p_stream->i_entry_count == 0 )
        {
            /* Here is another bad hack.
             * To make sure audio/video are in sync, we report a corrected
             * length for the 1st sample. */
            p_stream->entry[p_stream->i_entry_count].i_length =
                __MAX( p_data->i_length, 0 ) +
                p_data->i_pts - p_sys->i_start_dts;
        }

        p_stream->i_entry_count++;
        if( p_stream->i_entry_count >= p_stream->i_entry_max )
        {
            p_stream->i_entry_max += 1000;
            p_stream->entry =
                realloc( p_stream->entry,
                         p_stream->i_entry_max * sizeof( mp4_entry_t ) );
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

static void bo_init( bo_t *p_bo, int i_size, uint8_t *p_buffer,
                     vlc_bool_t b_grow )
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

static void bo_add_descr( bo_t *p_bo, uint8_t tag, uint32_t i_size )
{
    uint32_t i_length;
    uint8_t  vals[4];

    i_length = i_size;
    vals[3] = (unsigned char)(i_length & 0x7f);
    i_length >>= 7;
    vals[2] = (unsigned char)((i_length & 0x7f) | 0x80); 
    i_length >>= 7;
    vals[1] = (unsigned char)((i_length & 0x7f) | 0x80); 
    i_length >>= 7;
    vals[0] = (unsigned char)((i_length & 0x7f) | 0x80);

    bo_add_8( p_bo, tag );

    if( i_size < 0x00000080 )
    {
        bo_add_8( p_bo, vals[3] );
    }
    else if( i_size < 0x00004000 )
    {
        bo_add_8( p_bo, vals[2] );
        bo_add_8( p_bo, vals[3] );
    }
    else if( i_size < 0x00200000 )
    {
        bo_add_8( p_bo, vals[1] );
        bo_add_8( p_bo, vals[2] );
        bo_add_8( p_bo, vals[3] );
    }
    else if( i_size < 0x10000000 )
    {
        bo_add_8( p_bo, vals[0] );
        bo_add_8( p_bo, vals[1] );
        bo_add_8( p_bo, vals[2] );
        bo_add_8( p_bo, vals[3] );
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

static int64_t get_timestamp()
{
    int64_t i_timestamp = 0;

#ifdef HAVE_TIME_H
    i_timestamp = time(NULL);
    i_timestamp += 2082844800; // MOV/MP4 start date is 1/1/1904
    // 208284480 is (((1970 - 1904) * 365) + 17) * 24 * 60 * 60
#endif

    return i_timestamp;
}
