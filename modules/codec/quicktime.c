/*****************************************************************************
 * quicktime.c: a quicktime decoder that uses the QT library/dll
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: quicktime.c,v 1.3 2003/05/21 19:55:25 hartman Exp $
 *
 * Authors: Laurent Aimar <fenrir at via.ecp.fr>
 *          Derk-Jan Hartman <thedj at users.sf.net>
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
#include <vlc/vlc.h>
#include <vlc/aout.h>
#include <vlc/vout.h>
#include <vlc/decoder.h>
#include <vlc/input.h>

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */
#include "codecs.h"

#ifdef SYS_DARWIN
#include <QuickTime/QuickTimeComponents.h>
#include <QuickTime/Movies.h>
#include <QuickTime/ImageCodec.h>
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder    ( vlc_object_t * );

static int  RunDecoderAudio( decoder_fifo_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin();
    set_description( _("QT binary library decoder") );
    set_capability( "decoder", 10 );
    set_callbacks( OpenDecoder, NULL );

    /* create a mutex */
    var_Create( p_module->p_libvlc, "qt_mutex", VLC_VAR_MUTEX );
vlc_module_end();



#define FCC( a, b , c, d ) \
    ((uint32_t)( ((a)<<24)|((b)<<16)|((c)<<8)|(d)))

#ifdef LOADER
typedef struct OpaqueSoundConverter*    SoundConverter;
typedef unsigned long                   UnsignedFixed;
typedef uint8_t                          Byte;
typedef struct SoundComponentData {
    long                            flags;
    OSType                          format;
    short                           numChannels;
    short                           sampleSize;
    UnsignedFixed                   sampleRate;
    long                            sampleCount;
    Byte *                          buffer;
    long                            reserved;
} SoundComponentData;
#endif

typedef struct
{
    /* Input properties */
    decoder_fifo_t *p_fifo;

    /* library */
#ifdef LOADER
    HMODULE     qtml;
    ldt_fs_t    *ldt_fs;
    OSErr       (*InitializeQTML)         	( long flags );
#endif
    OSErr 	(*EnterMovies)			( void );
    OSErr 	(*ExitMovies)			( void );
    int         (*SoundConverterOpen)		( const SoundComponentData *, const SoundComponentData *, SoundConverter* );
    int         (*SoundConverterClose)		( SoundConverter );
    int         (*SoundConverterSetInfo)	( SoundConverter , OSType ,void * );
    int         (*SoundConverterGetBufferSizes) ( SoundConverter, unsigned long,
                                                  unsigned long*, unsigned long*, unsigned long* );
    int         (*SoundConverterBeginConversion)( SoundConverter );
    int         (*SoundConverterEndConversion)  ( SoundConverter, void *, unsigned long *, unsigned long *);
    int         (*SoundConverterConvertBuffer)  ( SoundConverter, const void *, unsigned long, void *,
                                                  unsigned long *,unsigned long * );
    SoundConverter      myConverter;
    SoundComponentData  InputFormatInfo, OutputFormatInfo;

    long            FramesToGet;
    unsigned int    InFrameSize;
    unsigned int    OutFrameSize;

    /* Output properties */
    aout_instance_t *   p_aout;       /* opaque */
    aout_input_t *      p_aout_input; /* opaque */
    audio_sample_format_t output_format;

    audio_date_t        date;
    mtime_t             pts;

    /* buffer */
    unsigned int        i_buffer;
    unsigned int        i_buffer_size;
    uint8_t             *p_buffer;

    uint8_t             buffer_out[1000000];    /* FIXME */
} adec_thread_t;


static int pi_channels_maps[6] =
{
    0,
    AOUT_CHAN_CENTER,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARLEFT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARLEFT
};


static uint16_t GetWBE( uint8_t *p_buff )
{
    return( (p_buff[0]<<8) + p_buff[1] );
}

static uint32_t GetDWBE( uint8_t *p_buff )
{
    return( (p_buff[0] << 24) + ( p_buff[1] <<16 ) +
            ( p_buff[2] <<8 ) + p_buff[3] );
}


static int GetPESData( uint8_t *p_buf, int i_max, pes_packet_t *p_pes )
{
    int i_copy;
    int i_count;

    data_packet_t   *p_data;

    i_count = 0;
    p_data = p_pes->p_first;
    while( p_data != NULL && i_count < i_max )
    {

        i_copy = __MIN( p_data->p_payload_end - p_data->p_payload_start,
                        i_max - i_count );

        if( i_copy > 0 )
        {
            memcpy( p_buf,
                    p_data->p_payload_start,
                    i_copy );
        }

        p_data = p_data->p_next;
        i_count += i_copy;
        p_buf   += i_copy;
    }

    if( i_count < i_max )
    {
        memset( p_buf, 0, i_max - i_count );
    }
    return( i_count );
}

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to choose.
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_fifo_t *p_fifo = (decoder_fifo_t*) p_this;

    switch( p_fifo->i_fourcc )
    {
        case VLC_FOURCC('Q','D','M','C'): /* QDesign */
        case VLC_FOURCC('Q','D','M','2'): /* QDesign* 2 */
        case VLC_FOURCC('Q','c','l','p'): /* Qualcomm Purevoice Codec */
        case VLC_FOURCC('Q','C','L','P'): /* Qualcomm Purevoice Codec */
        case VLC_FOURCC('M','A','C','3'): /* MACE3 audio decoder */
        case VLC_FOURCC('M','A','C','6'): /* MACE6 audio decoder */
        case VLC_FOURCC('f','l','3','2'): /* 32-bit Floating Point */
        case VLC_FOURCC('f','l','6','4'): /* 64-bit Floating Point */
        case VLC_FOURCC('i','n','2','4'): /* 24-bit Interger */
        case VLC_FOURCC('i','n','3','2'): /* 32-bit Integer */
        case VLC_FOURCC('m','p','4','a'): /* MPEG-4 Audio */
        case VLC_FOURCC('d','v','c','a'): /* DV Audio */
        case VLC_FOURCC('s','o','w','t'): /* 16-bit Little Endian */
        case VLC_FOURCC('t','w','o','s'): /* 16-bit Big Endian */
        case VLC_FOURCC('a','l','a','w'): /* ALaw 2:1 */
        case VLC_FOURCC('u','l','a','w'): /* mu-Law 2:1 */
        case VLC_FOURCC('r','a','w',' '): /* 8-bit offset binaries */
	case 0x31:				/* MS GSM */
	case 0x32:				/* MSN Audio */
	case 0x0011:				/* DVI IMA */
	case 0x6D730002:			/* Microsoft ADPCM-ACM */
	case 0x6D730011:			/* DVI Intel IMAADPCM-ACM */

            p_fifo->pf_run = RunDecoderAudio;
            return VLC_SUCCESS;
        default:
            return VLC_EGENERIC;
    }
}

