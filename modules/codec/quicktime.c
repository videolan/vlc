/*****************************************************************************
 * quicktime.c: a quicktime decoder that uses the QT library/dll
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: quicktime.c,v 1.7 2003/06/15 22:32:06 hartman Exp $
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

/* for windows do we require Quicktime compents header? */

#ifdef LOADER
#include "w32dll/loader/qtx/qtxsdk/components.h"
#include "w32dll/loader/wine/windef.h"
#include "w32dll/loader/ldt_keeper.h"
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder    ( vlc_object_t * );

static int  RunDecoderAudio( decoder_fifo_t * );
static int  RunDecoderVideo( decoder_fifo_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin();
    set_description( _("QuickTime library decoder") );
    set_capability( "decoder", 10 );
    set_callbacks( OpenDecoder, NULL );

    /* create a mutex */
    var_Create( p_module->p_libvlc, "qt_mutex", VLC_VAR_MUTEX );
vlc_module_end();



#define FCC( a, b , c, d ) \
    ((uint32_t)( ((a)<<24)|((b)<<16)|((c)<<8)|(d)))

#ifndef SYS_DARWIN
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
#endif /* SYS_DARWIN */

typedef struct
{
    /* Input properties */
    decoder_fifo_t *p_fifo;

    /* library */
#ifndef SYS_DARWIN
#ifdef LOADER
    ldt_fs_t    *ldt_fs;
#endif /* LOADER */
    HMODULE     qtml;
    OSErr       (*InitializeQTML)         	( long flags );
#endif /* SYS_DARWIN */
    int         (*SoundConverterOpen)		( const SoundComponentData *,
                                                    const SoundComponentData *, SoundConverter* );
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

typedef struct
{
    /* Input properties */
    decoder_fifo_t *p_fifo;

    /* library */
#ifndef SYS_DARWIN
#ifdef LOADER
    ldt_fs_t          *ldt_fs;
#endif /* LOADER */
    HMODULE           qtml;
    OSErr             (*InitializeQTML)		( long flags );
#endif /* SYS_DARWIN */
    Component         (*FindNextComponent)	( Component prev, ComponentDescription* desc );
    ComponentInstance (*OpenComponent)		( Component c );
    ComponentResult   (*ImageCodecInitialize)	( ComponentInstance ci, ImageSubCodecDecompressCapabilities * cap);
    ComponentResult   (*ImageCodecGetCodecInfo)	( ComponentInstance ci,
                                                    CodecInfo *info );
    ComponentResult   (*ImageCodecPreDecompress)( ComponentInstance ci,
                                                    CodecDecompressParams * params );
    ComponentResult   (*ImageCodecBandDecompress)( ComponentInstance ci,
                                                    CodecDecompressParams * params );
    PixMapHandle      (*GetGWorldPixMap)	( GWorldPtr offscreenGWorld );
    OSErr             (*QTNewGWorldFromPtr)	( GWorldPtr *gw,
                                                    OSType pixelFormat,
                                                    const Rect *boundsRect,
                                                    CTabHandle cTable,
                                                    /*GDHandle*/ void *aGDevice, /*unused*/
                                                    GWorldFlags flags,
                                                    void *baseAddr,
                                                    long rowBytes );
    OSErr             (*NewHandleClear)		( Size byteCount );

    ComponentInstance       ci;
    Rect                    OutBufferRect;   /* the dimensions of our GWorld */
    GWorldPtr               OutBufferGWorld; /* a GWorld is some kind of
                                                description for a drawing
                                                environment */
    ImageDescriptionHandle  framedescHandle;

    CodecDecompressParams   decpar;          /* for ImageCodecPreDecompress() */
    CodecCapabilities       codeccap;        /* for decpar */


    /* Output properties */
    vout_thread_t *     p_vout;
    uint8_t *           plane;
    mtime_t             pts;

    /* buffer */
    unsigned int        i_buffer;
    uint8_t             *p_buffer;

} vdec_thread_t;

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
        case VLC_FOURCC('S','V','Q','3'): /* Sorenson v3 */
        case VLC_FOURCC('S','V','Q','1'): /* Sorenson v1 */
        case VLC_FOURCC('Z','y','G','o'):
        case VLC_FOURCC('V','P','3','1'):
        case VLC_FOURCC('3','I','V','1'):
        case VLC_FOURCC('r','l','e',' '): /* QuickTime animation (RLE) */
        case VLC_FOURCC('r','p','z','a'): /* QuickTime Apple Video */
        case VLC_FOURCC('a','z','p','r'): /* QuickTime animation (RLE) */
            p_fifo->pf_run = RunDecoderVideo;
            return VLC_SUCCESS;

        case VLC_FOURCC('Q','D','M','C'): /* QDesign */
        case VLC_FOURCC('Q','D','M','2'): /* QDesign* 2 */
        case VLC_FOURCC('Q','c','l','p'): /* Qualcomm Purevoice Codec */
        case VLC_FOURCC('Q','C','L','P'): /* Qualcomm Purevoice Codec */
        case VLC_FOURCC('M','A','C','3'): /* MACE3 audio decoder */
        case VLC_FOURCC('M','A','C','6'): /* MACE6 audio decoder */
        case VLC_FOURCC('d','v','c','a'): /* DV Audio */
        case VLC_FOURCC('s','o','w','t'): /* 16-bit Little Endian */
        case VLC_FOURCC('t','w','o','s'): /* 16-bit Big Endian */
        case VLC_FOURCC('a','l','a','w'): /* ALaw 2:1 */
        case VLC_FOURCC('u','l','a','w'): /* mu-Law 2:1 */
        case VLC_FOURCC('r','a','w',' '): /* 8-bit offset binaries */
        case VLC_FOURCC('f','l','3','2'): /* 32-bit Floating Point */
        case VLC_FOURCC('f','l','6','4'): /* 64-bit Floating Point */
        case VLC_FOURCC('i','n','2','4'): /* 24-bit Interger */
        case VLC_FOURCC('i','n','3','2'): /* 32-bit Integer */
        case 0x0011:				/* DVI IMA */
	case 0x6D730002:			/* Microsoft ADPCM-ACM */
	case 0x6D730011:			/* DVI Intel IMAADPCM-ACM */

            p_fifo->pf_run = RunDecoderAudio;
            return VLC_SUCCESS;
        default:
            return VLC_EGENERIC;
    }
}

