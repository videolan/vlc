/*****************************************************************************
 * libavi.c : 
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: libavi.c,v 1.4 2002/11/05 10:07:56 gbazin Exp $
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

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */
#include <errno.h>
#include <sys/types.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "libavi.h"

#define AVI_DEBUG 1
#define AVIFOURCC_PRINT( x ) \
    (x)&0xff,           \
    ( (x) >>  8 )&0xff, \
    ( (x) >> 16 )&0xff, \
    ( (x) >> 24 )&0xff
    
#define FREE( p ) \
    if( p ) {free( p ); p = NULL; }

#define __EVEN( x ) ( (x)&0x01 ? (x)+1 : (x) )
  
/* Some functions to manipulate memory */
static u16 GetWLE( u8 *p_buff )
{
    return( (p_buff[0]) + ( p_buff[1] <<8 ) );
}

static u32 GetDWLE( u8 *p_buff )
{
    return( p_buff[0] + ( p_buff[1] <<8 ) +
            ( p_buff[2] <<16 ) + ( p_buff[3] <<24 ) );
}
static vlc_fourcc_t GetFOURCC( byte_t *p_buff )
{
    return( VLC_FOURCC( p_buff[0], p_buff[1], p_buff[2], p_buff[3] ) );
}
/*****************************************************************************
 * Some basic functions to manipulate stream more easily in vlc
 *
 * AVI_TellAbsolute get file position
 * 
 * AVI_SeekAbsolute seek in the file
 *
 * AVI_ReadData read data from the file in a buffer
 *
 * AVI_SkipBytes skip bytes
 *
 *****************************************************************************/
off_t AVI_TellAbsolute( input_thread_t *p_input )
{
    off_t i_pos;
    
    vlc_mutex_lock( &p_input->stream.stream_lock );
    
    i_pos= p_input->stream.p_selected_area->i_tell;
//            - ( p_input->p_last_data - p_input->p_current_data  );

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return( i_pos );
}
 
int AVI_SeekAbsolute( input_thread_t *p_input,
                      off_t i_pos)
{
    off_t i_filepos;

    if( i_pos >= p_input->stream.p_selected_area->i_size )
    {
        return( 0 );
    }
    
    i_filepos = AVI_TellAbsolute( p_input );

    if( i_filepos == i_pos )
    {
        return( 1 );
    }

    if( p_input->stream.b_seekable &&
        p_input->stream.i_method == INPUT_METHOD_FILE )
    {
        p_input->pf_seek( p_input, i_pos );
        input_AccessReinit( p_input );
        return( 1 );
    }
    else
    {
        data_packet_t   *p_data;
        int             i_skip = i_pos - i_filepos;
        
        msg_Warn( p_input, "will skip %d bytes, slow", i_skip );
        if( i_skip < 0 )
        {
            return( 0 ); // failed
        }
        while (i_skip > 0 )
        {
            int i_read;
            
            i_read = input_SplitBuffer( p_input, &p_data, 
                                        __MIN( 4096, i_skip ) );
            if( i_read < 0 )
            {
                return( 0 );
            }
            i_skip -= i_read;
            
            input_DeletePacket( p_input->p_method_data, p_data );
            if( i_read == 0 && i_skip > 0 )
            {
                return( 0 );
            }
        }
#if 0
        while( i_skip > 0 )
        {
            i_peek = input_Peek( p_input, &p_peek, i_skip+1 );
            i_peek--;
            i_skip -= i_peek;
            vlc_mutex_lock( &p_input->stream.stream_lock );
            p_input->p_current_data += i_peek;  // skip them
            vlc_mutex_unlock( &p_input->stream.stream_lock );
            if( i_peek <= 0 )
            {
                return( 0);
            }
        }
#endif
        return( 1 );
    }
}

/* return 1 if success, 0 if fail */
int AVI_ReadData( input_thread_t *p_input, u8 *p_buff, int i_size )
{
    data_packet_t *p_data;

    int i_count;
    int i_read = 0;

    
    if( !i_size )
    {
        return( 0 );
    }

    do
    {
        i_count = input_SplitBuffer(p_input, &p_data, __MIN( i_size, 1024 ) );
        if( i_count <= 0 )
        {
            return( i_read );
        }
        memcpy( p_buff, p_data->p_payload_start, i_count );
        input_DeletePacket( p_input->p_method_data, p_data );
        
        p_buff += i_count;
        i_size -= i_count;
        i_read += i_count;
                
    } while( i_size );
    
    return( i_read );
}

