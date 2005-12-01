/*****************************************************************************
 * common.c : audio output management of common data structures
 *****************************************************************************
 * Copyright (C) 2002-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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
#include <stdlib.h>                            /* calloc(), malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>

#include "audio_output.h"
#include "aout_internal.h"


/*
 * Instances management (internal and external)
 */

/*****************************************************************************
 * aout_New: initialize aout structure
 *****************************************************************************/
aout_instance_t * __aout_New( vlc_object_t * p_parent )
{
    aout_instance_t * p_aout;
    vlc_value_t val;

    /* Allocate descriptor. */
    p_aout = vlc_object_create( p_parent, VLC_OBJECT_AOUT );
    if( p_aout == NULL )
    {
        return NULL;
    }

    /* Initialize members. */
    vlc_mutex_init( p_parent, &p_aout->input_fifos_lock );
    vlc_mutex_init( p_parent, &p_aout->mixer_lock );
    vlc_mutex_init( p_parent, &p_aout->output_fifo_lock );
    p_aout->i_nb_inputs = 0;
    p_aout->mixer.f_multiplier = 1.0;
    p_aout->mixer.b_error = 1;
    p_aout->output.b_error = 1;
    p_aout->output.b_starving = 1;

    var_Create( p_aout, "intf-change", VLC_VAR_BOOL );
    val.b_bool = VLC_TRUE;
    var_Set( p_aout, "intf-change", val );

    return p_aout;
}

/*****************************************************************************
 * aout_Delete: destroy aout structure
 *****************************************************************************/
void aout_Delete( aout_instance_t * p_aout )
{
    var_Destroy( p_aout, "intf-change" );

    vlc_mutex_destroy( &p_aout->input_fifos_lock );
    vlc_mutex_destroy( &p_aout->mixer_lock );
    vlc_mutex_destroy( &p_aout->output_fifo_lock );

    /* Free structure. */
    vlc_object_destroy( p_aout );
}


/*
 * Formats management (internal and external)
 */

/*****************************************************************************
 * aout_FormatNbChannels : return the number of channels
 *****************************************************************************/
unsigned int aout_FormatNbChannels( const audio_sample_format_t * p_format )
{
    static const uint32_t pi_channels[] =
        { AOUT_CHAN_CENTER, AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT,
          AOUT_CHAN_REARCENTER, AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT,
          AOUT_CHAN_MIDDLELEFT, AOUT_CHAN_MIDDLERIGHT, AOUT_CHAN_LFE };
    unsigned int i_nb = 0, i;

    for ( i = 0; i < sizeof(pi_channels)/sizeof(uint32_t); i++ )
    {
        if ( p_format->i_physical_channels & pi_channels[i] ) i_nb++;
    }

    return i_nb;
}

/*****************************************************************************
 * aout_FormatPrepare : compute the number of bytes per frame & frame length
 *****************************************************************************/
void aout_FormatPrepare( audio_sample_format_t * p_format )
{
    int i_result;

    switch ( p_format->i_format )
    {
    case VLC_FOURCC('u','8',' ',' '):
    case VLC_FOURCC('s','8',' ',' '):
        i_result = 1;
        break;

    case VLC_FOURCC('u','1','6','l'):
    case VLC_FOURCC('s','1','6','l'):
    case VLC_FOURCC('u','1','6','b'):
    case VLC_FOURCC('s','1','6','b'):
        i_result = 2;
        break;

    case VLC_FOURCC('u','2','4','l'):
    case VLC_FOURCC('s','2','4','l'):
    case VLC_FOURCC('u','2','4','b'):
    case VLC_FOURCC('s','2','4','b'):
        i_result = 3;
        break;

    case VLC_FOURCC('f','l','3','2'):
    case VLC_FOURCC('f','i','3','2'):
        i_result = 4;
        break;

    case VLC_FOURCC('s','p','d','i'):
    case VLC_FOURCC('s','p','d','b'): /* Big endian spdif output */
    case VLC_FOURCC('a','5','2',' '):
    case VLC_FOURCC('d','t','s',' '):
    case VLC_FOURCC('m','p','g','a'):
    case VLC_FOURCC('m','p','g','3'):
    default:
        /* For these formats the caller has to indicate the parameters
         * by hand. */
        return;
    }

    p_format->i_bytes_per_frame = i_result * aout_FormatNbChannels( p_format );
    p_format->i_frame_length = 1;
}

