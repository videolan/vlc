/*****************************************************************************
 * avi.c : AVI file Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: avi.c,v 1.6 2002/10/15 00:55:07 fenrir Exp $
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

#include "libioRIFF.h"
#include "libavi.h"
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
    set_capability( "demux", 160 );
    set_callbacks( AVIInit, __AVIEnd );
vlc_module_end();

/*****************************************************************************
 * Some usefull functions to manipulate memory 
 *****************************************************************************/
static int __AVI_GetDataInPES( input_thread_t *, pes_packet_t **, int, int );

static u16 GetWLE( byte_t *p_buff )
{
    return( p_buff[0] + ( p_buff[1] << 8 ) );
}
static u32 GetDWLE( byte_t *p_buff )
{
    return( p_buff[0] + ( p_buff[1] << 8 ) + 
            ( p_buff[2] << 16 ) + ( p_buff[3] << 24 ) );
}
static u32 GetDWBE( byte_t *p_buff )
{
    return( p_buff[3] + ( p_buff[2] << 8 ) + 
            ( p_buff[1] << 16 ) + ( p_buff[0] << 24 ) );
}
static vlc_fourcc_t GetFOURCC( byte_t *p_buff )
{
    return( VLC_FOURCC( p_buff[0], p_buff[1], p_buff[2], p_buff[3] ) );
}

static inline off_t __EVEN( off_t i )
{
    return( (i & 1) ? i+1 : i );
}

#define __ABS( x ) ( (x) < 0 ? (-(x)) : (x) )

/* Test if it seems that it's a key frame */
static int AVI_GetKeyFlag( vlc_fourcc_t i_fourcc, u8 *p_byte )
{
    switch( i_fourcc )
    {
        case FOURCC_DIV1:
            /* we have:
                startcode:      0x00000100   32bits
                framenumber     ?             5bits
                piture type     0(I),1(P)     2bits
             */
            if( GetDWBE( p_byte ) != 0x00000100 ) 
            {
            /* it's not an msmpegv1 stream, strange...*/
                return( AVIIF_KEYFRAME );
            }
            else
            {
                return( p_byte[4]&0x06 ? 0 : AVIIF_KEYFRAME);
            }
        case FOURCC_DIV2:
        case FOURCC_DIV3:   // wmv1 also
            /* we have
                picture type    0(I),1(P)     2bits
             */
            return( p_byte[0]&0xC0 ? 0 : AVIIF_KEYFRAME );
        case FOURCC_mp4v:
            /* we should find first occurence of 0x000001b6 (32bits)
                startcode:      0x000001b6   32bits
                piture type     0(I),1(P)     2bits
            */
            if( GetDWBE( p_byte ) != 0x000001b6 )
            {
                /* not true , need to find the first VOP header */
                return( AVIIF_KEYFRAME );
            }
            else
            {
                return( p_byte[4]&0xC0 ? 0 : AVIIF_KEYFRAME );
            }
        default:
            /* I can't do it, so said yes */
            return( AVIIF_KEYFRAME );
    }
}

vlc_fourcc_t AVI_FourccGetCodec( int i_cat, vlc_fourcc_t i_codec )
{
    switch( i_cat )
    {
        case( AUDIO_ES ):
            switch( i_codec )
            {
                case( WAVE_FORMAT_PCM ):
                    return( VLC_FOURCC( 'a', 'r', 'a', 'w' ) );
                case( WAVE_FORMAT_MPEG ):
                case( WAVE_FORMAT_MPEGLAYER3 ):
                    return( VLC_FOURCC( 'm', 'p', 'g', 'a' ) );
                case( WAVE_FORMAT_A52 ):
                    return( VLC_FOURCC( 'a', '5', '2', ' ' ) );
                case( WAVE_FORMAT_WMA1 ):
                    return( VLC_FOURCC( 'w', 'm', 'a', '1' ) );
                case( WAVE_FORMAT_WMA2 ):
                    return( VLC_FOURCC( 'w', 'm', 'a', '2' ) );
                default:
                    return( VLC_FOURCC( 'm', 's', 
                                        ( i_codec >> 8 )&0xff, 
                                        i_codec&0xff ) );
            }
        case( VIDEO_ES ):
            // XXX DIV1 <- msmpeg4v1, DIV2 <- msmpeg4v2, DIV3 <- msmpeg4v3, mp4v for mpeg4 
            switch( i_codec )
            {
                case FOURCC_DIV1:
                case FOURCC_div1:
                case FOURCC_MPG4:
                case FOURCC_mpg4:
                    return( FOURCC_DIV1 );
                case FOURCC_DIV2:
                case FOURCC_div2:
                case FOURCC_MP42:
                case FOURCC_mp42:
                case FOURCC_MPG3:
                case FOURCC_mpg3:
                    return( FOURCC_DIV2 );
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
                    return( FOURCC_DIV3 );
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
                    return( FOURCC_mp4v );
            }
        default:
            return( VLC_FOURCC( 'u', 'n', 'd', 'f' ) );
    }
}
/*****************************************************************************
 * Data and functions to manipulate pes buffer
 *****************************************************************************/
#define BUFFER_MAXTOTALSIZE     500*1024 /* 1/2 Mo */
#define BUFFER_MAXSPESSIZE      200*1024
static int  AVI_PESBuffer_IsFull( AVIStreamInfo_t *p_info )
{
    return( p_info->i_pes_totalsize > BUFFER_MAXTOTALSIZE ? 1 : 0);
}
static void AVI_PESBuffer_Add( input_buffers_t *p_method_data,
                               AVIStreamInfo_t *p_info,
                               pes_packet_t *p_pes,
                               int i_posc,
                               int i_posb )
{
    AVIESBuffer_t   *p_buffer_pes;

    if( p_info->i_pes_totalsize > BUFFER_MAXTOTALSIZE )
    {
        input_DeletePES( p_method_data, p_pes );
        return;
    }
    
    if( !( p_buffer_pes = malloc( sizeof( AVIESBuffer_t ) ) ) )
    {
        input_DeletePES( p_method_data, p_pes );
        return;
    }
    p_buffer_pes->p_next = NULL;
    p_buffer_pes->p_pes = p_pes;
    p_buffer_pes->i_posc = i_posc;
    p_buffer_pes->i_posb = i_posb;

    if( p_info->p_pes_last ) 
    {
        p_info->p_pes_last->p_next = p_buffer_pes;
    }
    p_info->p_pes_last = p_buffer_pes;
    if( !p_info->p_pes_first )
    {
        p_info->p_pes_first = p_buffer_pes;
    }
    p_info->i_pes_count++;
    p_info->i_pes_totalsize += p_pes->i_pes_size;
}
static pes_packet_t *AVI_PESBuffer_Get( AVIStreamInfo_t *p_info )
{
    AVIESBuffer_t   *p_buffer_pes;
    pes_packet_t    *p_pes;
    if( p_info->p_pes_first )
    {
        p_buffer_pes = p_info->p_pes_first;
        p_info->p_pes_first = p_buffer_pes->p_next;
        if( !p_info->p_pes_first )
        {
            p_info->p_pes_last = NULL;
        }
        p_pes = p_buffer_pes->p_pes;

        free( p_buffer_pes );
        p_info->i_pes_count--;
        p_info->i_pes_totalsize -= p_pes->i_pes_size;
        return( p_pes );
    }
    else
    {
        return( NULL );
    }
}
static int AVI_PESBuffer_Drop( input_buffers_t *p_method_data,
                                AVIStreamInfo_t *p_info )
{
    pes_packet_t *p_pes = AVI_PESBuffer_Get( p_info );
    if( p_pes )
    {
        input_DeletePES( p_method_data, p_pes );
        return( 1 );
    }
    else
    {
        return( 0 );
    }
}
static void AVI_PESBuffer_Flush( input_buffers_t *p_method_data,
                                 AVIStreamInfo_t *p_info )
{
    while( p_info->p_pes_first )
    {
        AVI_PESBuffer_Drop( p_method_data, p_info );
    }
}