int  AVI_SkipBytes( input_thread_t *p_input, int i_count )
{
    /* broken with new use of i_tell */
#if 0
    int i_buff_size;
    vlc_mutex_lock( &p_input->stream.stream_lock );
    i_buff_size = p_input->p_last_data - p_input->p_current_data;
    vlc_mutex_unlock( &p_input->stream.stream_lock );
            
    if( i_count > 0 && i_count + 1 < i_buff_size )
    {
        u8 *p_peek;
        
        input_Peek( p_input, &p_peek, i_count + 1 );

        vlc_mutex_lock( &p_input->stream.stream_lock );
        p_input->p_current_data += i_count;  // skip them
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        return( 1 );
    }
    else
#endif
    {
        return( AVI_SeekAbsolute( p_input, 
                              AVI_TellAbsolute( p_input ) + i_count ) );
    }
}

/*****************************************************************************
 *
 * AVI_TestFile: look at first bytes to see if it's a valid avi file
 * 
 * unseekable: ok
 *
 *****************************************************************************/
int AVI_TestFile( input_thread_t *p_input )
{
    u8  *p_peek;
    
    if( input_Peek( p_input, &p_peek, 8 ) < 8 )
    {
        msg_Err( p_input, "cannot peek()" );
        return( 0 );
    }

    if( GetDWLE( p_peek ) == AVIFOURCC_RIFF && 
        GetDWLE( p_peek + 8 ) == AVIFOURCC_AVI )
    {
        return( 1 );
    }
    else
    {
        return( 0 );
    }
}
/****************************************************************************
 *
 * Basics functions to manipulates chunks
 *
 ****************************************************************************/
static int AVI_ChunkReadCommon( input_thread_t *p_input,
                                avi_chunk_t *p_chk )
{
    u8  *p_peek;
    int i_peek;

    memset( p_chk, 0, sizeof( avi_chunk_t ) );

    if( ( i_peek = input_Peek( p_input, &p_peek, 8 ) ) < 8 )
    {
        return( 0 );
    }
    
    p_chk->common.i_chunk_fourcc = GetDWLE( p_peek );
    p_chk->common.i_chunk_size   = GetDWLE( p_peek + 4 );
    p_chk->common.i_chunk_pos    = AVI_TellAbsolute( p_input );

    p_chk->common.p_father = NULL;
    p_chk->common.p_next = NULL;
    p_chk->common.p_first = NULL;
    p_chk->common.p_next = NULL;
#ifdef AVI_DEBUG
    msg_Dbg( p_input, 
             "Found Chunk fourcc:%c%c%c%c size:%lld pos:%lld",
             AVIFOURCC_PRINT( p_chk->common.i_chunk_fourcc ),
             p_chk->common.i_chunk_size,
             p_chk->common.i_chunk_pos );
#endif
    return( 1 );
}

static int AVI_NextChunk( input_thread_t *p_input,
                          avi_chunk_t *p_chk )
{
    avi_chunk_t chk;
    
    if( !p_chk )
    {
        if( !AVI_ChunkReadCommon( p_input, &chk ) )
        {
            return( 0 );
        }
        p_chk = &chk;
    }

    if( p_chk->common.p_father )
    {
        if( p_chk->common.p_father->common.i_chunk_pos + 
                __EVEN( p_chk->common.p_father->common.i_chunk_size ) + 8 <
            p_chk->common.i_chunk_pos + 
                __EVEN( p_chk->common.i_chunk_size ) + 8 )
        {
            return( 0 );
        }
    }
    return( AVI_SeekAbsolute( p_input,
                              p_chk->common.i_chunk_pos + 
                                  __EVEN( p_chk->common.i_chunk_size ) + 8 ) );
}

int _AVI_ChunkGoto( input_thread_t *p_input,
                   avi_chunk_t *p_chk )
{
    if( !p_chk )
    {
        return( 0 );
    }
    return( AVI_SeekAbsolute( p_input, p_chk->common.i_chunk_pos ) );
}

/****************************************************************************
 *
 * Functions to read chunks 
 *
 ****************************************************************************/