#ifdef LOADER
HMODULE   WINAPI LoadLibraryA(LPCSTR);
FARPROC   WINAPI GetProcAddress(HMODULE,LPCSTR);
int       WINAPI FreeLibrary(HMODULE);
#endif

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

static int RunDecoderAudio( decoder_fifo_t *p_fifo )
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

static int InitThreadAudio( adec_thread_t *p_dec )
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
#ifdef LOADER
    p_dec->ldt_fs = Setup_LDT_Keeper();
#endif /* LOADER */
    msg_Dbg( p_dec->p_fifo, "trying to load `qtmlClient.dll'" );
    if( !( p_dec->qtml = LoadLibraryA("qtmlClient.dll") ) )
    {
        msg_Err( p_dec->p_fifo, "cannot load qtmlClient.dll");
        goto exit_error;
    }

    msg_Dbg( p_dec->p_fifo, "qtmlClient.dll loaded" );

    /* (void*) to shut up gcc */
    p_dec->InitializeQTML           = (void*)InitializeQTML;
#endif /* SYS_DARWIN */
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
    if( i_error )
    {
        msg_Err( p_dec->p_fifo, "error while SoundConverterOpen = %d", i_error );
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

#ifdef SYS_DARWIN
    vlc_mutex_unlock( &p_dec->p_fifo->p_vlc->quicktime_lock );
#endif

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
#ifdef LOADER
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
            /* enough data */

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

static void EndThreadAudio( adec_thread_t *p_dec )
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

#ifndef SYS_DARWIN
    FreeLibrary( p_dec->qtml );
    msg_Dbg( p_dec->p_fifo, "FreeLibrary ok." ); */
#endif
    vlc_mutex_unlock( lockval.p_address );

#ifdef LOADER
    Restore_LDT_Keeper( p_dec->ldt_fs );
    msg_Dbg( p_dec->p_fifo, "Restore_LDT_Keeper" ); */
#endif
#ifdef SYS_DARWIN
    ExitMovies();
#endif
    aout_DecDelete( p_dec->p_aout, p_dec->p_aout_input );
}