static void AVI_ParseStreamHeader( u32 i_id, int *pi_number, int *pi_type )
{
#define SET_PTR( p, v ) if( p ) *(p) = (v);
    int c1,c2;
/* XXX i_id have to be read using MKFOURCC and NOT VLC_FOURCC */
    c1 = ( i_id ) & 0xFF;
    c2 = ( i_id >>  8 ) & 0xFF;

    if( c1 < '0' || c1 > '9' || c2 < '0' || c2 > '9' )
    {
        SET_PTR( pi_number, 100); /* > max stream number */
        SET_PTR( pi_type, UNKNOWN_ES);
    }
    else
    {
        SET_PTR( pi_number, (c1 - '0') * 10 + (c2 - '0' ) );
        switch( ( i_id >> 16 ) & 0xFFFF )
        {
            case( AVITWOCC_wb ):
                SET_PTR( pi_type, AUDIO_ES );
                break;
             case( AVITWOCC_dc ):
             case( AVITWOCC_db ):
                SET_PTR( pi_type, VIDEO_ES);
                break;
             default:
                SET_PTR( pi_type, UNKNOWN_ES );
                break;
        }
    }
#undef SET_PTR
}

typedef struct avi_packet_s
{
    u32     i_fourcc;
    off_t   i_pos;
    u32     i_size;
    u32     i_type;     // only for AVIFOURCC_LIST
    u8      i_peek[8];  //first 8 bytes

    int     i_stream;
    int     i_cat;
} avi_packet_t;

static int AVI_PacketGetHeader( input_thread_t *p_input, avi_packet_t *p_pk )
{
    u8  *p_peek;
    
    if( input_Peek( p_input, &p_peek, 16 ) < 16 )
    {
        return( 0 );
    }
    p_pk->i_fourcc  = GetDWLE( p_peek );
    p_pk->i_size    = GetDWLE( p_peek + 4 );
    p_pk->i_pos     = AVI_TellAbsolute( p_input );
    if( p_pk->i_fourcc == AVIFOURCC_LIST )
    {
        p_pk->i_type = GetDWLE( p_peek + 8 );
    }
    else
    {
        p_pk->i_type = 0;
    }
    
    memcpy( p_pk->i_peek, p_peek + 8, 8 );

    AVI_ParseStreamHeader( p_pk->i_fourcc, &p_pk->i_stream, &p_pk->i_cat );
    return( 1 );
}

static int AVI_PacketNext( input_thread_t *p_input )
{
    avi_packet_t    avi_ck;

    if( !AVI_PacketGetHeader( p_input, &avi_ck ) )
    {
        return( 0 );
    }
    if( avi_ck.i_fourcc == AVIFOURCC_LIST && avi_ck.i_type == AVIFOURCC_rec )
    {
        return( AVI_SkipBytes( p_input, 12 ) );
    }
    else
    {
        return( AVI_SkipBytes( p_input, __EVEN( avi_ck.i_size ) + 8 ));
    }
}
static int AVI_PacketRead( input_thread_t   *p_input,
                           avi_packet_t     *p_pk,
                           pes_packet_t     **pp_pes )
{

    if( __AVI_GetDataInPES( p_input, pp_pes, p_pk->i_size + 8, 1) 
            != p_pk->i_size + 8)
    {
        return( 0 );
    }
    (*pp_pes)->p_first->p_payload_start += 8;
    (*pp_pes)->i_pes_size -= 8;
    return( 1 );
}

static int AVI_PacketSearch( input_thread_t *p_input )
{
    demux_sys_t     *p_avi = p_input->p_demux_data;

    avi_packet_t    avi_pk;
    for( ;; )
    {
        if( !AVI_SkipBytes( p_input, 1 ) )
        {
            return( 0 );
        }
        AVI_PacketGetHeader( p_input, &avi_pk );
        if( avi_pk.i_stream < p_avi->i_streams &&
            ( avi_pk.i_cat == AUDIO_ES || avi_pk.i_cat == VIDEO_ES ) )
        {
            return( 1 );
        }
        switch( avi_pk.i_fourcc )
        {
            case AVIFOURCC_JUNK:
            case AVIFOURCC_LIST:
            case AVIFOURCC_idx1:
                return( 1 );
        }
    }
}


static void __AVI_AddEntryIndex( AVIStreamInfo_t *p_info,
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
            index.i_flags   = p_idx1->entry[i_index].i_flags&(~AVIIF_FIXKEYFRAME);
            index.i_pos     = p_idx1->entry[i_index].i_pos + i_offset;
            index.i_length  = p_idx1->entry[i_index].i_length;
            __AVI_AddEntryIndex( p_avi->pp_info[i_stream],
                                 &index );
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
        
        if( !AVI_PacketGetHeader( p_input, &pk ) )
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
            __AVI_AddEntryIndex( p_avi->pp_info[pk.i_stream],
                                 &index );
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
                    if( !AVI_PacketSearch( p_input ) )
                    {
                        msg_Warn( p_input, "lost sync, abord index creation" );
                        goto print_stat;
                    }
            }
        }
        if( pk.i_pos + pk.i_size >= i_movi_end ||
            !AVI_PacketNext( p_input ) )
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
 * Stream managment
 *****************************************************************************/
static int  AVI_StreamStart  ( input_thread_t *, demux_sys_t *, int );
static int  AVI_StreamSeek   ( input_thread_t *, demux_sys_t *, int, mtime_t );
static void AVI_StreamStop   ( input_thread_t *, demux_sys_t *, int );

static int  AVI_StreamStart( input_thread_t *p_input,  
                             demux_sys_t *p_avi, int i_stream )
{
#define p_stream    p_avi->pp_info[i_stream]
    if( !p_stream->p_es )
    {
        msg_Warn( p_input, "stream[%d] unselectable", i_stream );
        return( 0 );
    }
    if( p_stream->i_activated )
    {
        msg_Warn( p_input, "stream[%d] already selected", i_stream );
        return( 1 );
    }
    
    if( !p_stream->p_es->p_decoder_fifo )
    {
        vlc_mutex_lock( &p_input->stream.stream_lock );
        input_SelectES( p_input, p_stream->p_es );
        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }
    p_stream->i_activated = p_stream->p_es->p_decoder_fifo ? 1 : 0;

    AVI_StreamSeek( p_input, p_avi, i_stream, p_avi->i_time );

    return( p_stream->i_activated );
#undef  p_stream
}