/*****************************************************************************
 * aout_FormatPrintChannels : print a channel in a human-readable form
 *****************************************************************************/
const char * aout_FormatPrintChannels( const audio_sample_format_t * p_format )
{
    switch ( p_format->i_physical_channels & AOUT_CHAN_PHYSMASK )
    {
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
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT:
        return "3F2R";

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
    case AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
          | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE:
        return "3F2R/LFE";
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

/*****************************************************************************
 * aout_FormatPrint : print a format in a human-readable form
 *****************************************************************************/
void aout_FormatPrint( aout_instance_t * p_aout, const char * psz_text,
                       const audio_sample_format_t * p_format )
{
    msg_Dbg( p_aout, "%s '%4.4s' %d Hz %s frame=%d samples/%d bytes", psz_text,
             (char *)&p_format->i_format, p_format->i_rate,
             aout_FormatPrintChannels( p_format ),
             p_format->i_frame_length, p_format->i_bytes_per_frame );
}

/*****************************************************************************
 * aout_FormatsPrint : print two formats in a human-readable form
 *****************************************************************************/
void aout_FormatsPrint( aout_instance_t * p_aout, const char * psz_text,
                        const audio_sample_format_t * p_format1,
                        const audio_sample_format_t * p_format2 )
{
    msg_Dbg( p_aout, "%s '%4.4s'->'%4.4s' %d Hz->%d Hz %s->%s",
             psz_text,
             (char *)&p_format1->i_format, (char *)&p_format2->i_format,
             p_format1->i_rate, p_format2->i_rate,
             aout_FormatPrintChannels( p_format1 ),
             aout_FormatPrintChannels( p_format2 ) );
}


/*
 * FIFO management (internal) - please understand that solving race conditions
 * is _your_ job, ie. in the audio output you should own the mixer lock
 * before calling any of these functions.
 */

/*****************************************************************************
 * aout_FifoInit : initialize the members of a FIFO
 *****************************************************************************/
void aout_FifoInit( aout_instance_t * p_aout, aout_fifo_t * p_fifo,
                    uint32_t i_rate )
{
    p_fifo->p_first = NULL;
    p_fifo->pp_last = &p_fifo->p_first;
    aout_DateInit( &p_fifo->end_date, i_rate );
}

/*****************************************************************************
 * aout_FifoPush : push a packet into the FIFO
 *****************************************************************************/
void aout_FifoPush( aout_instance_t * p_aout, aout_fifo_t * p_fifo,
                    aout_buffer_t * p_buffer )
{
    *p_fifo->pp_last = p_buffer;
    p_fifo->pp_last = &p_buffer->p_next;
    *p_fifo->pp_last = NULL;
    /* Enforce the continuity of the stream. */
    if ( aout_DateGet( &p_fifo->end_date ) )
    {
        p_buffer->start_date = aout_DateGet( &p_fifo->end_date );
        p_buffer->end_date = aout_DateIncrement( &p_fifo->end_date,
                                                 p_buffer->i_nb_samples );
    }
    else
    {
        aout_DateSet( &p_fifo->end_date, p_buffer->end_date );
    }
}

/*****************************************************************************
 * aout_FifoSet : set end_date and trash all buffers (because they aren't
 * properly dated)
 *****************************************************************************/
void aout_FifoSet( aout_instance_t * p_aout, aout_fifo_t * p_fifo,
                   mtime_t date )
{
    aout_buffer_t * p_buffer;

    aout_DateSet( &p_fifo->end_date, date );
    p_buffer = p_fifo->p_first;
    while ( p_buffer != NULL )
    {
        aout_buffer_t * p_next = p_buffer->p_next;
        aout_BufferFree( p_buffer );
        p_buffer = p_next;
    }
    p_fifo->p_first = NULL;
    p_fifo->pp_last = &p_fifo->p_first;
}

/*****************************************************************************
 * aout_FifoMoveDates : Move forwards or backwards all dates in the FIFO
 *****************************************************************************/
void aout_FifoMoveDates( aout_instance_t * p_aout, aout_fifo_t * p_fifo,
                         mtime_t difference )
{
    aout_buffer_t * p_buffer;

    aout_DateMove( &p_fifo->end_date, difference );
    p_buffer = p_fifo->p_first;
    while ( p_buffer != NULL )
    {
        p_buffer->start_date += difference;
        p_buffer->end_date += difference;
        p_buffer = p_buffer->p_next;
    }
}

/*****************************************************************************
 * aout_FifoNextStart : return the current end_date
 *****************************************************************************/
mtime_t aout_FifoNextStart( aout_instance_t * p_aout, aout_fifo_t * p_fifo )
{
    return aout_DateGet( &p_fifo->end_date );
}

/*****************************************************************************
 * aout_FifoFirstDate : return the playing date of the first buffer in the
 * FIFO
 *****************************************************************************/
mtime_t aout_FifoFirstDate( aout_instance_t * p_aout, aout_fifo_t * p_fifo )
{
    return p_fifo->p_first ?  p_fifo->p_first->start_date : 0;
}

/*****************************************************************************
 * aout_FifoPop : get the next buffer out of the FIFO
 *****************************************************************************/
aout_buffer_t * aout_FifoPop( aout_instance_t * p_aout, aout_fifo_t * p_fifo )
{
    aout_buffer_t * p_buffer;

    p_buffer = p_fifo->p_first;
    if ( p_buffer == NULL ) return NULL;
    p_fifo->p_first = p_buffer->p_next;
    if ( p_fifo->p_first == NULL )
    {
        p_fifo->pp_last = &p_fifo->p_first;
    }

    return p_buffer;
}

/*****************************************************************************
 * aout_FifoDestroy : destroy a FIFO and its buffers
 *****************************************************************************/
void aout_FifoDestroy( aout_instance_t * p_aout, aout_fifo_t * p_fifo )
{
    aout_buffer_t * p_buffer;

    p_buffer = p_fifo->p_first;
    while ( p_buffer != NULL )
    {
        aout_buffer_t * p_next = p_buffer->p_next;
        aout_BufferFree( p_buffer );
        p_buffer = p_next;
    }

    p_fifo->p_first = NULL;
    p_fifo->pp_last = &p_fifo->p_first;
}


/*
 * Date management (internal and external)
 */

/*****************************************************************************
 * aout_DateInit : set the divider of an audio_date_t
 *****************************************************************************/
void aout_DateInit( audio_date_t * p_date, uint32_t i_divider )
{
    p_date->date = 0;
    p_date->i_divider = i_divider;
    p_date->i_remainder = 0;
}

/*****************************************************************************
 * aout_DateSet : set the date of an audio_date_t
 *****************************************************************************/
void aout_DateSet( audio_date_t * p_date, mtime_t new_date )
{
    p_date->date = new_date;
    p_date->i_remainder = 0;
}

/*****************************************************************************
 * aout_DateMove : move forwards or backwards the date of an audio_date_t
 *****************************************************************************/
void aout_DateMove( audio_date_t * p_date, mtime_t difference )
{
    p_date->date += difference;
}

/*****************************************************************************
 * aout_DateGet : get the date of an audio_date_t
 *****************************************************************************/
mtime_t aout_DateGet( const audio_date_t * p_date )
{
    return p_date->date;
}

/*****************************************************************************
 * aout_DateIncrement : increment the date and return the result, taking
 * into account rounding errors
 *****************************************************************************/
mtime_t aout_DateIncrement( audio_date_t * p_date, uint32_t i_nb_samples )
{
    mtime_t i_dividend = (mtime_t)i_nb_samples * 1000000;
    p_date->date += i_dividend / p_date->i_divider;
    p_date->i_remainder += (int)(i_dividend % p_date->i_divider);
    if ( p_date->i_remainder >= p_date->i_divider )
    {
        /* This is Bresenham algorithm. */
        p_date->date++;
        p_date->i_remainder -= p_date->i_divider;
    }
    return p_date->date;
}

/*****************************************************************************
 * aout_CheckChannelReorder : Check if we need to do some channel re-ordering
 *****************************************************************************/
int aout_CheckChannelReorder( const uint32_t *pi_chan_order_in,
                              const uint32_t *pi_chan_order_out,
                              uint32_t i_channel_mask,
                              int i_channels, int *pi_chan_table )
{
    vlc_bool_t b_chan_reorder = VLC_FALSE;
    int i, j, k, l;

    if( i_channels > AOUT_CHAN_MAX ) return VLC_FALSE;

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
        if( pi_chan_table[i] != i ) b_chan_reorder = VLC_TRUE;
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
