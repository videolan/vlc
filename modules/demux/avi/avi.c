/*****************************************************************************
 * avi.c : AVI file Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: avi.c,v 1.16 2002/12/04 15:47:31 fenrir Exp $
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */
#include <errno.h>
#include <sys/types.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "video.h"

#include "libavi.h"

#define __AVI_SUBTITLE__ 1

#ifdef __AVI_SUBTITLE__
#   include "../util/sub.h"
#endif
#include "avi.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int    AVIInit   ( vlc_object_t * );
static void __AVIEnd    ( vlc_object_t * );
static int    AVISeek   ( input_thread_t *, mtime_t, int );
static int    AVIDemux_Seekable  ( input_thread_t * );
static int    AVIDemux_UnSeekable( input_thread_t *p_input );

#define AVIEnd(a) __AVIEnd(VLC_OBJECT(a))

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    add_category_hint( "demuxer", NULL );
        add_bool( "avi-interleaved", 0, NULL,
                  "force interleaved method", 
                  "force interleaved method" );
        add_bool( "avi-index", 0, NULL,
                  "force index creation", 
                  "force index creation" );

    set_description( "avi demuxer" );
    set_capability( "demux", 212 );
    set_callbacks( AVIInit, __AVIEnd );
vlc_module_end();

/*****************************************************************************
 * Some useful functions to manipulate memory 
 *****************************************************************************/

static uint16_t GetWLE( uint8_t *p_buff )
{
    return (uint16_t)p_buff[0] | ( ((uint16_t)p_buff[1]) << 8 );
}

static uint32_t GetDWLE( uint8_t *p_buff )
{
    return (uint32_t)p_buff[0] | ( ((uint32_t)p_buff[1]) << 8 ) |
            ( ((uint32_t)p_buff[2]) << 16 ) | ( ((uint32_t)p_buff[3]) << 24 );
}

static uint32_t GetDWBE( uint8_t *p_buff )
{
    return (uint32_t)p_buff[3] | ( ((uint32_t)p_buff[2]) << 8 ) |
            ( ((uint32_t)p_buff[1]) << 16 ) | ( ((uint32_t)p_buff[0]) << 24 );
}
static vlc_fourcc_t GetFOURCC( byte_t *p_buff )
{
    return VLC_FOURCC( p_buff[0], p_buff[1], p_buff[2], p_buff[3] );
}

static inline off_t __EVEN( off_t i )
{
    return (i & 1) ? i + 1 : i;
}

#define __ABS( x ) ( (x) < 0 ? (-(x)) : (x) )

/* read data in a pes */
static int input_ReadInPES( input_thread_t *p_input, 
                            pes_packet_t **pp_pes, 
                            int i_size )
{
    pes_packet_t *p_pes;
    data_packet_t *p_data;

    
    if( !(p_pes = input_NewPES( p_input->p_method_data ) ) )
    {
        pp_pes = NULL;
        return -1;
    }

    *pp_pes = p_pes;

    if( !i_size )
    {
        p_pes->p_first = 
            p_pes->p_last  = 
                input_NewPacket( p_input->p_method_data, 0 );
        p_pes->i_nb_data = 1;
        p_pes->i_pes_size = 0;
        return 0;
    }
    
    p_pes->i_nb_data = 0;
    p_pes->i_pes_size = 0;

    while( p_pes->i_pes_size < i_size )
    {
        int i_read;

        i_read = input_SplitBuffer(p_input, 
                                   &p_data, 
                                   __MIN( i_size - 
                                          p_pes->i_pes_size, 1024 ) );
        if( i_read <= 0 )
        {
            return p_pes->i_pes_size;
        }
        
        if( !p_pes->p_first )
        {
            p_pes->p_first = p_data;
        }
        else
        {
            p_pes->p_last->p_next = p_data;
        }
        p_pes->p_last = p_data;
        p_pes->i_nb_data++;
        p_pes->i_pes_size += i_read;
    } 


    return p_pes->i_pes_size;
}

/* Test if it seems that it's a key frame */
static int AVI_GetKeyFlag( vlc_fourcc_t i_fourcc, uint8_t *p_byte )
{
    switch( i_fourcc )
    {
        case FOURCC_DIV1:
            /* we have:
             *  startcode:      0x00000100   32bits
             *  framenumber     ?             5bits
             *  piture type     0(I),1(P)     2bits
             */
            if( GetDWBE( p_byte ) != 0x00000100 ) 
            {
                /* it's not an msmpegv1 stream, strange...*/
                return AVIIF_KEYFRAME;
            }
            else
            {
                return p_byte[4] & 0x06 ? 0 : AVIIF_KEYFRAME;
            }
        case FOURCC_DIV2:
        case FOURCC_DIV3:   // wmv1 also
            /* we have
             *  picture type    0(I),1(P)     2bits
             */
            return p_byte[0] & 0xC0 ? 0 : AVIIF_KEYFRAME;
        case FOURCC_mp4v:
            /* we should find first occurence of 0x000001b6 (32bits)
             *  startcode:      0x000001b6   32bits
             *  piture type     0(I),1(P)     2bits
             */
            if( GetDWBE( p_byte ) != 0x000001b6 )
            {
                /* not true , need to find the first VOP header */
                return AVIIF_KEYFRAME;
            }
            else
            {
                return p_byte[4] & 0xC0 ? 0 : AVIIF_KEYFRAME;
            }
        default:
            /* I can't do it, so say yes */
            return AVIIF_KEYFRAME;
    }
}

vlc_fourcc_t AVI_FourccGetCodec( int i_cat, vlc_fourcc_t i_codec )
{
    switch( i_cat )
    {
        case AUDIO_ES:
            switch( i_codec )
            {
                case WAVE_FORMAT_PCM:
                    return VLC_FOURCC( 'a', 'r', 'a', 'w' );
                case WAVE_FORMAT_MPEG:
                case WAVE_FORMAT_MPEGLAYER3:
                    return VLC_FOURCC( 'm', 'p', 'g', 'a' );
                case WAVE_FORMAT_A52:
                    return VLC_FOURCC( 'a', '5', '2', ' ' );
                case WAVE_FORMAT_WMA1:
                    return VLC_FOURCC( 'w', 'm', 'a', '1' );
                case WAVE_FORMAT_WMA2:
                    return VLC_FOURCC( 'w', 'm', 'a', '2' );
                default:
                    return VLC_FOURCC( 'm', 's', 
                                       ( i_codec >> 8 )&0xff, i_codec&0xff );
            }
        case VIDEO_ES:
            // XXX DIV1 <- msmpeg4v1, DIV2 <- msmpeg4v2, DIV3 <- msmpeg4v3, mp4v for mpeg4 
            switch( i_codec )
            {
                case FOURCC_DIV1:
                case FOURCC_div1:
                case FOURCC_MPG4:
                case FOURCC_mpg4:
                    return FOURCC_DIV1;
                case FOURCC_DIV2:
                case FOURCC_div2:
                case FOURCC_MP42:
                case FOURCC_mp42:
                case FOURCC_MPG3:
                case FOURCC_mpg3:
                    return FOURCC_DIV2;
                case FOURCC_div3:
                case FOURCC_MP43:
                case FOURCC_mp43:
                case FOURCC_DIV3:
                case FOURCC_DIV4:
                case FOURCC_div4:
                case FOURCC_DIV5:
                case FOURCC_div5:
                case FOURCC_DIV6:
                case FOURCC_div6:
                case FOURCC_AP41:
                case FOURCC_3IV1:
                    return FOURCC_DIV3;
                case FOURCC_DIVX:
                case FOURCC_divx:
                case FOURCC_MP4S:
                case FOURCC_mp4s:
                case FOURCC_M4S2:
                case FOURCC_m4s2:
                case FOURCC_xvid:
                case FOURCC_XVID:
                case FOURCC_XviD:
                case FOURCC_DX50:
                case FOURCC_mp4v:
                case FOURCC_4:
                    return FOURCC_mp4v;
            }
        default:
            return VLC_FOURCC( 'u', 'n', 'd', 'f' );
    }
}