static void    AVI_StreamStop( input_thread_t *p_input,
                               demux_sys_t *p_avi, int i_stream )
{
#define p_stream    p_avi->pp_info[i_stream]

    if( !p_stream->i_activated )
    {
        msg_Warn( p_input, "stream[%d] already unselected", i_stream );
        return;
    }
    
//    AVI_PESBuffer_Flush( p_input->p_method_data, p_stream );

    if( p_stream->p_es->p_decoder_fifo )
    {
        vlc_mutex_lock( &p_input->stream.stream_lock );
        input_UnselectES( p_input, p_stream->p_es );
        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }

            
    p_stream->i_activated = 0;

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
                 "stream[%d] length:%lld (based on index)",
                 i_stream,
                 i_length );
        i_maxlength = __MAX( i_maxlength, i_length );
#undef p_stream                         
    }

    return( i_maxlength );
}

/*****************************************************************************
 * AVIEnd: frees unused data
 *****************************************************************************/
static void __AVIEnd ( vlc_object_t * p_this )
{   
    input_thread_t *    p_input = (input_thread_t *)p_this;
    int i;
    demux_sys_t *p_avi = p_input->p_demux_data  ; 
    
    if( p_avi->p_movi ) 
            RIFF_DeleteChunk( p_input, p_avi->p_movi );
    if( p_avi->pp_info )
    {
        for( i = 0; i < p_avi->i_streams; i++ )
        {
            if( p_avi->pp_info[i] ) 
            {
                if( p_avi->pp_info[i]->p_index )
                {
                      free( p_avi->pp_info[i]->p_index );
                      AVI_PESBuffer_Flush( p_input->p_method_data, 
                                           p_avi->pp_info[i] );
                }
                free( p_avi->pp_info[i] ); 
            }
        }
         free( p_avi->pp_info );
    }
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

    p_input->pf_demux = AVIDemux_Seekable;
    if( !AVI_TestFile( p_input ) )
    {
        msg_Warn( p_input, "avi module discarded (invalid headr)" );
        return( -1 );
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
        return( -1 );
    }
    memset( p_avi, 0, sizeof( demux_sys_t ) );
    p_avi->i_time = 0;
    p_avi->i_pcr  = 0;
    p_avi->i_rate = DEFAULT_RATE;
    p_avi->b_seekable = ( ( p_input->stream.b_seekable )
                        &&( p_input->stream.i_method == INPUT_METHOD_FILE ) );
    /* *** for unseekable stream, automaticaly use AVIDemux_interleaved *** */
    if( !p_avi->b_seekable || config_GetInt( p_input, "avi-interleaved" ) )
    {
        p_input->pf_demux = AVIDemux_UnSeekable;
    }
    
    if( !AVI_ChunkReadRoot( p_input, &p_avi->ck_root, p_avi->b_seekable ) )
    {
        msg_Err( p_input, "avi module discarded (invalid file)" );
        return( -1 );
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
        return( -1 );
    }
    
    if( !( p_avih = (avi_chunk_avih_t*)AVI_ChunkFind( p_hdrl, 
                                                      AVIFOURCC_avih, 0 ) ) )
    {
        msg_Err( p_input, "cannot find avih chunk" );
        return( -1 );
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
        return( -1 );
    }

    /*  create one program */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1)
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        AVIEnd( p_input );
        msg_Err( p_input, "cannot init stream" );
        return( -1 );
    }
    if( input_AddProgram( p_input, 0, 0) == NULL )
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        AVIEnd( p_input );
        msg_Err( p_input, "cannot add program" );
        return( -1 );
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
                            sizeof( AVIStreamInfo_t* ) );
    memset( p_avi->pp_info, 
            0, 
            sizeof( AVIStreamInfo_t* ) * p_avi->i_streams );

    for( i = 0 ; i < p_avi->i_streams; i++ )
    {
        avi_chunk_list_t    *p_avi_strl;
        avi_chunk_strh_t    *p_avi_strh;
        avi_chunk_strf_auds_t    *p_avi_strf_auds;
        avi_chunk_strf_vids_t    *p_avi_strf_vids;
        int     i_init_size;
        void    *p_init_data;
#define p_info  p_avi->pp_info[i]
        p_info = malloc( sizeof(AVIStreamInfo_t ) );
        memset( p_info, 0, sizeof( AVIStreamInfo_t ) );        
    
        p_avi_strl = (avi_chunk_list_t*)AVI_ChunkFind( p_hdrl, 
                                                       AVIFOURCC_strl, i );
        p_avi_strh = (avi_chunk_strh_t*)AVI_ChunkFind( p_avi_strl, 
                                                       AVIFOURCC_strh, 0 );
        p_avi_strf_auds = 
            p_avi_strf_vids = AVI_ChunkFind( p_avi_strl, AVIFOURCC_strf, 0 );

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
                                        p_avi_strf_auds->i_formattag );
                p_info->i_codec  = p_info->i_fourcc;
                i_init_size = p_avi_strf_auds->i_chunk_size;
                p_init_data = p_avi_strf_auds->p_wfx;
                msg_Dbg( p_input, "stream[%d] audio(0x%x) %d channels %dHz %dbits",
                        i,
                        p_avi_strf_auds->i_formattag,
                        p_avi_strf_auds->i_channels,
                        p_avi_strf_auds->i_samplespersec,
                        p_avi_strf_auds->i_bitspersample );
                break;
                
            case( AVIFOURCC_vids ):
                p_info->i_cat = VIDEO_ES;
                /* XXX quick hack for playing ffmpeg video, I don't know 
                    who is doing something wrong */
                p_info->i_samplesize = 0;
                p_info->i_fourcc = p_avi_strf_vids->i_compression;
                p_info->i_codec = 
                    AVI_FourccGetCodec( VIDEO_ES, p_info->i_fourcc );
                i_init_size = p_avi_strf_vids->i_chunk_size;
                p_init_data = p_avi_strf_vids->p_bih;
                msg_Dbg( p_input, "stream[%d] video(%4.4s) %dx%d %dbpp %ffps",
                        i,
                         (char*)&p_avi_strf_vids->i_compression,
                         p_avi_strf_vids->i_width,
                         p_avi_strf_vids->i_height,
                         p_avi_strf_vids->i_bitcount,
                         (float)p_info->i_rate /
                             (float)p_info->i_scale );
                break;
            default:
                msg_Err( p_input, "stream[%d] unknown type", i );
                p_info->i_cat = UNKNOWN_ES;
                i_init_size = 0;
                p_init_data = NULL;
                break;
        }
        p_info->i_activated = 0;
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

    /* create a pseudo p_movi */
    p_avi->p_movi = malloc( sizeof( riffchunk_t ) );
    p_avi->p_movi->i_id = AVIFOURCC_LIST;
    p_avi->p_movi->i_type = AVIFOURCC_movi;
    p_avi->p_movi->i_size = p_movi->i_chunk_size;
    p_avi->p_movi->i_pos = p_movi->i_chunk_pos;
    p_avi->p_movi->p_data = NULL;
        

    for( i = 0; i < p_avi->i_streams; i++ )
    {
#define p_info  p_avi->pp_info[i]
        switch( p_info->p_es->i_cat )
        {
            case( VIDEO_ES ):

                if( (p_avi->p_info_video == NULL) ) 
                {
                    p_avi->p_info_video = p_info;
                    /* TODO add test to see if a decoder has been found */
                    AVI_StreamStart( p_input, p_avi, i );
                }
                break;

            case( AUDIO_ES ):
                if( (p_avi->p_info_audio == NULL) ) 
                {
                    p_avi->p_info_audio = p_info;
                    AVI_StreamStart( p_input, p_avi, i );
                }
                break;
            default:
                break;
        }
#undef p_info    
    }

    /* we select the first audio and video ES */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( !p_avi->p_info_video ) 
    {
        msg_Warn( p_input, "no video stream found" );
    }
    if( !p_avi->p_info_audio )
    {
        msg_Warn( p_input, "no audio stream found!" );
    }
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
    return( 0 );
}




