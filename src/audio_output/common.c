/*****************************************************************************
 * common.c : audio output management of common data structures
 *****************************************************************************
 * Copyright (C) 2002-2007 VLC authors and VideoLAN
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

#include <limits.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_aout.h>
#include "aout_internal.h"

/*
 * Formats management (internal and external)
 */

/*****************************************************************************
 * aout_BitsPerSample : get the number of bits per sample
 *****************************************************************************/
unsigned int aout_BitsPerSample( vlc_fourcc_t i_format )
{
    switch( vlc_fourcc_GetCodec( AUDIO_ES, i_format ) )
    {
    case VLC_CODEC_U8:
    case VLC_CODEC_S8:
    case VLC_CODEC_ALAW:
    case VLC_CODEC_MULAW:
        return 8;

    case VLC_CODEC_U16L:
    case VLC_CODEC_S16L:
    case VLC_CODEC_U16B:
    case VLC_CODEC_S16B:
        return 16;

    case VLC_CODEC_U24L:
    case VLC_CODEC_S24L:
    case VLC_CODEC_U24B:
    case VLC_CODEC_S24B:
        return 24;

    case VLC_CODEC_S24L32:
    case VLC_CODEC_S24B32:
    case VLC_CODEC_U32L:
    case VLC_CODEC_U32B:
    case VLC_CODEC_S32L:
    case VLC_CODEC_S32B:
    case VLC_CODEC_F32L:
    case VLC_CODEC_F32B:
        return 32;

    case VLC_CODEC_F64L:
    case VLC_CODEC_F64B:
        return 64;

    default:
        /* For these formats the caller has to indicate the parameters
         * by hand. */
        return 0;
    }
}

/*****************************************************************************
 * aout_FormatPrepare : compute the number of bytes per frame & frame length
 *****************************************************************************/
void aout_FormatPrepare( audio_sample_format_t * p_format )
{
    p_format->i_channels = aout_FormatNbChannels( p_format );
    p_format->i_bitspersample = aout_BitsPerSample( p_format->i_format );
    if( p_format->i_bitspersample > 0 )
    {
        p_format->i_bytes_per_frame = ( p_format->i_bitspersample / 8 )
                                    * aout_FormatNbChannels( p_format );
        p_format->i_frame_length = 1;
    }
}

/*****************************************************************************
 * aout_FormatPrintChannels : print a channel in a human-readable form
 *****************************************************************************/
