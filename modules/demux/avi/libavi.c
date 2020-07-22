/*****************************************************************************
 * libavi.c : LibAVI
 *****************************************************************************
 * Copyright (C) 2001 VLC authors and VideoLAN
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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


#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_demux.h>                                   /* stream_*, *_ES */
#include <vlc_codecs.h>                            /* VLC_BITMAPINFOHEADER */

#include "libavi.h"

#include <limits.h>

#ifndef NDEBUG
# define AVI_DEBUG 1
#endif

#define __EVEN( x ) (((x) + 1) & ~1)

static vlc_fourcc_t GetFOURCC( const uint8_t *p_buff )
{
    return VLC_FOURCC( p_buff[0], p_buff[1], p_buff[2], p_buff[3] );
}

static uint64_t AVI_ChunkSize( const avi_chunk_t *p_ck )
{
    return __EVEN(p_ck->common.i_chunk_size) + 8;
}

static uint64_t AVI_ChunkEnd( const avi_chunk_t *p_ck )
{
    return p_ck->common.i_chunk_pos + AVI_ChunkSize( p_ck );
}

/****************************************************************************
 *
 * Basics functions to manipulates chunks
 *
 ****************************************************************************/
static int AVI_ChunkReadCommon( stream_t *s, avi_chunk_t *p_chk,
                                const avi_chunk_t *p_father )
{
    const uint8_t *p_peek;

    memset( p_chk, 0, sizeof( avi_chunk_t ) );

    const uint64_t i_pos = vlc_stream_Tell( s );
    if( vlc_stream_Peek( s, &p_peek, 8 ) < 8 )
    {
        if( stream_Size( s ) > 0 && (uint64_t) stream_Size( s ) > i_pos )
            msg_Warn( s, "can't peek at %"PRIu64, i_pos );
        else
            msg_Dbg( s, "no more data at %"PRIu64, i_pos );
        return VLC_EGENERIC;
    }

    p_chk->common.i_chunk_fourcc = GetFOURCC( p_peek );
    p_chk->common.i_chunk_size   = GetDWLE( p_peek + 4 );
    p_chk->common.i_chunk_pos    = i_pos;

    if( p_chk->common.i_chunk_size >= UINT64_MAX - 8 ||
        p_chk->common.i_chunk_pos > UINT64_MAX - 8 ||
        UINT64_MAX - p_chk->common.i_chunk_pos - 8 < __EVEN(p_chk->common.i_chunk_size) )
        return VLC_EGENERIC;

    if( p_father && AVI_ChunkEnd( p_chk ) > AVI_ChunkEnd( p_father ) )
    {
        msg_Warn( s, "chunk %4.4s does not fit into parent %"PRIu64,
                  (char*)&p_chk->common.i_chunk_fourcc, AVI_ChunkEnd( p_father ) );

        /* How hard is to produce files with the correct declared size ? */
        if( p_father->common.i_chunk_fourcc != AVIFOURCC_RIFF ||
            p_father->common.p_father == NULL ||
            p_father->common.p_father->common.p_father != NULL ) /* Root > RIFF only */
            return VLC_EGENERIC;
    }

#ifdef AVI_DEBUG
    msg_Dbg( (vlc_object_t*)s,
             "found chunk, fourcc: %4.4s size:%"PRIu64" pos:%"PRIu64,
             (char*)&p_chk->common.i_chunk_fourcc,
             p_chk->common.i_chunk_size,
             p_chk->common.i_chunk_pos );
#endif
    return VLC_SUCCESS;
}

static int AVI_GotoNextChunk( stream_t *s, const avi_chunk_t *p_chk )
{
    bool b_seekable = false;
    const uint64_t i_offset = AVI_ChunkEnd( p_chk );
    if ( !vlc_stream_Control(s, STREAM_CAN_SEEK, &b_seekable) && b_seekable )
    {
        return vlc_stream_Seek( s, i_offset );
    }
    else
    {
        ssize_t i_read = i_offset - vlc_stream_Tell( s );
        return (i_read >=0 && vlc_stream_Read( s, NULL, i_read ) == i_read) ?
                    VLC_SUCCESS : VLC_EGENERIC;
    }
}

static int AVI_NextChunk( stream_t *s, avi_chunk_t *p_chk )
{
    avi_chunk_t chk;

    if( !p_chk )
    {
        if( AVI_ChunkReadCommon( s, &chk, NULL ) )
        {
            return VLC_EGENERIC;
        }
        p_chk = &chk;
    }

    return AVI_GotoNextChunk( s, p_chk );
}

/****************************************************************************
 *
 * Functions to read chunks
 *
 ****************************************************************************/