/*****************************************************************************
 * Function to convert pts to chunk or byte
 *****************************************************************************/

static inline mtime_t AVI_PTSToChunk( AVIStreamInfo_t *p_info, 
                                        mtime_t i_pts )
{
    return( (mtime_t)((s64)i_pts *
                      (s64)p_info->i_rate /
                      (s64)p_info->i_scale /
                      (s64)1000000 ) );
}
static inline mtime_t AVI_PTSToByte( AVIStreamInfo_t *p_info,
                                       mtime_t i_pts )
{
    return( (mtime_t)((s64)i_pts * 
                      (s64)p_info->i_samplesize *
                      (s64)p_info->i_rate /
                      (s64)p_info->i_scale /
                      (s64)1000000 ) );

}
static mtime_t AVI_GetPTS( AVIStreamInfo_t *p_info )
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
        return( (mtime_t)( (s64)1000000 *
                   (s64)i_len *
                    (s64)p_info->i_scale /
                    (s64)p_info->i_rate /
                    (s64)p_info->i_samplesize ) );
    }
    else
    {
        /* even if p_info->i_idxposc isn't valid, there isn't any problem */
        return( (mtime_t)( (s64)1000000 *
                    (s64)(p_info->i_idxposc ) *
                    (s64)p_info->i_scale /
                    (s64)p_info->i_rate) );
    }
}


/*****************************************************************************
 * Functions to acces streams data 
 * Uses it, because i plane to read unseekable stream
 * XXX NEVER set directly i_idxposc and i_idxposb unless you know what you do 
 *****************************************************************************/

/* FIXME FIXME change b_pad to number of bytes to skipp after reading */
static int __AVI_GetDataInPES( input_thread_t *p_input,
                               pes_packet_t   **pp_pes,
                               int i_size,
                               int b_pad )
{

    int i_read;
    data_packet_t *p_data;

    
    if( !(*pp_pes = input_NewPES( p_input->p_method_data ) ) )
    {
        return( 0 );
    }

    
    if( !i_size )
    {
        p_data = input_NewPacket( p_input->p_method_data, 0 );
        (*pp_pes)->p_first = (*pp_pes)->p_last  = p_data;
        (*pp_pes)->i_nb_data = 1;
        (*pp_pes)->i_pes_size = 0;
        return( 0 );
    }
    
    if( ( i_size&1 )&&( b_pad ) )
    {
        b_pad = 1;
        i_size++;
    }
    else
    {
        b_pad = 0;
    }

    do
    {
        i_read = input_SplitBuffer(p_input, &p_data, __MIN( i_size - 
                                                              (*pp_pes)->i_pes_size, 1024 ) );
        if( i_read < 0 )
        {
            return( (*pp_pes)->i_pes_size );
        }
        if( !(*pp_pes)->p_first )
        {
            (*pp_pes)->p_first = 
                    (*pp_pes)->p_last  = p_data;
            (*pp_pes)->i_nb_data = 1;
            (*pp_pes)->i_pes_size = i_read;
        }
        else
        {
            (*pp_pes)->p_last->p_next = 
                    (*pp_pes)->p_last = p_data;
            (*pp_pes)->i_nb_data++;
            (*pp_pes)->i_pes_size += i_read;
        }
    } while( ((*pp_pes)->i_pes_size < i_size)&&( i_read ) );

    if( b_pad )
    {
        (*pp_pes)->i_pes_size--;
        (*pp_pes)->p_last->p_payload_end--;
        i_size--;
    }

	return( i_size );
}

static int __AVI_SeekAndGetChunk( input_thread_t  *p_input,
                                  AVIStreamInfo_t *p_info )
{
    pes_packet_t *p_pes;
    int i_length, i_ret;
    
    i_length = __MIN( p_info->p_index[p_info->i_idxposc].i_length 
                        - p_info->i_idxposb,
                            BUFFER_MAXSPESSIZE );

    AVI_SeekAbsolute( p_input, 
                      (off_t)p_info->p_index[p_info->i_idxposc].i_pos + 
                            p_info->i_idxposb + 8);

    i_ret = __AVI_GetDataInPES( p_input, &p_pes, i_length , 0);

    if( i_ret != i_length )
    {
        return( 0 );
    }
    /*  TODO test key frame if i_idxposb == 0*/
    AVI_PESBuffer_Add( p_input->p_method_data,
                       p_info,
                       p_pes,
                       p_info->i_idxposc,
                       p_info->i_idxposb );
    return( 1 );
}
/* TODO check if it's correct (humm...) and optimisation ... */
/* return 0 if we choose to get only the ck we want 
 *        1 if index is invalid
 *        2 if there is a ck_other before ck_info and the last proced ck_info*/
/* XXX XXX XXX avi file is some BIG shit, and sometime index give 
 * a refenrence to the same chunk BUT with a different size ( usually 0 )
 */