static int AVI_ChunkRead_list( input_thread_t *p_input,
                               avi_chunk_t *p_container,
                               int b_seekable )
{
    avi_chunk_t *p_chk;
    u8 *p_peek;
    
    if( p_container->common.i_chunk_size < 8 )
    {
        /* empty box */
        msg_Warn( p_input, "empty list chunk" );
        return( 0 );
    }
    if( input_Peek( p_input, &p_peek, 12 ) < 12 )
    {
        msg_Warn( p_input, "cannot peek while reading list chunk" );
        return( 0 );
    }
    p_container->list.i_type = GetDWLE( p_peek + 8 );

    if( p_container->common.i_chunk_fourcc == AVIFOURCC_LIST &&
        p_container->list.i_type == AVIFOURCC_movi )
    {
        msg_Dbg( p_input, "Skipping movi chunk" );
        if( b_seekable )
        {
            return( AVI_NextChunk( p_input, p_container ) );
        }
        else
        {
            return( 1 ); // point at begining of LIST-movi 
        }
    }

    AVI_SkipBytes( p_input, 12 );
#ifdef AVI_DEBUG
    msg_Dbg( p_input, 
             "found LIST chunk: \'%c%c%c%c\'",
             AVIFOURCC_PRINT( p_container->list.i_type ) );
#endif
    for( ; ; )
    {
        p_chk = malloc( sizeof( avi_chunk_t ) );
        memset( p_chk, 0, sizeof( avi_chunk_t ) );
        if( !p_container->common.p_first )
        {
            p_container->common.p_first = p_chk;
        }
        else
        {
            p_container->common.p_last->common.p_next = p_chk;
        }
        p_container->common.p_last = p_chk;

        if( !AVI_ChunkRead( p_input, p_chk, p_container, b_seekable ) ||
           ( AVI_TellAbsolute( p_input ) >=
                p_chk->common.p_father->common.i_chunk_pos + 
                    __EVEN( p_chk->common.p_father->common.i_chunk_size ) ) )
        {
            break;
        }
        /* If we can't seek then stop when we 've found LIST-movi */
        if( p_chk->common.i_chunk_fourcc == AVIFOURCC_LIST &&
            p_chk->list.i_type == AVIFOURCC_movi && !b_seekable )
        {
            break;
        }

    } 
    
    return( 1 );
}

#define AVI_READCHUNK_ENTER \
    s64 i_read = __EVEN(p_chk->common.i_chunk_size ) + 8; \
    u8  *p_read, *p_buff;    \
    if( !( p_read = p_buff = malloc(i_read ) ) ) \
    { \
        return( 0 ); \
    } \
    i_read = AVI_ReadData( p_input, p_read, i_read ); \
    p_read += 8; \
    i_read -= 8
    
#define AVI_READCHUNK_EXIT( code ) \
    free( p_buff ); \
    if( i_read < 0 ) \
    { \
        msg_Warn( p_input, "not enougth data" ); \
    } \
    return( code )
#define AVI_READ2BYTES( i_word ) \
    i_word = GetWLE( p_read ); \
    p_read += 2; \
    i_read -= 2

#define AVI_READ4BYTES( i_dword ) \
    i_dword = GetDWLE( p_read ); \
    p_read += 4; \
    i_read -= 4
    
#define AVI_READFOURCC( i_dword ) \
    i_dword = GetFOURCC( p_read ); \
    p_read += 4; \
    i_read -= 4

static int AVI_ChunkRead_avih( input_thread_t *p_input,
                               avi_chunk_t *p_chk,
                               int b_seekable )
{
    AVI_READCHUNK_ENTER;

    AVI_READ4BYTES( p_chk->avih.i_microsecperframe);
    AVI_READ4BYTES( p_chk->avih.i_maxbytespersec );
    AVI_READ4BYTES( p_chk->avih.i_reserved1 );
    AVI_READ4BYTES( p_chk->avih.i_flags );
    AVI_READ4BYTES( p_chk->avih.i_totalframes );
    AVI_READ4BYTES( p_chk->avih.i_initialframes );
    AVI_READ4BYTES( p_chk->avih.i_streams );
    AVI_READ4BYTES( p_chk->avih.i_suggestedbuffersize );
    AVI_READ4BYTES( p_chk->avih.i_width );
    AVI_READ4BYTES( p_chk->avih.i_height );
    AVI_READ4BYTES( p_chk->avih.i_scale );
    AVI_READ4BYTES( p_chk->avih.i_rate );
    AVI_READ4BYTES( p_chk->avih.i_start );
    AVI_READ4BYTES( p_chk->avih.i_length );
#ifdef AVI_DEBUG
    msg_Dbg( p_input, 
             "avih: streams:%d flags:%s%s%s%s %dx%d", 
             p_chk->avih.i_streams,
             p_chk->avih.i_flags&AVIF_HASINDEX?" HAS_INDEX":"",
             p_chk->avih.i_flags&AVIF_MUSTUSEINDEX?" MUST_USE_INDEX":"",
             p_chk->avih.i_flags&AVIF_ISINTERLEAVED?" IS_INTERLEAVED":"",
             p_chk->avih.i_flags&AVIF_TRUSTCKTYPE?" TRUST_CKTYPE":"",
             p_chk->avih.i_width, p_chk->avih.i_height );
#endif 
    AVI_READCHUNK_EXIT( 1 );
}