static void AVI_ParseStreamHeader( vlc_fourcc_t i_id,
                                   int *pi_number, int *pi_type )
{
#define SET_PTR( p, v ) if( p ) *(p) = (v);
    int c1, c2;

    c1 = ((uint8_t *)&i_id)[0];
    c2 = ((uint8_t *)&i_id)[1];

    if( c1 < '0' || c1 > '9' || c2 < '0' || c2 > '9' )
    {
        SET_PTR( pi_number, 100 ); /* > max stream number */
        SET_PTR( pi_type, UNKNOWN_ES );
    }
    else
    {
        SET_PTR( pi_number, (c1 - '0') * 10 + (c2 - '0' ) );
        switch( VLC_TWOCC( ((uint8_t *)&i_id)[2], ((uint8_t *)&i_id)[3] ) )
        {
            case AVITWOCC_wb:
                SET_PTR( pi_type, AUDIO_ES );
                break;
            case AVITWOCC_dc:
            case AVITWOCC_db:
                SET_PTR( pi_type, VIDEO_ES );
                break;
            default:
                SET_PTR( pi_type, UNKNOWN_ES );
                break;
        }
    }
#undef SET_PTR
}

static int AVI_PacketGetHeader( input_thread_t *p_input, avi_packet_t *p_pk )
{
    uint8_t  *p_peek;
    
    if( input_Peek( p_input, &p_peek, 16 ) < 16 )
    {
        return VLC_EGENERIC;
    }
    p_pk->i_fourcc  = GetFOURCC( p_peek );
    p_pk->i_size    = GetDWLE( p_peek + 4 );
    p_pk->i_pos     = AVI_TellAbsolute( p_input );
    if( p_pk->i_fourcc == AVIFOURCC_LIST )
    {
        p_pk->i_type = GetFOURCC( p_peek + 8 );
    }
    else
    {
        p_pk->i_type = 0;
    }
    
    memcpy( p_pk->i_peek, p_peek + 8, 8 );

    AVI_ParseStreamHeader( p_pk->i_fourcc, &p_pk->i_stream, &p_pk->i_cat );
    return VLC_SUCCESS;
}

static int AVI_PacketNext( input_thread_t *p_input )
{
    avi_packet_t    avi_ck;

    if( AVI_PacketGetHeader( p_input, &avi_ck ) )
    {
        return VLC_EGENERIC;
    }
    if( avi_ck.i_fourcc == AVIFOURCC_LIST && avi_ck.i_type == AVIFOURCC_rec )
    {
        return AVI_SkipBytes( p_input, 12 );
    }
    else
    {
        return AVI_SkipBytes( p_input, __EVEN( avi_ck.i_size ) + 8 );
    }
}
static int AVI_PacketRead( input_thread_t   *p_input,
                           avi_packet_t     *p_pk,
                           pes_packet_t     **pp_pes )
{
    int i_size;
    vlc_bool_t b_pad;

    i_size = __EVEN( p_pk->i_size + 8 );
    b_pad  = ( i_size != p_pk->i_size + 8 );
    
    if( input_ReadInPES( p_input, pp_pes, i_size ) != i_size )
    {
        return VLC_EGENERIC;
    }
    (*pp_pes)->p_first->p_payload_start += 8;
    (*pp_pes)->i_pes_size -= 8;

    if( b_pad )
    {
        (*pp_pes)->p_last->p_payload_end--;
        (*pp_pes)->i_pes_size--;
    }

    return VLC_SUCCESS;
}

static int AVI_PacketSearch( input_thread_t *p_input )
{
    demux_sys_t     *p_avi = p_input->p_demux_data;

    avi_packet_t    avi_pk;
    for( ;; )
    {
        if( AVI_SkipBytes( p_input, 1 ) )
        {
            return VLC_EGENERIC;
        }
        AVI_PacketGetHeader( p_input, &avi_pk );
        if( avi_pk.i_stream < p_avi->i_streams &&
            ( avi_pk.i_cat == AUDIO_ES || avi_pk.i_cat == VIDEO_ES ) )
        {
            return VLC_SUCCESS;
        }
        switch( avi_pk.i_fourcc )
        {
            case AVIFOURCC_JUNK:
            case AVIFOURCC_LIST:
            case AVIFOURCC_idx1:
                return VLC_SUCCESS;
        }
    }
}


static void __AVI_AddEntryIndex( avi_stream_t *p_info,
                                 AVIIndexEntry_t *p_index)
{
    if( p_info->p_index == NULL )
    {
        p_info->i_idxmax = 16384;
        p_info->i_idxnb = 0;
        if( !( p_info->p_index = calloc( p_info->i_idxmax, 
                                  sizeof( AVIIndexEntry_t ) ) ) )
        {
            return;
        }
    }
    if( p_info->i_idxnb >= p_info->i_idxmax )
    {
        p_info->i_idxmax += 16384;
        if( !( p_info->p_index = realloc( (void*)p_info->p_index,
                           p_info->i_idxmax * 
                           sizeof( AVIIndexEntry_t ) ) ) )
        {
            return;
        }
    }
    /* calculate cumulate length */
    if( p_info->i_idxnb > 0 )
    {
        p_index->i_lengthtotal = 
            p_info->p_index[p_info->i_idxnb - 1].i_length +
                p_info->p_index[p_info->i_idxnb - 1].i_lengthtotal;
    }
    else
    {
        p_index->i_lengthtotal = 0;
    }

    p_info->p_index[p_info->i_idxnb] = *p_index;
    p_info->i_idxnb++;

}

static void AVI_IndexAddEntry( demux_sys_t *p_avi, 
                               int i_stream, 
                               AVIIndexEntry_t *p_index)
{
    __AVI_AddEntryIndex( p_avi->pp_info[i_stream],
                         p_index );
    if( p_avi->i_movi_lastchunk_pos < p_index->i_pos )
    {
        p_avi->i_movi_lastchunk_pos = p_index->i_pos;
    }
}

static void AVI_IndexLoad( input_thread_t *p_input )
{
    demux_sys_t *p_avi = p_input->p_demux_data;
    
    avi_chunk_list_t    *p_riff;
    avi_chunk_list_t    *p_movi;
    avi_chunk_idx1_t    *p_idx1;

    int i_stream;
    int i_index;
    off_t   i_offset;
    
    p_riff = (avi_chunk_list_t*)AVI_ChunkFind( &p_avi->ck_root, 
                                               AVIFOURCC_RIFF, 0);
    
    p_idx1 = (avi_chunk_idx1_t*)AVI_ChunkFind( p_riff, AVIFOURCC_idx1, 0);
    p_movi = (avi_chunk_list_t*)AVI_ChunkFind( p_riff, AVIFOURCC_movi, 0);

    if( !p_idx1 )
    {
        msg_Warn( p_input, "cannot find idx1 chunk, no index defined" );
        return;
    }
    for( i_stream = 0; i_stream < p_avi->i_streams; i_stream++ )
    {
        p_avi->pp_info[i_stream]->i_idxnb  = 0;
        p_avi->pp_info[i_stream]->i_idxmax = 0;
        p_avi->pp_info[i_stream]->p_index  = NULL;
    }
    /* *** calculate offset *** */
    if( p_idx1->i_entry_count > 0 && 
        p_idx1->entry[0].i_pos < p_movi->i_chunk_pos )
    {
        i_offset = p_movi->i_chunk_pos + 8;
    }
    else
    {
        i_offset = 0;
    }

    for( i_index = 0; i_index < p_idx1->i_entry_count; i_index++ )
    {
        int i_cat;
        
        AVI_ParseStreamHeader( p_idx1->entry[i_index].i_fourcc,
                               &i_stream,
                               &i_cat );
        if( i_stream < p_avi->i_streams &&
            i_cat == p_avi->pp_info[i_stream]->i_cat )
        {
            AVIIndexEntry_t index;
            index.i_id      = p_idx1->entry[i_index].i_fourcc;
            index.i_flags   = 
                p_idx1->entry[i_index].i_flags&(~AVIIF_FIXKEYFRAME);
            index.i_pos     = p_idx1->entry[i_index].i_pos + i_offset;
            index.i_length  = p_idx1->entry[i_index].i_length;
            AVI_IndexAddEntry( p_avi, i_stream, &index );
        }
    }
    for( i_stream = 0; i_stream < p_avi->i_streams; i_stream++ )
    {
        msg_Dbg( p_input, 
                "stream[%d] creating %d index entries", 
                i_stream,
                p_avi->pp_info[i_stream]->i_idxnb );
    }
    
}