static int AVI_ChunkRead_list( stream_t *s, avi_chunk_t *p_container )
{
    avi_chunk_t *p_chk;
    const uint8_t *p_peek;
    bool b_seekable;
    int i_ret = VLC_SUCCESS;

    if( p_container->common.i_chunk_size > 0 && p_container->common.i_chunk_size < 4 )
    {
        /* empty box */
        msg_Warn( (vlc_object_t*)s, "empty list chunk" );
        return VLC_EGENERIC;
    }
    if( vlc_stream_Peek( s, &p_peek, 12 ) < 12 )
    {
        msg_Warn( (vlc_object_t*)s, "unexpected end of file while reading list chunk" );
        return VLC_EGENERIC;
    }

    vlc_stream_Control( s, STREAM_CAN_SEEK, &b_seekable );

    p_container->list.i_type = GetFOURCC( p_peek + 8 );

    /* XXX fixed for on2 hack */
    if( p_container->common.i_chunk_fourcc == AVIFOURCC_ON2 && p_container->list.i_type == AVIFOURCC_ON2f )
    {
        p_container->common.i_chunk_fourcc = AVIFOURCC_RIFF;
        p_container->list.i_type = AVIFOURCC_AVI;
    }

    if( p_container->common.i_chunk_fourcc == AVIFOURCC_LIST &&
        p_container->list.i_type == AVIFOURCC_movi )
    {
        if( !b_seekable )
            return VLC_SUCCESS;
        msg_Dbg( (vlc_object_t*)s, "skipping movi chunk" );
        return AVI_NextChunk( s, p_container ); /* points at begining of LIST-movi if not seekable */
    }

    if( vlc_stream_Read( s, NULL, 12 ) != 12 )
    {
        msg_Warn( (vlc_object_t*)s, "cannot enter chunk" );
        return VLC_EGENERIC;
    }

#ifdef AVI_DEBUG
    msg_Dbg( (vlc_object_t*)s,
             "found LIST chunk: \'%4.4s\'",
             (char*)&p_container->list.i_type );
#endif
    msg_Dbg( (vlc_object_t*)s, "<list \'%4.4s\'>", (char*)&p_container->list.i_type );

    union  avi_chunk_u **pp_append = &p_container->common.p_first;
    for( ; ; )
    {
        p_chk = calloc( 1, sizeof( avi_chunk_t ) );
        if( !p_chk )
            return VLC_EGENERIC;

        i_ret = AVI_ChunkRead( s, p_chk, p_container );
        if( i_ret )
        {
            AVI_ChunkClean( s, p_chk );
            free( p_chk );
            p_chk = NULL;
            if( i_ret != AVI_ZEROSIZED_CHUNK )
                break;
        }

        if( p_chk )
        {
            *pp_append = p_chk;
            while( *pp_append )
                pp_append = &((*pp_append)->common.p_next);
        }

        if( p_container->common.i_chunk_size > 0 &&
            vlc_stream_Tell( s ) >= AVI_ChunkEnd( p_container ) )
        {
            break;
        }

        /* If we can't seek then stop when we 've found LIST-movi */
        if( p_chk &&
            p_chk->common.i_chunk_fourcc == AVIFOURCC_LIST &&
            p_chk->list.i_type == AVIFOURCC_movi &&
            ( !b_seekable || p_chk->common.i_chunk_size == 0 ) )
        {
            break;
        }

    }
    msg_Dbg( (vlc_object_t*)s, "</list \'%4.4s\'>%x", (char*)&p_container->list.i_type, i_ret );

    if( i_ret == AVI_ZERO_FOURCC || i_ret == AVI_ZEROSIZED_CHUNK )
        return AVI_GotoNextChunk( s, p_container );

    return VLC_SUCCESS;
}

/* Allow to append indexes after starting playback */
int AVI_ChunkFetchIndexes( stream_t *s, avi_chunk_t *p_riff )
{
    avi_chunk_t *p_movi = AVI_ChunkFind( p_riff, AVIFOURCC_movi, 0, true );
    if ( !p_movi )
        return VLC_EGENERIC;

    avi_chunk_t *p_chk;
    const uint64_t i_indexpos = AVI_ChunkEnd( p_movi );
    bool b_seekable = false;
    int i_ret = VLC_SUCCESS;

    vlc_stream_Control( s, STREAM_CAN_SEEK, &b_seekable );
    if ( !b_seekable || vlc_stream_Seek( s, i_indexpos ) )
        return VLC_EGENERIC;

    union  avi_chunk_u **pp_append = &p_riff->common.p_first;
    for( ; ; )
    {
        p_chk = calloc( 1, sizeof( avi_chunk_t ) );
        if( !p_chk )
        {
            i_ret = VLC_EGENERIC;
            break;
        }

        i_ret = AVI_ChunkRead( s, p_chk, p_riff );
        if( i_ret )
        {
            AVI_ChunkClean( s, p_chk );
            free( p_chk );
            break;
        }

        *pp_append = p_chk;
        while( *pp_append )
            pp_append = &((*pp_append)->common.p_next);

        if( p_chk->common.p_father->common.i_chunk_size > 0 &&
           ( vlc_stream_Tell( s ) >
             p_chk->common.p_father->common.i_chunk_pos +
             __EVEN( p_chk->common.p_father->common.i_chunk_size ) ) )
        {
            break;
        }

        /* If we can't seek then stop when we 've found any index */
        if( p_chk->common.i_chunk_fourcc == AVIFOURCC_indx ||
            p_chk->common.i_chunk_fourcc == AVIFOURCC_idx1 )
        {
            break;
        }

    }

    return i_ret;
}

#define AVI_READCHUNK_ENTER \
    int64_t i_read = __EVEN(p_chk->common.i_chunk_size ) + 8; \
    if( i_read > 100000000 ) \
    { \
        msg_Err( s, "Big chunk ignored" ); \
        return VLC_EGENERIC; \
    } \
    uint8_t  *p_read, *p_buff;    \
    if( !( p_read = p_buff = malloc(i_read ) ) ) \
    { \
        return VLC_EGENERIC; \
    } \
    i_read = vlc_stream_Read( s, p_read, i_read ); \
    if( i_read < (int64_t)__EVEN(p_chk->common.i_chunk_size ) + 8 ) \
    { \
        free( p_buff ); \
        return VLC_EGENERIC; \
    }\
    p_read += 8; \
    i_read -= 8

#define AVI_READ( res, func, size ) \
    if( i_read < size ) { \
        free( p_buff); \
        return VLC_EGENERIC; \
    } \
    i_read -= size; \
    res = func( p_read ); \
    p_read += size \

#define AVI_READCHUNK_EXIT( code ) \
    do { \
        free( p_buff ); \
        return code; \
    } while(0)