static int AVI_ChunkRead_strh( input_thread_t *p_input,
                               avi_chunk_t *p_chk,
                               int b_seekable )
{
    AVI_READCHUNK_ENTER;

    AVI_READFOURCC( p_chk->strh.i_type );
    AVI_READFOURCC( p_chk->strh.i_handler );
    AVI_READ4BYTES( p_chk->strh.i_flags );
    AVI_READ4BYTES( p_chk->strh.i_reserved1 );
    AVI_READ4BYTES( p_chk->strh.i_initialframes );
    AVI_READ4BYTES( p_chk->strh.i_scale );
    AVI_READ4BYTES( p_chk->strh.i_rate );
    AVI_READ4BYTES( p_chk->strh.i_start );
    AVI_READ4BYTES( p_chk->strh.i_length );
    AVI_READ4BYTES( p_chk->strh.i_suggestedbuffersize );
    AVI_READ4BYTES( p_chk->strh.i_quality );
    AVI_READ4BYTES( p_chk->strh.i_samplesize );
#ifdef AVI_DEBUG
    msg_Dbg( p_input, 
             "strh: type:%c%c%c%c handler:0x%8.8x samplesize:%d %.2ffps",
             AVIFOURCC_PRINT( p_chk->strh.i_type ),
             p_chk->strh.i_handler,
             p_chk->strh.i_samplesize,
             ( p_chk->strh.i_scale ? 
                (float)p_chk->strh.i_rate / (float)p_chk->strh.i_scale : -1) );
#endif
    
    AVI_READCHUNK_EXIT( 1 );
}