static void AVI_IndexCreate( input_thread_t *p_input )
{
    demux_sys_t *p_avi = p_input->p_demux_data;
    
    avi_chunk_list_t    *p_riff;
    avi_chunk_list_t    *p_movi;

    int i_stream;
    off_t   i_movi_end;
    
    p_riff = (avi_chunk_list_t*)AVI_ChunkFind( &p_avi->ck_root, 
                                               AVIFOURCC_RIFF, 0);
    p_movi = (avi_chunk_list_t*)AVI_ChunkFind( p_riff, AVIFOURCC_movi, 0);
    
    if( !p_movi )
    {
        msg_Err( p_input, "cannot find p_movi" );
        return;
    }

    for( i_stream = 0; i_stream < p_avi->i_streams; i_stream++ )
    {
        p_avi->pp_info[i_stream]->i_idxnb  = 0;
        p_avi->pp_info[i_stream]->i_idxmax = 0;
        p_avi->pp_info[i_stream]->p_index  = NULL;
    }
    i_movi_end = __MIN( p_movi->i_chunk_pos + p_movi->i_chunk_size,
                        p_input->stream.p_selected_area->i_size );

    AVI_SeekAbsolute( p_input, p_movi->i_chunk_pos + 12);
    msg_Warn( p_input, "creating index from LIST-movi, will take time !" );
    for( ;; )
    {
        avi_packet_t pk;
        
        if( AVI_PacketGetHeader( p_input, &pk ) )
        {
            break;
        }
        if( pk.i_stream < p_avi->i_streams &&
            pk.i_cat == p_avi->pp_info[pk.i_stream]->i_cat )
        {
            AVIIndexEntry_t index;
            index.i_id      = pk.i_fourcc;
            index.i_flags   = 
               AVI_GetKeyFlag(p_avi->pp_info[pk.i_stream]->i_codec, pk.i_peek);
            index.i_pos     = pk.i_pos;
            index.i_length  = pk.i_size;
            AVI_IndexAddEntry( p_avi, pk.i_stream, &index );
        }
        else
        {
            switch( pk.i_fourcc )
            {
                case AVIFOURCC_idx1:
                    goto print_stat;
                case AVIFOURCC_rec:
                case AVIFOURCC_JUNK:
                    break;
                default:
                    msg_Warn( p_input, "need resync, probably broken avi" );
                    if( AVI_PacketSearch( p_input ) )
                    {
                        msg_Warn( p_input, "lost sync, abord index creation" );
                        goto print_stat;
                    }
            }
        }
        if( pk.i_pos + pk.i_size >= i_movi_end ||
            AVI_PacketNext( p_input ) )
        {
            break;
        }
    }

print_stat:
    for( i_stream = 0; i_stream < p_avi->i_streams; i_stream++ )
    {
        msg_Dbg( p_input, 
                "stream[%d] creating %d index entries", 
                i_stream,
                p_avi->pp_info[i_stream]->i_idxnb );
    }
}


/*****************************************************************************
 * Stream management
 *****************************************************************************/
static vlc_bool_t AVI_StreamStart ( input_thread_t *, demux_sys_t *, int );
static int  AVI_StreamSeek   ( input_thread_t *, demux_sys_t *, int, mtime_t );
static void AVI_StreamStop   ( input_thread_t *, demux_sys_t *, int );

static vlc_bool_t AVI_StreamStart( input_thread_t *p_input,  
                                   demux_sys_t *p_avi, int i_stream )
{
#define p_stream    p_avi->pp_info[i_stream]
    if( !p_stream->p_es )
    {
        msg_Warn( p_input, "stream[%d] unselectable", i_stream );
        return VLC_FALSE;
    }
    if( p_stream->b_activated )
    {
        msg_Warn( p_input, "stream[%d] already selected", i_stream );
        return VLC_TRUE;
    }
    
    if( !p_stream->p_es->p_decoder_fifo )
    {
        vlc_mutex_lock( &p_input->stream.stream_lock );
        input_SelectES( p_input, p_stream->p_es );
        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }
    p_stream->b_activated = p_stream->p_es->p_decoder_fifo ? VLC_TRUE
                                                           : VLC_FALSE;
    if( p_stream->b_activated && p_avi->b_seekable)
    {
        AVI_StreamSeek( p_input, p_avi, i_stream, p_avi->i_time );
    }

    return p_stream->b_activated;
#undef  p_stream
}

static void    AVI_StreamStop( input_thread_t *p_input,
                               demux_sys_t *p_avi, int i_stream )
{
#define p_stream    p_avi->pp_info[i_stream]

    if( !p_stream->b_activated )
    {
        msg_Warn( p_input, "stream[%d] already unselected", i_stream );
        return;
    }
    
    if( p_stream->p_es->p_decoder_fifo )
    {
        vlc_mutex_lock( &p_input->stream.stream_lock );
        input_UnselectES( p_input, p_stream->p_es );
        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }

            
    p_stream->b_activated = VLC_FALSE;

#undef  p_stream
}

/****************************************************************************
 * AVI_MovieGetLength give max streams length in second
 ****************************************************************************/
static mtime_t  AVI_MovieGetLength( input_thread_t *p_input, demux_sys_t *p_avi )
{
    int i_stream;
    mtime_t i_maxlength;
    
    i_maxlength = 0;
    for( i_stream = 0; i_stream < p_avi->i_streams; i_stream++ )
    {
#define p_stream  p_avi->pp_info[i_stream]
        mtime_t i_length;
        /* fix length for each stream */
        if( p_stream->i_idxnb < 1 || !p_stream->p_index )
        {
            continue;
        }

        if( p_stream->i_samplesize )
        {
            i_length = 
                (mtime_t)( p_stream->p_index[p_stream->i_idxnb-1].i_lengthtotal + 
                           p_stream->p_index[p_stream->i_idxnb-1].i_length ) /
                (mtime_t)p_stream->i_scale /
                (mtime_t)p_stream->i_rate /
                (mtime_t)p_stream->i_samplesize;
        }
        else
        {
            i_length = (mtime_t)p_stream->i_idxnb *
                       (mtime_t)p_stream->i_scale /
                       (mtime_t)p_stream->i_rate;
        }

        msg_Dbg( p_input, 
                 "stream[%d] length:"I64Fd" (based on index)",
                 i_stream,
                 i_length );
        i_maxlength = __MAX( i_maxlength, i_length );
#undef p_stream                         
    }

    return i_maxlength;
}

/*****************************************************************************
 * AVIEnd: frees unused data
 *****************************************************************************/
static void __AVIEnd ( vlc_object_t * p_this )
{   
    input_thread_t *    p_input = (input_thread_t *)p_this;
    int i;
    demux_sys_t *p_avi = p_input->p_demux_data  ; 
    
    if( p_avi->pp_info )
    {
        for( i = 0; i < p_avi->i_streams; i++ )
        {
            if( p_avi->pp_info[i] ) 
            {
                if( p_avi->pp_info[i]->p_index )
                {
                      free( p_avi->pp_info[i]->p_index );
                }
                free( p_avi->pp_info[i] ); 
            }
        }
         free( p_avi->pp_info );
    }
#ifdef __AVI_SUBTITLE__
    if( p_avi->p_sub )
    {
        subtitle_Close( p_avi->p_sub );
        p_avi->p_sub = NULL;
    }
#endif
    AVI_ChunkFreeRoot( p_input, &p_avi->ck_root );
}

/*****************************************************************************
 * AVIInit: check file and initializes AVI structures
 *****************************************************************************/