static inline int __AVI_GetChunkMethod( input_thread_t  *p_input,
                                 AVIStreamInfo_t *p_info,
                                 AVIStreamInfo_t *p_other )
{
    int i_info_pos;
    int i_other_pos;
    
    int i_info_pos_last;
    int i_other_pos_last;

    /*If we don't have a valid entry we need to parse from last 
        defined chunk and it's the only way that we return 1*/
    if( p_info->i_idxposc >= p_info->i_idxnb )
    {
        return( 1 );
    }

    /* KNOW we have a valid entry for p_info */
    /* we return 0 if we haven't an valid entry for p_other */ 
    if( ( !p_other )||( p_other->i_idxposc >= p_other->i_idxnb ) )
    {
        return( 0 );
    }

    /* KNOW there are 2 streams with valid entry */
   
    /* we return 0 if for one of the two streams we will not read 
       chunk-aligned */
    if( ( p_info->i_idxposb )||( p_other->i_idxposb ) )
    { 
        return( 0 );
    }
    
    /* KNOW we have a valid entry for the 2 streams
         and for the 2 we want an aligned chunk (given by i_idxposc )*/
        /* if in stream, the next chunk is back than the one we 
           have just read, it's useless to parse */
    i_info_pos  = p_info->p_index[p_info->i_idxposc].i_pos;
    i_other_pos = p_other->p_index[p_other->i_idxposc].i_pos ;

    i_info_pos_last  = p_info->i_idxposc ? 
                            p_info->p_index[p_info->i_idxposc - 1].i_pos : 0;
    i_other_pos_last = p_other->i_idxposc ?
                            p_other->p_index[p_other->i_idxposc - 1].i_pos : 0 ;
   
    
    if( ( ( p_info->i_idxposc )&&( i_info_pos <= i_info_pos_last ) ) ||
        ( ( p_other->i_idxposc )&&( i_other_pos <= i_other_pos_last ) ) )
    {
        return( 0 );
    }

    /* KNOW for the 2 streams, the ck we want are after the last read 
           or it's the first */

    /* if the first ck_other we want isn't between ck_info_last 
       and ck_info, don't parse */
    /* TODO fix this, use also number in buffered PES */
    if( ( i_other_pos > i_info_pos) /* ck_other too far */
        ||( i_other_pos < i_info_pos_last ) ) /* it's too late for ck_other */
    {
        return( 0 );
    } 
   
    /* we Know we will find ck_other, and before ck_info 
        "if ck_info is too far" will be handle after */
    return( 2 );
}

                         
static inline int __AVI_ChooseSize( int l1, int l2 )
{
    /* XXX l2 is prefered if 0 otherwise min not equal to 0 */
    if( !l2 )
    { 
        return( 0 );
    }
    return( !l1 ? l2 : __MIN( l1,l2 ) );
}

/* We know we will read chunk align */
static int __AVI_GetAndPutChunkInBuffer( input_thread_t  *p_input,
                                  AVIStreamInfo_t *p_info,
                                  int i_size,
                                  int i_ck )
{

    pes_packet_t    *p_pes;
    int i_length; 

    i_length = __MIN( i_size, BUFFER_MAXSPESSIZE );

    /* Skip chunk header */

    if( __AVI_GetDataInPES( p_input, &p_pes, i_length + 8,1 ) != i_length +8 )
    {
        return( 0 );
    }
    p_pes->p_first->p_payload_start += 8;
    p_pes->i_pes_size -= 8;

    i_size = GetDWLE( p_pes->p_first->p_demux_start + 4);

    AVI_PESBuffer_Add( p_input->p_method_data,
                       p_info,
                       p_pes,
                       i_ck,
                       0 );
    /* skip unwanted bytes */
    if( i_length != i_size)
    {
        msg_Err( p_input, "Chunk Size mismatch" );
        AVI_SeekAbsolute( p_input,
                          __EVEN( AVI_TellAbsolute( p_input ) + 
                                  i_size - i_length ) );
    }
    return( 1 );
}

/* XXX Don't use this function directly ! XXX */
static int __AVI_GetChunk( input_thread_t  *p_input,
                           AVIStreamInfo_t *p_info,
                           int b_load )
{
    demux_sys_t *p_avi = p_input->p_demux_data;
    AVIStreamInfo_t *p_other;
    int i_method;
    off_t i_posmax;
    int i;
   
#define p_info_i p_avi->pp_info[i]
    while( p_info->p_pes_first )
    {
        if( ( p_info->p_pes_first->i_posc == p_info->i_idxposc ) 
             &&( p_info->i_idxposb >= p_info->p_pes_first->i_posb )
             &&( p_info->i_idxposb < p_info->p_pes_first->i_posb + 
                     p_info->p_pes_first->p_pes->i_pes_size ) )
  
        {
            return( 1 ); /* we have it in buffer */
        }
        else
        {
            AVI_PESBuffer_Drop( p_input->p_method_data, p_info );
        }
    }
    /* up to now we handle only one audio and video streams at the same time */
    p_other = (p_info == p_avi->p_info_video ) ?
                     p_avi->p_info_audio : p_avi->p_info_video ;

    i_method = __AVI_GetChunkMethod( p_input, p_info, p_other );

    if( !i_method )
    {
        /* get directly the good chunk */
        return( b_load ? __AVI_SeekAndGetChunk( p_input, p_info ) : 1 );
    }
    /* We will parse
        * because invalid index
        * or will find ck_other before ck_info 
    */
/*    msg_Warn( p_input, "method %d", i_method ); */
    /* we will calculate the better position we have to reach */
    if( i_method == 1 )
    {
        /* invalid index */
    /*  the position max we have already reached */
        /* FIXME this isn't the better because sometime will fail to
            put in buffer p_other since it could be too far */
        AVIStreamInfo_t *p_info_max = p_info;
        
        for( i = 0; i < p_avi->i_streams; i++ )
        {
            if( p_info_i->i_idxnb )
            {
                if( p_info_max->i_idxnb )
                {
                    if( p_info_i->p_index[p_info_i->i_idxnb -1 ].i_pos >
                            p_info_max->p_index[p_info_max->i_idxnb -1 ].i_pos )
                    {
                        p_info_max = p_info_i;
                    }
                }
                else
                {
                    p_info_max = p_info_i;
                }
            }
        }
        if( p_info_max->i_idxnb )
        {
            /* be carefull that size between index and ck can sometime be 
              different without any error (and other time it's an error) */
           i_posmax = p_info_max->p_index[p_info_max->i_idxnb -1 ].i_pos; 
           /* so choose this, and I know that we have already reach it */
        }
        else
        {
            i_posmax = p_avi->p_movi->i_pos + 12;
        }
    }
    else
    {
        if( !b_load )
        {
            return( 1 ); /* all is ok */
        }
        /* valid index */
        /* we know that the entry and the last one are valid for the 2 stream */
        /* and ck_other will come *before* index so go directly to it*/
        i_posmax = p_other->p_index[p_other->i_idxposc].i_pos;
    }

    AVI_SeekAbsolute( p_input, i_posmax );
    /* the first chunk we will see is :
            * the last chunk that we have already seen for broken index 
            * the first ck for other with good index */ 
    for( ; ; ) /* infinite parsing until the ck we want */
    {
        riffchunk_t  *p_ck;
        int i_type;
        
        /* Get the actual chunk in the stream */
        if( !(p_ck = RIFF_ReadChunk( p_input )) )
        {
            return( 0 );
        }
/*        msg_Dbg( p_input, "ck: %4.4s len %d", &p_ck->i_id, p_ck->i_size ); */
        /* special case for LIST-rec chunk */
        if( ( p_ck->i_id == AVIFOURCC_LIST )&&( p_ck->i_type == AVIFOURCC_rec ) )
        {
            AVI_SkipBytes( p_input, 12 );
//            RIFF_DescendChunk( p_input );
            RIFF_DeleteChunk( p_input, p_ck );
            continue;
        }
        AVI_ParseStreamHeader( p_ck->i_id, &i, &i_type );
        /* littles checks but not too much if you want to read all file */ 
        if( i >= p_avi->i_streams )
        {
            RIFF_DeleteChunk( p_input, p_ck );
            if( RIFF_NextChunk( p_input, p_avi->p_movi ) != 0 )
            {
                return( 0 );
            }
        }
        else
        {
            int i_size;

            /* have we found a new entry (not present in index)? */
            if( ( !p_info_i->i_idxnb )
                ||(p_info_i->p_index[p_info_i->i_idxnb-1].i_pos < p_ck->i_pos))
            {
                AVIIndexEntry_t index;

                index.i_id = p_ck->i_id;
                index.i_flags = AVI_GetKeyFlag( p_info_i->i_codec,
                                                (u8*)&p_ck->i_8bytes);
                index.i_pos = p_ck->i_pos;
                index.i_length = p_ck->i_size;
                __AVI_AddEntryIndex( p_info_i, &index );   
            }


            /* TODO check if p_other is full and then if is possible 
                go directly to the good chunk */
            if( ( p_info_i == p_other )
                &&( !AVI_PESBuffer_IsFull( p_other ) )
                &&( ( !p_other->p_pes_last )||
                    ( p_other->p_pes_last->p_pes->i_pes_size != 
                                                    BUFFER_MAXSPESSIZE ) ) )
            {
                int i_ck = p_other->p_pes_last ? 
                        p_other->p_pes_last->i_posc + 1 : p_other->i_idxposc;
                i_size = __AVI_ChooseSize( p_ck->i_size,
                                           p_other->p_index[i_ck].i_length);
               
                if( p_other->p_index[i_ck].i_pos == p_ck->i_pos )
                {
                    if( !__AVI_GetAndPutChunkInBuffer( p_input, p_other, 
                                                       i_size, i_ck ) )
                    {
                        RIFF_DeleteChunk( p_input, p_ck );
                        return( 0 );
                    }
                }
                else
                {
                    if( RIFF_NextChunk( p_input, p_avi->p_movi ) != 0 )
                    {
                        RIFF_DeleteChunk( p_input, p_ck );

                        return( 0 );
                    }

                }
                        
                RIFF_DeleteChunk( p_input, p_ck );
            }
            else
            if( ( p_info_i == p_info)
                &&( p_info->i_idxposc < p_info->i_idxnb ) )
            {
                /* the first ck_info is ok otherwise it should be 
                        loaded without parsing */
                i_size = __AVI_ChooseSize( p_ck->i_size,
                                 p_info->p_index[p_info->i_idxposc].i_length);


                RIFF_DeleteChunk( p_input, p_ck );
                
                return( b_load ? __AVI_GetAndPutChunkInBuffer( p_input,
                                                               p_info,
                                                               i_size,
                                                     p_info->i_idxposc ) : 1 );
            }
            else
            {
                /* skip it */
                RIFF_DeleteChunk( p_input, p_ck );
                if( RIFF_NextChunk( p_input, p_avi->p_movi ) != 0 )
                {
                    return( 0 );
                }
            }
        }

    
    }
    
#undef p_info_i
}