static int AVI_ChunkRead_strf( input_thread_t *p_input,
                               avi_chunk_t *p_chk,
                               int b_seekable )
{
    avi_chunk_t *p_strh;

    AVI_READCHUNK_ENTER;
    if( p_chk->common.p_father == NULL )
    {
        msg_Err( p_input, "malformed avi file" );
        AVI_READCHUNK_EXIT( 0 );
    }
    if( !( p_strh = AVI_ChunkFind( p_chk->common.p_father, AVIFOURCC_strh, 0 ) ) )
    {
        msg_Err( p_input, "malformed avi file" );
        AVI_READCHUNK_EXIT( 0 );
    }
    
    switch( p_strh->strh.i_type )
    {
        case( AVIFOURCC_auds ):
            AVI_READ2BYTES( p_chk->strf.auds.i_formattag );
            AVI_READ2BYTES( p_chk->strf.auds.i_channels );
            AVI_READ4BYTES( p_chk->strf.auds.i_samplespersec );
            AVI_READ4BYTES( p_chk->strf.auds.i_avgbytespersec );
            AVI_READ2BYTES( p_chk->strf.auds.i_blockalign );
            AVI_READ2BYTES( p_chk->strf.auds.i_bitspersample );
            if( p_chk->strf.auds.i_formattag != WAVE_FORMAT_PCM )
            {
                AVI_READ2BYTES( p_chk->strf.auds.i_size );
            }
            p_chk->strf.auds.p_wfx = malloc( p_chk->common.i_chunk_size );
            memcpy( p_chk->strf.auds.p_wfx, 
                    p_buff + 8, 
                    p_chk->common.i_chunk_size );
#ifdef AVI_DEBUG
            msg_Dbg( p_input, 
                     "strf: audio:0x%4.4x channels:%d %dHz %dbits/sample %dkb/s",
                     p_chk->strf.auds.i_formattag,
                     p_chk->strf.auds.i_channels,
                     p_chk->strf.auds.i_samplespersec,
                     p_chk->strf.auds.i_bitspersample,
                     p_chk->strf.auds.i_avgbytespersec * 8 / 1024 );
#endif
            break;
        case( AVIFOURCC_vids ):
            p_strh->strh.i_samplesize = 0; // XXX for ffmpeg avi file
            p_chk->strf.vids.p_bih = malloc( p_chk->common.i_chunk_size );
            AVI_READ4BYTES( p_chk->strf.vids.p_bih->biSize );
            AVI_READ4BYTES( p_chk->strf.vids.p_bih->biWidth );
            AVI_READ4BYTES( p_chk->strf.vids.p_bih->biHeight );
            AVI_READ2BYTES( p_chk->strf.vids.p_bih->biPlanes );
            AVI_READ2BYTES( p_chk->strf.vids.p_bih->biBitCount );
            AVI_READFOURCC( p_chk->strf.vids.p_bih->biCompression );
            AVI_READ4BYTES( p_chk->strf.vids.p_bih->biSizeImage );
            AVI_READ4BYTES( p_chk->strf.vids.p_bih->biXPelsPerMeter );
            AVI_READ4BYTES( p_chk->strf.vids.p_bih->biYPelsPerMeter );
            AVI_READ4BYTES( p_chk->strf.vids.p_bih->biClrUsed );
            AVI_READ4BYTES( p_chk->strf.vids.p_bih->biClrImportant );
            memcpy( &p_chk->strf.vids.p_bih[1],
                    p_buff + sizeof(BITMAPINFOHEADER),
                    p_chk->common.i_chunk_size - sizeof(BITMAPINFOHEADER) );
#ifdef AVI_DEBUG
            msg_Dbg( p_input,
                     "strf: video:%c%c%c%c %dx%d planes:%d %dbpp",
                     AVIFOURCC_PRINT( p_chk->strf.vids.p_bih->biCompression ),
                     p_chk->strf.vids.p_bih->biWidth,
                     p_chk->strf.vids.p_bih->biHeight,
                     p_chk->strf.vids.p_bih->biPlanes,
                     p_chk->strf.vids.p_bih->biBitCount );
#endif
            break;
        default:
            msg_Warn( p_input, "unknown stream type" );
            break;
    }
    AVI_READCHUNK_EXIT( 1 );
}
static void AVI_ChunkFree_strf( input_thread_t *p_input,
                               avi_chunk_t *p_chk )
{

}

static int AVI_ChunkRead_strd( input_thread_t *p_input,
                               avi_chunk_t *p_chk,
                               int b_seekable )
{
    AVI_READCHUNK_ENTER;
    p_chk->strd.p_data = malloc( p_chk->common.i_chunk_size );
    memcpy( p_chk->strd.p_data,
            p_buff,
            p_chk->common.i_chunk_size );
    AVI_READCHUNK_EXIT( 1 );
}

static int AVI_ChunkRead_idx1( input_thread_t *p_input,
                               avi_chunk_t *p_chk,
                               int b_seekable )
{
    int i_count, i_index;

    AVI_READCHUNK_ENTER;

    i_count = __MIN( p_chk->common.i_chunk_size, i_read ) / 16;

    p_chk->idx1.i_entry_count = i_count;
    p_chk->idx1.i_entry_max   = i_count;
    if( i_count > 0 )
    {
        p_chk->idx1.entry = calloc( i_count, sizeof( idx1_entry_t ) );

        for( i_index = 0; i_index < i_count ; i_index++ )
        {
            AVI_READ4BYTES( p_chk->idx1.entry[i_index].i_fourcc );
            AVI_READ4BYTES( p_chk->idx1.entry[i_index].i_flags );
            AVI_READ4BYTES( p_chk->idx1.entry[i_index].i_pos );
            AVI_READ4BYTES( p_chk->idx1.entry[i_index].i_length );
        }
    }
    else
    {
        p_chk->idx1.entry = NULL;
    }
#ifdef AVI_DEBUG
    msg_Dbg( p_input, "idx1: index entry:%d", i_count );
#endif
    AVI_READCHUNK_EXIT( 1 );
}