static int AVIInit( vlc_object_t * p_this )
{   
    input_thread_t *    p_input = (input_thread_t *)p_this;
    avi_chunk_t         ck_riff;
    avi_chunk_list_t    *p_riff = (avi_chunk_list_t*)&ck_riff;
    avi_chunk_list_t    *p_hdrl, *p_movi;
#if 0
    avi_chunk_list_t    *p_INFO;
    avi_chunk_strz_t    *p_name;
#endif
    avi_chunk_avih_t    *p_avih;
    demux_sys_t *p_avi;
    es_descriptor_t *p_es = NULL; /* avoid warning */
    int i;
#ifdef __AVI_SUBTITLE__
    mtime_t i_microsecperframe = 0; // for some subtitle format
#endif
    
    vlc_bool_t b_stream_audio, b_stream_video; 

    p_input->pf_demux = AVIDemux_Seekable;
    if( AVI_TestFile( p_input ) )
    {
        msg_Warn( p_input, "avi module discarded (invalid headr)" );
        return VLC_EGENERIC;
    }

    /* Initialize access plug-in structures. */
    if( p_input->i_mtu == 0 )
    {
        /* Improve speed. */
        p_input->i_bufsize = INPUT_DEFAULT_BUFSIZE;
    }

    if( !( p_input->p_demux_data = 
                    p_avi = malloc( sizeof(demux_sys_t) ) ) )
    {
        msg_Err( p_input, "out of memory" );
        return VLC_ENOMEM;
    }
    memset( p_avi, 0, sizeof( demux_sys_t ) );
    p_avi->i_time = 0;
    p_avi->i_pcr  = 0;
    p_avi->b_seekable = ( ( p_input->stream.b_seekable )
                        &&( p_input->stream.i_method == INPUT_METHOD_FILE ) );
    p_avi->i_movi_lastchunk_pos = 0;

    /* *** for unseekable stream, automaticaly use AVIDemux_interleaved *** */
    if( !p_avi->b_seekable || config_GetInt( p_input, "avi-interleaved" ) )
    {
        p_input->pf_demux = AVIDemux_UnSeekable;
    }
    
    if( AVI_ChunkReadRoot( p_input, &p_avi->ck_root, p_avi->b_seekable ) )
    {
        msg_Err( p_input, "avi module discarded (invalid file)" );
        return VLC_EGENERIC;
    }
    AVI_ChunkDumpDebug( p_input, &p_avi->ck_root );


    p_riff  = (avi_chunk_list_t*)AVI_ChunkFind( &p_avi->ck_root, 
                                                AVIFOURCC_RIFF, 0 );
    p_hdrl  = (avi_chunk_list_t*)AVI_ChunkFind( p_riff,
                                                AVIFOURCC_hdrl, 0 );
    p_movi  = (avi_chunk_list_t*)AVI_ChunkFind( p_riff, 
                                                AVIFOURCC_movi, 0 );
#if 0
    p_INFO  = (avi_chunk_list_t*)AVI_ChunkFind( p_riff,
                                                AVIFOURCC_INFO, 0 );
    p_name  = (avi_chunk_strz_t*)AVI_ChunkFind( p_INFO,
                                                AVIFOURCC_INAM, 0 );
    if( p_name )
    {
        
    }
#endif

    if( !p_hdrl || !p_movi )
    {
        msg_Err( p_input, "avi module discarded (invalid file)" );
        return VLC_EGENERIC;
    }
    
    if( !( p_avih = (avi_chunk_avih_t*)AVI_ChunkFind( p_hdrl, 
                                                      AVIFOURCC_avih, 0 ) ) )
    {
        msg_Err( p_input, "cannot find avih chunk" );
        return VLC_EGENERIC;
    }
    p_avi->i_streams = AVI_ChunkCount( p_hdrl, AVIFOURCC_strl );
    if( p_avih->i_streams != p_avi->i_streams )
    {
        msg_Warn( p_input, 
                  "found %d stream but %d are declared",
                  p_avi->i_streams,
                  p_avih->i_streams );
    }
    if( p_avi->i_streams == 0 )
    {
        AVIEnd( p_input );
        msg_Err( p_input, "no stream defined!" );
        return VLC_EGENERIC;
    }

    /*  create one program */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1)
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        AVIEnd( p_input );
        msg_Err( p_input, "cannot init stream" );
        return VLC_EGENERIC;
    }
    if( input_AddProgram( p_input, 0, 0) == NULL )
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        AVIEnd( p_input );
        msg_Err( p_input, "cannot add program" );
        return VLC_EGENERIC;
    }
    p_input->stream.p_selected_program = p_input->stream.pp_programs[0];
    vlc_mutex_unlock( &p_input->stream.stream_lock ); 
    
    /* print informations on streams */
    msg_Dbg( p_input, "AVIH: %d stream, flags %s%s%s%s ", 
             p_avi->i_streams,
             p_avih->i_flags&AVIF_HASINDEX?" HAS_INDEX":"",
             p_avih->i_flags&AVIF_MUSTUSEINDEX?" MUST_USE_INDEX":"",
             p_avih->i_flags&AVIF_ISINTERLEAVED?" IS_INTERLEAVED":"",
             p_avih->i_flags&AVIF_TRUSTCKTYPE?" TRUST_CKTYPE":"" );

    /* now read info on each stream and create ES */
    p_avi->pp_info = calloc( p_avi->i_streams, 
                            sizeof( avi_stream_t* ) );
    memset( p_avi->pp_info, 
            0, 
            sizeof( avi_stream_t* ) * p_avi->i_streams );

    for( i = 0 ; i < p_avi->i_streams; i++ )
    {
        avi_chunk_list_t    *p_avi_strl;
        avi_chunk_strh_t    *p_avi_strh;
        avi_chunk_strf_auds_t    *p_avi_strf_auds;
        avi_chunk_strf_vids_t    *p_avi_strf_vids;
        int     i_init_size;
        void    *p_init_data;
#define p_info  p_avi->pp_info[i]
        p_info = malloc( sizeof(avi_stream_t ) );
        memset( p_info, 0, sizeof( avi_stream_t ) );        
    
        p_avi_strl = (avi_chunk_list_t*)AVI_ChunkFind( p_hdrl, 
                                                       AVIFOURCC_strl, i );
        p_avi_strh = (avi_chunk_strh_t*)AVI_ChunkFind( p_avi_strl, 
                                                       AVIFOURCC_strh, 0 );
        p_avi_strf_auds = (avi_chunk_strf_auds_t*)
            p_avi_strf_vids = (avi_chunk_strf_vids_t*)
                AVI_ChunkFind( p_avi_strl, AVIFOURCC_strf, 0 );

        if( !p_avi_strl || !p_avi_strh || 
                ( !p_avi_strf_auds && !p_avi_strf_vids ) )
        {
            msg_Warn( p_input, "stream[%d] incomlete", i );
            continue;
        }
        
        /* *** Init p_info *** */
        p_info->i_rate  = p_avi_strh->i_rate;
        p_info->i_scale = p_avi_strh->i_scale;
        p_info->i_samplesize = p_avi_strh->i_samplesize;

        switch( p_avi_strh->i_type )
        {
            case( AVIFOURCC_auds ):
                p_info->i_cat = AUDIO_ES;
                p_info->i_fourcc = 
                    AVI_FourccGetCodec( AUDIO_ES, 
                                        p_avi_strf_auds->p_wf->wFormatTag );
                p_info->i_codec  = p_info->i_fourcc;
                i_init_size = p_avi_strf_auds->i_chunk_size;
                p_init_data = p_avi_strf_auds->p_wf;
                msg_Dbg( p_input, "stream[%d] audio(0x%x) %d channels %dHz %dbits",
                        i,
                        p_avi_strf_auds->p_wf->wFormatTag,
                        p_avi_strf_auds->p_wf->nChannels,
                        p_avi_strf_auds->p_wf->nSamplesPerSec,
                        p_avi_strf_auds->p_wf->wBitsPerSample );
                break;
                
            case( AVIFOURCC_vids ):
                p_info->i_cat = VIDEO_ES;
                /* XXX quick hack for playing ffmpeg video, I don't know 
                    who is doing something wrong */
                p_info->i_samplesize = 0;
                p_info->i_fourcc = p_avi_strf_vids->p_bih->biCompression;
                p_info->i_codec = 
                    AVI_FourccGetCodec( VIDEO_ES, p_info->i_fourcc );
                i_init_size = p_avi_strf_vids->i_chunk_size;
                p_init_data = p_avi_strf_vids->p_bih;
                msg_Dbg( p_input, "stream[%d] video(%4.4s) %dx%d %dbpp %ffps",
                        i,
                         (char*)&p_avi_strf_vids->p_bih->biCompression,
                         p_avi_strf_vids->p_bih->biWidth,
                         p_avi_strf_vids->p_bih->biHeight,
                         p_avi_strf_vids->p_bih->biBitCount,
                         (float)p_info->i_rate /
                             (float)p_info->i_scale );
#ifdef __AVI_SUBTITLE__
                if( i_microsecperframe == 0 )
                {
                    i_microsecperframe = (mtime_t)1000000 *
                                         (mtime_t)p_info->i_scale /
                                         (mtime_t)p_info->i_rate;
                }
#endif
                break;
            default:
                msg_Err( p_input, "stream[%d] unknown type", i );
                p_info->i_cat = UNKNOWN_ES;
                i_init_size = 0;
                p_init_data = NULL;
                break;
        }
        p_info->b_activated = VLC_FALSE;
        /* add one ES */
        vlc_mutex_lock( &p_input->stream.stream_lock );
        p_info->p_es =
            p_es = input_AddES( p_input,
                                p_input->stream.p_selected_program, 1+i,
                                i_init_size );
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        p_es->i_stream_id =i; /* XXX: i don't use it */ 
        p_es->i_fourcc = p_info->i_fourcc;
        p_es->i_cat = p_info->i_cat;

        /* We copy strf for decoder in p_es->p_demux_data */
        if( p_init_data )
        {
            memcpy( p_es->p_demux_data, 
                    p_init_data,
                    i_init_size );
        }
#undef p_info           
    }