static inline uint8_t GetB( uint8_t *ptr )
{
    return *ptr;
}

#define AVI_READ1BYTE( i_byte ) \
    AVI_READ( i_byte, GetB, 1 )

#define AVI_READ2BYTES( i_word ) \
    AVI_READ( i_word, GetWLE, 2 )

#define AVI_READ4BYTES( i_dword ) \
    AVI_READ( i_dword, GetDWLE, 4 )

#define AVI_READ8BYTES( i_qword ) \
    AVI_READ( i_qword, GetQWLE, 8 )

#define AVI_READFOURCC( i_dword ) \
    AVI_READ( i_dword, GetFOURCC, 4 )

static int AVI_ChunkRead_avih( stream_t *s, avi_chunk_t *p_chk )
{
    AVI_READCHUNK_ENTER;

    p_chk->common.i_chunk_fourcc = AVIFOURCC_avih;
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
    msg_Dbg( (vlc_object_t*)s,
             "avih: streams:%d flags:%s%s%s%s %dx%d",
             p_chk->avih.i_streams,
             p_chk->avih.i_flags&AVIF_HASINDEX?" HAS_INDEX":"",
             p_chk->avih.i_flags&AVIF_MUSTUSEINDEX?" MUST_USE_INDEX":"",
             p_chk->avih.i_flags&AVIF_ISINTERLEAVED?" IS_INTERLEAVED":"",
             p_chk->avih.i_flags&AVIF_TRUSTCKTYPE?" TRUST_CKTYPE":"",
             p_chk->avih.i_width, p_chk->avih.i_height );
#endif
    AVI_READCHUNK_EXIT( VLC_SUCCESS );
}

static int AVI_ChunkRead_strh( stream_t *s, avi_chunk_t *p_chk )
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
    msg_Dbg( (vlc_object_t*)s,
             "strh: type:%4.4s handler:0x%8.8x samplesize:%d %.2ffps",
             (char*)&p_chk->strh.i_type,
             p_chk->strh.i_handler,
             p_chk->strh.i_samplesize,
             ( p_chk->strh.i_scale ?
                (float)p_chk->strh.i_rate / (float)p_chk->strh.i_scale : -1) );
#endif

    AVI_READCHUNK_EXIT( VLC_SUCCESS );
}

