/*****************************************************************************
 * mp4.c: mp4/mov muxer
 *****************************************************************************
 * Copyright (C) 2001, 2002, 2003, 2006 the VideoLAN team
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/sout.h>

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include "iso_lang.h"
#include "vlc_meta.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define FASTSTART_TEXT N_("Create \"Fast Start\" files")
#define FASTSTART_LONGTEXT N_( \
    "Create \"Fast Start\" files. " \
    "\"Fast Start\" files are optimized for downloads and allow the user " \
    "to start previewing the file while it is downloading.")

static int  Open   ( vlc_object_t * );
static void Close  ( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-mp4-"

vlc_module_begin();
    set_description( _("MP4/MOV muxer") );
    set_category( CAT_SOUT );
    set_subcategory( SUBCAT_SOUT_MUX );
    set_shortname( "MP4" );

    add_bool( SOUT_CFG_PREFIX "faststart", 1, NULL,
              FASTSTART_TEXT, FASTSTART_LONGTEXT,
              VLC_TRUE );
    set_capability( "sout mux", 5 );
    add_shortcut( "mp4" );
    add_shortcut( "mov" );
    add_shortcut( "3gp" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static const char *ppsz_sout_options[] = {
    "faststart", NULL
};

static int Control( sout_mux_t *, int, va_list );
static int AddStream( sout_mux_t *, sout_input_t * );
static int DelStream( sout_mux_t *, sout_input_t * );
static int Mux      ( sout_mux_t * );

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
typedef struct
{
    uint64_t i_pos;
    int      i_size;

    mtime_t  i_pts_dts;
    mtime_t  i_length;
    unsigned int i_flags;

} mp4_entry_t;

typedef struct
{
    es_format_t   fmt;
    int           i_track_id;

    /* index */
    unsigned int i_entry_count;
    unsigned int i_entry_max;
    mp4_entry_t  *entry;
    int64_t      i_length_neg;

    /* stats */
    int64_t      i_dts_start;
    int64_t      i_duration;

    /* for later stco fix-up (fast start files) */
    uint64_t i_stco_pos;
    vlc_bool_t b_stco64;

    /* for h264 */
    struct
    {
        int     i_profile;
        int     i_level;

        int     i_sps;
        uint8_t *sps;
        int     i_pps;
        uint8_t *pps;
    } avc;

    /* for spu */
    int64_t i_last_dts;

} mp4_stream_t;

struct sout_mux_sys_t
{
    vlc_bool_t b_mov;
    vlc_bool_t b_3gp;
    vlc_bool_t b_64_ext;
    vlc_bool_t b_fast_start;

    uint64_t i_mdat_pos;
    uint64_t i_pos;

    int64_t  i_dts_start;

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

static block_t *bo_to_sout( sout_instance_t *p_sout,  bo_t *box );

static bo_t *GetMoovBox( sout_mux_t *p_mux );

static block_t *ConvertSUBT( sout_mux_t *, mp4_stream_t *, block_t *);
static void ConvertAVC1( sout_mux_t *, mp4_stream_t *, block_t * );

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_mux_t      *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t  *p_sys;
    bo_t            *box;

    msg_Dbg( p_mux, "Mp4 muxer opend" );
    sout_CfgParse( p_mux, SOUT_CFG_PREFIX, ppsz_sout_options, p_mux->p_cfg );

    p_mux->pf_control   = Control;
    p_mux->pf_addstream = AddStream;
    p_mux->pf_delstream = DelStream;
    p_mux->pf_mux       = Mux;
    p_mux->p_sys        = p_sys = malloc( sizeof( sout_mux_sys_t ) );
    p_sys->i_pos        = 0;
    p_sys->i_nb_streams = 0;
    p_sys->pp_streams   = NULL;
    p_sys->i_mdat_pos   = 0;
    p_sys->b_mov        = p_mux->psz_mux && !strcmp( p_mux->psz_mux, "mov" );
    p_sys->b_3gp        = p_mux->psz_mux && !strcmp( p_mux->psz_mux, "3gp" );
    p_sys->i_dts_start  = 0;


