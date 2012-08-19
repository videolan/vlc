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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <limits.h>
#include <assert.h>
#include <math.h>

#include <vlc_common.h>
#include <vlc_aout.h>
#include <vlc_aout_intf.h>
#include <vlc_modules.h>
#include "aout_internal.h"
#include "libvlc.h"

/*
 * Instances management (internal and external)
 */

/* Local functions */
static void aout_Destructor( vlc_object_t * p_this );

static int var_Copy (vlc_object_t *src, const char *name, vlc_value_t prev,
                     vlc_value_t value, void *data)
{
    vlc_object_t *dst = data;

    (void) src; (void) prev;
    return var_Set (dst, name, value);
}

#undef aout_New
/*****************************************************************************
 * aout_New: initialize aout structure
 *****************************************************************************/
audio_output_t *aout_New( vlc_object_t * p_parent )
{
    audio_output_t *aout = vlc_custom_create (p_parent,
                                              sizeof (aout_instance_t),
                                              "audio output");
    if (unlikely(aout == NULL))
        return NULL;

    aout_owner_t *owner = aout_owner (aout);

    vlc_mutex_init (&owner->lock);
    owner->module = NULL;
    owner->input = NULL;

    aout->volume_set = NULL;
    aout->mute_set = NULL;
    vlc_object_set_destructor (aout, aout_Destructor);

    /*
     * Persistent audio output variables
     */
    vlc_value_t val, text;
    module_config_t *cfg;
    char *str;

    var_Create (aout, "volume", VLC_VAR_FLOAT);
    var_AddCallback (aout, "volume", var_Copy, p_parent);
    var_Create (aout, "mute", VLC_VAR_BOOL | VLC_VAR_DOINHERIT);
    var_AddCallback (aout, "mute", var_Copy, p_parent);

    /* Visualizations */
    var_Create (aout, "visual", VLC_VAR_STRING | VLC_VAR_HASCHOICE);
    text.psz_string = _("Visualizations");
    var_Change (aout, "visual", VLC_VAR_SETTEXT, &text, NULL);
    val.psz_string = (char *)"";
    text.psz_string = _("Disable");
    var_Change (aout, "visual", VLC_VAR_ADDCHOICE, &val, &text);
    val.psz_string = (char *)"spectrometer";
    text.psz_string = _("Spectrometer");
    var_Change (aout, "visual", VLC_VAR_ADDCHOICE, &val, &text);
    val.psz_string = (char *)"scope";
    text.psz_string = _("Scope");
    var_Change (aout, "visual", VLC_VAR_ADDCHOICE, &val, &text);
    val.psz_string = (char *)"spectrum";
    text.psz_string = _("Spectrum");
    var_Change (aout, "visual", VLC_VAR_ADDCHOICE, &val, &text);
    val.psz_string = (char *)"vuMeter";
    text.psz_string = _("Vu meter");
    var_Change (aout, "visual", VLC_VAR_ADDCHOICE, &val, &text);
    /* Look for goom plugin */
    if (module_exists ("goom"))
    {
        val.psz_string = (char *)"goom";
        text.psz_string = (char *)"Goom";
        var_Change (aout, "visual", VLC_VAR_ADDCHOICE, &val, &text);
    }
    /* Look for libprojectM plugin */
    if (module_exists ("projectm"))
    {
        val.psz_string = (char *)"projectm";
        text.psz_string = (char*)"projectM";
        var_Change (aout, "visual", VLC_VAR_ADDCHOICE, &val, &text);
    }
    /* Look for VSXu plugin */
    if (module_exists ("vsxu"))
    {
        val.psz_string = (char *)"vsxu";
        text.psz_string = (char*)"Vovoid VSXu";
        var_Change (aout, "visual", VLC_VAR_ADDCHOICE, &val, &text);
    }
    str = var_GetNonEmptyString (aout, "effect-list");
    if (str != NULL)
    {
        var_SetString (aout, "visual", str);
        free (str);
    }

    /* Equalizer */
    var_Create (aout, "equalizer", VLC_VAR_STRING | VLC_VAR_HASCHOICE);
    text.psz_string = _("Equalizer");
    var_Change (aout, "equalizer", VLC_VAR_SETTEXT, &text, NULL);
    val.psz_string = (char*)"";
    text.psz_string = _("Disable");
    var_Change (aout, "equalizer", VLC_VAR_ADDCHOICE, &val, &text);
    cfg = config_FindConfig (VLC_OBJECT(aout), "equalizer-preset");
    if (likely(cfg != NULL))
        for (unsigned i = 0; i < cfg->list_count; i++)
        {
            val.psz_string = cfg->list.psz[i];
            text.psz_string = vlc_gettext(cfg->list_text[i]);
            var_Change (aout, "equalizer", VLC_VAR_ADDCHOICE, &val, &text);
        }

    var_Create (aout, "audio-filter", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    text.psz_string = _("Audio filters");
    var_Change (aout, "audio-filter", VLC_VAR_SETTEXT, &text, NULL);


    var_Create (aout, "audio-visual", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    text.psz_string = _("Audio visualizations");
    var_Change (aout, "audio-visual", VLC_VAR_SETTEXT, &text, NULL);


    /* Replay gain */
    var_Create (aout, "audio-replay-gain-mode",
                VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    text.psz_string = _("Replay gain");
    var_Change (aout, "audio-replay-gain-mode", VLC_VAR_SETTEXT, &text, NULL);
    cfg = config_FindConfig (VLC_OBJECT(aout), "audio-replay-gain-mode");
    if (likely(cfg != NULL))
        for (unsigned i = 0; i < cfg->list_count; i++)
        {
            val.psz_string = cfg->list.psz[i];
            text.psz_string = vlc_gettext(cfg->list_text[i]);
            var_Change (aout, "audio-replay-gain-mode", VLC_VAR_ADDCHOICE,
                            &val, &text);
        }

    return aout;
}

void aout_Destroy (audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner (aout);

    if (owner->module != NULL)
        aout_Shutdown (aout);

    var_DelCallback (aout, "mute", var_Copy, aout->p_parent);
    var_SetFloat (aout, "volume", -1.f);
    var_DelCallback (aout, "volume", var_Copy, aout->p_parent);
    vlc_object_release (aout);
}

/*****************************************************************************
 * aout_Destructor: destroy aout structure
 *****************************************************************************/
static void aout_Destructor (vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_owner_t *owner = aout_owner (aout);

    vlc_mutex_destroy (&owner->lock);
}

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

    case VLC_CODEC_S32L:
    case VLC_CODEC_S32B:
    case VLC_CODEC_F32L:
    case VLC_CODEC_F32B:
    case VLC_CODEC_FI32:
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
int aout_CheckChannelReorder( const uint32_t *pi_chan_order_in,
                              const uint32_t *pi_chan_order_out,
                              uint32_t i_channel_mask,
                              int i_channels, int *pi_chan_table )
{
    bool b_chan_reorder = false;
    int i, j, k, l;

    if( i_channels > AOUT_CHAN_MAX )
        return false;

    if( pi_chan_order_in == NULL )
        pi_chan_order_in = pi_vlc_chan_order_wg4;
    if( pi_chan_order_out == NULL )
        pi_chan_order_out = pi_vlc_chan_order_wg4;

    for( i = 0, j = 0; pi_chan_order_in[i]; i++ )
    {
        if( !(i_channel_mask & pi_chan_order_in[i]) ) continue;

        for( k = 0, l = 0; pi_chan_order_in[i] != pi_chan_order_out[k]; k++ )
        {
            if( i_channel_mask & pi_chan_order_out[k] ) l++;
        }

        pi_chan_table[j++] = l;
    }

    for( i = 0; i < i_channels; i++ )
    {
        if( pi_chan_table[i] != i ) b_chan_reorder = true;
    }

    return b_chan_reorder;
}

/*****************************************************************************
 * aout_ChannelReorder :
 *****************************************************************************/
void aout_ChannelReorder( uint8_t *p_buf, int i_buffer,
                          int i_channels, const int *pi_chan_table,
                          int i_bits_per_sample )
{
    uint8_t p_tmp[AOUT_CHAN_MAX * 4];
    int i, j;

    if( i_bits_per_sample == 8 )
    {
        for( i = 0; i < i_buffer / i_channels; i++ )
        {
            for( j = 0; j < i_channels; j++ )
            {
                p_tmp[pi_chan_table[j]] = p_buf[j];
            }

            memcpy( p_buf, p_tmp, i_channels );
            p_buf += i_channels;
        }
    }
    else if( i_bits_per_sample == 16 )
    {
        for( i = 0; i < i_buffer / i_channels / 2; i++ )
        {
            for( j = 0; j < i_channels; j++ )
            {
                p_tmp[2 * pi_chan_table[j]]     = p_buf[2 * j];
                p_tmp[2 * pi_chan_table[j] + 1] = p_buf[2 * j + 1];
            }

            memcpy( p_buf, p_tmp, 2 * i_channels );
            p_buf += 2 * i_channels;
        }
    }
    else if( i_bits_per_sample == 24 )
    {
        for( i = 0; i < i_buffer / i_channels / 3; i++ )
        {
            for( j = 0; j < i_channels; j++ )
            {
                p_tmp[3 * pi_chan_table[j]]     = p_buf[3 * j];
                p_tmp[3 * pi_chan_table[j] + 1] = p_buf[3 * j + 1];
                p_tmp[3 * pi_chan_table[j] + 2] = p_buf[3 * j + 2];
            }

            memcpy( p_buf, p_tmp, 3 * i_channels );
            p_buf += 3 * i_channels;
        }
    }
    else if( i_bits_per_sample == 32 )
    {
        for( i = 0; i < i_buffer / i_channels / 4; i++ )
        {
            for( j = 0; j < i_channels; j++ )
            {
                p_tmp[4 * pi_chan_table[j]]     = p_buf[4 * j];
                p_tmp[4 * pi_chan_table[j] + 1] = p_buf[4 * j + 1];
                p_tmp[4 * pi_chan_table[j] + 2] = p_buf[4 * j + 2];
                p_tmp[4 * pi_chan_table[j] + 3] = p_buf[4 * j + 3];
            }

            memcpy( p_buf, p_tmp, 4 * i_channels );
            p_buf += 4 * i_channels;
        }
    }
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
    else  if( i_bits_per_sample == 24 )
        ExtractChannel( p_dst, i_dst_channels, p_src, i_src_channels, i_sample_count, pi_selection, 3 );
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
        const char *psz_name;
        int        i_order;
    } filter[] = {
        { "equalizer",  0 },
        { NULL,         INT_MAX },
    };
    for( int i = 0; filter[i].psz_name; i++ )
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



