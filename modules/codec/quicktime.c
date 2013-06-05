/*****************************************************************************
 * quicktime.c: a quicktime decoder that uses the QT library/dll
 *****************************************************************************
 * Copyright (C) 2003, 2008 - 2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir at via.ecp.fr>
 *          Derk-Jan Hartman <hartman at videolan.org>>
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>

#if !defined (__APPLE__) && !defined(_WIN32)
# define LOADER 1
#endif

#ifdef __APPLE__
#include <QuickTime/QuickTimeComponents.h>
#include <QuickTime/Movies.h>
#include <QuickTime/ImageCodec.h>
#endif

/* for windows do we require Quicktime compents header? */
#ifdef LOADER
#include "qtx/qtxsdk/components.h"
#include "wine/windef.h"
#include "ldt_keeper.h"

HMODULE   WINAPI LoadLibraryA(LPCSTR);
FARPROC   WINAPI GetProcAddress(HMODULE,LPCSTR);
int       WINAPI FreeLibrary(HMODULE);

#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("QuickTime library decoder") )
    set_capability( "decoder", 0 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    set_callbacks( Open, Close )

vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int           OpenAudio( decoder_t * );
static int           OpenVideo( decoder_t * );

static block_t       *DecodeAudio( decoder_t *, block_t ** );
#ifndef _WIN32
static picture_t     *DecodeVideo( decoder_t *, block_t ** );
#endif

#define FCC( a, b , c, d ) \
    ((uint32_t)( ((a)<<24)|((b)<<16)|((c)<<8)|(d)))

#ifndef __APPLE__
typedef struct OpaqueSoundConverter*    SoundConverter;
#ifndef LOADER
typedef long                            OSType;
typedef int                             OSErr;
#endif
typedef unsigned long                   UnsignedFixed;
typedef uint8_t                         Byte;

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

#endif /* __APPLE__ */

struct decoder_sys_t
{
    /* library */
#ifndef __APPLE__
#ifdef LOADER
    ldt_fs_t    *ldt_fs;
#endif /* LOADER */

    HMODULE qtml;
    HINSTANCE qts;
    OSErr (*InitializeQTML)             ( long flags );
    OSErr (*TerminateQTML)              ( void );
#endif /* __APPLE__ */

    /* Audio */
    int (*SoundConverterOpen)           ( const SoundComponentData *,
                                          const SoundComponentData *,
                                          SoundConverter* );
    int (*SoundConverterClose)          ( SoundConverter );
    int (*SoundConverterSetInfo)        ( SoundConverter , OSType, void * );
    int (*SoundConverterGetBufferSizes) ( SoundConverter, unsigned long,
                                          unsigned long*, unsigned long*,
                                          unsigned long* );
    int (*SoundConverterBeginConversion)( SoundConverter );
    int (*SoundConverterEndConversion)  ( SoundConverter, void *,
                                          unsigned long *, unsigned long *);
    int (*SoundConverterConvertBuffer)  ( SoundConverter, const void *,
                                          unsigned long, void *,
                                          unsigned long *, unsigned long * );
    SoundConverter      myConverter;
    SoundComponentData  InputFormatInfo, OutputFormatInfo;

    unsigned long   FramesToGet;
    unsigned int    InFrameSize;
    unsigned int    OutFrameSize;

#ifndef _WIN32
    /* Video */
    Component         (*FindNextComponent)
        ( Component prev, ComponentDescription* desc );

    ComponentInstance (*OpenComponent)
        ( Component c );

    ComponentResult   (*ImageCodecInitialize)
        ( ComponentInstance ci, ImageSubCodecDecompressCapabilities * cap);

    ComponentResult   (*ImageCodecGetCodecInfo)
        ( ComponentInstance ci, CodecInfo *info );

    ComponentResult   (*ImageCodecPreDecompress)
        ( ComponentInstance ci, CodecDecompressParams * params );

    ComponentResult   (*ImageCodecBandDecompress)
        ( ComponentInstance ci, CodecDecompressParams * params );

    PixMapHandle      (*GetGWorldPixMap)
        ( GWorldPtr offscreenGWorld );

    OSErr             (*QTNewGWorldFromPtr)
        ( GWorldPtr *gw, OSType pixelFormat, const Rect *boundsRect,
          CTabHandle cTable, /*GDHandle*/ void *aGDevice, /*unused*/
          GWorldFlags flags, void *baseAddr, long rowBytes );

    OSErr             (*NewHandleClear)( Size byteCount );

    ComponentInstance       ci;
    Rect                    OutBufferRect;   /* the dimensions of our GWorld */
    GWorldPtr               OutBufferGWorld; /* a GWorld is some kind of
                                                description for a drawing
                                                environment */
    ImageDescriptionHandle  framedescHandle;

    CodecDecompressParams   decpar;          /* for ImageCodecPreDecompress()*/
    CodecCapabilities       codeccap;        /* for decpar */
#endif

    /* Output properties */
    uint8_t *           plane;
    mtime_t             pts;
    date_t              date;

    int                 i_late; /* video */

    /* buffer */
    unsigned int        i_buffer;
    unsigned int        i_buffer_size;
    uint8_t             *p_buffer;

    /* Audio only */
    uint8_t             out_buffer[1000000];    /* FIXME */
    int                 i_out_frames;
    int                 i_out;
};

static const int pi_channels_maps[6] =
{
    0,
    AOUT_CHAN_CENTER,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARLEFT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARLEFT
};

static int QTAudioInit( decoder_t * );
#ifndef _WIN32
static int QTVideoInit( decoder_t * );
#endif

/*****************************************************************************
 * Open: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to choose.
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

#ifdef __APPLE__
    OSErr err;
    SInt32 qtVersion, macosversion;

    err = Gestalt(gestaltQuickTimeVersion, &qtVersion);
    err = Gestalt(gestaltSystemVersion, &macosversion);
#ifndef NDEBUG
    msg_Dbg( p_this, "Mac OS version is %#lx", macosversion );
    msg_Dbg( p_this, "Quicktime version is %#lx", qtVersion );
#endif

    /* bail out. This plugin is soo Carbon, that it can't be used on 10.5 at all */
    msg_Info( p_dec, "Your Mac OS version is to new to use this plugin for anything." );
    return VLC_EGENERIC;
#endif

    switch( p_dec->fmt_in.i_codec )
    {
        case VLC_CODEC_H264:
        case VLC_CODEC_CINEPAK:
        case VLC_FOURCC('I','V','4','1'): /* Indeo Video IV */
        case VLC_FOURCC('i','v','4','1'): /* dto. */
#ifdef __APPLE__
        case VLC_FOURCC('p','x','l','t'): /* Pixlet */
#endif
        case VLC_CODEC_DV:
        case VLC_CODEC_SVQ3: /* Sorenson v3 */
    /*    case VLC_CODEC_SVQ1:  Sorenson v1
        case VLC_FOURCC('Z','y','G','o'):
        case VLC_FOURCC('V','P','3','1'):
        case VLC_FOURCC('3','I','V','1'): */
        case VLC_CODEC_QTRLE:
        case VLC_CODEC_RPZA:
#ifdef LOADER
        p_dec->p_sys = NULL;
        p_dec->pf_decode_video = DecodeVideo;
        p_dec->fmt_out.i_cat = VIDEO_ES;
        return VLC_SUCCESS;
#else
        return OpenVideo( p_dec );
#endif

#ifdef __APPLE__
        case VLC_FOURCC('I','L','B','C'): /* iLBC */
            if ((err != noErr) || (qtVersion < 0x07500000)) 
                return VLC_EGENERIC;
        case VLC_FOURCC('i','l','b','c'): /* iLBC */
            if ((err != noErr) || (qtVersion < 0x07500000)) 
                return VLC_EGENERIC;
#endif
        case VLC_CODEC_AMR_NB: /* 3GPP AMR audio */
        case VLC_FOURCC('s','a','m','b'): /* 3GPP AMR-WB audio */
        case VLC_CODEC_MP4A: /* MPEG-4 audio */
        case VLC_FOURCC('Q','D','M','C'): /* QDesign */
        case VLC_CODEC_QDM2: /* QDesign* 2 */
        case VLC_CODEC_QCELP: /* Qualcomm Purevoice Codec */
        case VLC_FOURCC('Q','C','L','P'): /* Qualcomm Purevoice Codec */
        case VLC_CODEC_MACE3: /* MACE3 audio decoder */
        case VLC_CODEC_MACE6: /* MACE6 audio decoder */
        case VLC_FOURCC('d','v','c','a'): /* DV Audio */
        case VLC_FOURCC('s','o','w','t'): /* 16-bit Little Endian */
        case VLC_FOURCC('t','w','o','s'): /* 16-bit Big Endian */
        case VLC_CODEC_ALAW: /* ALaw 2:1 */
        case VLC_FOURCC('u','l','a','w'): /* mu-Law 2:1 */
        case VLC_FOURCC('r','a','w',' '): /* 8-bit offset binaries */
        case VLC_CODEC_FL32: /* 32-bit Floating Point */
        case VLC_CODEC_FL64: /* 64-bit Floating Point */
        case VLC_FOURCC('i','n','2','4'): /* 24-bit Interger */
        case VLC_FOURCC('i','n','3','2'): /* 32-bit Integer */
        case 0x0011:                            /* DVI IMA */
        case 0x6D730002:                        /* Microsoft ADPCM-ACM */
        case 0x6D730011:                        /* DVI Intel IMAADPCM-ACM */
#ifdef LOADER
        p_dec->p_sys = NULL;
        p_dec->pf_decode_audio = DecodeAudio;
        p_dec->fmt_out.i_cat = AUDIO_ES;
        return VLC_SUCCESS;
#else
        return OpenAudio( p_dec );
#endif

        default:
            return VLC_EGENERIC;
    }
}