#ifdef __AVI_SUBTITLE__
    if( ( p_avi->p_sub = subtitle_New( p_input, NULL, i_microsecperframe ) ) )
    {
        subtitle_Select( p_avi->p_sub );
    }
#endif

    if( config_GetInt( p_input, "avi-index" ) )
    {
        if( p_avi->b_seekable )
        {
            AVI_IndexCreate( p_input );
        }
        else
        {
            msg_Warn( p_input, "cannot create index (unseekable stream)" );
            AVI_IndexLoad( p_input );
        }
    }
    else
    {
        AVI_IndexLoad( p_input );
    }
    
    /* *** movie length in sec *** */
#if 0
    p_avi->i_length = (mtime_t)p_avih->i_totalframes * 
                      (mtime_t)p_avih->i_microsecperframe / 
                      (mtime_t)1000000;
#endif

    p_avi->i_length = AVI_MovieGetLength( p_input, p_avi );
    if( p_avi->i_length < (mtime_t)p_avih->i_totalframes *
                          (mtime_t)p_avih->i_microsecperframe /
                          (mtime_t)1000000 )
    {
        msg_Warn( p_input, "broken or missing index, 'seek' will be axproximative or will have strange behavour" );
    }

    vlc_mutex_lock( &p_input->stream.stream_lock ); 
    if( p_avi->i_length )
    {
        p_input->stream.i_mux_rate = 
            p_input->stream.p_selected_area->i_size / 50 / p_avi->i_length;
    }
    else
    {
        p_input->stream.i_mux_rate = 0;
    }
    vlc_mutex_unlock( &p_input->stream.stream_lock ); 

    b_stream_audio = VLC_FALSE;
    b_stream_video = VLC_FALSE;
    
    for( i = 0; i < p_avi->i_streams; i++ )
    {
#define p_info  p_avi->pp_info[i]
        switch( p_info->p_es->i_cat )
        {
            case( VIDEO_ES ):

                if( !b_stream_video ) 
                {
                    b_stream_video = AVI_StreamStart( p_input, p_avi, i );
                }
                break;

            case( AUDIO_ES ):
                if( !b_stream_audio ) 
                {
                    b_stream_audio = AVI_StreamStart( p_input, p_avi, i );
                }
                break;
            default:
                break;
        }
#undef p_info    
    }

    if( !b_stream_video ) 
    {
        msg_Warn( p_input, "no video stream found" );
    }
    if( !b_stream_audio )
    {
        msg_Warn( p_input, "no audio stream found!" );
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.p_selected_program->b_is_ok = 1;
    vlc_mutex_unlock( &p_input->stream.stream_lock );
    
    if( p_avi->b_seekable )
    {
        AVI_ChunkGoto( p_input, p_movi );
    }
    else
    {
        // already at begining of p_movi
    }
    AVI_SkipBytes( p_input, 12 ); // enter in p_movi

    p_avi->i_movi_begin = p_movi->i_chunk_pos;
    return VLC_SUCCESS;
}




/*****************************************************************************
 * Function to convert pts to chunk or byte
 *****************************************************************************/

static inline mtime_t AVI_PTSToChunk( avi_stream_t *p_info, 
                                        mtime_t i_pts )
{
    return (mtime_t)((int64_t)i_pts *
                     (int64_t)p_info->i_rate /
                     (int64_t)p_info->i_scale /
                     (int64_t)1000000 );
}
static inline mtime_t AVI_PTSToByte( avi_stream_t *p_info,
                                       mtime_t i_pts )
{
    return (mtime_t)((int64_t)i_pts * 
                     (int64_t)p_info->i_samplesize *
                     (int64_t)p_info->i_rate /
                     (int64_t)p_info->i_scale /
                     (int64_t)1000000 );

}

static mtime_t AVI_GetDPTS( avi_stream_t *p_stream, int i_count )
{
    if( p_stream->i_samplesize )
    {
        return (mtime_t)( (int64_t)1000000 *
                   (int64_t)i_count *
                   (int64_t)p_stream->i_scale /
                   (int64_t)p_stream->i_rate /
                   (int64_t)p_stream->i_samplesize );
    }
    else
    {
        return (mtime_t)( (int64_t)1000000 *
                   (int64_t)i_count *
                   (int64_t)p_stream->i_scale /
                   (int64_t)p_stream->i_rate);
    }

}

static mtime_t AVI_GetPTS( avi_stream_t *p_info )
{
    
    if( p_info->i_samplesize )
    {
        /* we need a valid entry we will emulate one */
        int i_len;
        if( p_info->i_idxposc == p_info->i_idxnb )
        {
            if( p_info->i_idxposc )
            {
                /* use the last entry */
                i_len = p_info->p_index[p_info->i_idxnb - 1].i_lengthtotal
                            + p_info->p_index[p_info->i_idxnb - 1].i_length
                            + p_info->i_idxposb; /* should be 0 */
            }
            else
            {
                i_len = p_info->i_idxposb; 
                /* no valid entry use only offset*/
            }
        }
        else
        {
            i_len = p_info->p_index[p_info->i_idxposc].i_lengthtotal
                                + p_info->i_idxposb;
        }
        return (mtime_t)( (int64_t)1000000 *
                  (int64_t)i_len *
                   (int64_t)p_info->i_scale /
                   (int64_t)p_info->i_rate /
                   (int64_t)p_info->i_samplesize );
    }
    else
    {
        /* even if p_info->i_idxposc isn't valid, there isn't any problem */
        return (mtime_t)( (int64_t)1000000 *
                   (int64_t)(p_info->i_idxposc ) *
                   (int64_t)p_info->i_scale /
                   (int64_t)p_info->i_rate);
    }
}