const char * aout_FormatPrintChannels( const audio_sample_format_t * p_format )
{
    switch ( p_format->i_physical_channels )
    {
    case AOUT_CHAN_LEFT:
    case AOUT_CHAN_RIGHT:
    case AOUT_CHAN_CENTER:
        if ( (p_format->i_original_channels & AOUT_CHAN_CENTER)
              || (p_format->i_original_channels
                   & (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT)) )
            return "Mono";
        else if ( p_format->i_original_channels & AOUT_CHAN_LEFT )
            return "Left";
        return "Right";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT:
        if ( p_format->i_original_channels & AOUT_CHAN_REVERSESTEREO )
        {
            if ( p_format->i_original_channels & AOUT_CHAN_DOLBYSTEREO )
                return "Dolby/Reverse";
            return "Stereo/Reverse";
        }
        else
        {
            if ( p_format->i_original_channels & AOUT_CHAN_DOLBYSTEREO )
                return "Dolby";
            else if ( p_format->i_original_channels & AOUT_CHAN_DUALMONO )
                return "Dual-mono";
            else if ( p_format->i_original_channels == AOUT_CHAN_CENTER )
                return "Stereo/Mono";
            else if ( !(p_format->i_original_channels & AOUT_CHAN_RIGHT) )
                return "Stereo/Left";
            else if ( !(p_format->i_original_channels & AOUT_CHAN_LEFT) )
                return "Stereo/Right";
            return "Stereo";
        }
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER:
        return "3F";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARCENTER:
        return "2F1R";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARCENTER:
        return "3F1R";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT:
        return "2F2R";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
          | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT:
        return "2F2M";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT:
        return "3F2R";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT:
        return "3F2M";

    case AOUT_CHAN_CENTER | AOUT_CHAN_LFE:
        if ( (p_format->i_original_channels & AOUT_CHAN_CENTER)
              || (p_format->i_original_channels
                   & (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT)) )
            return "Mono/LFE";
        else if ( p_format->i_original_channels & AOUT_CHAN_LEFT )
            return "Left/LFE";
        return "Right/LFE";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_LFE:
        if ( p_format->i_original_channels & AOUT_CHAN_DOLBYSTEREO )
            return "Dolby/LFE";
        else if ( p_format->i_original_channels & AOUT_CHAN_DUALMONO )
            return "Dual-mono/LFE";
        else if ( p_format->i_original_channels == AOUT_CHAN_CENTER )
            return "Mono/LFE";
        else if ( !(p_format->i_original_channels & AOUT_CHAN_RIGHT) )
            return "Stereo/Left/LFE";
        else if ( !(p_format->i_original_channels & AOUT_CHAN_LEFT) )
            return "Stereo/Right/LFE";
         return "Stereo/LFE";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER | AOUT_CHAN_LFE:
        return "3F/LFE";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARCENTER
          | AOUT_CHAN_LFE:
        return "2F1R/LFE";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARCENTER | AOUT_CHAN_LFE:
        return "3F1R/LFE";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE:
        return "2F2R/LFE";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT
          | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_LFE:
        return "2F2M/LFE";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE:
        return "3F2R/LFE";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_LFE:
        return "3F2M/LFE";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_MIDDLELEFT
          | AOUT_CHAN_MIDDLERIGHT:
        return "3F2M2R";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_MIDDLELEFT
          | AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_LFE:
        return "3F2M2R/LFE";
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARCENTER | AOUT_CHAN_MIDDLELEFT
          | AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_LFE:
        return "3F2M1R/LFE";
    }

    return "ERROR";
}

#undef aout_FormatPrint
/**
 * Prints an audio sample format in a human-readable form.
 */
void aout_FormatPrint( vlc_object_t *obj, const char *psz_text,
                       const audio_sample_format_t *p_format )
{
    msg_Dbg( obj, "%s '%4.4s' %d Hz %s frame=%d samples/%d bytes", psz_text,
             (char *)&p_format->i_format, p_format->i_rate,
             aout_FormatPrintChannels( p_format ),
             p_format->i_frame_length, p_format->i_bytes_per_frame );
}

#undef aout_FormatsPrint
/**
 * Prints two formats in a human-readable form
 */
void aout_FormatsPrint( vlc_object_t *obj, const char * psz_text,
                        const audio_sample_format_t * p_format1,
                        const audio_sample_format_t * p_format2 )
{
    msg_Dbg( obj, "%s '%4.4s'->'%4.4s' %d Hz->%d Hz %s->%s",
             psz_text,
             (char *)&p_format1->i_format, (char *)&p_format2->i_format,
             p_format1->i_rate, p_format2->i_rate,
             aout_FormatPrintChannels( p_format1 ),
             aout_FormatPrintChannels( p_format2 ) );
}

/*****************************************************************************
 * aout_CheckChannelReorder : Check if we need to do some channel re-ordering
 *****************************************************************************/
unsigned aout_CheckChannelReorder( const uint32_t *chans_in,
                                   const uint32_t *chans_out,
                                   uint32_t mask, uint8_t *restrict table )
{
    unsigned channels = 0;

    if( chans_in == NULL )
        chans_in = pi_vlc_chan_order_wg4;
    if( chans_out == NULL )
        chans_out = pi_vlc_chan_order_wg4;

    for( unsigned i = 0; chans_in[i]; i++ )
    {
        const uint32_t chan = chans_in[i];
        if( !(mask & chan) )
            continue;

        unsigned index = 0;
        for( unsigned j = 0; chan != chans_out[j]; j++ )
            if( mask & chans_out[j] )
                index++;

        table[channels++] = index;
    }

    for( unsigned i = 0; i < channels; i++ )
        if( table[i] != i )
            return channels;
    return 0;
}