static vlc_mutex_t qt_mutex = VLC_STATIC_MUTEX;

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* get lock, avoid segfault */
    vlc_mutex_lock( &qt_mutex );

    if( p_dec->fmt_out.i_cat == AUDIO_ES )
    {
        int i_error;
        unsigned long ConvertedFrames=0;
        unsigned long ConvertedBytes=0;

        i_error = p_sys->SoundConverterEndConversion( p_sys->myConverter, NULL,
                                                      &ConvertedFrames,
                                                      &ConvertedBytes );
        msg_Dbg( p_dec, "SoundConverterEndConversion => %d", i_error );

        i_error = p_sys->SoundConverterClose( p_sys->myConverter );
        msg_Dbg( p_dec, "SoundConverterClose => %d", i_error );

        free( p_sys->p_buffer );
    }
    else if( p_dec->fmt_out.i_cat == VIDEO_ES )
    {
        free( p_sys->plane );
    }

#ifndef __APPLE__
    FreeLibrary( p_sys->qtml );
    FreeLibrary( p_sys->qts );
    msg_Dbg( p_dec, "FreeLibrary ok." );
#else
    ExitMovies();
#endif

#if 0
    /* Segfault */
#ifdef LOADER
    Restore_LDT_Keeper( p_sys->ldt_fs );
    msg_Dbg( p_dec, "Restore_LDT_Keeper" );