static int AVI_StreamChunkFind( input_thread_t *p_input,
                                int i_stream )
{
    demux_sys_t *p_avi = p_input->p_demux_data;
    avi_packet_t avi_pk;

    /* find first chunk of i_stream that isn't in index */

    if( p_avi->i_movi_lastchunk_pos >= p_avi->i_movi_begin )
    {
        AVI_SeekAbsolute( p_input, p_avi->i_movi_lastchunk_pos );
        if( AVI_PacketNext( p_input ) )
        {
            return VLC_EGENERIC;
        }
    }
    else
    {
        AVI_SeekAbsolute( p_input, p_avi->i_movi_begin );
    }

    for( ;; )
    {

        if( AVI_PacketGetHeader( p_input, &avi_pk ) )
        {
            msg_Err( p_input, "cannot get packet header" );
            return VLC_EGENERIC;
        }
        if( avi_pk.i_stream >= p_avi->i_streams ||
            ( avi_pk.i_cat != AUDIO_ES && avi_pk.i_cat != VIDEO_ES ) )
        {
            switch( avi_pk.i_fourcc )
            {
                case AVIFOURCC_LIST:
                    AVI_SkipBytes( p_input, 12 );
                    break;
                default:
                    if( AVI_PacketNext( p_input ) )
                    {
                        return VLC_EGENERIC;
                    }
                    break;
            }
        }
        else
        {
            /* add this chunk to the index */
            AVIIndexEntry_t index;
            
            index.i_id = avi_pk.i_fourcc;
            index.i_flags = 
               AVI_GetKeyFlag(p_avi->pp_info[avi_pk.i_stream]->i_codec,
                              avi_pk.i_peek);
            index.i_pos = avi_pk.i_pos;
            index.i_length = avi_pk.i_size;
            AVI_IndexAddEntry( p_avi, avi_pk.i_stream, &index );

            if( avi_pk.i_stream == i_stream  )
            {
                return VLC_SUCCESS;
            }
            
            if( AVI_PacketNext( p_input ) )
            {
                return VLC_EGENERIC;
            }
        }
    }
}


/* be sure that i_ck will be a valid index entry */
static int AVI_SetStreamChunk( input_thread_t    *p_input,
                               int i_stream,
                               int i_ck )
{
    demux_sys_t *p_avi = p_input->p_demux_data;
    avi_stream_t *p_stream = p_avi->pp_info[i_stream];
    
    p_stream->i_idxposc = i_ck;
    p_stream->i_idxposb = 0;

    if(  i_ck >= p_stream->i_idxnb )
    {
        p_stream->i_idxposc = p_stream->i_idxnb - 1;
        do
        {
            p_stream->i_idxposc++;
            if( AVI_StreamChunkFind( p_input, i_stream ) )
            {
                return VLC_EGENERIC;
            }

        } while( p_stream->i_idxposc < i_ck );
    }

    return VLC_SUCCESS;
}


/* XXX FIXME up to now, we assume that all chunk are one after one */
static int AVI_SetStreamBytes( input_thread_t    *p_input, 
                               int i_stream,
                               off_t   i_byte )
{
    demux_sys_t *p_avi = p_input->p_demux_data;
    avi_stream_t *p_stream = p_avi->pp_info[i_stream];

    if( ( p_stream->i_idxnb > 0 )
        &&( i_byte < p_stream->p_index[p_stream->i_idxnb - 1].i_lengthtotal + 
                p_stream->p_index[p_stream->i_idxnb - 1].i_length ) )
    {
        /* index is valid to find the ck */
        /* uses dichototmie to be fast enougth */
        int i_idxposc = __MIN( p_stream->i_idxposc, p_stream->i_idxnb - 1 );
        int i_idxmax  = p_stream->i_idxnb;
        int i_idxmin  = 0;
        for( ;; )
        {
            if( p_stream->p_index[i_idxposc].i_lengthtotal > i_byte )
            {
                i_idxmax  = i_idxposc ;
                i_idxposc = ( i_idxmin + i_idxposc ) / 2 ;
            }
            else
            {
                if( p_stream->p_index[i_idxposc].i_lengthtotal + 
                        p_stream->p_index[i_idxposc].i_length <= i_byte)
                {
                    i_idxmin  = i_idxposc ;
                    i_idxposc = (i_idxmax + i_idxposc ) / 2 ;
                }
                else
                {
                    p_stream->i_idxposc = i_idxposc;
                    p_stream->i_idxposb = i_byte - 
                            p_stream->p_index[i_idxposc].i_lengthtotal;
                    return VLC_SUCCESS;
                }
            }
        }
        
    }
    else
    {
        p_stream->i_idxposc = p_stream->i_idxnb - 1;
        p_stream->i_idxposb = 0;
        do
        {
            p_stream->i_idxposc++;
            if( AVI_StreamChunkFind( p_input, i_stream ) )
            {
                return VLC_EGENERIC;
            }

        } while( p_stream->p_index[p_stream->i_idxposc].i_lengthtotal +
                    p_stream->p_index[p_stream->i_idxposc].i_length <= i_byte );

        p_stream->i_idxposb = i_byte -
                       p_stream->p_index[p_stream->i_idxposc].i_lengthtotal;
        return VLC_SUCCESS;
    }
}

static int AVI_StreamSeek( input_thread_t *p_input,
                           demux_sys_t  *p_avi,
                           int i_stream, 
                           mtime_t i_date )
{
#define p_stream    p_avi->pp_info[i_stream]
    mtime_t i_oldpts;
    
    i_oldpts = AVI_GetPTS( p_stream );

    if( !p_stream->i_samplesize )
    {
        if( AVI_SetStreamChunk( p_input,
                                i_stream, 
                                AVI_PTSToChunk( p_stream, i_date ) ) )
        {
            return VLC_EGENERIC;
        }
                
        /* search key frame */
        msg_Dbg( p_input, 
                 "old:"I64Fd" %s new "I64Fd,
                 i_oldpts, 
                 i_oldpts > i_date ? ">" : "<",
                 i_date );

        if( i_date < i_oldpts )
        {
            while( p_stream->i_idxposc > 0 && 
               !( p_stream->p_index[p_stream->i_idxposc].i_flags & 
                                                            AVIIF_KEYFRAME ) )
            {
                if( AVI_SetStreamChunk( p_input,
                                        i_stream,
                                        p_stream->i_idxposc - 1 ) )
                {
                    return VLC_EGENERIC;
                }
            }
        }
        else
        {
            while( p_stream->i_idxposc < p_stream->i_idxnb &&
                    !( p_stream->p_index[p_stream->i_idxposc].i_flags &
                                                            AVIIF_KEYFRAME ) )
            {
                if( AVI_SetStreamChunk( p_input, 
                                        i_stream, 
                                        p_stream->i_idxposc + 1 ) )
                {
                    return VLC_EGENERIC;
                }
            }
        }
    }
    else
    {
        if( AVI_SetStreamBytes( p_input,
                                i_stream,
                                AVI_PTSToByte( p_stream, i_date ) ) )
        {
            return VLC_EGENERIC;
        }
    }
    return VLC_SUCCESS;
#undef p_stream
}

/*****************************************************************************
 * AVISeek: goto to i_date or i_percent
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int    AVISeek   ( input_thread_t *p_input, 
                          mtime_t i_date, int i_percent )
{

    demux_sys_t *p_avi = p_input->p_demux_data;
    int         i_stream;
    msg_Dbg( p_input, 
             "seek requested: "I64Fd" secondes %d%%", 
             i_date / 1000000,
             i_percent );

    if( p_avi->b_seekable )
    {
        if( !p_avi->i_length )
        {
            avi_stream_t *p_stream;
            uint64_t i_pos;

            /* use i_percent to create a true i_date */
            msg_Warn( p_input, 
                      "mmh, seeking without index at %d%%"
                      " work only for interleaved file", i_percent );

            if( i_percent >= 100 )
            {
                msg_Err( p_input, "cannot seek so far !" );
                return( -1 );
            }
            i_percent = __MAX( i_percent, 0 );
            
            /* try to find chunk that is at i_percent or the file */
            i_pos = __MAX( i_percent * 
                           p_input->stream.p_selected_area->i_size / 100,
                           p_avi->i_movi_begin );
            /* search first selected stream */
            for( i_stream = 0, p_stream = NULL; 
                        i_stream < p_avi->i_streams; i_stream++ )
            {
                p_stream = p_avi->pp_info[i_stream];
                if( p_stream->b_activated )
                {
                    break;
                }
            }
            if( !p_stream || !p_stream->b_activated )
            {
                msg_Err( p_input, "cannot find any selected stream" );
                return( -1 );
            }
            
            /* be sure that the index exit */
            if( AVI_SetStreamChunk( p_input, 
                                    i_stream,
                                    0 ) )
            {
                msg_Err( p_input, "cannot seek" );
                return( -1 );
            }
           
            while( i_pos >= p_stream->p_index[p_stream->i_idxposc].i_pos +
               p_stream->p_index[p_stream->i_idxposc].i_length + 8 )
            {
                /* search after i_idxposc */
                if( AVI_SetStreamChunk( p_input, 
                                        i_stream, p_stream->i_idxposc + 1 ) )
                {
                    msg_Err( p_input, "cannot seek" );
                    return( -1 );
                }
            }
            i_date = AVI_GetPTS( p_stream );
            /* TODO better support for i_samplesize != 0 */
            msg_Dbg( p_input, "estimate date "I64Fd, i_date );
        }

