/*****************************************************************************
 * common.c : audio output management of common data structures
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: common.c,v 1.7 2002/11/12 13:57:13 sam Exp $
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
    p_aout->output.b_starving = 1;

    vlc_object_attach( p_aout, p_parent->p_vlc );

    return p_aout;
}

/*****************************************************************************
 * aout_Delete: destroy aout structure
 *****************************************************************************/
void aout_Delete( aout_instance_t * p_aout )
{
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
int aout_FormatNbChannels( audio_sample_format_t * p_format )
{
    int i_nb;

    switch ( p_format->i_channels & AOUT_CHAN_MASK )
    {
    case AOUT_CHAN_CHANNEL1:
    case AOUT_CHAN_CHANNEL2:
    case AOUT_CHAN_MONO:
        i_nb = 1;
        break;

    case AOUT_CHAN_CHANNEL:
    case AOUT_CHAN_STEREO:
    case AOUT_CHAN_DOLBY:
        i_nb = 2;
        break;

    case AOUT_CHAN_3F:
    case AOUT_CHAN_2F1R:
        i_nb = 3;
        break;

    case AOUT_CHAN_3F1R:
    case AOUT_CHAN_2F2R:
        i_nb = 4;
        break;

    case AOUT_CHAN_3F2R:
        i_nb = 5;
        break;

    default:
        i_nb = 0;
    }

    if ( p_format->i_channels & AOUT_CHAN_LFE )
        return i_nb + 1;
    else
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

    case VLC_FOURCC('f','l','3','2'):
    case VLC_FOURCC('f','i','3','2'):
        i_result = 4;
        break;

    case VLC_FOURCC('s','p','d','i'):
    case VLC_FOURCC('a','5','2',' '):
    case VLC_FOURCC('d','t','s',' '):
    default:
        /* For these formats the caller has to indicate the parameters
         * by hand. */
        return;
    }

    p_format->i_bytes_per_frame = i_result * aout_FormatNbChannels( p_format );
    p_format->i_frame_length = 1;
}

/*****************************************************************************
 * FormatPrintChannels : print a channel in a human-readable form
 *****************************************************************************/
static const char * FormatPrintChannels( int i_channels )
{
    switch ( i_channels )
    {
    case AOUT_CHAN_CHANNEL: return "CHANNEL";
    case AOUT_CHAN_CHANNEL1: return "CHANNEL1";
    case AOUT_CHAN_CHANNEL2: return "CHANNEL2";
    case AOUT_CHAN_MONO: return "MONO";
    case AOUT_CHAN_STEREO: return "STEREO";
    case AOUT_CHAN_3F: return "3F";
    case AOUT_CHAN_2F1R: return "2F1R";
    case AOUT_CHAN_3F1R: return "3F1R";
    case AOUT_CHAN_2F2R: return "2F2R";
    case AOUT_CHAN_3F2R: return "3F2R";
    case AOUT_CHAN_DOLBY: return "DOLBY";
    case AOUT_CHAN_CHANNEL | AOUT_CHAN_LFE: return "CHANNEL|LFE";
    case AOUT_CHAN_CHANNEL1 | AOUT_CHAN_LFE: return "CHANNEL1|LFE";
    case AOUT_CHAN_CHANNEL2 | AOUT_CHAN_LFE: return "CHANNEL2|LFE";
    case AOUT_CHAN_MONO | AOUT_CHAN_LFE: return "MONO|LFE";
    case AOUT_CHAN_STEREO | AOUT_CHAN_LFE: return "STEREO|LFE";
    case AOUT_CHAN_3F | AOUT_CHAN_LFE: return "3F|LFE";
    case AOUT_CHAN_2F1R | AOUT_CHAN_LFE: return "2F1R|LFE";
    case AOUT_CHAN_3F1R | AOUT_CHAN_LFE: return "3F1R|LFE";
    case AOUT_CHAN_2F2R | AOUT_CHAN_LFE: return "2F2R|LFE";
    case AOUT_CHAN_3F2R | AOUT_CHAN_LFE: return "3F2R|LFE";
    case AOUT_CHAN_DOLBY | AOUT_CHAN_LFE: return "DOLBY|LFE";
    }

    return "ERROR";
}

/*****************************************************************************
 * aout_FormatPrint : print a format in a human-readable form
 *****************************************************************************/
void aout_FormatPrint( aout_instance_t * p_aout, const char * psz_text,
                       const audio_sample_format_t * p_format )
{
    msg_Dbg( p_aout, "%s format='%4.4s' rate=%d channels=%s", psz_text,
             (char *)&p_format->i_format, p_format->i_rate,
             FormatPrintChannels( p_format->i_channels ) );
}

/*****************************************************************************
 * aout_FormatsPrint : print two formats in a human-readable form
 *****************************************************************************/
void aout_FormatsPrint( aout_instance_t * p_aout, const char * psz_text,
                        const audio_sample_format_t * p_format1,
                        const audio_sample_format_t * p_format2 )
{
    msg_Dbg( p_aout, "%s format='%4.4s'->'%4.4s' rate=%d->%d channels=%s->%s",
             psz_text,
             (char *)&p_format1->i_format, (char *)&p_format2->i_format,
             p_format1->i_rate, p_format2->i_rate,
             FormatPrintChannels( p_format1->i_channels ),
             FormatPrintChannels( p_format2->i_channels ) );
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
                    u32 i_rate )
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
}


/*
 * Date management (internal and external)
 */

/*****************************************************************************
 * aout_DateInit : set the divider of an audio_date_t
 *****************************************************************************/
void aout_DateInit( audio_date_t * p_date, u32 i_divider )
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
mtime_t aout_DateIncrement( audio_date_t * p_date, u32 i_nb_samples )
{
    mtime_t i_dividend = (mtime_t)i_nb_samples * 1000000;
    p_date->date += i_dividend / p_date->i_divider;
    p_date->i_remainder += i_dividend % p_date->i_divider;
    if ( p_date->i_remainder >= p_date->i_divider )
    {
        /* This is Bresenham algorithm. */
        p_date->date++;
        p_date->i_remainder -= p_date->i_divider;
    }
    return p_date->date;
}