#endif
#endif

    vlc_mutex_unlock( &qt_mutex );

    free( p_sys );
}

/*****************************************************************************
 * OpenAudio:
 *****************************************************************************/
static int OpenAudio( decoder_t *p_dec )
{
    decoder_sys_t *p_sys;

    int             i_error;
    char            fcc[4];
    unsigned long   WantedBufferSize;
    unsigned long   InputBufferSize = 0;
    unsigned long   OutputBufferSize = 0;

    /* get lock, avoid segfault */
    vlc_mutex_lock( &qt_mutex );

    p_sys = calloc( 1, sizeof( decoder_sys_t ) );
    p_dec->p_sys = p_sys;
    p_dec->pf_decode_audio = DecodeAudio;

    if( p_dec->fmt_in.i_original_fourcc )
        memcpy( fcc, &p_dec->fmt_in.i_original_fourcc, 4 );
    else
        memcpy( fcc, &p_dec->fmt_in.i_codec, 4 );

#ifdef __APPLE__
    EnterMovies();
#endif

    if( QTAudioInit( p_dec ) )
    {
        msg_Err( p_dec, "cannot initialize QT");
        goto exit_error;
    }

#ifndef __APPLE__
    if( ( i_error = p_sys->InitializeQTML( 6 + 16 ) ) )
    {
        msg_Dbg( p_dec, "error on InitializeQTML = %d", i_error );
        goto exit_error;
    }
#endif

    /* input format settings */
    p_sys->InputFormatInfo.flags       = 0;
    p_sys->InputFormatInfo.sampleCount = 0;
    p_sys->InputFormatInfo.buffer      = NULL;
    p_sys->InputFormatInfo.reserved    = 0;
    p_sys->InputFormatInfo.numChannels = p_dec->fmt_in.audio.i_channels;
    p_sys->InputFormatInfo.sampleSize  = p_dec->fmt_in.audio.i_bitspersample;
    p_sys->InputFormatInfo.sampleRate  = p_dec->fmt_in.audio.i_rate;
    p_sys->InputFormatInfo.format      = FCC( fcc[0], fcc[1], fcc[2], fcc[3] );

    /* output format settings */
    p_sys->OutputFormatInfo.flags       = 0;
    p_sys->OutputFormatInfo.sampleCount = 0;
    p_sys->OutputFormatInfo.buffer      = NULL;
    p_sys->OutputFormatInfo.reserved    = 0;
    p_sys->OutputFormatInfo.numChannels = p_dec->fmt_in.audio.i_channels;
    p_sys->OutputFormatInfo.sampleSize  = 16;
    p_sys->OutputFormatInfo.sampleRate  = p_dec->fmt_in.audio.i_rate;
    p_sys->OutputFormatInfo.format      = FCC( 'N', 'O', 'N', 'E' );


    i_error = p_sys->SoundConverterOpen( &p_sys->InputFormatInfo,
                                         &p_sys->OutputFormatInfo,
                                         &p_sys->myConverter );
    if( i_error )
    {
        msg_Err( p_dec, "error on SoundConverterOpen = %d", i_error );
        goto exit_error;
    }

#if 0
    /* tell the sound converter we accept VBR formats */
    i_error = SoundConverterSetInfo( p_dec->myConverter, siClientAcceptsVBR,
                                     (void *)true );
#endif

    if( p_dec->fmt_in.i_extra > 36 + 8 )
    {
        i_error = p_sys->SoundConverterSetInfo( p_sys->myConverter,
                                                FCC( 'w', 'a', 'v', 'e' ),
                                                ((uint8_t*)p_dec->fmt_in.p_extra) + 36 + 8 );

        msg_Dbg( p_dec, "error on SoundConverterSetInfo = %d", i_error );
    }

    WantedBufferSize = p_sys->OutputFormatInfo.numChannels *
                       p_sys->OutputFormatInfo.sampleRate * 2;
    p_sys->FramesToGet = 0;

    i_error = p_sys->SoundConverterGetBufferSizes( p_sys->myConverter,
                                                   WantedBufferSize,
                                                   &p_sys->FramesToGet,
                                                   &InputBufferSize,
                                                   &OutputBufferSize );

    msg_Dbg( p_dec, "WantedBufferSize=%li InputBufferSize=%li "
             "OutputBufferSize=%li FramesToGet=%li",
             WantedBufferSize, InputBufferSize, OutputBufferSize,
             p_sys->FramesToGet );

    p_sys->InFrameSize  = (InputBufferSize + p_sys->FramesToGet - 1 ) /
                            p_sys->FramesToGet;
    p_sys->OutFrameSize = OutputBufferSize / p_sys->FramesToGet;

    msg_Dbg( p_dec, "frame size %d -> %d",
             p_sys->InFrameSize, p_sys->OutFrameSize );

    if( (i_error = p_sys->SoundConverterBeginConversion(p_sys->myConverter)) )
    {
        msg_Err( p_dec,
                 "error on SoundConverterBeginConversion = %d", i_error );
        goto exit_error;
    }


    es_format_Init( &p_dec->fmt_out, AUDIO_ES, VLC_CODEC_S16N );
    p_dec->fmt_out.audio.i_rate = p_sys->OutputFormatInfo.sampleRate;
    p_dec->fmt_out.audio.i_channels = p_sys->OutputFormatInfo.numChannels;
    p_dec->fmt_out.audio.i_physical_channels =
    p_dec->fmt_out.audio.i_original_channels =
        pi_channels_maps[p_sys->OutputFormatInfo.numChannels];

    date_Init( &p_sys->date, p_dec->fmt_out.audio.i_rate, 1 );

    p_sys->i_buffer      = 0;
    p_sys->i_buffer_size = 100*1000;
    p_sys->p_buffer      = malloc( p_sys->i_buffer_size );
    if( !p_sys->p_buffer )
        goto exit_error;

    p_sys->i_out = 0;
    p_sys->i_out_frames = 0;

    vlc_mutex_unlock( &qt_mutex );
    return VLC_SUCCESS;

exit_error:

#ifdef LOADER
    Restore_LDT_Keeper( p_sys->ldt_fs );
#endif
    vlc_mutex_unlock( &qt_mutex );

    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * DecodeAudio:
 *****************************************************************************/
static block_t *DecodeAudio( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    block_t     *p_block;
    int         i_error;

#ifdef LOADER
    /* We must do open and close in the same thread (unless we do
     * Setup_LDT_Keeper in the main thread before all others */
    if( p_sys == NULL )
    {
        if( OpenAudio( p_dec ) )
        {
            /* Fatal */
            p_dec->b_error = true;
            return NULL;
        }

        p_sys = p_dec->p_sys;
    }
#endif

    if( pp_block == NULL || *pp_block == NULL )
    {
        return NULL;
    }
    p_block = *pp_block;

    if( p_sys->i_out_frames > 0 && p_sys->i_out >= p_sys->i_out_frames )
    {
        /* Ask new data */
        p_sys->i_out = 0;
        p_sys->i_out_frames = 0;

        *pp_block = NULL;
        return NULL;
    }

    if( p_sys->i_out_frames <= 0 )
    {
        p_sys->pts = p_block->i_pts;

        mtime_t i_display_date = 0;
        if( !(p_block->i_flags & BLOCK_FLAG_PREROLL) )
            i_display_date = decoder_GetDisplayDate( p_dec, p_block->i_pts );

        if( i_display_date > 0 && i_display_date < mdate() )
        {
            block_Release( p_block );
            *pp_block = NULL;
            return NULL;
        }

        /* Append data */
        if( p_sys->i_buffer_size < p_sys->i_buffer + p_block->i_buffer )
        {
            p_sys->i_buffer_size = p_sys->i_buffer + p_block->i_buffer + 1024;
            p_sys->p_buffer = xrealloc( p_sys->p_buffer, p_sys->i_buffer_size );
        }
        memcpy( &p_sys->p_buffer[p_sys->i_buffer], p_block->p_buffer,
                p_block->i_buffer );
        p_sys->i_buffer += p_block->i_buffer;

        if( p_sys->i_buffer > p_sys->InFrameSize )
        {
            int i_frames = p_sys->i_buffer / p_sys->InFrameSize;
            unsigned long i_out_frames, i_out_bytes;
            vlc_mutex_lock( &qt_mutex );

            i_error = p_sys->SoundConverterConvertBuffer( p_sys->myConverter,
                                                          p_sys->p_buffer,
                                                          i_frames,
                                                          p_sys->out_buffer,
                                                          &i_out_frames,
                                                          &i_out_bytes );
            vlc_mutex_unlock( &qt_mutex );

            /*
            msg_Dbg( p_dec, "decoded %d frames -> %ld frames (error=%d)",
                     i_frames, i_out_frames, i_error );

            msg_Dbg( p_dec, "decoded %ld frames = %ld bytes",
                     i_out_frames, i_out_bytes );
            */

            p_sys->i_buffer -= i_frames * p_sys->InFrameSize;
            if( p_sys->i_buffer > 0 )
            {
                memmove( &p_sys->p_buffer[0],
                         &p_sys->p_buffer[i_frames * p_sys->InFrameSize],
                         p_sys->i_buffer );
            }

            if( p_sys->pts > VLC_TS_INVALID &&
                p_sys->pts != date_Get( &p_sys->date ) )
            {
                date_Set( &p_sys->date, p_sys->pts );
            }
            else if( !date_Get( &p_sys->date ) )
            {
                return NULL;
            }

            if( !i_error && i_out_frames > 0 )
            {
                /* we have others samples */
                p_sys->i_out_frames = i_out_frames;
                p_sys->i_out = 0;
            }
        }
    }

    if( p_sys->i_out < p_sys->i_out_frames )
    {
        block_t *p_out;
        int  i_frames = __MIN( p_sys->i_out_frames - p_sys->i_out, 1000 );

        p_out = decoder_NewAudioBuffer( p_dec, i_frames );

        if( p_out )
        {
            p_out->i_pts = date_Get( &p_sys->date );
            p_out->i_length = date_Increment( &p_sys->date, i_frames )
                              - p_out->i_pts;

            memcpy( p_out->p_buffer,
                    &p_sys->out_buffer[2 * p_sys->i_out * p_dec->fmt_out.audio.i_channels],
                    p_out->i_buffer );

            p_sys->i_out += i_frames;
        }
        return p_out;
    }

    return NULL;
}

/*****************************************************************************
 * OpenVideo:
 *****************************************************************************/
static int OpenVideo( decoder_t *p_dec )
{
#ifndef _WIN32
    decoder_sys_t *p_sys = malloc( sizeof( decoder_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;

    long                                i_result;
    ComponentDescription                desc;
    Component                           prev;
    ComponentResult                     cres;
    ImageSubCodecDecompressCapabilities icap;   /* for ImageCodecInitialize() */
    CodecInfo                           cinfo;  /* for ImageCodecGetCodecInfo() */
    ImageDescription                    *id;

    char                fcc[4];
    int     i_vide = p_dec->fmt_in.i_extra;
    uint8_t *p_vide = p_dec->fmt_in.p_extra;

    p_dec->p_sys = p_sys;
    p_dec->pf_decode_video = DecodeVideo;
    p_sys->i_late = 0;

    if( i_vide <= 0 )
    {
        msg_Err( p_dec, "missing extra info" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    if( p_dec->fmt_in.i_original_fourcc )
        memcpy( fcc, &p_dec->fmt_in.i_original_fourcc, 4 );
    else
        memcpy( fcc, &p_dec->fmt_in.i_codec, 4 );

    msg_Dbg( p_dec, "quicktime_video %4.4s %dx%d",
             fcc, p_dec->fmt_in.video.i_width, p_dec->fmt_in.video.i_height );

    /* get lock, avoid segfault */
    vlc_mutex_lock( &qt_mutex );

#ifdef __APPLE__
    EnterMovies();
#endif

    if( QTVideoInit( p_dec ) )
    {
        msg_Err( p_dec, "cannot initialize QT");
        goto exit_error;
    }

#ifndef __APPLE__
    if( ( i_result = p_sys->InitializeQTML( 6 + 16 ) ) )
    {
        msg_Dbg( p_dec, "error on InitializeQTML = %d", (int)i_result );
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

    if( !( prev = p_sys->FindNextComponent( NULL, &desc ) ) )
    {
        msg_Err( p_dec, "cannot find requested component" );
        goto exit_error;
    }
    msg_Dbg( p_dec, "component id=0x%p", prev );

    p_sys->ci =  p_sys->OpenComponent( prev );
    msg_Dbg( p_dec, "component instance p=0x%p", p_sys->ci );

    memset( &icap, 0, sizeof( ImageSubCodecDecompressCapabilities ) );
    cres =  p_sys->ImageCodecInitialize( p_sys->ci, &icap );
    msg_Dbg( p_dec, "ImageCodecInitialize->0x%X size=%d (%d)",
             (int)cres, (int)icap.recordSize, (int)icap.decompressRecordSize);

    memset( &cinfo, 0, sizeof( CodecInfo ) );
    cres =  p_sys->ImageCodecGetCodecInfo( p_sys->ci, &cinfo );
    msg_Dbg( p_dec,
             "Flags: compr: 0x%x decomp: 0x%x format: 0x%x",
             (unsigned int)cinfo.compressFlags,
             (unsigned int)cinfo.decompressFlags,
             (unsigned int)cinfo.formatFlags );
    msg_Dbg( p_dec, "quicktime_video: Codec name: %.*s",
             ((unsigned char*)&cinfo.typeName)[0],
             ((unsigned char*)&cinfo.typeName)+1 );

    /* make a yuy2 gworld */
    p_sys->OutBufferRect.top    = 0;
    p_sys->OutBufferRect.left   = 0;
    p_sys->OutBufferRect.right  = p_dec->fmt_in.video.i_width;
    p_sys->OutBufferRect.bottom = p_dec->fmt_in.video.i_height;


    /* codec data FIXME use codec not SVQ3 */
    msg_Dbg( p_dec, "vide = %d", i_vide  );
    id = malloc( sizeof( ImageDescription ) + ( i_vide - 70 ) );
    if( !id )
        goto exit_error;
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

    msg_Dbg( p_dec, "idSize=%d ver=%d rev=%d vendor=%d tempQ=%d "
             "spaQ=%d w=%d h=%d dpi=%d%d dataSize=%d depth=%d frameCount=%d clutID=%d",
             (int)id->idSize, id->version, id->revisionLevel, (int)id->vendor,
             (int)id->temporalQuality, (int)id->spatialQuality,
             (int)id->width, (int)id->height,
             (int)id->hRes, (int)id->vRes,
             (int)id->dataSize,
             id->depth,
             id->frameCount,
             id->clutID );

    p_sys->framedescHandle = (ImageDescriptionHandle) NewHandleClear( id->idSize );
    memcpy( *p_sys->framedescHandle, id, id->idSize );

    if( p_dec->fmt_in.video.i_width != 0 && p_dec->fmt_in.video.i_height != 0) 
        p_sys->plane = malloc( p_dec->fmt_in.video.i_width * p_dec->fmt_in.video.i_height * 3 );
    if( !p_sys->plane )
        goto exit_error;

    i_result = p_sys->QTNewGWorldFromPtr( &p_sys->OutBufferGWorld,
                                          /*pixel format of new GWorld==YUY2 */
                                          kYUVSPixelFormat,
                                          /* we should benchmark if yvu9 is
                                           * faster for svq3, too */
                                          &p_sys->OutBufferRect,
                                          0, 0, 0,
                                          p_sys->plane,
                                          p_dec->fmt_in.video.i_width * 2 );

    msg_Dbg( p_dec, "NewGWorldFromPtr returned:%ld",
             65536 - ( i_result&0xffff ) );

    memset( &p_sys->decpar, 0, sizeof( CodecDecompressParams ) );
    p_sys->decpar.imageDescription = p_sys->framedescHandle;
    p_sys->decpar.startLine        = 0;
    p_sys->decpar.stopLine         = ( **p_sys->framedescHandle ).height;
    p_sys->decpar.frameNumber      = 1;
    p_sys->decpar.matrixFlags      = 0;
    p_sys->decpar.matrixType       = 0;
    p_sys->decpar.matrix           = 0;
    p_sys->decpar.capabilities     = &p_sys->codeccap;
    p_sys->decpar.accuracy         = codecNormalQuality;
    p_sys->decpar.srcRect          = p_sys->OutBufferRect;
    p_sys->decpar.transferMode     = srcCopy;
    p_sys->decpar.dstPixMap        = **p_sys->GetGWorldPixMap( p_sys->OutBufferGWorld );/*destPixmap;  */

    cres =  p_sys->ImageCodecPreDecompress( p_sys->ci, &p_sys->decpar );
    msg_Dbg( p_dec, "quicktime_video: ImageCodecPreDecompress cres=0x%X",
             (int)cres );

    es_format_Init( &p_dec->fmt_out, VIDEO_ES, VLC_CODEC_YUYV);
    p_dec->fmt_out.video.i_width = p_dec->fmt_in.video.i_width;
    p_dec->fmt_out.video.i_height= p_dec->fmt_in.video.i_height;
    p_dec->fmt_out.video.i_sar_num = 1;
    p_dec->fmt_out.video.i_sar_den = 1;

    vlc_mutex_unlock( &qt_mutex );
    return VLC_SUCCESS;

exit_error:
#ifdef LOADER
    Restore_LDT_Keeper( p_sys->ldt_fs );
#endif
    vlc_mutex_unlock( &qt_mutex );

#else
    VLC_UNUSED( p_dec );
#endif /* !_WIN32 */

    return VLC_EGENERIC;
}

#ifndef _WIN32
/*****************************************************************************
 * DecodeVideo:
 *****************************************************************************/
static picture_t *DecodeVideo( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t       *p_block;
    picture_t     *p_pic;
    mtime_t       i_pts;

    ComponentResult cres;

#ifdef LOADER
    /* We must do open and close in the same thread (unless we do
     * Setup_LDT_Keeper in the main thread before all others */
    if( p_sys == NULL )
    {
        if( OpenVideo( p_dec ) )
        {
            /* Fatal */
            p_dec->b_error = true;
            return NULL;
        }
        p_sys = p_dec->p_sys;
    }
#endif

    if( pp_block == NULL || *pp_block == NULL )
    {
        return NULL;
    }
    p_block = *pp_block;
    *pp_block = NULL;
 
    i_pts = p_block->i_pts > VLC_TS_INVALID ? p_block->i_pts : p_block->i_dts;

    mtime_t i_display_date = 0;
    if( !(p_block->i_flags & BLOCK_FLAG_PREROLL) )
        i_display_date = decoder_GetDisplayDate( p_dec, i_pts );

    if( i_display_date > 0 && i_display_date < mdate() )
    {
        p_sys->i_late++;
    }
    else
    {
        p_sys->i_late = 0;
    }
#ifndef NDEBUG
    msg_Dbg( p_dec, "bufsize: %zu", p_block->i_buffer);
#endif

    if( p_sys->i_late > 10 )
    {
        msg_Dbg( p_dec, "late buffer dropped (%"PRId64")", i_pts );
        block_Release( p_block );
        return NULL;
    }
 
    vlc_mutex_lock( &qt_mutex );

    if( ( p_pic = decoder_NewPicture( p_dec ) ) )
    {
        p_sys->decpar.data                  = (Ptr)p_block->p_buffer;
        p_sys->decpar.bufferSize            = p_block->i_buffer;
        (**p_sys->framedescHandle).dataSize = p_block->i_buffer;

        cres = p_sys->ImageCodecBandDecompress( p_sys->ci, &p_sys->decpar );

        ++p_sys->decpar.frameNumber;

        if( cres &0xFFFF )
        {
            msg_Dbg( p_dec, "quicktime_video: ImageCodecBandDecompress"
                     " cres=0x%X (-0x%X) %d :(",
                     (int)cres,(int)-cres, (int)cres );
        }

        memcpy( p_pic->p[0].p_pixels, p_sys->plane,
                p_dec->fmt_in.video.i_width * p_dec->fmt_in.video.i_height * 2 );
        p_pic->date = i_pts;
    }
 
    vlc_mutex_unlock( &qt_mutex );

    block_Release( p_block );
    return p_pic;
}
#endif /* !_WIN32 */

/*****************************************************************************
 * QTAudioInit:
 *****************************************************************************/
static int QTAudioInit( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

#ifdef __APPLE__
    p_sys->SoundConverterOpen       = (void*)SoundConverterOpen;
    p_sys->SoundConverterClose      = (void*)SoundConverterClose;
    p_sys->SoundConverterSetInfo    = (void*)SoundConverterSetInfo;
    p_sys->SoundConverterGetBufferSizes = (void*)SoundConverterGetBufferSizes;
    p_sys->SoundConverterConvertBuffer  = (void*)SoundConverterConvertBuffer;
    p_sys->SoundConverterBeginConversion= (void*)SoundConverterBeginConversion;
    p_sys->SoundConverterEndConversion  = (void*)SoundConverterEndConversion;
#else

#ifdef LOADER
    p_sys->ldt_fs = Setup_LDT_Keeper();
#endif /* LOADER */

    p_sys->qts = LoadLibraryA( "QuickTime.qts" );
    if( p_sys->qts == NULL )
    {
        msg_Dbg( p_dec, "failed loading QuickTime.qts" );
        return VLC_EGENERIC;
    }
    p_sys->qtml = LoadLibraryA( "qtmlClient.dll" );
    if( p_sys->qtml == NULL )
    {
        msg_Dbg( p_dec, "failed loading qtmlClient.dll" );
        return VLC_EGENERIC;
    }

    p_sys->InitializeQTML               = (void *)GetProcAddress( p_sys->qtml, "InitializeQTML" );
    p_sys->TerminateQTML                = (void *)GetProcAddress( p_sys->qtml, "TerminateQTML" );
    p_sys->SoundConverterOpen           = (void *)GetProcAddress( p_sys->qtml, "SoundConverterOpen" );
    p_sys->SoundConverterClose          = (void *)GetProcAddress( p_sys->qtml, "SoundConverterClose" );
    p_sys->SoundConverterSetInfo        = (void *)GetProcAddress( p_sys->qtml, "SoundConverterSetInfo" );
    p_sys->SoundConverterGetBufferSizes = (void *)GetProcAddress( p_sys->qtml, "SoundConverterGetBufferSizes" );
    p_sys->SoundConverterConvertBuffer  = (void *)GetProcAddress( p_sys->qtml, "SoundConverterConvertBuffer" );
    p_sys->SoundConverterEndConversion  = (void *)GetProcAddress( p_sys->qtml, "SoundConverterEndConversion" );
    p_sys->SoundConverterBeginConversion= (void *)GetProcAddress( p_sys->qtml, "SoundConverterBeginConversion");

    if( p_sys->InitializeQTML == NULL )
    {
        msg_Err( p_dec, "failed getting proc address InitializeQTML" );
        return VLC_EGENERIC;
    }
    if( p_sys->SoundConverterOpen == NULL ||
        p_sys->SoundConverterClose == NULL ||
        p_sys->SoundConverterSetInfo == NULL ||
        p_sys->SoundConverterGetBufferSizes == NULL ||
        p_sys->SoundConverterConvertBuffer == NULL ||
        p_sys->SoundConverterEndConversion == NULL ||
        p_sys->SoundConverterBeginConversion == NULL )
    {
        msg_Err( p_dec, "failed getting proc address" );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_dec, "standard init done" );
#endif /* else __APPLE__ */

    return VLC_SUCCESS;
}

#ifndef _WIN32
/*****************************************************************************
 * QTVideoInit:
 *****************************************************************************/
static int QTVideoInit( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

#ifdef __APPLE__
    p_sys->FindNextComponent        = (void*)FindNextComponent;
    p_sys->OpenComponent            = (void*)OpenComponent;
    p_sys->ImageCodecInitialize     = (void*)ImageCodecInitialize;
    p_sys->ImageCodecGetCodecInfo   = (void*)ImageCodecGetCodecInfo;
    p_sys->ImageCodecPreDecompress  = (void*)ImageCodecPreDecompress;
    p_sys->ImageCodecBandDecompress = (void*)ImageCodecBandDecompress;
    p_sys->GetGWorldPixMap          = (void*)GetGWorldPixMap;
    p_sys->QTNewGWorldFromPtr       = (void*)QTNewGWorldFromPtr;
    p_sys->NewHandleClear           = (void*)NewHandleClear;
#else

#ifdef LOADER
    p_sys->ldt_fs = Setup_LDT_Keeper();
#endif /* LOADER */
    p_sys->qts = LoadLibraryA( "QuickTime.qts" );
    if( p_sys->qts == NULL )
    {
        msg_Dbg( p_dec, "failed loading QuickTime.qts" );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_dec, "QuickTime.qts loaded" );
    p_sys->qtml = LoadLibraryA( "qtmlClient.dll" );
    if( p_sys->qtml == NULL )
    {
        msg_Dbg( p_dec, "failed loading qtmlClient.dll" );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_dec, "qtmlClient.dll loaded" );

    /* (void*) to shut up gcc */
    p_sys->InitializeQTML           = (void*)GetProcAddress( p_sys->qtml, "InitializeQTML" );
    p_sys->FindNextComponent        = (void*)GetProcAddress( p_sys->qtml, "FindNextComponent" );
    p_sys->OpenComponent            = (void*)GetProcAddress( p_sys->qtml, "OpenComponent" );
    p_sys->ImageCodecInitialize     = (void*)GetProcAddress( p_sys->qtml, "ImageCodecInitialize" );
    p_sys->ImageCodecGetCodecInfo   = (void*)GetProcAddress( p_sys->qtml, "ImageCodecGetCodecInfo" );
    p_sys->ImageCodecPreDecompress  = (void*)GetProcAddress( p_sys->qtml, "ImageCodecPreDecompress" );
    p_sys->ImageCodecBandDecompress = (void*)GetProcAddress( p_sys->qtml, "ImageCodecBandDecompress" );
    p_sys->GetGWorldPixMap          = (void*)GetProcAddress( p_sys->qtml, "GetGWorldPixMap" );
    p_sys->QTNewGWorldFromPtr       = (void*)GetProcAddress( p_sys->qtml, "QTNewGWorldFromPtr" );
    p_sys->NewHandleClear           = (void*)GetProcAddress( p_sys->qtml, "NewHandleClear" );

    if( p_sys->InitializeQTML == NULL )
    {
        msg_Dbg( p_dec, "failed getting proc address InitializeQTML" );
        return VLC_EGENERIC;
    }
    if( p_sys->FindNextComponent == NULL ||
        p_sys->OpenComponent == NULL ||
        p_sys->ImageCodecInitialize == NULL ||
        p_sys->ImageCodecGetCodecInfo == NULL ||
        p_sys->ImageCodecPreDecompress == NULL ||
        p_sys->ImageCodecBandDecompress == NULL ||
        p_sys->GetGWorldPixMap == NULL ||
        p_sys->QTNewGWorldFromPtr == NULL ||
        p_sys->NewHandleClear == NULL )
    {
        msg_Err( p_dec, "failed getting proc address" );
        return VLC_EGENERIC;
    }
#endif /* __APPLE__ */

    return VLC_SUCCESS;
}
#endif /* !_WIN32 */