/**
 * Reorders audio samples within a block of linear audio interleaved samples.
 * \param ptr start address of the block of samples
 * \param bytes size of the block in bytes (must be a multiple of the product
 *              of the channels count and the sample size)
 * \param channels channels count (also length of the chans_table table)
 * \param chans_table permutation table to reorder the channels
 *                    (usually computed by aout_CheckChannelReorder())
 * \param fourcc sample format (must be a linear sample format)
 * \note The samples must be naturally aligned in memory.
 */
void aout_ChannelReorder( void *ptr, size_t bytes, unsigned channels,
                          const uint8_t *restrict chans_table, vlc_fourcc_t fourcc )
{
    assert( channels != 0 );
    assert( channels <= AOUT_CHAN_MAX );

    /* The audio formats supported in audio output are inlined. For other
     * formats (used in demuxers and muxers), memcpy() is used to avoid
     * breaking type punning. */
#define REORDER_TYPE(type) \
do { \
    const size_t frames = (bytes / sizeof (type)) / channels; \
    type *buf = ptr; \
\
    for( size_t i = 0; i < frames; i++ ) \
    { \
        type tmp[AOUT_CHAN_MAX]; \
\
        for( size_t j = 0; j < channels; j++ ) \
            tmp[chans_table[j]] = buf[j]; \
        memcpy( buf, tmp, sizeof (type) * channels ); \
        buf += channels; \
    } \
} while(0)

    switch( fourcc )
    {
        case VLC_CODEC_U8:   REORDER_TYPE(uint8_t); break;
        case VLC_CODEC_S16N: REORDER_TYPE(int16_t); break;
        case VLC_CODEC_FL32: REORDER_TYPE(float);   break;
        case VLC_CODEC_S32N: REORDER_TYPE(int32_t); break;
        case VLC_CODEC_FL64: REORDER_TYPE(double);  break;

        default:
        {
            unsigned size = aout_BitsPerSample( fourcc ) / 8;
            const size_t frames = bytes / (size * channels);
            unsigned char *buf = ptr;

            assert( bytes != 0 );
            for( size_t i = 0; i < frames; i++ )
            {
                unsigned char tmp[AOUT_CHAN_MAX * size];

                for( size_t j = 0; j < channels; j++ )
                    memcpy( tmp + size * chans_table[j], buf + size * j, size );
                memcpy( buf, tmp, size * channels );
                buf += size * channels;
            }
            break;
        }
    }
}

/**
 * Interleaves audio samples within a block of samples.
 * \param dst destination buffer for interleaved samples
 * \param srcv source buffers (one per plane) of uninterleaved samples
 * \param samples number of samples (per channel/per plane)
 * \param chans channels/planes count
 * \param fourcc sample format (must be a linear sample format)
 * \note The samples must be naturally aligned in memory.
 * \warning Destination and source buffers MUST NOT overlap.
 */
void aout_Interleave( void *restrict dst, const void *const *srcv,
                      unsigned samples, unsigned chans, vlc_fourcc_t fourcc )
{
#define INTERLEAVE_TYPE(type) \
do { \
    type *d = dst; \
    for( size_t i = 0; i < chans; i++ ) { \
        const type *s = srcv[i]; \
        for( size_t j = 0, k = 0; j < samples; j++, k += chans ) \
            d[k] = *(s++); \
        d++; \
    } \
} while(0)

    switch( fourcc )
    {
        case VLC_CODEC_U8:   INTERLEAVE_TYPE(uint8_t);  break;
        case VLC_CODEC_S16N: INTERLEAVE_TYPE(uint16_t); break;
        case VLC_CODEC_FL32: INTERLEAVE_TYPE(float);    break;
        case VLC_CODEC_S32N: INTERLEAVE_TYPE(int32_t);  break;
        case VLC_CODEC_FL64: INTERLEAVE_TYPE(double);   break;
        default:             assert(0);
    }
#undef INTERLEAVE_TYPE
}

/**
 * Deinterleaves audio samples within a block of samples.
 * \param dst destination buffer for planar samples
 * \param src source buffer with interleaved samples
 * \param samples number of samples (per channel/per plane)
 * \param chans channels/planes count
 * \param fourcc sample format (must be a linear sample format)
 * \note The samples must be naturally aligned in memory.
 * \warning Destination and source buffers MUST NOT overlap.
 */