/* be sure that i_ck will be a valid index entry */
static int AVI_SetStreamChunk( input_thread_t    *p_input,
                               AVIStreamInfo_t   *p_info,
                               int   i_ck )
{

    p_info->i_idxposc = i_ck;
    p_info->i_idxposb = 0;

    if(  i_ck < p_info->i_idxnb )
    {
        return( 1 );
    }
    else
    {
        p_info->i_idxposc = p_info->i_idxnb - 1;
        do
        {
            p_info->i_idxposc++;
            if( !__AVI_GetChunk( p_input, p_info, 0 ) )
            {
                return( 0 );
            }
        } while( p_info->i_idxposc < i_ck );

        return( 1 );
    }
}


/* XXX FIXME up to now, we assume that all chunk are one after one */
static int AVI_SetStreamBytes( input_thread_t    *p_input, 
                               AVIStreamInfo_t   *p_info,
                               off_t   i_byte )
{
    if( ( p_info->i_idxnb > 0 )
        &&( i_byte < p_info->p_index[p_info->i_idxnb - 1].i_lengthtotal + 
                p_info->p_index[p_info->i_idxnb - 1].i_length ) )
    {
        /* index is valid to find the ck */
        /* uses dichototmie to be fast enougth */
        int i_idxposc = __MIN( p_info->i_idxposc, p_info->i_idxnb - 1 );
        int i_idxmax  = p_info->i_idxnb;
        int i_idxmin  = 0;
        for( ;; )
        {
            if( p_info->p_index[i_idxposc].i_lengthtotal > i_byte )
            {
                i_idxmax  = i_idxposc ;
                i_idxposc = ( i_idxmin + i_idxposc ) / 2 ;
            }
            else
            {
                if( p_info->p_index[i_idxposc].i_lengthtotal + 
                        p_info->p_index[i_idxposc].i_length <= i_byte)
                {
                    i_idxmin  = i_idxposc ;
                    i_idxposc = (i_idxmax + i_idxposc ) / 2 ;
                }
                else
                {
                    p_info->i_idxposc = i_idxposc;
                    p_info->i_idxposb = i_byte - 
                            p_info->p_index[i_idxposc].i_lengthtotal;
                    return( 1 );
                }
            }
        }
        
    }
    else
    {
        p_info->i_idxposc = p_info->i_idxnb - 1;
        p_info->i_idxposb = 0;
        do
        {
            p_info->i_idxposc++;
            if( !__AVI_GetChunk( p_input, p_info, 0 ) )
            {
                return( 0 );
            }
        } while( p_info->p_index[p_info->i_idxposc].i_lengthtotal +
                    p_info->p_index[p_info->i_idxposc].i_length <= i_byte );

        p_info->i_idxposb = i_byte -
                       p_info->p_index[p_info->i_idxposc].i_lengthtotal;
        return( 1 );
    }
}

static pes_packet_t *AVI_ReadStreamChunkInPES(  input_thread_t  *p_input,
                                                AVIStreamInfo_t *p_info )

{
    if( p_info->i_idxposc > p_info->i_idxnb )
    {
        return( NULL );
    }

    /* we want chunk (p_info->i_idxposc,0) */
    p_info->i_idxposb = 0;
    if( !__AVI_GetChunk( p_input, p_info, 1) )
    {
        msg_Err( p_input, "Got one chunk : failed" );
        return( NULL );
    }
    p_info->i_idxposc++;
    return( AVI_PESBuffer_Get( p_info ) );
}