    if( !p_sys->b_mov )
    {
        /* Now add ftyp header */
        box = box_new( "ftyp" );
        if( p_sys->b_3gp ) bo_add_fourcc( box, "3gp4" );
        else bo_add_fourcc( box, "isom" );
        bo_add_32be  ( box, 0 );
        if( p_sys->b_3gp ) bo_add_fourcc( box, "3gp4" );
        else bo_add_fourcc( box, "mp41" );
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

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_mux_t      *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t  *p_sys = p_mux->p_sys;
    block_t   *p_hdr;
    bo_t            bo, *moov;
    vlc_value_t     val;

    int             i_trak;
    uint64_t        i_moov_pos;

    msg_Dbg( p_mux, "Close" );

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
    var_Get( p_this, SOUT_CFG_PREFIX "faststart", &val );
    p_sys->b_fast_start = val.b_bool;
    while( p_sys->b_fast_start )
    {
        /* Move data to the end of the file so we can fit the moov header
         * at the start */
        block_t *p_buf;
        int64_t i_chunk, i_size = p_sys->i_pos - p_sys->i_mdat_pos;
        int i_moov_size = moov->i_buffer;

        while( i_size > 0 )
        {
            i_chunk = __MIN( 32768, i_size );
            p_buf = block_New( p_mux, i_chunk );
            sout_AccessOutSeek( p_mux->p_access,
                                p_sys->i_mdat_pos + i_size - i_chunk );
            if( sout_AccessOutRead( p_mux->p_access, p_buf ) < i_chunk )
            {
                msg_Warn( p_this, "read() not supported by access output, "
                          "won't create a fast start file" );
                p_sys->b_fast_start = VLC_FALSE;
                block_Release( p_buf );
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

        es_format_Clean( &p_stream->fmt );
        if( p_stream->avc.i_sps ) free( p_stream->avc.sps );
        if( p_stream->avc.i_pps ) free( p_stream->avc.pps );
        free( p_stream->entry );
        free( p_stream );
    }
    if( p_sys->i_nb_streams ) free( p_sys->pp_streams );
    free( p_sys );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( sout_mux_t *p_mux, int i_query, va_list args )
{
    vlc_bool_t *pb_bool;

   switch( i_query )
   {
       case MUX_CAN_ADD_STREAM_WHILE_MUXING:
           pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t * );
           *pb_bool = VLC_FALSE;
           return VLC_SUCCESS;

       case MUX_GET_ADD_STREAM_WAIT:
           pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t * );
           *pb_bool = VLC_TRUE;
           return VLC_SUCCESS;

       case MUX_GET_MIME:   /* Not needed, as not streamable */
        default:
            return VLC_EGENERIC;
   }
}

/*****************************************************************************
 * AddStream:
 *****************************************************************************/
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
        case VLC_FOURCC( 'S', 'V', 'Q', '1' ):
        case VLC_FOURCC( 'S', 'V', 'Q', '3' ):
        case VLC_FOURCC( 'H', '2', '6', '3' ):
        case VLC_FOURCC( 'h', '2', '6', '4' ):
        case VLC_FOURCC( 's', 'a', 'm', 'r' ):
        case VLC_FOURCC( 's', 'a', 'w', 'b' ):
            break;
        case VLC_FOURCC( 's', 'u', 'b', 't' ):
            msg_Warn( p_mux, "subtitle track added like in .mov (even when creating .mp4)" );
            break;
        default:
            msg_Err( p_mux, "unsupported codec %4.4s in mp4",
                     (char*)&p_input->p_fmt->i_codec );
            return VLC_EGENERIC;
    }

    p_stream                = malloc( sizeof( mp4_stream_t ) );
    es_format_Copy( &p_stream->fmt, p_input->p_fmt );
    p_stream->i_track_id    = p_sys->i_nb_streams + 1;
    p_stream->i_length_neg  = 0;
    p_stream->i_entry_count = 0;
    p_stream->i_entry_max   = 1000;
    p_stream->entry         =
        calloc( p_stream->i_entry_max, sizeof( mp4_entry_t ) );
    p_stream->i_dts_start   = 0;
    p_stream->i_duration    = 0;
    p_stream->avc.i_profile = 77;
    p_stream->avc.i_level   = 51;
    p_stream->avc.i_sps     = 0;
    p_stream->avc.sps       = NULL;
    p_stream->avc.i_pps     = 0;
    p_stream->avc.pps       = NULL;

    p_input->p_sys          = p_stream;

    msg_Dbg( p_mux, "adding input" );

    TAB_APPEND( p_sys->i_nb_streams, p_sys->pp_streams, p_stream );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * DelStream:
 *****************************************************************************/
static int DelStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    msg_Dbg( p_mux, "removing input" );
    return VLC_SUCCESS;
}