#define p_stream    p_avi->pp_info[i_stream]
        p_avi->i_time = 0;
        /* seek for chunk based streams */
        for( i_stream = 0; i_stream < p_avi->i_streams; i_stream++ )
        {
            if( p_stream->b_activated && !p_stream->i_samplesize )
//            if( p_stream->b_activated )
            {
                AVI_StreamSeek( p_input, p_avi, i_stream, i_date );
                p_avi->i_time = __MAX( AVI_GetPTS( p_stream ), 
                                        p_avi->i_time );
            }
        }
#if 1
        if( p_avi->i_time )
        {
            i_date = p_avi->i_time;
        }
        /* seek for bytes based streams */
        for( i_stream = 0; i_stream < p_avi->i_streams; i_stream++ )
        {
            if( p_stream->b_activated && p_stream->i_samplesize )
            {
                AVI_StreamSeek( p_input, p_avi, i_stream, i_date );
//                p_avi->i_time = __MAX( AVI_GetPTS( p_stream ), p_avi->i_time );
            }
        }
        msg_Dbg( p_input, "seek: "I64Fd" secondes", p_avi->i_time /1000000 );
        /* set true movie time */
#endif
        if( !p_avi->i_time )
        {
            p_avi->i_time = i_date;
        }
#undef p_stream
        return( 1 );
    }
    else
    {
        msg_Err( p_input, "shouldn't yet be executed" );
        return( -1 );
    }
}

/*****************************************************************************
 * AVIDemux_Seekable: reads and demuxes data packets for stream seekable
 *****************************************************************************
 * AVIDemux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
typedef struct avi_stream_toread_s
{ 
    vlc_bool_t b_ok;

    int i_toread;
    
    off_t i_posf; // where we will read : 
                  // if i_idxposb == 0 : begining of chunk (+8 to acces data)
                  // else : point on data directly
} avi_stream_toread_t;

static int AVIDemux_Seekable( input_thread_t *p_input )
{
    int i_stream;
    vlc_bool_t b_stream;

    // cannot be more than 100 stream (dcXX or wbXX)
    avi_stream_toread_t toread[100]; 

    demux_sys_t *p_avi = p_input->p_demux_data;

    /* detect new selected/unselected streams */
    for( i_stream = 0; i_stream < p_avi->i_streams; i_stream++ )
    {
#define p_stream    p_avi->pp_info[i_stream]
        if( p_stream->p_es )
        {
            if( p_stream->p_es->p_decoder_fifo &&
                !p_stream->b_activated )
            {
                AVI_StreamStart( p_input, p_avi, i_stream );
            }
            else
            if( !p_stream->p_es->p_decoder_fifo &&
                p_stream->b_activated )
            {
                AVI_StreamStop( p_input, p_avi, i_stream );
            }
        }       
#undef  p_stream
    }

    if( p_input->stream.p_selected_program->i_synchro_state == SYNCHRO_REINIT )
    {
        mtime_t i_date;
        int i_percent;
        /* first wait for empty buffer, arbitrary time FIXME */
//        msleep( DEFAULT_PTS_DELAY );

        i_date = (mtime_t)1000000 *
                 (mtime_t)p_avi->i_length *
                 (mtime_t)AVI_TellAbsolute( p_input ) /
                 (mtime_t)p_input->stream.p_selected_area->i_size;
        i_percent = 100 * AVI_TellAbsolute( p_input ) / 
                        p_input->stream.p_selected_area->i_size;

//        input_ClockInit( p_input->stream.p_selected_program );
        AVISeek( p_input, i_date, i_percent);

#ifdef __AVI_SUBTITLE__
        if( p_avi->p_sub )
        {
            subtitle_Seek( p_avi->p_sub, p_avi->i_time );
        }
#endif
    }

    
    /* wait for the good time */

    p_avi->i_pcr = p_avi->i_time * 9 / 100;

    input_ClockManageRef( p_input,
                          p_input->stream.p_selected_program,
                          p_avi->i_pcr ); 


    p_avi->i_time += 100*1000;  /* read 100ms */

#ifdef __AVI_SUBTITLE__
    if( p_avi->p_sub )
    {
        subtitle_Demux( p_avi->p_sub, p_avi->i_time );
    }