static int AVI_ChunkRead_strf( stream_t *s, avi_chunk_t *p_chk )
{
    avi_chunk_t *p_strh;

    AVI_READCHUNK_ENTER;
    if( p_chk->common.p_father == NULL )
    {
        msg_Err( (vlc_object_t*)s, "malformed avi file" );
        AVI_READCHUNK_EXIT( VLC_EGENERIC );
    }
    if( !( p_strh = AVI_ChunkFind( p_chk->common.p_father, AVIFOURCC_strh, 0, false ) ) )
    {
        msg_Err( (vlc_object_t*)s, "malformed avi file" );
        AVI_READCHUNK_EXIT( p_chk->common.i_chunk_size > 0  ? VLC_EGENERIC : AVI_ZEROSIZED_CHUNK );
    }

    switch( p_strh->strh.i_type )
    {
        case( AVIFOURCC_auds ):
            p_chk->strf.auds.i_cat = AUDIO_ES;
            p_chk->strf.auds.p_wf = malloc( __MAX( p_chk->common.i_chunk_size, sizeof( WAVEFORMATEX ) ) );
            if ( !p_chk->strf.auds.p_wf )
            {
                AVI_READCHUNK_EXIT( VLC_ENOMEM );
            }
            AVI_READ2BYTES( p_chk->strf.auds.p_wf->wFormatTag );
            AVI_READ2BYTES( p_chk->strf.auds.p_wf->nChannels );
            AVI_READ4BYTES( p_chk->strf.auds.p_wf->nSamplesPerSec );
            AVI_READ4BYTES( p_chk->strf.auds.p_wf->nAvgBytesPerSec );
            AVI_READ2BYTES( p_chk->strf.auds.p_wf->nBlockAlign );
            AVI_READ2BYTES( p_chk->strf.auds.p_wf->wBitsPerSample );

            if( p_chk->strf.auds.p_wf->wFormatTag != WAVE_FORMAT_PCM
                 && p_chk->common.i_chunk_size > sizeof( WAVEFORMATEX ) )
            {
                AVI_READ2BYTES( p_chk->strf.auds.p_wf->cbSize );

                /* prevent segfault */
                if( p_chk->strf.auds.p_wf->cbSize >
                        p_chk->common.i_chunk_size - sizeof( WAVEFORMATEX ) )
                {
                    p_chk->strf.auds.p_wf->cbSize =
                        p_chk->common.i_chunk_size - sizeof( WAVEFORMATEX );
                }

                if( p_chk->strf.auds.p_wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE )
                {
                    msg_Dbg( s, "Extended header found" );
                }
            }
            else
            {
                p_chk->strf.auds.p_wf->cbSize = 0;
            }
            if( p_chk->strf.auds.p_wf->cbSize > 0 )
            {
                memcpy( &p_chk->strf.auds.p_wf[1] ,
                        p_buff + 8 + sizeof( WAVEFORMATEX ),    /*  8=fourcc+size */
                        p_chk->strf.auds.p_wf->cbSize );
            }
#ifdef AVI_DEBUG
            msg_Dbg( (vlc_object_t*)s,
                     "strf: audio:0x%4.4x channels:%d %dHz %dbits/sample %dkbps",
                     p_chk->strf.auds.p_wf->wFormatTag,
                     p_chk->strf.auds.p_wf->nChannels,
                     p_chk->strf.auds.p_wf->nSamplesPerSec,
                     p_chk->strf.auds.p_wf->wBitsPerSample,
                     p_chk->strf.auds.p_wf->nAvgBytesPerSec * 8 / 1000 );
#endif
            break;
        case( AVIFOURCC_vids ):
            p_strh->strh.i_samplesize = 0; /* XXX for ffmpeg avi file */
            p_chk->strf.vids.i_cat = VIDEO_ES;
            p_chk->strf.vids.p_bih = malloc( __MAX( p_chk->common.i_chunk_size,
                                         sizeof( *p_chk->strf.vids.p_bih ) ) );
            if ( !p_chk->strf.vids.p_bih )
            {
                AVI_READCHUNK_EXIT( VLC_ENOMEM );
            }
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
            if( p_chk->strf.vids.p_bih->biSize > p_chk->common.i_chunk_size )
            {
                p_chk->strf.vids.p_bih->biSize = p_chk->common.i_chunk_size;
            }
            if ( p_chk->common.i_chunk_size > sizeof(VLC_BITMAPINFOHEADER) )
            {
                uint64_t i_extrasize = p_chk->common.i_chunk_size - sizeof(VLC_BITMAPINFOHEADER);

                /* There's a color palette appended, set up VLC_BITMAPINFO */
                memcpy( &p_chk->strf.vids.p_bih[1],
                        p_buff + 8 + sizeof(VLC_BITMAPINFOHEADER), /* 8=fourrc+size */
                        i_extrasize );

                if ( !p_chk->strf.vids.p_bih->biClrUsed )
                {
                    if( p_chk->strf.vids.p_bih->biBitCount < 32 )
                        p_chk->strf.vids.p_bih->biClrUsed = (1 << p_chk->strf.vids.p_bih->biBitCount);
                    else
                        p_chk->strf.vids.p_bih->biBitCount = UINT16_MAX;
                }

                if( i_extrasize / sizeof(uint32_t) > UINT32_MAX )
                    p_chk->strf.vids.p_bih->biClrUsed = UINT32_MAX;
                else
                {
                    p_chk->strf.vids.p_bih->biClrUsed =
                            __MIN( i_extrasize / sizeof(uint32_t),
                                   p_chk->strf.vids.p_bih->biClrUsed );
                }

                /* stay within VLC's limits */
                p_chk->strf.vids.p_bih->biClrUsed =
                    __MIN( VIDEO_PALETTE_COLORS_MAX, p_chk->strf.vids.p_bih->biClrUsed );
            }
            else p_chk->strf.vids.p_bih->biClrUsed = 0;
#ifdef AVI_DEBUG
            msg_Dbg( (vlc_object_t*)s,
                     "strf: video:%4.4s %"PRIu32"x%"PRIu32" planes:%d %dbpp",
                     (char*)&p_chk->strf.vids.p_bih->biCompression,
                     (uint32_t)p_chk->strf.vids.p_bih->biWidth,
                     p_chk->strf.vids.p_bih->biHeight <= INT32_MAX ? p_chk->strf.vids.p_bih->biHeight
                                                                   : -1 * p_chk->strf.vids.p_bih->biHeight,
                     p_chk->strf.vids.p_bih->biPlanes,
                     p_chk->strf.vids.p_bih->biBitCount );
#endif
            break;
        case AVIFOURCC_iavs:
        case AVIFOURCC_ivas:
            p_chk->strf.common.i_cat = UNKNOWN_ES;
            break;
        case( AVIFOURCC_txts ):
            p_chk->strf.common.i_cat = SPU_ES;
            break;
        default:
            msg_Warn( (vlc_object_t*)s, "unknown stream type: %4.4s",
                    (char*)&p_strh->strh.i_type );
            p_chk->strf.common.i_cat = UNKNOWN_ES;
            break;
    }
    AVI_READCHUNK_EXIT( VLC_SUCCESS );
}
static void AVI_ChunkFree_strf( avi_chunk_t *p_chk )
{
    avi_chunk_strf_t *p_strf = (avi_chunk_strf_t*)p_chk;
    if( p_strf->common.i_cat == AUDIO_ES )
    {
        FREENULL( p_strf->auds.p_wf );
    }
    else if( p_strf->common.i_cat == VIDEO_ES )
    {
        FREENULL( p_strf->vids.p_bih );
    }
}

static int AVI_ChunkRead_strd( stream_t *s, avi_chunk_t *p_chk )
{
    if ( p_chk->common.i_chunk_size == 0 )
    {
        msg_Dbg( (vlc_object_t*)s, "Zero sized pre-JUNK section met" );
        return AVI_ZEROSIZED_CHUNK;
    }

    AVI_READCHUNK_ENTER;
    p_chk->strd.p_data = malloc( p_chk->common.i_chunk_size );
    if( p_chk->strd.p_data )
        memcpy( p_chk->strd.p_data, p_buff + 8, p_chk->common.i_chunk_size );
    AVI_READCHUNK_EXIT( p_chk->strd.p_data ? VLC_SUCCESS : VLC_EGENERIC );
}

static void AVI_ChunkFree_strd( avi_chunk_t *p_chk )
{
    free( p_chk->strd.p_data );
}