static int MuxGetStream( sout_mux_t *p_mux, int *pi_stream, mtime_t *pi_dts )
{
    mtime_t i_dts;
    int     i_stream;
    int     i;

    for( i = 0, i_dts = 0, i_stream = -1; i < p_mux->i_nb_inputs; i++ )
    {
        block_fifo_t   *p_fifo = p_mux->pp_inputs[i]->p_fifo;
        block_t *p_buf;

        if( p_fifo->i_depth <= 1 )
        {
            if( p_mux->pp_inputs[i]->p_fmt->i_cat != SPU_ES )
            {
                return -1; // wait that all fifo have at least 2 packets
            }
            /* For SPU, we wait only 1 packet */
            continue;
        }

        p_buf = block_FifoShow( p_fifo );
        if( i_stream < 0 || p_buf->i_dts < i_dts )
        {
            i_dts = p_buf->i_dts;
            i_stream = i;
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
    return i_stream;
}

/*****************************************************************************
 * Mux:
 *****************************************************************************/
static int Mux( sout_mux_t *p_mux )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;

    for( ;; )
    {
        sout_input_t    *p_input;
        int             i_stream;
        mp4_stream_t    *p_stream;
        block_t         *p_data;
        mtime_t         i_dts;

        if( MuxGetStream( p_mux, &i_stream, &i_dts) < 0 )
        {
            return( VLC_SUCCESS );
        }

        p_input  = p_mux->pp_inputs[i_stream];
        p_stream = (mp4_stream_t*)p_input->p_sys;

        p_data  = block_FifoGet( p_input->p_fifo );
        if( p_stream->fmt.i_codec == VLC_FOURCC( 'h', '2', '6', '4' ) )
        {
            ConvertAVC1( p_mux, p_stream, p_data );
        }
        else if( p_stream->fmt.i_codec == VLC_FOURCC( 's', 'u', 'b', 't' ) )
        {
            p_data = ConvertSUBT( p_mux, p_stream, p_data );
        }

        if( p_stream->fmt.i_cat != SPU_ES )
        {
            /* Fix length of the sample */
            if( p_input->p_fifo->i_depth > 0 )
            {
                block_t *p_next = block_FifoShow( p_input->p_fifo );
                int64_t       i_diff  = p_next->i_dts - p_data->i_dts;

                if( i_diff < I64C(1000000 ) )   /* protection */
                {
                    p_data->i_length = i_diff;
                }
            }
            if( p_data->i_length <= 0 )
            {
                msg_Warn( p_mux, "i_length <= 0" );
                p_stream->i_length_neg += p_data->i_length - 1;
                p_data->i_length = 1;
            }
            else if( p_stream->i_length_neg < 0 )
            {
                int64_t i_recover = __MIN( p_data->i_length / 4, - p_stream->i_length_neg );

                p_data->i_length -= i_recover;
                p_stream->i_length_neg += i_recover;
            }
        }

        /* Save starting time */
        if( p_stream->i_entry_count == 0 )
        {
            p_stream->i_dts_start = p_data->i_dts;

            /* Update global dts_start */
            if( p_sys->i_dts_start <= 0 ||
                p_stream->i_dts_start < p_sys->i_dts_start )
            {
                p_sys->i_dts_start = p_stream->i_dts_start;
            }
        }

        if( p_stream->fmt.i_cat == SPU_ES && p_stream->i_entry_count > 0 )
        {
            int64_t i_length = p_data->i_dts - p_stream->i_last_dts;

            if( i_length <= 0 )
            {
                /* FIXME handle this broken case */
                i_length = 1;
            }

            /* Fix last entry */
            if( p_stream->entry[p_stream->i_entry_count-1].i_length <= 0 )
            {
                p_stream->entry[p_stream->i_entry_count-1].i_length = i_length;
            }
        }


        /* add index entry */
        p_stream->entry[p_stream->i_entry_count].i_pos    = p_sys->i_pos;
        p_stream->entry[p_stream->i_entry_count].i_size   = p_data->i_buffer;
        p_stream->entry[p_stream->i_entry_count].i_pts_dts=
            __MAX( p_data->i_pts - p_data->i_dts, 0 );
        p_stream->entry[p_stream->i_entry_count].i_length = p_data->i_length;
        p_stream->entry[p_stream->i_entry_count].i_flags  = p_data->i_flags;

        p_stream->i_entry_count++;
        /* XXX: -1 to always have 2 entry for easy adding of empty SPU */
        if( p_stream->i_entry_count >= p_stream->i_entry_max - 1 )
        {
            p_stream->i_entry_max += 1000;
            p_stream->entry =
                realloc( p_stream->entry,
                         p_stream->i_entry_max * sizeof( mp4_entry_t ) );
        }

        /* update */
        p_stream->i_duration += p_data->i_length;
        p_sys->i_pos += p_data->i_buffer;

        /* Save the DTS */
        p_stream->i_last_dts = p_data->i_dts;

        /* write data */
        sout_AccessOutWrite( p_mux->p_access, p_data );

        if( p_stream->fmt.i_cat == SPU_ES )
        {
            int64_t i_length = p_stream->entry[p_stream->i_entry_count-1].i_length;

            if( i_length != 0 )
            {
                /* TODO */
                msg_Dbg( p_mux, "writing an empty sub" ) ;

                /* Append a idx entry */
                p_stream->entry[p_stream->i_entry_count].i_pos    = p_sys->i_pos;
                p_stream->entry[p_stream->i_entry_count].i_size   = 3;
                p_stream->entry[p_stream->i_entry_count].i_pts_dts= 0;
                p_stream->entry[p_stream->i_entry_count].i_length = 0;
                p_stream->entry[p_stream->i_entry_count].i_flags  = 0;

                /* XXX: No need to grow the entry here */
                p_stream->i_entry_count++;

                /* Fix last dts */
                p_stream->i_last_dts += i_length;

                /* Write a " " */
                p_data = block_New( p_mux, 3 );
                p_data->p_buffer[0] = 0;
                p_data->p_buffer[1] = 1;
                p_data->p_buffer[2] = ' ';

                p_sys->i_pos += p_data->i_buffer;

                sout_AccessOutWrite( p_mux->p_access, p_data );
            }

            /* Fix duration */
            p_stream->i_duration = p_stream->i_last_dts - p_stream->i_dts_start;
        }
    }

    return( VLC_SUCCESS );
}

/*****************************************************************************
 *
 *****************************************************************************/
static block_t *ConvertSUBT( sout_mux_t *p_mux, mp4_stream_t *tk, block_t *p_block )
{
    p_block = block_Realloc( p_block, 2, p_block->i_buffer );

    /* No trailling '\0' */
    if( p_block->i_buffer > 2 && p_block->p_buffer[p_block->i_buffer-1] == '\0' )
        p_block->i_buffer--;

    p_block->p_buffer[0] = ( (p_block->i_buffer - 2) >> 8 )&0xff;
    p_block->p_buffer[1] = ( (p_block->i_buffer - 2)      )&0xff;

    return p_block;
}

static void ConvertAVC1( sout_mux_t *p_mux, mp4_stream_t *tk, block_t *p_block )
{
    uint8_t *last = p_block->p_buffer;  /* Assume it starts with 0x00000001 */
    uint8_t *dat  = &p_block->p_buffer[4];
    uint8_t *end = &p_block->p_buffer[p_block->i_buffer];


    /* Replace the 4 bytes start code with 4 bytes size,
     * FIXME are all startcodes 4 bytes ? (I don't think :( */
    while( dat < end )
    {
        int i_size;

        while( dat < end - 4 )
        {
            if( dat[0] == 0x00 && dat[1] == 0x00  &&
                dat[2] == 0x00 && dat[3] == 0x01 )
            {
                break;
            }
            dat++;
        }
        if( dat >= end - 4 )
        {
            dat = end;
        }

        /* Fix size */
        i_size = dat - &last[4];
        last[0] = ( i_size >> 24 )&0xff;
        last[1] = ( i_size >> 16 )&0xff;
        last[2] = ( i_size >>  8 )&0xff;
        last[3] = ( i_size       )&0xff;

        if( (last[4]&0x1f) == 7 && tk->avc.i_sps <= 0 )  /* SPS */
        {
            tk->avc.i_sps = i_size;
            tk->avc.sps = malloc( i_size );
            memcpy( tk->avc.sps, &last[4], i_size );

            tk->avc.i_profile = tk->avc.sps[1];
            tk->avc.i_level   = tk->avc.sps[3];
        }
        else if( (last[4]&0x1f) == 8 && tk->avc.i_pps <= 0 )   /* PPS */
        {
            tk->avc.i_pps = i_size;
            tk->avc.pps = malloc( i_size );
            memcpy( tk->avc.pps, &last[4], i_size );
        }

        last = dat;

        dat += 4;
    }
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
    unsigned int i;
    int64_t i_bitrate_avg = 0;
    int64_t i_bitrate_max = 0;

    /* Compute avg/max bitrate */
    for( i = 0; i < p_stream->i_entry_count; i++ )
    {
        i_bitrate_avg += p_stream->entry[i].i_size;
        if( p_stream->entry[i].i_length > 0)
        {
            int64_t i_bitrate = I64C(8000000) * p_stream->entry[i].i_size / p_stream->entry[i].i_length;
            if( i_bitrate > i_bitrate_max )
                i_bitrate_max = i_bitrate;
        }
    }

    if( p_stream->i_duration > 0 )
        i_bitrate_avg = I64C(8000000) * i_bitrate_avg / p_stream->i_duration;
    else
        i_bitrate_avg = 0;
    if( i_bitrate_max <= 1 )
        i_bitrate_max = 0x7fffffff;

    /* */
    if( p_stream->fmt.i_extra > 0 )
    {
        i_decoder_specific_info_size =
            GetDescrLength( p_stream->fmt.i_extra );
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

    switch( p_stream->fmt.i_codec )
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
                p_stream->fmt.audio.i_rate < 32000 ? 0x69 : 0x6b;
            break;
        default:
            i_object_type_indication = 0x00;
            break;
    }
    i_stream_type = p_stream->fmt.i_cat == VIDEO_ES ? 0x04 : 0x05;

    bo_add_8   ( esds, i_object_type_indication );
    bo_add_8   ( esds, ( i_stream_type << 2 ) | 1 );
    bo_add_24be( esds, 1024 * 1024 );       // bufferSizeDB
    bo_add_32be( esds, i_bitrate_max );     // maxBitrate
    bo_add_32be( esds, i_bitrate_avg );     // avgBitrate

    if( p_stream->fmt.i_extra > 0 )
    {
        int i;

        /* DecoderSpecificInfo */
        bo_add_descr( esds, 0x05, p_stream->fmt.i_extra );

        for( i = 0; i < p_stream->fmt.i_extra; i++ )
        {
            bo_add_8( esds, ((uint8_t*)p_stream->fmt.p_extra)[i] );
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

static bo_t *GetDamrTag( mp4_stream_t *p_stream )
{
    bo_t *damr;

    damr = box_new( "damr" );

    bo_add_fourcc( damr, "REFC" );
    bo_add_8( damr, 0 );

    if( p_stream->fmt.i_codec == VLC_FOURCC( 's', 'a', 'm', 'r' ) )
        bo_add_16be( damr, 0x81ff ); /* Mode set (all modes for AMR_NB) */
    else
        bo_add_16be( damr, 0x83ff ); /* Mode set (all modes for AMR_WB) */
    bo_add_16be( damr, 0x1 ); /* Mode change period (no restriction) */

    box_fix( damr );

    return damr;
}

static bo_t *GetD263Tag( mp4_stream_t *p_stream )
{
    bo_t *d263;

    d263 = box_new( "d263" );

    bo_add_fourcc( d263, "VLC " );
    bo_add_16be( d263, 0xa );
    bo_add_8( d263, 0 );

    box_fix( d263 );

    return d263;
}

static bo_t *GetAvcCTag( mp4_stream_t *p_stream )
{
    bo_t *avcC;

    /* FIXME use better value */
    avcC = box_new( "avcC" );
    bo_add_8( avcC, 1 );      /* configuration version */
    bo_add_8( avcC, p_stream->avc.i_profile );
    bo_add_8( avcC, p_stream->avc.i_profile );     /* profile compatible ??? */
    bo_add_8( avcC, p_stream->avc.i_level );       /* level, 5.1 */
    bo_add_8( avcC, 0xff );   /* 0b11111100 | lengthsize = 0x11 */

    bo_add_8( avcC, 0xe0 | (p_stream->avc.i_sps > 0 ? 1 : 0) );   /* 0b11100000 | sps_count */
    if( p_stream->avc.i_sps > 0 )
    {
        bo_add_16be( avcC, p_stream->avc.i_sps );
        bo_add_mem( avcC, p_stream->avc.i_sps, p_stream->avc.sps );
    }

    bo_add_8( avcC, (p_stream->avc.i_pps > 0 ? 1 : 0) );   /* pps_count */
    if( p_stream->avc.i_pps > 0 )
    {
        bo_add_16be( avcC, p_stream->avc.i_pps );
        bo_add_mem( avcC, p_stream->avc.i_pps, p_stream->avc.pps );
    }
    box_fix( avcC );

    return avcC;
}

/* TODO: No idea about these values */
static bo_t *GetSVQ3Tag( mp4_stream_t *p_stream )
{
    bo_t *smi = box_new( "SMI " );

    if( p_stream->fmt.i_extra > 0x4e )
    {
        uint8_t *p_end = &((uint8_t*)p_stream->fmt.p_extra)[p_stream->fmt.i_extra];
        uint8_t *p     = &((uint8_t*)p_stream->fmt.p_extra)[0x46];

        while( p + 8 < p_end )
        {
            int i_size = GetDWBE( p );
            if( i_size <= 1 )
            {
                /* FIXME handle 1 as long size */
                break;
            }
            if( !strncmp( (const char *)&p[4], "SMI ", 4 ) )
            {
                bo_add_mem( smi, p_end - p - 8, &p[8] );
                return smi;
            }
            p += i_size;
        }
    }

    /* Create a dummy one in fallback */
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
    vlc_meta_t *p_meta = p_mux->p_sout->p_meta;
    int i_track;

    /* Requirements */
    for( i_track = 0; i_track < p_sys->i_nb_streams; i_track++ )
    {
        mp4_stream_t *p_stream = p_sys->pp_streams[i_track];

        if( p_stream->fmt.i_codec == VLC_FOURCC('m','p','4','v') ||
            p_stream->fmt.i_codec == VLC_FOURCC('m','p','4','a') )
        {
            bo_t *box = box_new( "\251req" );
            /* String length */
            bo_add_16be( box, sizeof("QuickTime 6.0 or greater") - 1);
            bo_add_16be( box, 0 );
            bo_add_mem( box, sizeof("QuickTime 6.0 or greater") - 1,
                        (uint8_t *)"QuickTime 6.0 or greater" );
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
                    (uint8_t*)PACKAGE_STRING " stream output" );
        box_fix( box );
        box_gather( udta, box );
    }

    /* Misc atoms */
    if( p_meta )
    {
        int i;
        for( i = 0; i < p_meta->i_meta; i++ )
        {
            bo_t *box = NULL;

            if( !strcmp( p_meta->name[i], VLC_META_TITLE ) )
                box = box_new( "\251nam" );
            else if( !strcmp( p_meta->name[i], VLC_META_AUTHOR ) )
                box = box_new( "\251aut" );
            else if( !strcmp( p_meta->name[i], VLC_META_ARTIST ) )
                box = box_new( "\251ART" );
            else if( !strcmp( p_meta->name[i], VLC_META_GENRE ) )
                box = box_new( "\251gen" );
            else if( !strcmp( p_meta->name[i], VLC_META_COPYRIGHT ) )
                box = box_new( "\251cpy" );
            else if( !strcmp( p_meta->name[i], VLC_META_DESCRIPTION ) )
                box = box_new( "\251des" );
            else if( !strcmp( p_meta->name[i], VLC_META_DATE ) )
                box = box_new( "\251day" );
            else if( !strcmp( p_meta->name[i], VLC_META_URL ) )
                box = box_new( "\251url" );

            if( box )
            {
                bo_add_16be( box, strlen( p_meta->value[i] ) );
                bo_add_16be( box, 0 );
                bo_add_mem( box, strlen( p_meta->value[i] ),
                            (uint8_t*)(p_meta->value[i]) );
                box_fix( box );
                box_gather( udta, box );
            }
        }
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

    switch( p_stream->fmt.i_codec )
    {
    case VLC_FOURCC('m','p','4','a'):
        memcpy( fcc, "mp4a", 4 );
        b_descr = VLC_TRUE;
        break;

    case VLC_FOURCC('s','a','m','r'):
    case VLC_FOURCC('s','a','w','b'):
        memcpy( fcc, (char*)&p_stream->fmt.i_codec, 4 );
        b_descr = VLC_TRUE;
        break;

    case VLC_FOURCC('m','p','g','a'):
        if( p_sys->b_mov )
            memcpy( fcc, ".mp3", 4 );
        else
        {
            memcpy( fcc, "mp4a", 4 );
            b_descr = VLC_TRUE;
        }
        break;

    default:
        memcpy( fcc, (char*)&p_stream->fmt.i_codec, 4 );
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
        p_stream->fmt.i_codec == VLC_FOURCC('m','p','4','a') )
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
    bo_add_16be( soun, p_stream->fmt.audio.i_channels );
    // sample size
    bo_add_16be( soun, p_stream->fmt.audio.i_bitspersample ?
                 p_stream->fmt.audio.i_bitspersample : 16 );
    bo_add_16be( soun, -2 );        // compression id
    bo_add_16be( soun, 0 );         // packet size (0)
    bo_add_16be( soun, p_stream->fmt.audio.i_rate ); // sampleratehi
    bo_add_16be( soun, 0 );                             // sampleratelo

    /* Extended data for SoundDescription V1 */
    if( p_sys->b_mov &&
        p_stream->fmt.i_codec == VLC_FOURCC('m','p','4','a') )
    {
        /* samples per packet */
        bo_add_32be( soun, p_stream->fmt.audio.i_frame_length );
        bo_add_32be( soun, 1536 ); /* bytes per packet */
        bo_add_32be( soun, 2 );    /* bytes per frame */
        /* bytes per sample */
        bo_add_32be( soun, 2 /*p_stream->fmt.audio.i_bitspersample/8 */);
    }

    /* Add an ES Descriptor */
    if( b_descr )
    {
        bo_t *box;

        if( p_sys->b_mov &&
            p_stream->fmt.i_codec == VLC_FOURCC('m','p','4','a') )
        {
            box = GetWaveTag( p_stream );
        }
        else if( p_stream->fmt.i_codec == VLC_FOURCC('s','a','m','r') )
        {
            box = GetDamrTag( p_stream );
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

    switch( p_stream->fmt.i_codec )
    {
    case VLC_FOURCC('m','p','4','v'):
    case VLC_FOURCC('m','p','g','v'):
        memcpy( fcc, "mp4v", 4 );
        break;

    case VLC_FOURCC('M','J','P','G'):
        memcpy( fcc, "mjpa", 4 );
        break;

    case VLC_FOURCC('S','V','Q','1'):
        memcpy( fcc, "SVQ1", 4 );
        break;

    case VLC_FOURCC('S','V','Q','3'):
        memcpy( fcc, "SVQ3", 4 );
        break;

    case VLC_FOURCC('H','2','6','3'):
        memcpy( fcc, "s263", 4 );
        break;

    case VLC_FOURCC('h','2','6','4'):
        memcpy( fcc, "avc1", 4 );
        break;

    default:
        memcpy( fcc, (char*)&p_stream->fmt.i_codec, 4 );
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

    bo_add_16be( vide, p_stream->fmt.video.i_width );  // i_width
    bo_add_16be( vide, p_stream->fmt.video.i_height ); // i_height

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
    switch( p_stream->fmt.i_codec )
    {
    case VLC_FOURCC('m','p','4','v'):
    case VLC_FOURCC('m','p','g','v'):
        {
            bo_t *esds = GetESDS( p_stream );

            box_fix( esds );
            box_gather( vide, esds );
        }
        break;

    case VLC_FOURCC('H','2','6','3'):
        {
            bo_t *d263 = GetD263Tag( p_stream );

            box_fix( d263 );
            box_gather( vide, d263 );
        }
        break;

    case VLC_FOURCC('S','V','Q','3'):
        {
            bo_t *esds = GetSVQ3Tag( p_stream );

            box_fix( esds );
            box_gather( vide, esds );
        }
        break;

    case VLC_FOURCC('h','2','6','4'):
        box_gather( vide, GetAvcCTag( p_stream ) );
        break;

    default:
        break;
    }

    box_fix( vide );

    return vide;
}

static bo_t *GetTextBox( sout_mux_t *p_mux, mp4_stream_t *p_stream )
{

    bo_t *text = box_new( "text" );
    int  i;

    for( i = 0; i < 6; i++ )
    {
        bo_add_8( text, 0 );        // reserved;
    }
    bo_add_16be( text, 1 );         // data-reference-index

    bo_add_32be( text, 0 );         // display flags
    bo_add_32be( text, 0 );         // justification
    for( i = 0; i < 3; i++ )
    {
        bo_add_16be( text, 0 );     // back ground color
    }

    bo_add_16be( text, 0 );         // box text
    bo_add_16be( text, 0 );         // box text
    bo_add_16be( text, 0 );         // box text
    bo_add_16be( text, 0 );         // box text

    bo_add_64be( text, 0 );         // reserved
    for( i = 0; i < 3; i++ )
    {
        bo_add_16be( text, 0xff );  // foreground color
    }

    bo_add_8 ( text, 9 );
    bo_add_mem( text, 9, (uint8_t*)"Helvetica" );

    box_fix( text );

    return text;
}

static bo_t *GetStblBox( sout_mux_t *p_mux, mp4_stream_t *p_stream )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    unsigned int i_chunk, i_stsc_last_val, i_stsc_entries, i, i_index;
    bo_t *stbl, *stsd, *stts, *stco, *stsc, *stsz, *stss;
    uint32_t i_timescale;
    int64_t i_dts, i_dts_q;

    stbl = box_new( "stbl" );

    /* sample description */
    stsd = box_full_new( "stsd", 0, 0 );
    bo_add_32be( stsd, 1 );
    if( p_stream->fmt.i_cat == AUDIO_ES )
    {
        bo_t *soun = GetSounBox( p_mux, p_stream );
        box_gather( stsd, soun );
    }
    else if( p_stream->fmt.i_cat == VIDEO_ES )
    {
        bo_t *vide = GetVideBox( p_mux, p_stream );
        box_gather( stsd, vide );
    }
    else if( p_stream->fmt.i_cat == SPU_ES )
    {
        box_gather( stsd, GetTextBox( p_mux, p_stream ) );
    }
    box_fix( stsd );

    /* chunk offset table */
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
    box_fix( stco );

    /* Fix stsc entry count */
    bo_fix_32be( stsc, 12, i_stsc_entries  );
    box_fix( stsc );

    /* add stts */
    stts = box_full_new( "stts", 0, 0 );
    bo_add_32be( stts, 0 );     // entry-count (fixed latter)

    if( p_stream->fmt.i_cat == AUDIO_ES )
        i_timescale = p_stream->fmt.audio.i_rate;
    else
        i_timescale = 1001;

    /* first, create quantified length */
    for( i = 0, i_dts = 0, i_dts_q = 0; i < p_stream->i_entry_count; i++ )
    {
        int64_t i_dts_deq = i_dts_q * I64C(1000000) / (int64_t)i_timescale;
        int64_t i_delta = p_stream->entry[i].i_length + i_dts - i_dts_deq;

        i_dts += p_stream->entry[i].i_length;

        p_stream->entry[i].i_length =
            i_delta * (int64_t)i_timescale / I64C(1000000);

        i_dts_q += p_stream->entry[i].i_length;
    }
    /* then write encoded table */
    for( i = 0, i_index = 0; i < p_stream->i_entry_count; i_index++)
    {
        int     i_first = i;
        int64_t i_delta = p_stream->entry[i].i_length;

        while( i < p_stream->i_entry_count )
        {
            i++;
            if( i >= p_stream->i_entry_count ||
                p_stream->entry[i].i_length != i_delta )
            {
                break;
            }
        }

        bo_add_32be( stts, i - i_first ); // sample-count
        bo_add_32be( stts, i_delta );     // sample-delta
    }
    bo_fix_32be( stts, 12, i_index );
    box_fix( stts );

    /* FIXME add ctts ?? FIXME */

    stsz = box_full_new( "stsz", 0, 0 );
    bo_add_32be( stsz, 0 );                             // sample-size
    bo_add_32be( stsz, p_stream->i_entry_count );       // sample-count
    for( i = 0; i < p_stream->i_entry_count; i++ )
    {
        bo_add_32be( stsz, p_stream->entry[i].i_size ); // sample-size
    }
    box_fix( stsz );

    /* create stss table */
    stss = NULL;
    for( i = 0, i_index = 0; i < p_stream->i_entry_count; i++ )
    {
        if( p_stream->entry[i].i_flags & BLOCK_FLAG_TYPE_I )
        {
            if( stss == NULL )
            {
                stss = box_full_new( "stss", 0, 0 );
                bo_add_32be( stss, 0 ); /* fixed later */
            }
            bo_add_32be( stss, 1 + i );
            i_index++;
        }
    }
    if( stss )
    {
        bo_fix_32be( stss, 12, i_index );
        box_fix( stss );
    }

    /* Now gather all boxes into stbl */
    box_gather( stbl, stsd );
    box_gather( stbl, stts );
    if( stss )
    {
        box_gather( stbl, stss );
    }
    box_gather( stbl, stsc );
    box_gather( stbl, stsz );
    p_stream->i_stco_pos = stbl->i_buffer + 16;
    box_gather( stbl, stco );

    /* finish stbl */
    box_fix( stbl );

    return stbl;
}

static int64_t get_timestamp();

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

    /* Create general info */
    for( i_trak = 0; i_trak < p_sys->i_nb_streams; i_trak++ )
    {
        mp4_stream_t *p_stream = p_sys->pp_streams[i_trak];
        i_movie_duration = __MAX( i_movie_duration, p_stream->i_duration );
    }
    msg_Dbg( p_mux, "movie duration %ds",
             (uint32_t)( i_movie_duration / (mtime_t)1000000 ) );

    i_movie_duration = i_movie_duration * i_movie_timescale / 1000000;

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

    /* Next available track id */
    bo_add_32be( mvhd, p_sys->i_nb_streams + 1 ); // next-track-id

    box_fix( mvhd );
    box_gather( moov, mvhd );

    for( i_trak = 0; i_trak < p_sys->i_nb_streams; i_trak++ )
    {
        mp4_stream_t *p_stream;
        uint32_t     i_timescale;

        bo_t *trak, *tkhd, *edts, *elst, *mdia, *mdhd, *hdlr;
        bo_t *minf, *dinf, *dref, *url, *stbl;

        p_stream = p_sys->pp_streams[i_trak];

        if( p_stream->fmt.i_cat == AUDIO_ES )
            i_timescale = p_stream->fmt.audio.i_rate;
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
        bo_add_16be( tkhd, p_stream->fmt.i_cat == AUDIO_ES ? 0x100 : 0 );
        bo_add_16be( tkhd, 0 );                     // reserved
        for( i = 0; i < 9; i++ )
        {
            bo_add_32be( tkhd, mvhd_matrix[i] );    // matrix
        }
        if( p_stream->fmt.i_cat == AUDIO_ES )
        {
            bo_add_32be( tkhd, 0 );                 // width (presentation)
            bo_add_32be( tkhd, 0 );                 // height(presentation)
        }
        else if( p_stream->fmt.i_cat == VIDEO_ES )
        {
            int i_width = p_stream->fmt.video.i_width << 16;
            if( p_stream->fmt.video.i_aspect > 0 )
            {
                i_width = (int64_t)p_stream->fmt.video.i_aspect *
                          ((int64_t)p_stream->fmt.video.i_height << 16) /
                          VOUT_ASPECT_FACTOR;
            }
            // width (presentation)
            bo_add_32be( tkhd, i_width );
            // height(presentation)
            bo_add_32be( tkhd, p_stream->fmt.video.i_height << 16 );
        }
        else
        {
            int i_width = 320 << 16;
            int i_height = 200;
            int i;
            for( i = 0; i < p_sys->i_nb_streams; i++ )
            {
                mp4_stream_t *tk = p_sys->pp_streams[i];
                if( tk->fmt.i_cat == VIDEO_ES )
                {
                    if( p_stream->fmt.video.i_aspect )
                        i_width = (int64_t)p_stream->fmt.video.i_aspect *
                                   ((int64_t)p_stream->fmt.video.i_height<<16) / VOUT_ASPECT_FACTOR;
                    else
                        i_width = p_stream->fmt.video.i_width << 16;
                    i_height = p_stream->fmt.video.i_height;
                    break;
                }
            }
            bo_add_32be( tkhd, i_width );     // width (presentation)
            bo_add_32be( tkhd, i_height << 16 );    // height(presentation)
        }

        box_fix( tkhd );
        box_gather( trak, tkhd );

        /* *** add /moov/trak/edts and elst */
        edts = box_new( "edts" );
        elst = box_full_new( "elst", p_sys->b_64_ext ? 1 : 0, 0 );
        if( p_stream->i_dts_start > p_sys->i_dts_start )
        {
            bo_add_32be( elst, 2 );

            if( p_sys->b_64_ext )
            {
                bo_add_64be( elst, (p_stream->i_dts_start-p_sys->i_dts_start) *
                             i_movie_timescale / I64C(1000000) );
                bo_add_64be( elst, -1 );
            }
            else
            {
                bo_add_32be( elst, (p_stream->i_dts_start-p_sys->i_dts_start) *
                             i_movie_timescale / I64C(1000000) );
                bo_add_32be( elst, -1 );
            }
            bo_add_16be( elst, 1 );
            bo_add_16be( elst, 0 );
        }
        else
        {
            bo_add_32be( elst, 1 );
        }
        if( p_sys->b_64_ext )
        {
            bo_add_64be( elst, p_stream->i_duration *
                         i_movie_timescale / I64C(1000000) );
            bo_add_64be( elst, 0 );
        }
        else
        {
            bo_add_32be( elst, p_stream->i_duration *
                         i_movie_timescale / I64C(1000000) );
            bo_add_32be( elst, 0 );
        }
        bo_add_16be( elst, 1 );
        bo_add_16be( elst, 0 );

        box_fix( elst );
        box_gather( edts, elst );
        box_fix( edts );
        box_gather( trak, edts );

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

        if( p_stream->fmt.psz_language )
        {
            char *psz = p_stream->fmt.psz_language;
            const iso639_lang_t *pl = NULL;
            uint16_t lang = 0x0;

            if( strlen( psz ) == 2 )
            {
                pl = GetLang_1( psz );
            }
            else if( strlen( psz ) == 3 )
            {
                pl = GetLang_2B( psz );
                if( !strcmp( pl->psz_iso639_1, "??" ) )
                {
                    pl = GetLang_2T( psz );
                }
            }
            if( pl && strcmp( pl->psz_iso639_1, "??" ) )
            {
                lang = ( ( pl->psz_iso639_2T[0] - 0x60 ) << 10 ) |
                       ( ( pl->psz_iso639_2T[1] - 0x60 ) <<  5 ) |
                       ( ( pl->psz_iso639_2T[2] - 0x60 ) );
            }
            bo_add_16be( mdhd, lang );          // language
        }
        else
        {
            bo_add_16be( mdhd, 0    );          // language
        }
        bo_add_16be( mdhd, 0    );              // predefined
        box_fix( mdhd );
        box_gather( mdia, mdhd );

        /* handler reference */
        hdlr = box_full_new( "hdlr", 0, 0 );

        if( p_sys->b_mov )
            bo_add_fourcc( hdlr, "mhlr" );         // media handler
        else
            bo_add_32be( hdlr, 0 );

        if( p_stream->fmt.i_cat == AUDIO_ES )
            bo_add_fourcc( hdlr, "soun" );
        else if( p_stream->fmt.i_cat == VIDEO_ES )
            bo_add_fourcc( hdlr, "vide" );
        else if( p_stream->fmt.i_cat == SPU_ES )
            bo_add_fourcc( hdlr, "text" );

        bo_add_32be( hdlr, 0 );         // reserved
        bo_add_32be( hdlr, 0 );         // reserved
        bo_add_32be( hdlr, 0 );         // reserved

        if( p_sys->b_mov )
            bo_add_8( hdlr, 12 );   /* Pascal string for .mov */

        if( p_stream->fmt.i_cat == AUDIO_ES )
            bo_add_mem( hdlr, 12, (uint8_t*)"SoundHandler" );
        else if( p_stream->fmt.i_cat == VIDEO_ES )
            bo_add_mem( hdlr, 12, (uint8_t*)"VideoHandler" );
        else
            bo_add_mem( hdlr, 12, (uint8_t*)"Text Handler" );

        if( !p_sys->b_mov )
            bo_add_8( hdlr, 0 );   /* asciiz string for .mp4, yes that's BRAIN DAMAGED F**K MP4 */

        box_fix( hdlr );
        box_gather( mdia, hdlr );

        /* minf*/
        minf = box_new( "minf" );

        /* add smhd|vmhd */
        if( p_stream->fmt.i_cat == AUDIO_ES )
        {
            bo_t *smhd;

            smhd = box_full_new( "smhd", 0, 0 );
            bo_add_16be( smhd, 0 );     // balance
            bo_add_16be( smhd, 0 );     // reserved
            box_fix( smhd );

            box_gather( minf, smhd );
        }
        else if( p_stream->fmt.i_cat == VIDEO_ES )
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
        else if( p_stream->fmt.i_cat == SPU_ES )
        {
            bo_t *gmhd = box_new( "gmhd" );
            bo_t *gmin = box_full_new( "gmin", 0, 1 );

            bo_add_16be( gmin, 0 );     // graphicsmode
            for( i = 0; i < 3; i++ )
            {
                bo_add_16be( gmin, 0 ); // opcolor
            }
            bo_add_16be( gmin, 0 );     // balance
            bo_add_16be( gmin, 0 );     // reserved
            box_fix( gmin );

            box_gather( gmhd, gmin );
            box_fix( gmhd );

            box_gather( minf, gmhd );
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

static block_t * bo_to_sout( sout_instance_t *p_sout,  bo_t *box )
{
    block_t *p_buf;

    p_buf = block_New( p_sout, box->i_buffer );
    if( box->i_buffer > 0 )
    {
        memcpy( p_buf->p_buffer, box->p_buffer, box->i_buffer );
    }

    return p_buf;
}

static void box_send( sout_mux_t *p_mux,  bo_t *box )
{
    block_t *p_buf;

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