static pes_packet_t *AVI_ReadStreamBytesInPES(  input_thread_t  *p_input,
                                                AVIStreamInfo_t *p_info,
                                                int i_byte )
{
    pes_packet_t    *p_pes;
    data_packet_t   *p_data;
    int             i_count = 0;
    int             i_read;

        
    if( !( p_pes = input_NewPES( p_input->p_method_data ) ) )
    {
        return( NULL );
    }
    if( !( p_data = input_NewPacket( p_input->p_method_data, i_byte ) ) )
    {
        input_DeletePES( p_input->p_method_data, p_pes );
        return( NULL );
    }

    p_pes->p_first =
            p_pes->p_last = p_data;
    p_pes->i_nb_data = 1;
    p_pes->i_pes_size = i_byte;

    while( i_byte > 0 )
    {
        if( !__AVI_GetChunk( p_input, p_info, 1) )
        {
         msg_Err( p_input, "Got one chunk : failed" );
           
            input_DeletePES( p_input->p_method_data, p_pes );
            return( NULL );
        }

        i_read = __MIN( p_info->p_pes_first->p_pes->i_pes_size - 
                            ( p_info->i_idxposb - p_info->p_pes_first->i_posb ),
                        i_byte);
        /* FIXME FIXME FIXME follow all data packet */
        memcpy( p_data->p_payload_start + i_count, 
                p_info->p_pes_first->p_pes->p_first->p_payload_start + 
                    p_info->i_idxposb - p_info->p_pes_first->i_posb,
                i_read );

        AVI_PESBuffer_Drop( p_input->p_method_data, p_info );
        i_byte  -= i_read;
        i_count += i_read;

        p_info->i_idxposb += i_read;
        if( p_info->p_index[p_info->i_idxposc].i_length <= p_info->i_idxposb )
        {
            p_info->i_idxposb -= p_info->p_index[p_info->i_idxposc].i_length;
            p_info->i_idxposc++;
        }
    }
   return( p_pes );
}

/*****************************************************************************
 * AVI_GetFrameInPES : get dpts length(µs) in pes from stream
 *****************************************************************************
 * Handle multiple pes, and set pts to the good value 
 *****************************************************************************/
static pes_packet_t *AVI_GetFrameInPES( input_thread_t *p_input,
                                        AVIStreamInfo_t *p_info,
                                        mtime_t i_dpts)
{
    int i;
    pes_packet_t *p_pes = NULL;
    pes_packet_t *p_pes_tmp = NULL;
    pes_packet_t *p_pes_first = NULL;
    mtime_t i_pts;

    if( i_dpts < 1000 ) 
    { 
        return( NULL ) ; 
    }

    if( !p_info->i_samplesize )
    {
        int i_chunk = __MAX( AVI_PTSToChunk( p_info, i_dpts), 1 );
        p_pes_first = NULL;
        for( i = 0; i < i_chunk; i++ )
        {
            /* get pts while is valid */
            i_pts = AVI_GetPTS( p_info ); 
 
            p_pes_tmp = AVI_ReadStreamChunkInPES( p_input, p_info );

            if( !p_pes_tmp )
            {
                return( p_pes_first );
            }
            p_pes_tmp->i_pts = i_pts;
            if( !p_pes_first )
            {
                p_pes_first = p_pes_tmp;
            }
            else
            {
                p_pes->p_next = p_pes_tmp;
            }
            p_pes = p_pes_tmp;
        }
        return( p_pes_first );
    }
    else
    {
        /* stream is byte based */
        int i_byte = AVI_PTSToByte( p_info, i_dpts);
        if( i_byte < 50 ) /* to avoid some problem with audio */
        {
            return( NULL );
        }
        i_pts = AVI_GetPTS( p_info );  /* ok even with broken index */
        p_pes = AVI_ReadStreamBytesInPES( p_input, p_info, i_byte);

        if( p_pes )
        {
            p_pes->i_pts = i_pts;
        }
        return( p_pes );
    }
}
/*****************************************************************************
 * AVI_DecodePES : send a pes to decoder 
 *****************************************************************************
 * Handle multiple pes, and update pts to the good value 
 *****************************************************************************/
static inline void AVI_DecodePES( input_thread_t *p_input,
                                  AVIStreamInfo_t *p_info,
                                  pes_packet_t *p_pes )
{
    pes_packet_t    *p_pes_next;
    /* input_decode want only one pes, but AVI_GetFrameInPES give
          multiple pes so send one by one */
    while( p_pes )
    {
        p_pes_next = p_pes->p_next;
        p_pes->p_next = NULL;
        p_pes->i_pts = input_ClockGetTS( p_input, 
                                         p_input->stream.p_selected_program, 
                                         p_pes->i_pts * 9/100);
        input_DecodePES( p_info->p_es->p_decoder_fifo, p_pes );
        p_pes = p_pes_next;
    }
  
}

static int AVI_StreamSeek( input_thread_t *p_input,
                           demux_sys_t  *p_avi,
                           int i_stream, 
                           mtime_t i_date )
{
#define p_stream    p_avi->pp_info[i_stream]
    mtime_t i_oldpts;
    
    AVI_PESBuffer_Flush( p_input->p_method_data, p_stream );
    i_oldpts = AVI_GetPTS( p_stream );

    if( !p_stream->i_samplesize )
    {
        AVI_SetStreamChunk( p_input,
                            p_stream, 
                            AVI_PTSToChunk( p_stream, i_date ) );
        /* search key frame */
        msg_Dbg( p_input, 
                 "old:%lld %s new %lld",
                 i_oldpts, 
                 i_oldpts > i_date ? ">" : "<",
                 i_date );

        if( i_date < i_oldpts )
        {
            while( p_stream->i_idxposc > 0 && 
               !( p_stream->p_index[p_stream->i_idxposc].i_flags & 
                                                            AVIIF_KEYFRAME ) )
            {
                if( !AVI_SetStreamChunk( p_input,
                                         p_stream,
                                         p_stream->i_idxposc - 1 ) )
                {
                    return( 0 );
                }
            }
        }
        else
        {
            while( !( p_stream->p_index[p_stream->i_idxposc].i_flags &
                                                            AVIIF_KEYFRAME ) )
            {
                if( !AVI_SetStreamChunk( p_input, 
                                         p_stream, 
                                         p_stream->i_idxposc + 1 ) )
                {
                    return( 0 );
                }
            }
        }
    }
    else
    {
        AVI_SetStreamBytes( p_input,
                            p_stream,
                            AVI_PTSToByte( p_stream, i_date ) );
    }
    return( 1 );
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
             "seek requested: %lld secondes %d%%", 
             i_date / 1000000,
             i_percent );

    if( p_avi->b_seekable )
    {
        if( !p_avi->i_length )
        {
            int i_index;
            AVIStreamInfo_t *p_stream;
            u64 i_pos;

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
                           p_avi->p_movi->i_pos );
            /* search first selected stream */
            for( i_index = 0,p_stream = NULL; 
                 i_index < p_avi->i_streams; i_stream++ )
            {
                p_stream = p_avi->pp_info[i_index];
                if( p_stream->i_activated )
                {
                    break;
                }
            }
            if( !p_stream || !p_stream->p_index )
            {
                msg_Err( p_input, "cannot find any selected stream" );
                return( -1 );
            }
            /* search chunk */
            p_stream->i_idxposc =  __MAX( p_stream->i_idxposc - 1, 0 );
            while( ( i_pos < p_stream->p_index[p_stream->i_idxposc].i_pos )
                   &&( p_stream->i_idxposc > 0 ) )
            {
                /* search before i_idxposc */
                if( !AVI_SetStreamChunk( p_input, 
                                         p_stream, p_stream->i_idxposc - 1 ) )
                {
                    msg_Err( p_input, "cannot seek" );
                    return( -1 );
                }
            }
            while( i_pos >= p_stream->p_index[p_stream->i_idxposc].i_pos +
               p_stream->p_index[p_stream->i_idxposc].i_length + 8 )
            {
                /* search after i_idxposc */
                if( !AVI_SetStreamChunk( p_input, 
                                         p_stream, p_stream->i_idxposc + 1 ) )
                {
                    msg_Err( p_input, "cannot seek" );
                    return( -1 );
                }
            }
            i_date = AVI_GetPTS( p_stream );
            /* TODO better support for i_samplesize != 0 */
            msg_Dbg( p_input, "estimate date %lld", i_date );
        }