/****************************************************************************
 ****************************************************************************
 **
 **     video part
 **
 **************************************************************************** 
 ****************************************************************************/

static int  InitThreadVideo     ( vdec_thread_t * );
static void DecodeThreadVideo   ( vdec_thread_t * );
static void EndThreadVideo      ( vdec_thread_t * );

static int  RunDecoderVideo( decoder_fifo_t *p_fifo )
{
    vdec_thread_t *p_dec;
    vlc_bool_t    b_error;

    p_dec = malloc( sizeof( vdec_thread_t ) );
    if( !p_dec )
    {
        msg_Err( p_fifo, "out of memory" );
        DecoderError( p_fifo );
        return VLC_EGENERIC;
    }
    p_dec->p_fifo = p_fifo;

    if( InitThreadVideo( p_dec ) != 0 )
    {
        DecoderError( p_fifo );
        return VLC_EGENERIC;
    }

    while( !p_dec->p_fifo->b_die && !p_dec->p_fifo->b_error )
    {
        DecodeThreadVideo( p_dec );
    }


    if( ( b_error = p_dec->p_fifo->b_error ) )
    {
        DecoderError( p_dec->p_fifo );
    }

    EndThreadVideo( p_dec );
    if( b_error )
    {
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*
 * InitThreadVideo: load and init library
 *
 */
static int InitThreadVideo( vdec_thread_t *p_dec )
{
    vlc_value_t				lockval;
    long				i_result;
    ComponentDescription		desc;
    Component				prev;
    ComponentResult			cres;
    ImageSubCodecDecompressCapabilities	icap;	/* for ImageCodecInitialize() */
    CodecInfo				cinfo;	/* for ImageCodecGetCodecInfo() */
    ImageDescription			*id;

    BITMAPINFOHEADER    *p_bih;
    int                 i_vide;
    uint8_t             *p_vide;
    char                fcc[4];

    if( !( p_bih  = (BITMAPINFOHEADER*)p_dec->p_fifo->p_bitmapinfoheader ) )
    {
        msg_Err( p_dec->p_fifo, "missing BITMAPINFOHEADER !!" );
        return VLC_EGENERIC;
    }
    i_vide = p_bih->biSize - sizeof( BITMAPINFOHEADER );
    p_vide = (uint8_t*)&p_bih[1];
    if( i_vide <= 0 || p_vide == NULL )
    {
        msg_Err( p_dec->p_fifo, "invalid BITMAPINFOHEADER !!" );
        return VLC_EGENERIC;
    }
    memcpy( fcc, &p_dec->p_fifo->i_fourcc, 4 );
    msg_Dbg( p_dec->p_fifo, "quicktime_video %4.4s %dx%d", fcc, p_bih->biWidth, p_bih->biHeight );

    /* get lock, avoid segfault */
    var_Get( p_dec->p_fifo->p_libvlc, "qt_mutex", &lockval );
    vlc_mutex_lock( lockval.p_address );
#ifdef SYS_DARWIN
    EnterMovies();
#else
#ifdef LOADER
    p_dec->ldt_fs = Setup_LDT_Keeper();
#endif /* LOADER */
    msg_Dbg( p_dec->p_fifo, "trying to load `qtmlClient.dll'" );
    if( !( p_dec->qtml = LoadLibraryA("qtmlClient.dll") ) )
    {
        msg_Err( p_dec->p_fifo, "cannot load qtmlClient.dll");
        goto exit_error;
    }

    msg_Dbg( p_dec->p_fifo, "qtmlClient.dll loaded" );

    /* (void*) to shut up gcc */
    p_dec->InitializeQTML           = (void*)InitializeQTML;
#endif /* SYS_DARWIN */
    p_dec->FindNextComponent        = (void*)FindNextComponent;
    p_dec->OpenComponent            = (void*)OpenComponent;
    p_dec->ImageCodecInitialize     = (void*)ImageCodecInitialize;
    p_dec->ImageCodecGetCodecInfo   = (void*)ImageCodecGetCodecInfo;
    p_dec->ImageCodecPreDecompress  = (void*)ImageCodecPreDecompress;
    p_dec->ImageCodecBandDecompress = (void*)ImageCodecBandDecompress;
    p_dec->GetGWorldPixMap          = (void*)GetGWorldPixMap;
    p_dec->QTNewGWorldFromPtr       = (void*)QTNewGWorldFromPtr;
    p_dec->NewHandleClear           = (void*)NewHandleClear;

#ifndef SYS_DARWIN
    /* some sanity check */
    if( !p_dec->InitializeQTML ||
        !p_dec->FindNextComponent ||
        !p_dec->OpenComponent ||
        !p_dec->ImageCodecBandDecompress )
    {
        msg_Err( p_dec->p_fifo, "error getting qtmlClient.dll symbols");
        goto exit_error;
    }

    if( ( i_result = p_dec->InitializeQTML( 6 + 16 ) ) )
    {
        msg_Dbg( p_dec->p_fifo, "error while InitializeQTML = %d", i_result );
        goto exit_error;
    }
#endif

    /* init ComponentDescription */
    memset( &desc, 0, sizeof( ComponentDescription ) );
    desc.componentType      = FCC( 'i', 'm', 'd', 'c' );
    desc.componentSubType   = FCC( fcc[0], fcc[1], fcc[2], fcc[3] );
    desc.componentManufacturer = 0;
    desc.componentFlags        = 0;
    desc.componentFlagsMask    = 0;

    if( !( prev = p_dec->FindNextComponent( NULL, &desc ) ) )
    {
        msg_Err( p_dec->p_fifo, "cannot find requested component" );
        goto exit_error;
    }
    msg_Dbg( p_dec->p_fifo, "component id=0x%p", prev );

    p_dec->ci =  p_dec->OpenComponent( prev );
    msg_Dbg( p_dec->p_fifo, "component instance p=0x%p", p_dec->ci );

    memset( &icap, 0, sizeof( ImageSubCodecDecompressCapabilities ) );
    cres =  p_dec->ImageCodecInitialize( p_dec->ci, &icap );
/*    msg_Dbg( p_dec->p_fifo, "ImageCodecInitialize->%p  size=%d (%d)\n",cres,icap.recordSize,icap.decompressRecordSize); */


    memset( &cinfo, 0, sizeof( CodecInfo ) );
    cres =  p_dec->ImageCodecGetCodecInfo( p_dec->ci, &cinfo );
    msg_Dbg( p_dec->p_fifo, "Flags: compr: 0x%lx  decomp: 0x%lx format: 0x%lx\n",
                cinfo.compressFlags, cinfo.decompressFlags, cinfo.formatFlags);
    msg_Dbg( p_dec->p_fifo, "quicktime_video: Codec name: %.*s\n", ((unsigned char*)&cinfo.typeName)[0],
                  ((unsigned char*)&cinfo.typeName)+1);

    /* make a yuy2 gworld */
    p_dec->OutBufferRect.top    = 0;
    p_dec->OutBufferRect.left   = 0;
    p_dec->OutBufferRect.right  = p_bih->biWidth;
    p_dec->OutBufferRect.bottom = p_bih->biHeight;


    /* codec data FIXME use codec not SVQ3 */
    msg_Dbg( p_dec->p_fifo, "vide = %d", i_vide );
    id = malloc( sizeof( ImageDescription ) + ( i_vide - 70 ) );
    id->idSize          = sizeof( ImageDescription ) + ( i_vide - 70 );
    id->cType           = FCC( fcc[0], fcc[1], fcc[2], fcc[3] );
    id->version         = GetWBE ( p_vide +  0 );
    id->revisionLevel   = GetWBE ( p_vide +  2 );
    id->vendor          = GetDWBE( p_vide +  4 );
    id->temporalQuality = GetDWBE( p_vide +  8 );
    id->spatialQuality  = GetDWBE( p_vide + 12 );
    id->width           = GetWBE ( p_vide + 16 );
    id->height          = GetWBE ( p_vide + 18 );
    id->hRes            = GetDWBE( p_vide + 20 );
    id->vRes            = GetDWBE( p_vide + 24 );
    id->dataSize        = GetDWBE( p_vide + 28 );
    id->frameCount      = GetWBE ( p_vide + 32 );
    memcpy( &id->name, p_vide + 34, 32 );
    id->depth           = GetWBE ( p_vide + 66 );
    id->clutID          = GetWBE ( p_vide + 68 );
    if( i_vide > 70 )
    {
        memcpy( ((char*)&id->clutID) + 2, p_vide + 70, i_vide - 70 );
    }

    msg_Dbg( p_dec->p_fifo, "idSize=%ld ver=%d rev=%d vendor=%ld tempQ=%d spaQ=%d w=%d h=%d dpi=%d%d dataSize=%d frameCount=%d clutID=%d",
             id->idSize, id->version, id->revisionLevel, id->vendor,
             (int)id->temporalQuality, (int)id->spatialQuality,
             id->width, id->height,
             (int)id->hRes, (int)id->vRes,
             (int)id->dataSize,
             id->frameCount,
             id->clutID );

    p_dec->framedescHandle = (ImageDescriptionHandle) p_dec->NewHandleClear( id->idSize );
    memcpy( *p_dec->framedescHandle, id, id->idSize );

    p_dec->plane = malloc( p_bih->biWidth * p_bih->biHeight * 3 );

    i_result =  p_dec->QTNewGWorldFromPtr( &p_dec->OutBufferGWorld,
                                           kYUVSPixelFormat, /*pixel format of new GWorld==YUY2 */
                                           &p_dec->OutBufferRect,   /*we should benchmark if yvu9 is faster for svq3, too */
                                           0, 0, 0,
                                           p_dec->plane,
                                           p_bih->biWidth * 2 );

    msg_Dbg( p_dec->p_fifo, "NewGWorldFromPtr returned:%ld\n", 65536-( i_result&0xffff ) );

    memset( &p_dec->decpar, 0, sizeof( CodecDecompressParams ) );
    p_dec->decpar.imageDescription = p_dec->framedescHandle;
    p_dec->decpar.startLine        = 0;
    p_dec->decpar.stopLine         = ( **p_dec->framedescHandle ).height;
    p_dec->decpar.frameNumber      = 1;
    p_dec->decpar.matrixFlags      = 0;
    p_dec->decpar.matrixType       = 0;
    p_dec->decpar.matrix           = 0;
    p_dec->decpar.capabilities     = &p_dec->codeccap;
    p_dec->decpar.accuracy         = codecNormalQuality;
    p_dec->decpar.srcRect          = p_dec->OutBufferRect;
    p_dec->decpar.transferMode     = srcCopy;
    p_dec->decpar.dstPixMap        = **p_dec->GetGWorldPixMap( p_dec->OutBufferGWorld );/*destPixmap;  */

    cres =  p_dec->ImageCodecPreDecompress( p_dec->ci, &p_dec->decpar );
    msg_Dbg( p_dec->p_fifo, "quicktime_video: ImageCodecPreDecompress cres=0x%X\n", (int)cres );

    p_dec->p_vout = vout_Request( p_dec->p_fifo, NULL,
                                  p_bih->biWidth, p_bih->biHeight,
                                  VLC_FOURCC( 'Y', 'U', 'Y', '2' ),
                                  VOUT_ASPECT_FACTOR * p_bih->biWidth / p_bih->biHeight );

    if( !p_dec->p_vout )
    {
        msg_Err( p_dec->p_fifo, "cannot get a vout" );
        goto exit_error;
    }

    p_dec->i_buffer = 1000*1000;
    p_dec->p_buffer = malloc( p_dec->i_buffer );

    vlc_mutex_unlock( lockval.p_address );
    return VLC_SUCCESS;

exit_error:
#ifdef LOADER
    Restore_LDT_Keeper( p_dec->ldt_fs );
#endif
    vlc_mutex_unlock( lockval.p_address );
    return VLC_EGENERIC;

}

static void DecodeThreadVideo( vdec_thread_t *p_dec )
{
    BITMAPINFOHEADER    *p_bih = (BITMAPINFOHEADER*)p_dec->p_fifo->p_bitmapinfoheader;
    pes_packet_t    *p_pes;
    vlc_value_t     lockval;
    picture_t       *p_pic;
    ComponentResult     cres;

    var_Get( p_dec->p_fifo->p_libvlc, "qt_mutex", &lockval );

    input_ExtractPES( p_dec->p_fifo, &p_pes );
    if( !p_pes )
    {
        msg_Err( p_dec->p_fifo, "cannot get PES" );
        p_dec->p_fifo->b_error = 1;
        return;
    }

    if( p_pes->i_pes_size > p_dec->i_buffer )
    {
        p_dec->i_buffer = 3 * p_pes->i_pes_size / 2;
        free( p_dec->p_buffer );
        p_dec->p_buffer = malloc( p_dec->i_buffer );
    }

    if( p_pes->i_pes_size > 0 && p_pes->i_pts > mdate() )
    {
        GetPESData( p_dec->p_buffer, p_dec->i_buffer, p_pes );

        while( !(p_pic = vout_CreatePicture( p_dec->p_vout, 0, 0, 0 ) ) )
        {
            if( p_dec->p_fifo->b_die || p_dec->p_fifo->b_error )
            {
                break;
            }
            msleep( VOUT_OUTMEM_SLEEP );
        }

        p_dec->decpar.data                  = p_dec->p_buffer;
        p_dec->decpar.bufferSize            = p_pes->i_pes_size;
        (**p_dec->framedescHandle).dataSize = p_pes->i_pes_size;

        vlc_mutex_lock( lockval.p_address );
        cres = p_dec->ImageCodecBandDecompress( p_dec->ci, &p_dec->decpar );
        vlc_mutex_unlock( lockval.p_address );

        ++p_dec->decpar.frameNumber;

        if( cres &0xFFFF )
        {
            msg_Dbg( p_dec->p_fifo,
                     "quicktime_video: ImageCodecBandDecompress cres=0x%X (-0x%X) %d :(\n",
                     (int)cres,(int)-cres, (int)cres );
        }

        memcpy( p_pic->p[0].p_pixels,
                p_dec->plane,
                p_bih->biWidth * p_bih->biHeight * 2 );

        vout_DatePicture( p_dec->p_vout, p_pic, p_pes->i_pts );
        vout_DisplayPicture( p_dec->p_vout, p_pic );
    }

    input_DeletePES( p_dec->p_fifo->p_packets_mgt, p_pes );
}

static void EndThreadVideo( vdec_thread_t *p_dec )
{
    msg_Dbg( p_dec->p_fifo, "QuickTime library video decoder closing" );
    free( p_dec->plane );
    vout_Request( p_dec->p_fifo, p_dec->p_vout, 0, 0, 0, 0 );

#ifndef SYS_DARWIN
    FreeLibrary( p_dec->qtml );
    msg_Dbg( p_dec->p_fifo, "FreeLibrary ok." );
#endif

#ifdef LOADER
    Restore_LDT_Keeper( p_dec->ldt_fs );
    msg_Dbg( p_dec->p_fifo, "Restore_LDT_Keeper" );
#endif
#ifdef SYS_DARWIN
    ExitMovies();
#endif
}