void aout_Deinterleave( void *restrict dst, const void *restrict src,
                      unsigned samples, unsigned chans, vlc_fourcc_t fourcc )
{
#define DEINTERLEAVE_TYPE(type) \
do { \
    type *d = dst; \
    const type *s = src; \
    for( size_t i = 0; i < chans; i++ ) { \
        for( size_t j = 0, k = 0; j < samples; j++, k += chans ) \
            *(d++) = s[k]; \
        s++; \
    } \
} while(0)

    switch( fourcc )
    {
        case VLC_CODEC_U8:   DEINTERLEAVE_TYPE(uint8_t);  break;
        case VLC_CODEC_S16N: DEINTERLEAVE_TYPE(uint16_t); break;
        case VLC_CODEC_FL32: DEINTERLEAVE_TYPE(float);    break;
        case VLC_CODEC_S32N: DEINTERLEAVE_TYPE(int32_t);  break;
        case VLC_CODEC_FL64: DEINTERLEAVE_TYPE(double);   break;
        default:             assert(0);
    }
#undef DEINTERLEAVE_TYPE
}

/*****************************************************************************
 * aout_ChannelExtract:
 *****************************************************************************/
static inline void ExtractChannel( uint8_t *pi_dst, int i_dst_channels,
                                   const uint8_t *pi_src, int i_src_channels,
                                   int i_sample_count,
                                   const int *pi_selection, int i_bytes )
{
    for( int i = 0; i < i_sample_count; i++ )
    {
        for( int j = 0; j < i_dst_channels; j++ )
            memcpy( &pi_dst[j * i_bytes], &pi_src[pi_selection[j] * i_bytes], i_bytes );
        pi_dst += i_dst_channels * i_bytes;
        pi_src += i_src_channels * i_bytes;
    }
}

void aout_ChannelExtract( void *p_dst, int i_dst_channels,
                          const void *p_src, int i_src_channels,
                          int i_sample_count, const int *pi_selection, int i_bits_per_sample )
{
    /* It does not work in place */
    assert( p_dst != p_src );

    /* Force the compiler to inline for the specific cases so it can optimize */
    if( i_bits_per_sample == 8 )
        ExtractChannel( p_dst, i_dst_channels, p_src, i_src_channels, i_sample_count, pi_selection, 1 );
    else  if( i_bits_per_sample == 16 )
        ExtractChannel( p_dst, i_dst_channels, p_src, i_src_channels, i_sample_count, pi_selection, 2 );
    else  if( i_bits_per_sample == 32 )
        ExtractChannel( p_dst, i_dst_channels, p_src, i_src_channels, i_sample_count, pi_selection, 4 );
    else  if( i_bits_per_sample == 64 )
        ExtractChannel( p_dst, i_dst_channels, p_src, i_src_channels, i_sample_count, pi_selection, 8 );
}

bool aout_CheckChannelExtraction( int *pi_selection,
                                  uint32_t *pi_layout, int *pi_channels,
                                  const uint32_t pi_order_dst[AOUT_CHAN_MAX],
                                  const uint32_t *pi_order_src, int i_channels )
{
    const uint32_t pi_order_dual_mono[] = { AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT };
    uint32_t i_layout = 0;
    int i_out = 0;
    int pi_index[AOUT_CHAN_MAX];

    /* */
    if( !pi_order_dst )
        pi_order_dst = pi_vlc_chan_order_wg4;

    /* Detect special dual mono case */
    if( i_channels == 2 &&
        pi_order_src[0] == AOUT_CHAN_CENTER && pi_order_src[1] == AOUT_CHAN_CENTER )
    {
        i_layout |= AOUT_CHAN_DUALMONO;
        pi_order_src = pi_order_dual_mono;
    }

    /* */
    for( int i = 0; i < i_channels; i++ )
    {
        /* Ignore unknown or duplicated channels or not present in output */
        if( !pi_order_src[i] || (i_layout & pi_order_src[i]) )
            continue;

        for( int j = 0; j < AOUT_CHAN_MAX; j++ )
        {
            if( pi_order_dst[j] == pi_order_src[i] )
            {
                assert( i_out < AOUT_CHAN_MAX );
                pi_index[i_out++] = i;
                i_layout |= pi_order_src[i];
                break;
            }
        }
    }

    /* */
    for( int i = 0, j = 0; i < AOUT_CHAN_MAX; i++ )
    {
        for( int k = 0; k < i_out; k++ )
        {
            if( pi_order_dst[i] == pi_order_src[pi_index[k]] )
            {
                pi_selection[j++] = pi_index[k];
                break;
            }
        }
    }

    *pi_layout = i_layout;
    *pi_channels = i_out;

    for( int i = 0; i < i_out; i++ )
    {
        if( pi_selection[i] != i )
            return true;
    }
    return i_out == i_channels;
}