#endif
    /* init toread */
    for( i_stream = 0; i_stream < p_avi->i_streams; i_stream++ )
    {
#define p_stream    p_avi->pp_info[i_stream]
        mtime_t i_dpts;

        toread[i_stream].b_ok = p_stream->b_activated;

        if( p_stream->i_idxposc < p_stream->i_idxnb )
        {
            toread[i_stream].i_posf = 
                p_stream->p_index[p_stream->i_idxposc].i_pos;
           if( p_stream->i_idxposb > 0 )
           {
                toread[i_stream].i_posf += 8 + p_stream->i_idxposb;
           }
        }
        else
        { 
            toread[i_stream].i_posf = -1;
        }

        i_dpts = p_avi->i_time - AVI_GetPTS( p_stream  );

        if( p_stream->i_samplesize )
        {
            toread[i_stream].i_toread = AVI_PTSToByte( p_stream, 
                                                       __ABS( i_dpts ) );
        }
        else
        {
            toread[i_stream].i_toread = AVI_PTSToChunk( p_stream,
                                                        __ABS( i_dpts ) );
        }
        
        if( i_dpts < 0 )
        {
            toread[i_stream].i_toread *= -1;
        }
#undef  p_stream
    }
    
    b_stream = VLC_FALSE;
    
    for( ;; )
    {
#define p_stream    p_avi->pp_info[i_stream]
        vlc_bool_t       b_done;
        pes_packet_t    *p_pes;
        off_t i_pos;
        int i;
        int i_size;
        
        /* search for first chunk to be read */
        for( i = 0, b_done = VLC_TRUE, i_pos = -1; i < p_avi->i_streams; i++ )
        {
            if( !toread[i].b_ok ||
                AVI_GetDPTS( p_avi->pp_info[i],
                             toread[i].i_toread ) <= -25 * 1000 )
            {
                continue;
            }

            if( toread[i].i_toread > 0 )
            {
                b_done = VLC_FALSE; // not yet finished
            }

            if( toread[i].i_posf > 0 )
            {
                if( i_pos == -1 || i_pos > toread[i_stream].i_posf )
                {
                    i_stream = i;
                    i_pos = toread[i].i_posf;
                }
            }
        }

        if( b_done )
        {
//            return( b_stream ? 1 : 0 );
            return( 1 );
        }
        
        if( i_pos == -1 )
        {
            /* no valid index, we will parse directly the stream */
            if( p_avi->i_movi_lastchunk_pos >= p_avi->i_movi_begin )
            {
                AVI_SeekAbsolute( p_input, p_avi->i_movi_lastchunk_pos );
                if( AVI_PacketNext( p_input ) )
                {
                    return( 0 );
                }
            }
            else
            {
                AVI_SeekAbsolute( p_input, p_avi->i_movi_begin );
            }

            for( ;; )
            {
                avi_packet_t avi_pk;

                if( AVI_PacketGetHeader( p_input, &avi_pk ) )
                {
                    msg_Err( p_input, "cannot get packet header" );
                    return( 0 );
                }
                if( avi_pk.i_stream >= p_avi->i_streams ||
                    ( avi_pk.i_cat != AUDIO_ES && avi_pk.i_cat != VIDEO_ES ) )
                {
                    switch( avi_pk.i_fourcc )
                    {
                        case AVIFOURCC_LIST:
                            AVI_SkipBytes( p_input, 12 );
                            break;
                        default:
                            if( AVI_PacketNext( p_input ) )
                            {
                                msg_Err( p_input, "cannot skip packet" );
                                return( 0 );
                            }
                            break;
                    }
                    continue;
                }
                else
                {
                    /* add this chunk to the index */
                    AVIIndexEntry_t index;
                    
                    index.i_id = avi_pk.i_fourcc;
                    index.i_flags = 
                       AVI_GetKeyFlag(p_avi->pp_info[avi_pk.i_stream]->i_codec,
                                      avi_pk.i_peek);
                    index.i_pos = avi_pk.i_pos;
                    index.i_length = avi_pk.i_size;
                    AVI_IndexAddEntry( p_avi, avi_pk.i_stream, &index );

                    i_stream = avi_pk.i_stream;
                    /* do we will read this data ? */
                    if( AVI_GetDPTS( p_stream,
                             toread[i_stream].i_toread ) > -25 * 1000 )
                    {
                        break;
                    }
                    else
                    {
                        if( AVI_PacketNext( p_input ) )
                        {
                            msg_Err( p_input, "cannot skip packet" );
                            return( 0 );
                        }
                    }
                }
            }
            
        }
        else
        {
            AVI_SeekAbsolute( p_input, i_pos );
        }

        /* read thoses data */
        if( p_stream->i_samplesize )
        {
            i_size = __MIN( p_stream->p_index[p_stream->i_idxposc].i_length - 
                                p_stream->i_idxposb,
                                100 * 1024 ); // 10Ko max
//                            toread[i_stream].i_toread );
        }
        else
        {
            i_size = p_stream->p_index[p_stream->i_idxposc].i_length;
        }

        if( p_stream->i_idxposb == 0 )
        {
            i_size += 8; // need to read and skip header
        }

        if( input_ReadInPES( p_input, &p_pes, __EVEN( i_size ) ) < 0 )
        {
            msg_Err( p_input, "failled reading data" );
            toread[i_stream].b_ok = VLC_FALSE;
            continue;
        }

        if( i_size % 2 )    // read was padded on word boundary
        {
            p_pes->p_last->p_payload_end--;
            p_pes->i_pes_size--;
        }
        // skip header
        if( p_stream->i_idxposb == 0 )
        {
            p_pes->p_first->p_payload_start += 8;
            p_pes->i_pes_size -= 8;
        }

        p_pes->i_pts = AVI_GetPTS( p_stream );
       
        /* read data */
        if( p_stream->i_samplesize )
        {
            if( p_stream->i_idxposb == 0 )
            {
                i_size -= 8;
            }
            toread[i_stream].i_toread -= i_size;
            p_stream->i_idxposb += i_size;
            if( p_stream->i_idxposb >= 
                    p_stream->p_index[p_stream->i_idxposc].i_length )
            {
                p_stream->i_idxposb = 0;
                p_stream->i_idxposc++;
            }
        }
        else
        {
            toread[i_stream].i_toread--;
            p_stream->i_idxposc++;
        }

        if( p_stream->i_idxposc < p_stream->i_idxnb)             
        {
            toread[i_stream].i_posf = 
                p_stream->p_index[p_stream->i_idxposc].i_pos;
            if( p_stream->i_idxposb > 0 )
            {
                toread[i_stream].i_posf += 8 + p_stream->i_idxposb;
            }
            
        }
        else
        {
            toread[i_stream].i_posf = -1;
        }

        b_stream = VLC_TRUE; // at least one read succeed
        
        if( p_stream->p_es && p_stream->p_es->p_decoder_fifo )
        {
            p_pes->i_dts =
                p_pes->i_pts = 
                    input_ClockGetTS( p_input,
                                      p_input->stream.p_selected_program,
                                      p_pes->i_pts * 9/100);
            
            input_DecodePES( p_stream->p_es->p_decoder_fifo, p_pes );
        }
        else
        {
            input_DeletePES( p_input->p_method_data, p_pes );
        }
    }
}


/*****************************************************************************
 * AVIDemux_UnSeekable: reads and demuxes data packets for unseekable file
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int AVIDemux_UnSeekable( input_thread_t *p_input )
{
    demux_sys_t     *p_avi = p_input->p_demux_data;
    avi_stream_t *p_stream_master;
    vlc_bool_t b_audio;
    int     i_stream;
    int     i_packet;

    /* *** send audio data to decoder only if rate == DEFAULT_RATE *** */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    b_audio = p_input->stream.control.i_rate == DEFAULT_RATE;
    vlc_mutex_unlock( &p_input->stream.stream_lock );    

    input_ClockManageRef( p_input,
                          p_input->stream.p_selected_program,
                          p_avi->i_pcr );
    /* *** find master stream for data packet skipping algo *** */
    /* *** -> first video, if any, or first audio ES *** */
    for( i_stream = 0, p_stream_master = NULL; 
            i_stream < p_avi->i_streams; i_stream++ )
    {
#define p_stream    p_avi->pp_info[i_stream]
        if( p_stream->p_es &&
            p_stream->p_es->p_decoder_fifo )
        {
            if( p_stream->i_cat == VIDEO_ES )
            {
                p_stream_master = p_stream;
                break;
            }
            if( p_stream->i_cat == AUDIO_ES && !p_stream_master )
            {
                p_stream_master = p_stream;
            }
        }
#undef p_stream
    }
    if( !p_stream_master )
    {
        msg_Err( p_input, "no more stream selected" );
        return( 0 );
    }

    p_avi->i_pcr = AVI_GetPTS( p_stream_master ) * 9 / 100;
    
    for( i_packet = 0; i_packet < 10; i_packet++)
    {
#define p_stream    p_avi->pp_info[avi_pk.i_stream]

        avi_packet_t    avi_pk;

        if( AVI_PacketGetHeader( p_input, &avi_pk ) )
        {
            return( 0 );
        }
//        AVI_ParseStreamHeader( avi_pk.i_fourcc, &i_stream, &i_cat );

        if( avi_pk.i_stream >= p_avi->i_streams ||
            ( avi_pk.i_cat != AUDIO_ES && avi_pk.i_cat != VIDEO_ES ) )
        {
            /* we haven't found an audio or video packet:
             *  - we have seek, found first next packet
             *  - others packets could be found, skip them
             */
            switch( avi_pk.i_fourcc )
            {
                case AVIFOURCC_JUNK:
                case AVIFOURCC_LIST:
                    return( !AVI_PacketNext( p_input ) ? 1 : 0 );
                case AVIFOURCC_idx1:
                    return( 0 );    // eof
                default:
                    msg_Warn( p_input, 
                              "seems to have lost position, resync" );
                    if( AVI_PacketSearch( p_input ) )
                    {
                        msg_Err( p_input, "resync failed" );
                        return( -1 );
                    }
            }
        }
        else
        {  
            /* do will send this packet to decoder ? */
            if( ( !b_audio && avi_pk.i_cat == AUDIO_ES )||
                !p_stream->p_es ||
                !p_stream->p_es->p_decoder_fifo )
            {
                if( AVI_PacketNext( p_input ) )
                {
                    return( 0 );
                }
            }
            else
            {
                /* it's a selected stream, check for time */
                if( __ABS( AVI_GetPTS( p_stream ) - 
                            AVI_GetPTS( p_stream_master ) )< 600*1000 )
                {
                    /* load it and send to decoder */
                    pes_packet_t    *p_pes;
                    if( AVI_PacketRead( p_input, &avi_pk, &p_pes ) || !p_pes)
                    {
                        return( -1 );
                    }
                    p_pes->i_pts = 
                        input_ClockGetTS( p_input, 
                                          p_input->stream.p_selected_program, 
                                          AVI_GetPTS( p_stream ) * 9/100);
                    input_DecodePES( p_stream->p_es->p_decoder_fifo, p_pes );
                }
                else
                {
                    if( AVI_PacketNext( p_input ) )
                    {
                        return( 0 );
                    }
                }
            }

            /* *** update stream time position *** */
            if( p_stream->i_samplesize )
            {
                p_stream->i_idxposb += avi_pk.i_size;
            }
            else
            {
                p_stream->i_idxposc++;
            }

        }

#undef p_stream     
    }

    return( 1 );
}