/****************************************************************************
 ****************************************************************************
 **
 **     audio part
 **
 **************************************************************************** 
 ****************************************************************************/

static int  InitThreadAudio     ( adec_thread_t * );
static void DecodeThreadAudio   ( adec_thread_t * );
static void EndThreadAudio      ( adec_thread_t * );

static int  RunDecoderAudio( decoder_fifo_t *p_fifo )
{
    adec_thread_t *p_dec;
    vlc_bool_t    b_error;

    p_dec = malloc( sizeof( adec_thread_t ) );
    if( !p_dec )
    {
        msg_Err( p_fifo, "out of memory" );
        DecoderError( p_fifo );
        return VLC_EGENERIC;
    }
    p_dec->p_fifo = p_fifo;

    if( InitThreadAudio( p_dec ) != 0 )
    {
        DecoderError( p_fifo );
        return VLC_EGENERIC;
    }

    while( !p_dec->p_fifo->b_die && !p_dec->p_fifo->b_error )
    {
        DecodeThreadAudio( p_dec );
    }


    if( ( b_error = p_dec->p_fifo->b_error ) )
    {
        DecoderError( p_dec->p_fifo );
    }

    EndThreadAudio( p_dec );
    if( b_error )
    {
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static int  InitThreadAudio     ( adec_thread_t *p_dec )
{
    vlc_value_t     lockval;
    int             i_error;
    char            fcc[4];
    unsigned long   WantedBufferSize;
    unsigned long   InputBufferSize = 0;
    unsigned long   OutputBufferSize = 0;

    WAVEFORMATEX    *p_wf;

    if( !( p_wf = (WAVEFORMATEX*)p_dec->p_fifo->p_waveformatex ) )
    {
        msg_Err( p_dec->p_fifo, "missing WAVEFORMATEX");
        return VLC_EGENERIC;
    }
    memcpy( fcc, &p_dec->p_fifo->i_fourcc, 4 );

    /* get lock, avoid segfault */
    var_Get( p_dec->p_fifo->p_libvlc, "qt_mutex", &lockval );
    vlc_mutex_lock( lockval.p_address );
#ifdef SYS_DARWIN
    EnterMovies();
#else
    p_dec->ldt_fs = Setup_LDT_Keeper();

    msg_Dbg( p_dec->p_fifo, "trying to load `qtmlClient.dll'" );
    if( !( p_dec->qtml = LoadLibraryA("qtmlClient.dll") ) )
    {
        msg_Err( p_dec->p_fifo, "cannot load qtmlClient.dll");
        goto exit_error;
    }

    msg_Dbg( p_dec->p_fifo, "qtmlClient.dll loaded" );

    /* (void*) to shut up gcc */
    p_dec->InitializeQTML           = (void*)InitializeQTML;
#endif
    p_dec->SoundConverterOpen       = (void*)SoundConverterOpen;
    p_dec->SoundConverterClose      = (void*)SoundConverterClose;
    p_dec->SoundConverterSetInfo    = (void*)SoundConverterSetInfo;
    p_dec->SoundConverterGetBufferSizes = (void*)SoundConverterGetBufferSizes;
    p_dec->SoundConverterConvertBuffer  = (void*)SoundConverterConvertBuffer;
    p_dec->SoundConverterBeginConversion= (void*)SoundConverterBeginConversion;
    p_dec->SoundConverterEndConversion  = (void*)SoundConverterEndConversion;

#ifndef SYS_DARWIN
    if( !p_dec->InitializeQTML ||
        !p_dec->SoundConverterOpen || !p_dec->SoundConverterClose ||
        !p_dec->SoundConverterSetInfo || !p_dec->SoundConverterGetBufferSizes ||
        !p_dec->SoundConverterConvertBuffer ||
        !p_dec->SoundConverterBeginConversion || !p_dec->SoundConverterEndConversion )
    {
        msg_Err( p_dec->p_fifo, "error getting qtmlClient.dll symbols");
        goto exit_error;
    }

    if( ( i_error = p_dec->InitializeQTML( 6 + 16 ) ) )
    {
        msg_Dbg( p_dec->p_fifo, "error while InitializeQTML = %d", i_error );
        goto exit_error;
    }
#endif

    /* input format settings */
    p_dec->InputFormatInfo.flags       = 0;
    p_dec->InputFormatInfo.sampleCount = 0;
    p_dec->InputFormatInfo.buffer      = NULL;
    p_dec->InputFormatInfo.reserved    = 0;
    p_dec->InputFormatInfo.numChannels = p_wf->nChannels;
    p_dec->InputFormatInfo.sampleSize  = p_wf->wBitsPerSample;
    p_dec->InputFormatInfo.sampleRate  = p_wf->nSamplesPerSec;
    p_dec->InputFormatInfo.format      = FCC( fcc[0], fcc[1], fcc[2], fcc[3] );

    /* output format settings */
    p_dec->OutputFormatInfo.flags       = 0;
    p_dec->OutputFormatInfo.sampleCount = 0;
    p_dec->OutputFormatInfo.buffer      = NULL;
    p_dec->OutputFormatInfo.reserved    = 0;
    p_dec->OutputFormatInfo.numChannels = p_wf->nChannels;
    p_dec->OutputFormatInfo.sampleSize  = 16;
    p_dec->OutputFormatInfo.sampleRate  = p_wf->nSamplesPerSec;
    p_dec->OutputFormatInfo.format      = FCC( 'N', 'O', 'N', 'E' );

#ifdef SYS_DARWIN
/* on OS X QT is not threadsafe */
    vlc_mutex_lock( &p_dec->p_fifo->p_vlc->quicktime_lock );
#endif

    i_error = p_dec->SoundConverterOpen( &p_dec->InputFormatInfo,
                                         &p_dec->OutputFormatInfo,
                                         &p_dec->myConverter );
#ifdef SYS_DARWIN
    vlc_mutex_unlock( &p_dec->p_fifo->p_vlc->quicktime_lock );
#endif

    if( i_error )
    {
        msg_Dbg( p_dec->p_fifo, "error while SoundConverterOpen = %d", i_error );
        goto exit_error;
    }

    if( p_wf->cbSize > 36 + 8 )
    {
        i_error = p_dec->SoundConverterSetInfo( p_dec->myConverter,
                                                FCC( 'w', 'a', 'v', 'e' ),
                                                ((uint8_t*)&p_wf[1]) + 36 + 8 );
        msg_Dbg( p_dec->p_fifo, "error while SoundConverterSetInfo = %d", i_error );
    }

    WantedBufferSize   = p_dec->OutputFormatInfo.numChannels * p_dec->OutputFormatInfo.sampleRate * 2;
    p_dec->FramesToGet = 0;
    i_error = p_dec->SoundConverterGetBufferSizes( p_dec->myConverter,
                                                   WantedBufferSize, &p_dec->FramesToGet,
                                                   &InputBufferSize, &OutputBufferSize );

    msg_Dbg( p_dec->p_fifo, "WantedBufferSize=%li InputBufferSize=%li OutputBufferSize=%li FramesToGet=%li",
             WantedBufferSize, InputBufferSize, OutputBufferSize, p_dec->FramesToGet );

    p_dec->InFrameSize  = (InputBufferSize + p_dec->FramesToGet - 1 ) / p_dec->FramesToGet;
    p_dec->OutFrameSize = OutputBufferSize / p_dec->FramesToGet;

    msg_Dbg( p_dec->p_fifo, "frame size %d -> %d", p_dec->InFrameSize, p_dec->OutFrameSize );

    i_error = p_dec->SoundConverterBeginConversion( p_dec->myConverter );
    if( i_error )
    {
        msg_Err( p_dec->p_fifo, "error while SoundConverterBeginConversion = %d", i_error );
        goto exit_error;
    }

    p_dec->output_format.i_format   = AOUT_FMT_S16_NE;
    p_dec->output_format.i_rate     = p_wf->nSamplesPerSec;
    p_dec->output_format.i_physical_channels =
        p_dec->output_format.i_original_channels =
            pi_channels_maps[p_wf->nChannels];
    aout_DateInit( &p_dec->date, p_dec->output_format.i_rate );
    p_dec->p_aout_input = aout_DecNew( p_dec->p_fifo,
                                       &p_dec->p_aout,
                                       &p_dec->output_format );
    if( !p_dec->p_aout_input )
    {
        msg_Err( p_dec->p_fifo, "cannot create aout" );
        goto exit_error;
    }

    p_dec->i_buffer      = 0;
    p_dec->i_buffer_size = 100*1000;
    p_dec->p_buffer      = malloc( p_dec->i_buffer_size );

    p_dec->pts = -1;

    vlc_mutex_unlock( lockval.p_address );
    return VLC_SUCCESS;

exit_error:
#ifndef SYS_DARWIN
    Restore_LDT_Keeper( p_dec->ldt_fs );
#endif
    vlc_mutex_unlock( lockval.p_address );
    return VLC_EGENERIC;

}
static void DecodeThreadAudio   ( adec_thread_t *p_dec )
{
    pes_packet_t    *p_pes;
    vlc_value_t     lockval;
    int             i_error;

    var_Get( p_dec->p_fifo->p_libvlc, "qt_mutex", &lockval );

    input_ExtractPES( p_dec->p_fifo, &p_pes );
    if( !p_pes )
    {
        msg_Err( p_dec->p_fifo, "cannot get PES" );
        p_dec->p_fifo->b_error = 1;
        return;
    }
    /*if( p_dec->pts <= 0 )*/
    {
        p_dec->pts = p_pes->i_pts;
    }

    if( p_pes->i_pes_size > 0 && p_pes->i_pts > mdate() )
    {

        if( p_dec->i_buffer_size < p_dec->i_buffer + p_pes->i_pes_size )
        {
            p_dec->i_buffer_size = p_dec->i_buffer + p_pes->i_pes_size + 1024;
            p_dec->p_buffer = realloc( p_dec->p_buffer,
                                       p_dec->i_buffer_size );
        }

        GetPESData( &p_dec->p_buffer[p_dec->i_buffer],
                    p_dec->i_buffer_size - p_dec->i_buffer, p_pes );
        p_dec->i_buffer += p_pes->i_pes_size;

        if( p_dec->i_buffer > p_dec->InFrameSize )
        {
            int i_frames = p_dec->i_buffer / p_dec->InFrameSize;
            long i_out_frames, i_out_bytes;
            /* enougth data */

            vlc_mutex_lock( lockval.p_address );
            i_error = p_dec->SoundConverterConvertBuffer( p_dec->myConverter,
                                                          p_dec->p_buffer,
                                                          i_frames,
                                                          p_dec->buffer_out,
                                                          &i_out_frames, &i_out_bytes );
            vlc_mutex_unlock( lockval.p_address );

            /*msg_Dbg( p_dec->p_fifo, "decoded %d frames -> %ld frames (error=%d)",
                     i_frames, i_out_frames, i_error );

            msg_Dbg( p_dec->p_fifo, "decoded %ld frames = %ld bytes", i_out_frames, i_out_bytes );*/
            p_dec->i_buffer -= i_frames * p_dec->InFrameSize;
            if( p_dec->i_buffer > 0 )
            {
                memmove( &p_dec->p_buffer[0],
                         &p_dec->p_buffer[i_frames * p_dec->InFrameSize],
                         p_dec->i_buffer );
            }

            if( i_out_frames > 0 )
            {
                aout_buffer_t   *p_aout_buffer;
                uint8_t         *p_buff = p_dec->buffer_out;

                /*msg_Dbg( p_dec->p_fifo, "pts=%lld date=%lld dateget=%lld",
                         p_dec->pts, mdate(), aout_DateGet( &p_dec->date ) );*/

                if( p_dec->pts != 0 && p_dec->pts != aout_DateGet( &p_dec->date ) )
                {
                    aout_DateSet( &p_dec->date, p_dec->pts );
                }
                else if( !aout_DateGet( &p_dec->date ) )
                {
                    input_DeletePES( p_dec->p_fifo->p_packets_mgt, p_pes );
                    return;
                }

                while( i_out_frames > 0 )
                {
                    int i_frames;

                    i_frames = __MIN( i_out_frames, 1000 );
                    p_aout_buffer = aout_DecNewBuffer( p_dec->p_aout,
                                                       p_dec->p_aout_input,
                                                       i_frames );
                    if( !p_aout_buffer )
                    {
                        msg_Err( p_dec->p_fifo, "cannot get aout buffer" );
                        p_dec->p_fifo->b_error = 1;
                        return;
                    }
                    p_aout_buffer->start_date = aout_DateGet( &p_dec->date );
                    p_aout_buffer->end_date = aout_DateIncrement( &p_dec->date,
                                                                  i_frames );

                    memcpy( p_aout_buffer->p_buffer,
                            p_buff,
                            p_aout_buffer->i_nb_bytes );

                    /*msg_Dbg( p_dec->p_fifo, "==> start=%lld end=%lld date=%lld",
                             p_aout_buffer->start_date, p_aout_buffer->end_date, mdate() );*/
                    aout_DecPlay( p_dec->p_aout, p_dec->p_aout_input, p_aout_buffer );
                    /*msg_Dbg( p_dec->p_fifo, "s1=%d s2=%d", i_framesperchannels, p_aout_buffer->i_nb_samples );

                    msg_Dbg( p_dec->p_fifo, "i_nb_bytes=%d i_nb_samples*4=%d", p_aout_buffer->i_nb_bytes, p_aout_buffer->i_nb_samples * 4 );*/
                    p_buff += i_frames * 4;
                    i_out_frames -= i_frames;
                }

                p_dec->pts = -1;
            }
        }
    }

    input_DeletePES( p_dec->p_fifo->p_packets_mgt, p_pes );
}
static void EndThreadAudio      ( adec_thread_t *p_dec )
{
    vlc_value_t             lockval;
    int i_error;
    unsigned long ConvertedFrames=0;
    unsigned long ConvertedBytes=0;

    /* get lock, avoid segfault */
    var_Get( p_dec->p_fifo->p_libvlc, "qt_mutex", &lockval );
    vlc_mutex_lock( lockval.p_address );

    i_error = p_dec->SoundConverterEndConversion( p_dec->myConverter, NULL, &ConvertedFrames, &ConvertedBytes );
    msg_Dbg( p_dec->p_fifo, "SoundConverterEndConversion => %d", i_error );

    i_error = p_dec->SoundConverterClose( p_dec->myConverter );
    msg_Dbg( p_dec->p_fifo, "SoundConverterClose => %d", i_error );

    /*FreeLibrary( p_dec->qtml );
    msg_Dbg( p_dec->p_fifo, "FreeLibrary ok." ); */

    vlc_mutex_unlock( lockval.p_address );

    /*Restore_LDT_Keeper( p_dec->ldt_fs );
    msg_Dbg( p_dec->p_fifo, "Restore_LDT_Keeper" ); */
#ifdef SYS_DARWIN
    ExitMovies();
#endif
    aout_DecDelete( p_dec->p_aout, p_dec->p_aout_input );
}

/* Video part will follow when the LOADER code arrives */