/* Return the order in which filters should be inserted */
static int FilterOrder( const char *psz_name )
{
    static const struct {
        const char psz_name[10];
        int        i_order;
    } filter[] = {
        { "equalizer",  0 },
    };
    for( unsigned i = 0; i < ARRAY_SIZE(filter); i++ )
    {
        if( !strcmp( filter[i].psz_name, psz_name ) )
            return filter[i].i_order;
    }
    return INT_MAX;
}

/* This function will add or remove a a module from a string list (colon
 * separated). It will return true if there is a modification
 * In case p_aout is NULL, we will use configuration instead of variable */
bool aout_ChangeFilterString( vlc_object_t *p_obj, vlc_object_t *p_aout,
                              const char *psz_variable,
                              const char *psz_name, bool b_add )
{
    if( *psz_name == '\0' )
        return false;

    char *psz_list;
    if( p_aout )
    {
        psz_list = var_GetString( p_aout, psz_variable );
    }
    else
    {
        psz_list = var_CreateGetString( p_obj->p_libvlc, psz_variable );
        var_Destroy( p_obj->p_libvlc, psz_variable );
    }

    /* Split the string into an array of filters */
    int i_count = 1;
    for( char *p = psz_list; p && *p; p++ )
        i_count += *p == ':';
    i_count += b_add;

    const char **ppsz_filter = calloc( i_count, sizeof(*ppsz_filter) );
    if( !ppsz_filter )
    {
        free( psz_list );
        return false;
    }
    bool b_present = false;
    i_count = 0;
    for( char *p = psz_list; p && *p; )
    {
        char *psz_end = strchr(p, ':');
        if( psz_end )
            *psz_end++ = '\0';
        else
            psz_end = p + strlen(p);
        if( *p )
        {
            b_present |= !strcmp( p, psz_name );
            ppsz_filter[i_count++] = p;
        }
        p = psz_end;
    }
    if( b_present == b_add )
    {
        free( ppsz_filter );
        free( psz_list );
        return false;
    }

    if( b_add )
    {
        int i_order = FilterOrder( psz_name );
        int i;
        for( i = 0; i < i_count; i++ )
        {
            if( FilterOrder( ppsz_filter[i] ) > i_order )
                break;
        }
        if( i < i_count )
            memmove( &ppsz_filter[i+1], &ppsz_filter[i], (i_count - i) * sizeof(*ppsz_filter) );
        ppsz_filter[i] = psz_name;
        i_count++;
    }
    else
    {
        for( int i = 0; i < i_count; i++ )
        {
            if( !strcmp( ppsz_filter[i], psz_name ) )
                ppsz_filter[i] = "";
        }
    }
    size_t i_length = 0;
    for( int i = 0; i < i_count; i++ )
        i_length += 1 + strlen( ppsz_filter[i] );

    char *psz_new = malloc( i_length + 1 );
    *psz_new = '\0';
    for( int i = 0; i < i_count; i++ )
    {
        if( *ppsz_filter[i] == '\0' )
            continue;
        if( *psz_new )
            strcat( psz_new, ":" );
        strcat( psz_new, ppsz_filter[i] );
    }
    free( ppsz_filter );
    free( psz_list );

    if( p_aout )
        var_SetString( p_aout, psz_variable, psz_new );
    else
        config_PutPsz( p_obj, psz_variable, psz_new );
    free( psz_new );

    return true;
}