static void AVI_ChunkFree_idx1( input_thread_t *p_input,
                               avi_chunk_t *p_chk )
{
    p_chk->idx1.i_entry_count = 0;
    p_chk->idx1.i_entry_max   = 0;
    FREE( p_chk->idx1.entry )
}

static struct 
{
    u32 i_fourcc;
    char *psz_type;
} AVI_strz_type[] =
{
    { AVIFOURCC_IARL, "archive location" },
    { AVIFOURCC_IART, "artist" },
    { AVIFOURCC_ICMS, "commisioned" },
    { AVIFOURCC_ICMT, "comments" },
    { AVIFOURCC_ICOP, "copyright" },
    { AVIFOURCC_ICRD, "creation date" },
    { AVIFOURCC_ICRP, "cropped" },
    { AVIFOURCC_IDIM, "dimensions" },
    { AVIFOURCC_IDPI, "dots per inch" },
    { AVIFOURCC_IENG, "enginner" },
    { AVIFOURCC_IGNR, "genre" },
    { AVIFOURCC_IKEY, "keywords" },
    { AVIFOURCC_ILGT, "lightness" },
    { AVIFOURCC_IMED, "medium" },
    { AVIFOURCC_INAM, "name" },
    { AVIFOURCC_IPLT, "palette setting" },
    { AVIFOURCC_IPRD, "product" },
    { AVIFOURCC_ISBJ, "subject" },
    { AVIFOURCC_ISFT, "software" },
    { AVIFOURCC_ISHP, "sharpness" },
    { AVIFOURCC_ISRC, "source" },
    { AVIFOURCC_ISRF, "source form" },
    { AVIFOURCC_ITCH, "technician" },
    { AVIFOURCC_ISMP, "time code" },
    { AVIFOURCC_IDIT, "digitalization time" },
    { 0,              "???" }
};
static int AVI_ChunkRead_strz( input_thread_t *p_input,
                               avi_chunk_t *p_chk,
                               int b_seekable )
{
    int i_index;
    avi_chunk_STRING_t *p_strz = (avi_chunk_STRING_t*)p_chk;
    AVI_READCHUNK_ENTER;

    for( i_index = 0;; i_index++)
    {
        if( !AVI_strz_type[i_index].i_fourcc ||
            AVI_strz_type[i_index].i_fourcc == p_strz->i_chunk_fourcc )
        {
            break;
        }
    }
    p_strz->p_type = strdup( AVI_strz_type[i_index].psz_type );
    p_strz->p_str = malloc( i_read + 1);

    if( p_strz->i_chunk_size )
    {
        memcpy( p_strz->p_str, p_read, i_read );
    }
    p_strz->p_str[i_read] = 0;
    
#ifdef AVI_DEBUG
    msg_Dbg( p_input, "%c%c%c%c: %s : %s", 
             AVIFOURCC_PRINT( p_strz->i_chunk_fourcc), p_strz->p_type, p_strz->p_str);
#endif
    AVI_READCHUNK_EXIT( 1 );
}
static void AVI_ChunkFree_strz( input_thread_t *p_input,
                                avi_chunk_t *p_chk )
{
    avi_chunk_STRING_t *p_strz = (avi_chunk_STRING_t*)p_chk;
    FREE( p_strz->p_type );
    FREE( p_strz->p_str );
}

static int AVI_ChunkRead_nothing( input_thread_t *p_input,
                               avi_chunk_t *p_chk,
                               int b_seekable )
{
    return( AVI_NextChunk( p_input, p_chk ) );
}
static void AVI_ChunkFree_nothing( input_thread_t *p_input,
                               avi_chunk_t *p_chk )
{

}