#define p_stream    p_avi->pp_info[i_stream]
        p_avi->i_time = 0;
        /* seek for chunk based streams */
        for( i_stream = 0; i_stream < p_avi->i_streams; i_stream++ )
        {
            if( p_stream->i_activated && !p_stream->i_samplesize )
//            if( p_stream->i_activated )
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
            if( p_stream->i_activated && p_stream->i_samplesize )
            {
                AVI_StreamSeek( p_input, p_avi, i_stream, i_date );
//                p_avi->i_time = __MAX( AVI_GetPTS( p_stream ), p_avi->i_time );
            }
        }
        msg_Dbg( p_input, "seek: %lld secondes", p_avi->i_time /1000000 );
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
 * TODO add support for unstreable file, just read a chunk and send it 
 *      to the right decoder, very easy
 *****************************************************************************/

static int AVIDemux_Seekable( input_thread_t *p_input )
{
    int i;
    int i_stream;
    int b_stream;

    demux_sys_t *p_avi = p_input->p_demux_data;

    /* detect new selected/unselected streams */
    for( i_stream = 0; i_stream < p_avi->i_streams; i_stream++ )
    {
#define p_stream    p_avi->pp_info[i_stream]
        if( p_stream->p_es )
        {
            if( p_stream->p_es->p_decoder_fifo &&
                !p_stream->i_activated )
            {
                AVI_StreamStart( p_input, p_avi, i_stream );
            }
            else
            if( !p_stream->p_es->p_decoder_fifo &&
                p_stream->i_activated )
            {
                AVI_StreamStop( p_input, p_avi, i_stream );
            }
        }       
#undef  p_stream
    }
    /* search new video and audio stream selected 
          if current have been unselected*/
    if( ( !p_avi->p_info_video )
            || ( !p_avi->p_info_video->p_es->p_decoder_fifo ) )
    {
        p_avi->p_info_video = NULL;
        for( i = 0; i < p_avi->i_streams; i++ )
        {
            if( ( p_avi->pp_info[i]->i_cat == VIDEO_ES )
                  &&( p_avi->pp_info[i]->p_es->p_decoder_fifo ) )
            {
                p_avi->p_info_video = p_avi->pp_info[i];
                break;
            }
        }
    }
    if( ( !p_avi->p_info_audio )
            ||( !p_avi->p_info_audio->p_es->p_decoder_fifo ) )
    {
        p_avi->p_info_audio = NULL;
        for( i = 0; i < p_avi->i_streams; i++ )
        {
            if( ( p_avi->pp_info[i]->i_cat == AUDIO_ES )
                  &&( p_avi->pp_info[i]->p_es->p_decoder_fifo ) )
            {
                p_avi->p_info_audio = p_avi->pp_info[i];
                break;
            }
        }
    }

    if( p_input->stream.p_selected_program->i_synchro_state == SYNCHRO_REINIT )
    {
        mtime_t i_date;
        int i_percent;
        /* first wait for empty buffer, arbitrary time FIXME */
        msleep( DEFAULT_PTS_DELAY );

        i_date = (mtime_t)1000000 *
                 (mtime_t)p_avi->i_length *
                 (mtime_t)AVI_TellAbsolute( p_input ) /
                 (mtime_t)p_input->stream.p_selected_area->i_size;
        i_percent = 100 * AVI_TellAbsolute( p_input ) / 
                        p_input->stream.p_selected_area->i_size;
        AVISeek( p_input, i_date, i_percent);
//        input_ClockInit( p_input->stream.p_selected_program );
    }
    /* wait for the good time */
    input_ClockManageRef( p_input,
                          p_input->stream.p_selected_program,
                          p_avi->i_pcr ); 

    p_avi->i_pcr = p_avi->i_time * 9 / 100;
    p_avi->i_time += 100*1000;  /* read 100ms */
    
    for( i_stream = 0, b_stream = 0; i_stream < p_avi->i_streams; i_stream++ )
    {
#define p_stream    p_avi->pp_info[i_stream]
        pes_packet_t    *p_pes;

        if( !p_stream->p_es ||
            !p_stream->p_es->p_decoder_fifo )
        {

            continue;
        }
        if( p_avi->i_time <= AVI_GetPTS( p_stream  ) )
        {
            msg_Warn( p_input, "skeeping stream %d", i_stream );
            b_stream = 1;
            continue;
        }
        p_pes = AVI_GetFrameInPES( p_input,
                                   p_stream,
                                   p_avi->i_time - AVI_GetPTS( p_stream  ) ); 
        if( p_pes )
        {
            AVI_DecodePES( p_input, p_stream, p_pes );
            b_stream = 1;
        }
#undef p_stream
    }

    /* at the end ? */
    return( b_stream ? 1 : 0 );

}


/*****************************************************************************
 * AVIDemux_UnSeekable: reads and demuxes data packets for unseekable
 *                       file
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int AVIDemux_UnSeekable( input_thread_t *p_input )
{
    demux_sys_t     *p_avi = p_input->p_demux_data;
    AVIStreamInfo_t *p_stream_master;
    int     i_stream;
    int     b_audio;
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

        if( !AVI_PacketGetHeader( p_input, &avi_pk ) )
        {
            return( 0 );
        }
//        AVI_ParseStreamHeader( avi_pk.i_fourcc, &i_stream, &i_cat );

        if( avi_pk.i_stream >= p_avi->i_streams ||
            ( avi_pk.i_cat != AUDIO_ES && avi_pk.i_cat != VIDEO_ES ) )
        {
            /* we haven't found an audio or video packet:
                - we have seek, found first next packet
                - others packets could be found, skip them
            */
            switch( avi_pk.i_fourcc )
            {
                case AVIFOURCC_JUNK:
                case AVIFOURCC_LIST:
                    return( AVI_PacketNext( p_input ) ? 1 : 0 );
                case AVIFOURCC_idx1:
                    return( 0 );    // eof
                default:
                    msg_Warn( p_input, 
                              "seems to have lost position, resync" );
                    if( !AVI_PacketSearch( p_input ) )
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
                if( !AVI_PacketNext( p_input ) )
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
                    if( !AVI_PacketRead( p_input, &avi_pk, &p_pes ) || !p_pes)
                    {
                        return( -1 );
                    }
                    p_pes->i_pts = AVI_GetPTS( p_stream );
                    AVI_DecodePES( p_input, p_stream, p_pes );
                }
                else
                {
                    if( !AVI_PacketNext( p_input ) )
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