static int AVI_ChunkRead_idx1( stream_t *s, avi_chunk_t *p_chk )
{
    unsigned int i_count, i_index;

    AVI_READCHUNK_ENTER;

    i_count = __MIN( (int64_t)p_chk->common.i_chunk_size, i_read ) / 16;

    p_chk->idx1.i_entry_count = i_count;
    p_chk->idx1.i_entry_max   = i_count;
    if( i_count > 0 )
    {
        p_chk->idx1.entry = calloc( i_count, sizeof( idx1_entry_t ) );
        if( !p_chk->idx1.entry )
            AVI_READCHUNK_EXIT( VLC_EGENERIC );

        for( i_index = 0; i_index < i_count ; i_index++ )
        {
            AVI_READFOURCC( p_chk->idx1.entry[i_index].i_fourcc );
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
    msg_Dbg( (vlc_object_t*)s, "idx1: index entry:%d", i_count );
#endif
    AVI_READCHUNK_EXIT( VLC_SUCCESS );
}

static void AVI_ChunkFree_idx1( avi_chunk_t *p_chk )
{
    p_chk->idx1.i_entry_count = 0;
    p_chk->idx1.i_entry_max   = 0;
    FREENULL( p_chk->idx1.entry );
}



static int AVI_ChunkRead_indx( stream_t *s, avi_chunk_t *p_chk )
{
    unsigned int i_count, i;
    int          i_ret = VLC_SUCCESS;
    int32_t      i_dummy;
    VLC_UNUSED(i_dummy);
    avi_chunk_indx_t *p_indx = (avi_chunk_indx_t*)p_chk;

    AVI_READCHUNK_ENTER;

    AVI_READ2BYTES( p_indx->i_longsperentry );
    AVI_READ1BYTE ( p_indx->i_indexsubtype );
    AVI_READ1BYTE ( p_indx->i_indextype );
    AVI_READ4BYTES( p_indx->i_entriesinuse );

    AVI_READ4BYTES( p_indx->i_id );
    p_indx->idx.std     = NULL;
    p_indx->idx.field   = NULL;
    p_indx->idx.super   = NULL;

    if( p_indx->i_indextype == AVI_INDEX_OF_CHUNKS && p_indx->i_indexsubtype == 0 )
    {
        AVI_READ8BYTES( p_indx->i_baseoffset );
        AVI_READ4BYTES( i_dummy );

        i_count = __MIN( p_indx->i_entriesinuse, i_read / 8 );
        p_indx->i_entriesinuse = i_count;
        p_indx->idx.std = calloc( i_count, sizeof( indx_std_entry_t ) );
        if( i_count == 0 || p_indx->idx.std )
        {
            for( i = 0; i < i_count; i++ )
            {
                AVI_READ4BYTES( p_indx->idx.std[i].i_offset );
                AVI_READ4BYTES( p_indx->idx.std[i].i_size );
            }
        }
        else i_ret = VLC_EGENERIC;
    }
    else if( p_indx->i_indextype == AVI_INDEX_OF_CHUNKS && p_indx->i_indexsubtype == AVI_INDEX_2FIELD )
    {
        AVI_READ8BYTES( p_indx->i_baseoffset );
        AVI_READ4BYTES( i_dummy );

        i_count = __MIN( p_indx->i_entriesinuse, i_read / 12 );
        p_indx->i_entriesinuse = i_count;
        p_indx->idx.field = calloc( i_count, sizeof( indx_field_entry_t ) );
        if( i_count == 0 || p_indx->idx.field )
        {
            for( i = 0; i < i_count; i++ )
            {
                AVI_READ4BYTES( p_indx->idx.field[i].i_offset );
                AVI_READ4BYTES( p_indx->idx.field[i].i_size );
                AVI_READ4BYTES( p_indx->idx.field[i].i_offsetfield2 );
            }
        }
        else i_ret = VLC_EGENERIC;
    }
    else if( p_indx->i_indextype == AVI_INDEX_OF_INDEXES )
    {
        p_indx->i_baseoffset = 0;
        AVI_READ4BYTES( i_dummy );
        AVI_READ4BYTES( i_dummy );
        AVI_READ4BYTES( i_dummy );

        i_count = __MIN( p_indx->i_entriesinuse, i_read / 16 );
        p_indx->i_entriesinuse = i_count;
        p_indx->idx.super = calloc( i_count, sizeof( indx_super_entry_t ) );
        if( i_count == 0 || p_indx->idx.super )
        {
            for( i = 0; i < i_count; i++ )
            {
                AVI_READ8BYTES( p_indx->idx.super[i].i_offset );
                AVI_READ4BYTES( p_indx->idx.super[i].i_size );
                AVI_READ4BYTES( p_indx->idx.super[i].i_duration );
            }
        }
        else i_ret = VLC_EGENERIC;
    }
    else
    {
        msg_Warn( (vlc_object_t*)s, "unknown type/subtype index" );
    }

#ifdef AVI_DEBUG
    msg_Dbg( (vlc_object_t*)s, "indx: type=%d subtype=%d entry=%d",
             p_indx->i_indextype, p_indx->i_indexsubtype, p_indx->i_entriesinuse );
#endif
    AVI_READCHUNK_EXIT( i_ret );
}
static void AVI_ChunkFree_indx( avi_chunk_t *p_chk )
{
    avi_chunk_indx_t *p_indx = (avi_chunk_indx_t*)p_chk;

    FREENULL( p_indx->idx.std );
    FREENULL( p_indx->idx.field );
    FREENULL( p_indx->idx.super );
}

static int AVI_ChunkRead_vprp( stream_t *s, avi_chunk_t *p_chk )
{
    avi_chunk_vprp_t *p_vprp = (avi_chunk_vprp_t*)p_chk;

    AVI_READCHUNK_ENTER;

    AVI_READ4BYTES( p_vprp->i_video_format_token );
    AVI_READ4BYTES( p_vprp->i_video_standard );
    AVI_READ4BYTES( p_vprp->i_vertical_refresh );
    AVI_READ4BYTES( p_vprp->i_h_total_in_t );
    AVI_READ4BYTES( p_vprp->i_v_total_in_lines );
    AVI_READ4BYTES( p_vprp->i_frame_aspect_ratio );
    AVI_READ4BYTES( p_vprp->i_frame_width_in_pixels );
    AVI_READ4BYTES( p_vprp->i_frame_height_in_pixels );
    AVI_READ4BYTES( p_vprp->i_nb_fields_per_frame );
    for( unsigned i = 0; i < __MIN( p_vprp->i_nb_fields_per_frame, 2 ); i++ )
    {
        AVI_READ4BYTES( p_vprp->field_info[i].i_compressed_bm_height );
        AVI_READ4BYTES( p_vprp->field_info[i].i_compressed_bm_width );
        AVI_READ4BYTES( p_vprp->field_info[i].i_valid_bm_height );
        AVI_READ4BYTES( p_vprp->field_info[i].i_valid_bm_width );
        AVI_READ4BYTES( p_vprp->field_info[i].i_valid_bm_x_offset );
        AVI_READ4BYTES( p_vprp->field_info[i].i_valid_bm_y_offset );
        AVI_READ4BYTES( p_vprp->field_info[i].i_video_x_offset_in_t );
        AVI_READ4BYTES( p_vprp->field_info[i].i_video_y_valid_start_line );
    }

#ifdef AVI_DEBUG
    msg_Dbg( (vlc_object_t*)s, "vprp: format:%d standard:%d",
             p_vprp->i_video_format_token, p_vprp->i_video_standard );
#endif
    AVI_READCHUNK_EXIT( VLC_SUCCESS );
}

static int AVI_ChunkRead_dmlh( stream_t *s, avi_chunk_t *p_chk )
{
    avi_chunk_dmlh_t *p_dmlh = (avi_chunk_dmlh_t*)p_chk;

    AVI_READCHUNK_ENTER;

    AVI_READ4BYTES( p_dmlh->dwTotalFrames );

#ifdef AVI_DEBUG
    msg_Dbg( (vlc_object_t*)s, "dmlh: dwTotalFrames %d",
             p_dmlh->dwTotalFrames );
#endif
    AVI_READCHUNK_EXIT( VLC_SUCCESS );
}

static const struct
{
    vlc_fourcc_t i_fourcc;
    const char *psz_type;
} AVI_strz_type[] =
{
    { AVIFOURCC_IARL, "Archive location" },
    { AVIFOURCC_IART, "Artist" },
    { AVIFOURCC_ICMS, "Commisioned" },
    { AVIFOURCC_ICMT, "Comments" },
    { AVIFOURCC_ICOP, "Copyright" },
    { AVIFOURCC_ICRD, "Creation date" },
    { AVIFOURCC_ICRP, "Cropped" },
    { AVIFOURCC_IDIM, "Dimensions" },
    { AVIFOURCC_IDPI, "Dots per inch" },
    { AVIFOURCC_IENG, "Engineer" },
    { AVIFOURCC_IGNR, "Genre" },
    { AVIFOURCC_ISGN, "Secondary Genre" },
    { AVIFOURCC_IKEY, "Keywords" },
    { AVIFOURCC_ILGT, "Lightness" },
    { AVIFOURCC_IMED, "Medium" },
    { AVIFOURCC_INAM, "Name" },
    { AVIFOURCC_IPLT, "Palette setting" },
    { AVIFOURCC_IPRD, "Product" },
    { AVIFOURCC_ISBJ, "Subject" },
    { AVIFOURCC_ISFT, "Software" },
    { AVIFOURCC_ISHP, "Sharpness" },
    { AVIFOURCC_ISRC, "Source" },
    { AVIFOURCC_ISRF, "Source form" },
    { AVIFOURCC_ITCH, "Technician" },
    { AVIFOURCC_ISMP, "Time code" },
    { AVIFOURCC_IDIT, "Digitalization time" },
    { AVIFOURCC_IWRI, "Writer" },
    { AVIFOURCC_IPRO, "Producer" },
    { AVIFOURCC_ICNM, "Cinematographer" },
    { AVIFOURCC_IPDS, "Production designer" },
    { AVIFOURCC_IEDT, "Editor" },
    { AVIFOURCC_ICDS, "Costume designer" },
    { AVIFOURCC_IMUS, "Music" },
    { AVIFOURCC_ISTD, "Production studio" },
    { AVIFOURCC_IDST, "Distributor" },
    { AVIFOURCC_ICNT, "Country" },
    { AVIFOURCC_ISTR, "Starring" },
    { AVIFOURCC_IFRM, "Total number of parts" },
    { AVIFOURCC_strn, "Stream name" },
    { AVIFOURCC_IAS1, "First Language" },
    { AVIFOURCC_IAS2, "Second Language" },
    { AVIFOURCC_IAS3, "Third Language" },
    { AVIFOURCC_IAS4, "Fourth Language" },
    { AVIFOURCC_IAS5, "Fifth Language" },
    { AVIFOURCC_IAS6, "Sixth Language" },
    { AVIFOURCC_IAS7, "Seventh Language" },
    { AVIFOURCC_IAS8, "Eighth Language" },
    { AVIFOURCC_IAS9, "Ninth Language" },

    { 0,              "???" }
};

static int AVI_ChunkRead_strz( stream_t *s, avi_chunk_t *p_chk )
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
    p_strz->p_str = malloc( p_strz->i_chunk_size + 1 );
    if( !p_strz->p_type || !p_strz->p_str )
    {
        free( p_strz->p_type );
        free( p_strz->p_str );
        AVI_READCHUNK_EXIT( VLC_EGENERIC );
    }
    memcpy( p_strz->p_str, p_read, p_strz->i_chunk_size );
    p_strz->p_str[p_strz->i_chunk_size] = 0;

#ifdef AVI_DEBUG
    msg_Dbg( (vlc_object_t*)s, "%4.4s: %s : %s",
             (char*)&p_strz->i_chunk_fourcc, p_strz->p_type, p_strz->p_str);
#endif
    AVI_READCHUNK_EXIT( VLC_SUCCESS );
}
static void AVI_ChunkFree_strz( avi_chunk_t *p_chk )
{
    avi_chunk_STRING_t *p_strz = (avi_chunk_STRING_t*)p_chk;
    FREENULL( p_strz->p_type );
    FREENULL( p_strz->p_str );
}

static int AVI_ChunkRead_nothing( stream_t *s, avi_chunk_t *p_chk )
{
    return AVI_NextChunk( s, p_chk );
}
static void AVI_ChunkFree_nothing( avi_chunk_t *p_chk )
{
    VLC_UNUSED( p_chk );
}

static const struct
{
    vlc_fourcc_t i_fourcc;
    int   (*AVI_ChunkRead_function)( stream_t *s, avi_chunk_t *p_chk );
    void  (*AVI_ChunkFree_function)( avi_chunk_t *p_chk );
} AVI_Chunk_Function [] =
{
    { AVIFOURCC_RIFF, AVI_ChunkRead_list, AVI_ChunkFree_nothing },
    { AVIFOURCC_ON2,  AVI_ChunkRead_list, AVI_ChunkFree_nothing },
    { AVIFOURCC_LIST, AVI_ChunkRead_list, AVI_ChunkFree_nothing },
    { AVIFOURCC_avih, AVI_ChunkRead_avih, AVI_ChunkFree_nothing },
    { AVIFOURCC_ON2h, AVI_ChunkRead_avih, AVI_ChunkFree_nothing },
    { AVIFOURCC_strh, AVI_ChunkRead_strh, AVI_ChunkFree_nothing },
    { AVIFOURCC_strf, AVI_ChunkRead_strf, AVI_ChunkFree_strf },
    { AVIFOURCC_strd, AVI_ChunkRead_strd, AVI_ChunkFree_strd },
    { AVIFOURCC_idx1, AVI_ChunkRead_idx1, AVI_ChunkFree_idx1 },
    { AVIFOURCC_indx, AVI_ChunkRead_indx, AVI_ChunkFree_indx },
    { AVIFOURCC_vprp, AVI_ChunkRead_vprp, AVI_ChunkFree_nothing },
    { AVIFOURCC_JUNK, AVI_ChunkRead_nothing, AVI_ChunkFree_nothing },
    { AVIFOURCC_dmlh, AVI_ChunkRead_dmlh, AVI_ChunkFree_nothing },

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
    { AVIFOURCC_ISGN, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
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
    { AVIFOURCC_ILNG, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_IRTD, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_IWEB, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_IPRT, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_IWRI, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_IPRO, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_ICNM, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_IPDS, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_IEDT, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_ICDS, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_IMUS, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_ISTD, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_IDST, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_ICNT, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_ISTR, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_IFRM, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_IAS1, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_IAS2, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_IAS3, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_IAS4, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_IAS5, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_IAS6, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_IAS7, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_IAS8, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { AVIFOURCC_IAS9, AVI_ChunkRead_strz, AVI_ChunkFree_strz },


    { AVIFOURCC_strn, AVI_ChunkRead_strz, AVI_ChunkFree_strz },
    { 0,           NULL,               NULL }
};

static int AVI_ChunkFunctionFind( vlc_fourcc_t i_fourcc )
{
    unsigned int i_index;
    for( i_index = 0; ; i_index++ )
    {
        if( ( AVI_Chunk_Function[i_index].i_fourcc == i_fourcc )||
            ( AVI_Chunk_Function[i_index].i_fourcc == 0 ) )
        {
            return i_index;
        }
    }
}

int  AVI_ChunkRead( stream_t *s, avi_chunk_t *p_chk, avi_chunk_t *p_father )
{
    int i_index;

    if( !p_chk )
    {
        msg_Warn( (vlc_object_t*)s, "cannot read null chunk" );
        return VLC_EGENERIC;
    }

    if( AVI_ChunkReadCommon( s, p_chk, p_father ) )
        return VLC_EGENERIC;

    if( p_chk->common.i_chunk_fourcc == VLC_FOURCC( 0, 0, 0, 0 ) )
    {
        msg_Warn( (vlc_object_t*)s, "found null fourcc chunk (corrupted file?)" );
        return AVI_ZERO_FOURCC;
    }
    p_chk->common.p_father = p_father;

    i_index = AVI_ChunkFunctionFind( p_chk->common.i_chunk_fourcc );
    if( AVI_Chunk_Function[i_index].AVI_ChunkRead_function )
    {
        return AVI_Chunk_Function[i_index].AVI_ChunkRead_function( s, p_chk );
    }
    else if( ( ((char*)&p_chk->common.i_chunk_fourcc)[0] == 'i' &&
               ((char*)&p_chk->common.i_chunk_fourcc)[1] == 'x' ) ||
             ( ((char*)&p_chk->common.i_chunk_fourcc)[2] == 'i' &&
               ((char*)&p_chk->common.i_chunk_fourcc)[3] == 'x' ) )
    {
        p_chk->common.i_chunk_fourcc = AVIFOURCC_indx;
        return AVI_ChunkRead_indx( s, p_chk );
    }

    msg_Warn( (vlc_object_t*)s, "unknown chunk: %4.4s (not loaded)",
            (char*)&p_chk->common.i_chunk_fourcc );
    return AVI_NextChunk( s, p_chk );
}

void AVI_ChunkClean( stream_t *s,
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
        AVI_ChunkClean( s, p_child );
        free( p_child );
        p_child = p_next;
    }

    i_index = AVI_ChunkFunctionFind( p_chk->common.i_chunk_fourcc );
    if( AVI_Chunk_Function[i_index].AVI_ChunkFree_function )
    {
#ifdef AVI_DEBUG
        msg_Dbg( (vlc_object_t*)s, "free chunk %4.4s",
                 (char*)&p_chk->common.i_chunk_fourcc );
#endif
        AVI_Chunk_Function[i_index].AVI_ChunkFree_function( p_chk);
    }
    else if( p_chk->common.i_chunk_fourcc != 0 )
    {
        msg_Warn( (vlc_object_t*)s, "unknown chunk: %4.4s (not unloaded)",
                (char*)&p_chk->common.i_chunk_fourcc );
    }
    p_chk->common.p_first = NULL;

    return;
}

static void AVI_ChunkDumpDebug_level( vlc_object_t *p_obj,
                                      avi_chunk_t  *p_chk, unsigned i_level )
{
    avi_chunk_t *p_child;

    char str[512];
    if( i_level >= (sizeof(str) - 1)/4 )
        return;

    memset( str, ' ', sizeof( str ) );
    for( unsigned i = 1; i < i_level; i++ )
    {
        str[i * 4] = '|';
    }
    if( p_chk->common.i_chunk_fourcc == AVIFOURCC_RIFF ||
        p_chk->common.i_chunk_fourcc == AVIFOURCC_ON2  ||
        p_chk->common.i_chunk_fourcc == AVIFOURCC_LIST )
    {
        snprintf( &str[i_level * 4], sizeof(str) - 4*i_level,
                 "%c %4.4s-%4.4s size:%"PRIu64" pos:%"PRIu64,
                 i_level ? '+' : '*',
                 (char*)&p_chk->common.i_chunk_fourcc,
                 (char*)&p_chk->list.i_type,
                 p_chk->common.i_chunk_size,
                 p_chk->common.i_chunk_pos );
    }
    else
    {
        snprintf( &str[i_level * 4], sizeof(str) - 4*i_level,
                 "+ %4.4s size:%"PRIu64" pos:%"PRIu64,
                 (char*)&p_chk->common.i_chunk_fourcc,
                 p_chk->common.i_chunk_size,
                 p_chk->common.i_chunk_pos );
    }
    msg_Dbg( p_obj, "%s", str );

    p_child = p_chk->common.p_first;
    while( p_child )
    {
        AVI_ChunkDumpDebug_level( p_obj, p_child, i_level + 1 );
        p_child = p_child->common.p_next;
    }
}

int AVI_ChunkReadRoot( stream_t *s, avi_chunk_t *p_root )
{
    avi_chunk_list_t *p_list = (avi_chunk_list_t*)p_root;
    avi_chunk_t      *p_chk;
    bool b_seekable;

    vlc_stream_Control( s, STREAM_CAN_SEEK, &b_seekable );

    p_list->i_chunk_pos  = 0;
    p_list->i_chunk_size = ((UINT64_MAX - 12) >> 1) << 1;
    p_list->i_chunk_fourcc = AVIFOURCC_LIST;
    p_list->p_father = NULL;
    p_list->p_next  = NULL;
    p_list->p_first = NULL;

    p_list->i_type = VLC_FOURCC( 'r', 'o', 'o', 't' );

    union  avi_chunk_u **pp_append = &p_root->common.p_first;
    for( ; ; )
    {
        p_chk = calloc( 1, sizeof( avi_chunk_t ) );
        if( !p_chk )
            return VLC_EGENERIC;

        if( AVI_ChunkRead( s, p_chk, p_root ) != VLC_SUCCESS )
        {
            AVI_ChunkClean( s, p_chk );
            free( p_chk );
            break;
        }

        *pp_append = p_chk;
        while( *pp_append )
            pp_append = &((*pp_append)->common.p_next);

        if( vlc_stream_Tell( s ) >=
                 p_chk->common.p_father->common.i_chunk_pos +
                 __EVEN( p_chk->common.p_father->common.i_chunk_size ) )
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

    p_list->i_chunk_size = stream_Size( s );

    AVI_ChunkDumpDebug_level( (vlc_object_t*)s, p_root, 0 );
    return VLC_SUCCESS;
}

void AVI_ChunkFreeRoot( stream_t *s,
                        avi_chunk_t  *p_chk )
{
    AVI_ChunkClean( s, p_chk );
}


int  AVI_ChunkCount_( avi_chunk_t *p_chk, vlc_fourcc_t i_fourcc, bool b_list )
{
    if( !p_chk )
        return 0;

    int i_count = 0;
    for( avi_chunk_t *p_child = p_chk->common.p_first;
                      p_child; p_child = p_child->common.p_next )
    {
        if( b_list && p_child->list.i_type == 0 )
            continue;

        if( p_child->common.i_chunk_fourcc != i_fourcc &&
            (!b_list || p_child->list.i_type != i_fourcc) )
            continue;

        i_count++;
    }

    return i_count;
}

void *AVI_ChunkFind_( avi_chunk_t *p_chk,
                      vlc_fourcc_t i_fourcc, int i_number, bool b_list )
{
    if( !p_chk )
        return NULL;

    for( avi_chunk_t *p_child = p_chk->common.p_first;
                      p_child; p_child = p_child->common.p_next )
    {
        if( b_list && p_child->list.i_type == 0 )
            continue;

        if( p_child->common.i_chunk_fourcc != i_fourcc &&
            (!b_list || p_child->list.i_type != i_fourcc) )
            continue;

        if( i_number-- == 0 )
            return p_child; /* We found it */
    }

    return NULL;
}