static struct
{
    u32   i_fourcc;
    int   (*AVI_ChunkRead_function)( input_thread_t *p_input, 
                                     avi_chunk_t *p_chk,
                                     int b_seekable );
    void  (*AVI_ChunkFree_function)( input_thread_t *p_input,
                                     avi_chunk_t *p_chk );
} AVI_Chunk_Function [] =
{
    { AVIFOURCC_RIFF, AVI_ChunkRead_list, AVI_ChunkFree_nothing },
    { AVIFOURCC_LIST, AVI_ChunkRead_list, AVI_ChunkFree_nothing },
    { AVIFOURCC_avih, AVI_ChunkRead_avih, AVI_ChunkFree_nothing },
    { AVIFOURCC_strh, AVI_ChunkRead_strh, AVI_ChunkFree_nothing },
    { AVIFOURCC_strf, AVI_ChunkRead_strf, AVI_ChunkFree_strf },
    { AVIFOURCC_strd, AVI_ChunkRead_strd, AVI_ChunkFree_nothing },
    { AVIFOURCC_idx1, AVI_ChunkRead_idx1, AVI_ChunkFree_idx1 },
    { AVIFOURCC_JUNK, AVI_ChunkRead_nothing, AVI_ChunkFree_nothing },

    { AVIFOURCC_IARL, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_IARL, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_IART, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_ICMS, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_ICMT, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_ICOP, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_ICRD, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_ICRP, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_IDIM, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_IDPI, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_IENG, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_IGNR, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_IKEY, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_ILGT, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_IMED, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_INAM, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_IPLT, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_IPRD, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_ISBJ, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_ISFT, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_ISHP, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_ISRC, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_ISRF, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_ITCH, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_ISMP, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_IDIT, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { 0,           NULL,               NULL }
};

static int AVI_ChunkFunctionFind( int i_fourcc )
{
    int i_index;
    for( i_index = 0; ; i_index++ )
    {
        if( ( AVI_Chunk_Function[i_index].i_fourcc == i_fourcc )||
            ( AVI_Chunk_Function[i_index].i_fourcc == 0 ) )
        {
            return( i_index );
        }
    }
}

int  _AVI_ChunkRead( input_thread_t *p_input,
                     avi_chunk_t *p_chk,
                     avi_chunk_t *p_father,
                     int b_seekable )
{
    int i_index;
    int i_result;


    if( !p_chk )
    {
        return( 0 );
    }

    if( !AVI_ChunkReadCommon( p_input, p_chk ) )
    {
        msg_Warn( p_input, "cannot read one chunk" );
        return( 0 );
    }
    p_chk->common.p_father = p_father;

    i_index = AVI_ChunkFunctionFind( p_chk->common.i_chunk_fourcc );
    if( AVI_Chunk_Function[i_index].AVI_ChunkRead_function )
    {
        i_result = 
            AVI_Chunk_Function[i_index].AVI_ChunkRead_function( p_input,
                                                                p_chk,
                                                                b_seekable );
    }
    else
    {
        msg_Warn( p_input, "unknown chunk (not loaded)" );
        i_result = AVI_NextChunk( p_input, p_chk );
    }

    return( i_result );
}

void _AVI_ChunkFree( input_thread_t *p_input,
                     avi_chunk_t *p_chk )
{
    int i_index;
    avi_chunk_t *p_child, *p_next;

    if( !p_chk )
    {
        return;
    }
    
    /* Free all child chunk */
    p_child = p_chk->common.p_first;
    while( p_child )
    {
        p_next = p_child->common.p_next;
        AVI_ChunkFree( p_input, p_child );
        free( p_child );
        p_child = p_next;
    }

    i_index = AVI_ChunkFunctionFind( p_chk->common.i_chunk_fourcc );
    if( AVI_Chunk_Function[i_index].AVI_ChunkFree_function )
    {
#ifdef AVI_DEBUG
        msg_Dbg( p_input, "free chunk %c%c%c%c", 
                 AVIFOURCC_PRINT( p_chk->common.i_chunk_fourcc ) );
#endif
        AVI_Chunk_Function[i_index].AVI_ChunkFree_function( p_input, p_chk);
    }
    else
    {
        msg_Warn( p_input, "unknown chunk (not unloaded)" );
    }
    p_chk->common.p_first = NULL;
    p_chk->common.p_last  = NULL;
    
    return;
}

int AVI_ChunkReadRoot( input_thread_t *p_input,
                       avi_chunk_t *p_root,
                       int b_seekable )
{
    avi_chunk_list_t *p_list = (avi_chunk_list_t*)p_root;
    avi_chunk_t      *p_chk;
    
    p_list->i_chunk_pos  = 0;
    p_list->i_chunk_size = p_input->stream.p_selected_area->i_size;
    p_list->i_chunk_fourcc = AVIFOURCC_LIST;
    p_list->p_father = NULL;
    p_list->p_next  = NULL;
    p_list->p_first = NULL;
    p_list->p_last  = NULL;

    p_list->i_type = MKFOURCC( 'r', 'o', 'o', 't' );
    
    for( ; ; )
    {
        p_chk = malloc( sizeof( avi_chunk_t ) );
        memset( p_chk, 0, sizeof( avi_chunk_t ) );
        if( !p_root->common.p_first )
        {
            p_root->common.p_first = p_chk;
        }
        else
        {
            p_root->common.p_last->common.p_next = p_chk;
        }
        p_root->common.p_last = p_chk;

        if( !AVI_ChunkRead( p_input, p_chk, p_root, b_seekable ) ||
           ( AVI_TellAbsolute( p_input ) >=
                p_chk->common.p_father->common.i_chunk_pos + 
                    __EVEN( p_chk->common.p_father->common.i_chunk_size ) ) )
        {
            break;
        }
        /* If we can't seek then stop when we 've found first RIFF-AVI */
        if( p_chk->common.i_chunk_fourcc == AVIFOURCC_RIFF &&
            p_chk->list.i_type == AVIFOURCC_AVI && !b_seekable )
        {
            break;
        }
    } 
    
    return( 1 );
}

void AVI_ChunkFreeRoot( input_thread_t *p_input,
                        avi_chunk_t  *p_chk )
{
    AVI_ChunkFree( p_input, p_chk );
}


int  _AVI_ChunkCount( avi_chunk_t *p_chk, u32 i_fourcc )
{
    int i_count;
    avi_chunk_t *p_child;

    if( !p_chk )
    {
        return( 0 );
    }

    i_count = 0;
    p_child = p_chk->common.p_first;
    while( p_child )
    {
        if( p_child->common.i_chunk_fourcc == i_fourcc ||
            ( p_child->common.i_chunk_fourcc == AVIFOURCC_LIST && 
              p_child->list.i_type == i_fourcc ) )
        {
            i_count++;
        }
        p_child = p_child->common.p_next;
    }
    return( i_count );
}

avi_chunk_t *_AVI_ChunkFind( avi_chunk_t *p_chk, u32 i_fourcc, int i_number )
{
    avi_chunk_t *p_child;
    if( !p_chk )
    {
        return( NULL );
    }
    p_child = p_chk->common.p_first;

    while( p_child )
    {
        if( p_child->common.i_chunk_fourcc == i_fourcc ||
            ( p_child->common.i_chunk_fourcc == AVIFOURCC_LIST && 
              p_child->list.i_type == i_fourcc ) )
        {
            if( i_number == 0 )
            {
                /* We found it */
                return( p_child );
            }

            i_number--;
        }
        p_child = p_child->common.p_next;
    }
    return( NULL );
}

static void AVI_ChunkDumpDebug_level( input_thread_t *p_input,
                                      avi_chunk_t  *p_chk, int i_level )
{
    char str[1024];
    int i;
    avi_chunk_t *p_child;
    
    memset( str, ' ', sizeof( str ) );
    for( i = 1; i < i_level; i++ )
    {
        str[i * 5] = '|';
    }
    if( p_chk->common.i_chunk_fourcc == AVIFOURCC_RIFF||
        p_chk->common.i_chunk_fourcc == AVIFOURCC_LIST )
    {
        sprintf( str + i_level * 5, 
                 "%c %c%c%c%c-%c%c%c%c size:%lld pos:%lld",
                 i_level ? '+' : '*',
                 AVIFOURCC_PRINT( p_chk->common.i_chunk_fourcc ),
                 AVIFOURCC_PRINT( p_chk->list.i_type ),
                 p_chk->common.i_chunk_size,
                 p_chk->common.i_chunk_pos );
    }
    else
    {
        sprintf( str + i_level * 5, 
                 "+ %c%c%c%c size:%lld pos:%lld",
                 AVIFOURCC_PRINT( p_chk->common.i_chunk_fourcc ),
                 p_chk->common.i_chunk_size,
                 p_chk->common.i_chunk_pos );
    }
    msg_Dbg( p_input, "%s", str );

    p_child = p_chk->common.p_first;
    while( p_child )
    {
        AVI_ChunkDumpDebug_level( p_input, p_child, i_level + 1 );
        p_child = p_child->common.p_next;
    }
}
void _AVI_ChunkDumpDebug( input_thread_t *p_input,
                         avi_chunk_t  *p_chk )
{
    AVI_ChunkDumpDebug_level( p_input, p_chk, 0 );
}

