/*****************************************************************************
 * dmo.c : DirectMedia Object decoder module for vlc
 *****************************************************************************
 * Copyright (C) 2002, 2003 VideoLAN
 * $Id$
 *
 * Author: Gildas Bazin <gbazin@videolan.org>
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/decoder.h>
#include <vlc/vout.h>

#ifndef WIN32
#    define LOADER
#else
#   include <objbase.h>
#endif

#ifdef LOADER
/* Need the w32dll loader from mplayer */
#   include <wine/winerror.h>
#   include <ldt_keeper.h>
#   include <wine/windef.h>
#endif

#include "codecs.h"
#include "dmo.h"

#ifdef LOADER
/* Not Needed */
long CoInitialize( void *pvReserved ) { return -1; }
void CoUninitialize( void ) { }

/* A few prototypes */
HMODULE WINAPI LoadLibraryA(LPCSTR);
#define LoadLibrary LoadLibraryA
FARPROC WINAPI GetProcAddress(HMODULE,LPCSTR);
int     WINAPI FreeLibrary(HMODULE);
typedef long STDCALL (*GETCLASS) ( const GUID*, const GUID*, void** );
#endif /* LOADER */

static int pi_channels_maps[7] =
{
    0,
    AOUT_CHAN_CENTER,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_CENTER | AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT
     | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static int  DecoderOpen  ( vlc_object_t * );
static void DecoderClose ( vlc_object_t * );
static void *DecodeBlock ( decoder_t *, block_t ** );

static void CopyPicture( decoder_t *, picture_t *, uint8_t * );

vlc_module_begin();
    set_description( _("DirectMedia Object decoder") );
    add_shortcut( "dmo" );
    set_capability( "decoder", 1 );
    set_callbacks( Open, DecoderClose );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/****************************************************************************
 * Decoder descriptor declaration
 ****************************************************************************/
struct decoder_sys_t
{
    HINSTANCE hmsdmo_dll;
    IMediaObject *p_dmo;

    int i_min_output;
    uint8_t *p_buffer;

    audio_date_t end_date;

#ifdef LOADER
    ldt_fs_t    *ldt_fs;
#endif
};

#ifdef LOADER
static const GUID guid_wmv9 = { 0x724bb6a4, 0xe526, 0x450f, { 0xaf, 0xfa, 0xab, 0x9b, 0x45, 0x12, 0x91, 0x11 } };
static const GUID guid_wma9 = { 0x27ca0808, 0x01f5, 0x4e7a, { 0x8b, 0x05, 0x87, 0xf8, 0x07, 0xa2, 0x33, 0xd1 } };

static const GUID guid_wmv = { 0x82d353df, 0x90bd, 0x4382, { 0x8b, 0xc2, 0x3f, 0x61, 0x92, 0xb7, 0x6e, 0x34 } };
static const GUID guid_wma = { 0x874131cb, 0x4ecc, 0x443b, { 0x89, 0x48, 0x74, 0x6b, 0x89, 0x59, 0x5d, 0x20 } };

static const struct
{
    vlc_fourcc_t i_fourcc;
    const char   *psz_dll;
    const GUID   *p_guid;
} codecs_table[] =
{
    /* WM3 */
    { VLC_FOURCC('W','M','V','3'), "wmv9dmod.dll", &guid_wmv9 },
    { VLC_FOURCC('w','m','v','3'), "wmv9dmod.dll", &guid_wmv9 },
    /* WMV2 */
    { VLC_FOURCC('W','M','V','2'), "wmvdmod.dll", &guid_wmv },
    { VLC_FOURCC('w','m','v','2'), "wmvdmod.dll", &guid_wmv },
    /* WMV1 */
    { VLC_FOURCC('W','M','V','1'), "wmvdmod.dll", &guid_wmv },
    { VLC_FOURCC('w','m','v','1'), "wmvdmod.dll", &guid_wmv },

    /* WMA 3*/
    { VLC_FOURCC('W','M','A','3'), "wma9dmod.dll", &guid_wma9 },
    { VLC_FOURCC('w','m','a','3'), "wma9dmod.dll", &guid_wma9 },

    /* */
    { 0, NULL, NULL }
};
#endif /* LOADER */

/*****************************************************************************
 * Open: open dmo codec
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
#ifndef LOADER
    return DecoderOpen( p_this );

#else
    decoder_t *p_dec = (decoder_t*)p_this;
    int i;
    /* We can't open it now, because of ldt_keeper or something
     * Open/Decode/Close has to be done in the same thread */

    p_dec->p_sys = NULL;

    /* Probe if we support it */
    for( i = 0; codecs_table[i].i_fourcc != 0; i++ )
    {
        if( codecs_table[i].i_fourcc == p_dec->fmt_in.i_codec )
        {
            msg_Dbg( p_dec, "DMO codec for %4.4s may work with dll=%s",
                     (char*)&p_dec->fmt_in.i_codec, codecs_table[i].psz_dll );

            /* Set callbacks */
            p_dec->pf_decode_video =
                (picture_t *(*)(decoder_t *, block_t **))DecodeBlock;
            p_dec->pf_decode_audio =
                (aout_buffer_t *(*)(decoder_t *, block_t **))DecodeBlock;
            return VLC_SUCCESS;
        }
    }

    return VLC_EGENERIC;
#endif /* LOADER */
}

/*****************************************************************************
 * DecoderOpen: open dmo codec
 *****************************************************************************/
static int DecoderOpen( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys = NULL;

    DMO_PARTIAL_MEDIATYPE dmo_partial_type;
    DMO_MEDIA_TYPE dmo_input_type, dmo_output_type;
    IMediaObject *p_dmo = NULL;

    VIDEOINFOHEADER *p_vih = NULL;
    WAVEFORMATEX *p_wf = NULL;

#ifdef LOADER
    ldt_fs_t *ldt_fs = Setup_LDT_Keeper();
#endif /* LOADER */

    HINSTANCE hmsdmo_dll = NULL;

    /* Look for a DMO which can handle the requested codec */
    if( p_dec->fmt_in.i_cat == AUDIO_ES )
    {
        uint16_t i_tag;
        dmo_partial_type.type = MEDIATYPE_Audio;
        dmo_partial_type.subtype = dmo_partial_type.type;
        dmo_partial_type.subtype.Data1 = p_dec->fmt_in.i_codec;
        fourcc_to_wf_tag( p_dec->fmt_in.i_codec, &i_tag );
        if( i_tag ) dmo_partial_type.subtype.Data1 = i_tag;
    }
    else
    {
        dmo_partial_type.type = MEDIATYPE_Video;
        dmo_partial_type.subtype = dmo_partial_type.type;
        dmo_partial_type.subtype.Data1 = p_dec->fmt_in.i_codec;
    }

#ifndef LOADER
  {
    IEnumDMO *p_enum_dmo = NULL;
    WCHAR *psz_dmo_name;
    GUID clsid_dmo;
    long (STDCALL *OurDMOEnum)( const GUID *, uint32_t, uint32_t,
                               const DMO_PARTIAL_MEDIATYPE *,
                               uint32_t, const DMO_PARTIAL_MEDIATYPE *,
                               IEnumDMO ** );

    /* Load msdmo DLL */
    hmsdmo_dll = LoadLibrary( "msdmo.dll" );
    if( hmsdmo_dll == NULL )
    {
        msg_Dbg( p_dec, "failed loading msdmo.dll" );
        return VLC_EGENERIC;
    }
    OurDMOEnum = (void *)GetProcAddress( hmsdmo_dll, "DMOEnum" );
    if( OurDMOEnum == NULL )
    {
        msg_Dbg( p_dec, "GetProcAddress failed to find DMOEnum()" );
        FreeLibrary( hmsdmo_dll );
        return VLC_EGENERIC;
    }


    /* Initialize OLE/COM */
    CoInitialize( 0 );

    if( OurDMOEnum( &GUID_NULL, 1 /*DMO_ENUMF_INCLUDE_KEYED*/,
                    1, &dmo_partial_type, 0, NULL, &p_enum_dmo ) )
    {
        goto error;
    }

    /* Pickup the first available codec */
    if( p_enum_dmo->vt->Next( p_enum_dmo, 1, &clsid_dmo,
                              &psz_dmo_name, NULL ) )
    {
        goto error;
    }
    p_enum_dmo->vt->Release( (IUnknown *)p_enum_dmo );

#if 1
    {
        char psz_temp[MAX_PATH];
        wcstombs( psz_temp, psz_dmo_name, MAX_PATH );
        msg_Dbg( p_dec, "found DMO: %s", psz_temp );
    }
#endif

    CoTaskMemFree( psz_dmo_name );

    /* Create DMO */
    if( CoCreateInstance( &clsid_dmo, NULL, CLSCTX_INPROC,
                          &IID_IMediaObject, (void **)&p_dmo ) )
    {
        msg_Err( p_dec, "can't create DMO" );
        goto error;
    }
  }

#else   /* LOADER */
  {
    GETCLASS GetClass;
    IClassFactory *cFactory = NULL;
    IUnknown *cObject = NULL;

    int i_err;
    int i_codec;
    for( i_codec = 0; codecs_table[i_codec].i_fourcc != 0; i_codec++ )
    {
        if( codecs_table[i_codec].i_fourcc == p_dec->fmt_in.i_codec )
            break;
    }
    if( codecs_table[i_codec].i_fourcc == 0 )
        return VLC_EGENERIC;    /* Can't happen */

    hmsdmo_dll = LoadLibrary( codecs_table[i_codec].psz_dll );
    if( hmsdmo_dll == NULL )
    {
        msg_Dbg( p_dec, "failed loading '%s'", codecs_table[i_codec].psz_dll );
        return VLC_EGENERIC;
    }

    GetClass = (GETCLASS)GetProcAddress( hmsdmo_dll, "DllGetClassObject" );
    if (!GetClass)
    {
        msg_Dbg( p_dec, "GetProcAddress failed to find DllGetClassObject()" );
        FreeLibrary( hmsdmo_dll );
        return VLC_EGENERIC;
    }

    i_err = GetClass( codecs_table[i_codec].p_guid, &IID_IClassFactory,
                      (void**)&cFactory );
    if( i_err || cFactory == NULL )
    {
        msg_Dbg( p_dec, "no such class object" );
        FreeLibrary( hmsdmo_dll );
        return VLC_EGENERIC;
    }

    i_err = cFactory->vt->CreateInstance( cFactory, 0, &IID_IUnknown,
                                          (void**)&cObject );
    cFactory->vt->Release((IUnknown*)cFactory );
    if( i_err || !cObject )
    {
        msg_Dbg( p_dec, "class factory failure" );
        FreeLibrary( hmsdmo_dll );
        return VLC_EGENERIC;
    }
    i_err = cObject->vt->QueryInterface( cObject, &IID_IMediaObject,
                                        (void**)&p_dmo );
    cObject->vt->Release((IUnknown*)cObject );
  }
#endif  /* LOADER */

    /* Setup input format */
    memset( &dmo_input_type, 0, sizeof(dmo_input_type) );
    dmo_input_type.majortype = dmo_partial_type.type;
    dmo_input_type.subtype = dmo_partial_type.subtype;
    dmo_input_type.pUnk = 0;

    if( p_dec->fmt_in.i_cat == AUDIO_ES )
    {
        int i_size = sizeof(WAVEFORMATEX) + p_dec->fmt_in.i_extra;
        p_wf = malloc( i_size );

        memset( p_wf, 0, sizeof(WAVEFORMATEX) );
        if( p_dec->fmt_in.i_extra )
            memcpy( &p_wf[1], p_dec->fmt_in.p_extra, p_dec->fmt_in.i_extra );

        p_wf->wFormatTag = dmo_partial_type.subtype.Data1;
        p_wf->nSamplesPerSec = p_dec->fmt_in.audio.i_rate;
        p_wf->nChannels = p_dec->fmt_in.audio.i_channels;
        p_wf->wBitsPerSample = p_dec->fmt_in.audio.i_bitspersample;
        p_wf->nBlockAlign = p_dec->fmt_in.audio.i_blockalign;
        p_wf->nAvgBytesPerSec = p_dec->fmt_in.i_bitrate / 8;
        p_wf->cbSize = p_dec->fmt_in.i_extra;

        dmo_input_type.formattype = FORMAT_WaveFormatEx;
        dmo_input_type.cbFormat   = i_size;
        dmo_input_type.pbFormat   = (char *)p_wf;
        dmo_input_type.pUnk       = NULL;
        dmo_input_type.bFixedSizeSamples = 1;
        dmo_input_type.bTemporalCompression = 0;
        dmo_input_type.lSampleSize = p_wf->nBlockAlign;
    }
    else
    {
        BITMAPINFOHEADER *p_bih;

        int i_size = sizeof(VIDEOINFOHEADER) + p_dec->fmt_in.i_extra;
        p_vih = malloc( i_size );

        memset( p_vih, 0, sizeof(VIDEOINFOHEADER) );
        if( p_dec->fmt_in.i_extra )
            memcpy( &p_vih[1], p_dec->fmt_in.p_extra, p_dec->fmt_in.i_extra );

        p_bih = &p_vih->bmiHeader;
        p_bih->biCompression = dmo_partial_type.subtype.Data1;
        p_bih->biWidth = p_dec->fmt_in.video.i_width;
        p_bih->biHeight = p_dec->fmt_in.video.i_height;
        p_bih->biBitCount = p_dec->fmt_in.video.i_bits_per_pixel;
        p_bih->biPlanes = 1;
        p_bih->biSize = i_size - sizeof(VIDEOINFOHEADER) +
            sizeof(BITMAPINFOHEADER);

        p_vih->rcSource.left = p_vih->rcSource.top = 0;
        p_vih->rcSource.right = p_dec->fmt_in.video.i_width;
        p_vih->rcSource.bottom = p_dec->fmt_in.video.i_height;
        p_vih->rcTarget = p_vih->rcSource;

        dmo_input_type.formattype = MEDIASUBTYPE_VideoInfo;
        dmo_input_type.bFixedSizeSamples = 0;
        dmo_input_type.bTemporalCompression = 1;
        dmo_input_type.cbFormat = i_size;
        dmo_input_type.pbFormat = (char *)p_vih;
    }

    if( p_dmo->vt->SetInputType( p_dmo, 0, &dmo_input_type, 0 ) )
    {
        msg_Err( p_dec, "can't set DMO input type" );
        goto error;
    }
    msg_Dbg( p_dec, "DMO input type set" );

    /* Setup output format */
    memset( &dmo_output_type, 0, sizeof(dmo_output_type) );
    dmo_output_type.majortype = dmo_partial_type.type;
    dmo_output_type.pUnk = 0;

    if( p_dec->fmt_in.i_cat == AUDIO_ES )
    {
        /* Setup the format */
        p_dec->fmt_out.i_codec = AOUT_FMT_S16_NE;
        p_dec->fmt_out.audio.i_rate     = p_dec->fmt_in.audio.i_rate;
        p_dec->fmt_out.audio.i_channels = p_dec->fmt_in.audio.i_channels;
        p_dec->fmt_out.audio.i_bitspersample = 16;//p_dec->fmt_in.audio.i_bitspersample; We request 16
        p_dec->fmt_out.audio.i_physical_channels =
            p_dec->fmt_out.audio.i_original_channels =
                pi_channels_maps[p_dec->fmt_out.audio.i_channels];

        p_wf->wFormatTag = WAVE_FORMAT_PCM;
        p_wf->nSamplesPerSec = p_dec->fmt_out.audio.i_rate;
        p_wf->nChannels = p_dec->fmt_out.audio.i_channels;
        p_wf->wBitsPerSample = p_dec->fmt_out.audio.i_bitspersample;
        p_wf->nBlockAlign =
            p_wf->wBitsPerSample / 8 * p_wf->nChannels;
        p_wf->nAvgBytesPerSec =
            p_wf->nSamplesPerSec * p_wf->nBlockAlign;
        p_wf->cbSize = 0;

        dmo_output_type.formattype = FORMAT_WaveFormatEx;
        dmo_output_type.subtype    = MEDIASUBTYPE_PCM;
        dmo_output_type.cbFormat   = sizeof(WAVEFORMATEX);
        dmo_output_type.pbFormat   = (char *)p_wf;
        dmo_output_type.bFixedSizeSamples = 1;
        dmo_output_type.bTemporalCompression = 0;
        dmo_output_type.lSampleSize = p_wf->nBlockAlign;
        dmo_output_type.pUnk = NULL;
    }
    else
    {
        BITMAPINFOHEADER *p_bih;

        p_dec->fmt_out.i_codec = VLC_FOURCC('I','4','2','0');
        p_dec->fmt_out.video.i_width = p_dec->fmt_in.video.i_width;
        p_dec->fmt_out.video.i_height = p_dec->fmt_in.video.i_height;
        p_dec->fmt_out.video.i_bits_per_pixel = 12;
        p_dec->fmt_out.video.i_aspect = VOUT_ASPECT_FACTOR *
            p_dec->fmt_out.video.i_width / p_dec->fmt_out.video.i_height;

        dmo_output_type.formattype = MEDIASUBTYPE_VideoInfo;
        dmo_output_type.subtype = MEDIASUBTYPE_YV12;

        p_bih = &p_vih->bmiHeader;
        p_bih->biCompression = dmo_partial_type.subtype.Data1;
        p_bih->biHeight *= -1;
        p_bih->biBitCount = p_dec->fmt_out.video.i_bits_per_pixel;
        p_bih->biSizeImage = p_dec->fmt_in.video.i_width *
            p_dec->fmt_in.video.i_height *
            (p_dec->fmt_in.video.i_bits_per_pixel + 7) / 8;

        //p_bih->biPlanes = 1;
        p_bih->biSize = sizeof(BITMAPINFOHEADER);

        dmo_output_type.bFixedSizeSamples = VLC_TRUE;
        dmo_output_type.bTemporalCompression = 0;
        dmo_output_type.lSampleSize = p_bih->biSizeImage;
        dmo_output_type.cbFormat = sizeof(VIDEOINFOHEADER);
        dmo_output_type.pbFormat = (char *)p_vih;
    }

    /* Enumerate output types */
    if( p_dec->fmt_in.i_cat == VIDEO_ES )
    {
        int i = 0;
        DMO_MEDIA_TYPE mt;

        while( !p_dmo->vt->GetOutputType( p_dmo, 0, i++, &mt ) )
        {
            msg_Dbg( p_dec, "available output chroma: %4.4s",
                     (char *)&mt.subtype.Data1 );
        }
    }

    /* Choose an output type.
     * FIXME, get rid of the dmo_output_type code above. */
    if( p_dec->fmt_in.i_cat == VIDEO_ES )
    {
        int i = 0;
        DMO_MEDIA_TYPE mt;

        while( !p_dmo->vt->GetOutputType( p_dmo, 0, i++, &mt ) )
        {
            if( dmo_output_type.subtype.Data1 == mt.subtype.Data1 )
            {
                *p_vih = *(VIDEOINFOHEADER *)mt.pbFormat;
                break;
            }
        }
    }

    if( p_dmo->vt->SetOutputType( p_dmo, 0, &dmo_output_type, 0 ) )
    {
        msg_Err( p_dec, "can't set DMO output type" );
        goto error;
    }
    msg_Dbg( p_dec, "DMO output type set" );

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys =
          (decoder_sys_t *)malloc(sizeof(decoder_sys_t)) ) == NULL )
    {
        msg_Err( p_dec, "out of memory" );
        goto error;
    }

    p_sys->hmsdmo_dll = hmsdmo_dll;
    p_sys->p_dmo = p_dmo;
#ifdef LOADER
    p_sys->ldt_fs = ldt_fs;
#endif

    /* Find out some properties of the output */
    {
        uint32_t i_size, i_align;

        p_sys->i_min_output = 0;
        if( p_dmo->vt->GetOutputSizeInfo( p_dmo, 0, &i_size, &i_align ) )
        {
            msg_Err( p_dec, "GetOutputSizeInfo() failed" );
            goto error;
        }
        else
        {
            msg_Dbg( p_dec, "GetOutputSizeInfo(): bytes %i, align %i",
                     i_size, i_align );
            p_sys->i_min_output = i_size;
            p_sys->p_buffer = malloc( i_size );
            if( !p_sys->p_buffer ) goto error;
        }
    }

    /* Set output properties */
    p_dec->fmt_out.i_cat = p_dec->fmt_in.i_cat;
    if( p_dec->fmt_out.i_cat == AUDIO_ES )
        aout_DateInit( &p_sys->end_date, p_dec->fmt_in.audio.i_rate );
    else
        aout_DateInit( &p_sys->end_date, 25 /* FIXME */ );
    aout_DateSet( &p_sys->end_date, 0 );

    /* Set callbacks */
    p_dec->pf_decode_video = (picture_t *(*)(decoder_t *, block_t **))
        DecodeBlock;
    p_dec->pf_decode_audio = (aout_buffer_t *(*)(decoder_t *, block_t **))
        DecodeBlock;

    if( p_vih ) free( p_vih );
    if( p_wf ) free( p_wf );

    return VLC_SUCCESS;

 error:
    /* Uninitialize OLE/COM */
    CoUninitialize();
    FreeLibrary( hmsdmo_dll );

    if( p_vih ) free( p_vih );
    if( p_wf )  free( p_wf );
    if( p_sys ) free( p_sys );

    return VLC_EGENERIC;
}

/*****************************************************************************
 * DecoderClose: close codec
 *****************************************************************************/
void DecoderClose( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* Uninitialize OLE/COM */
    CoUninitialize();

    FreeLibrary( p_sys->hmsdmo_dll );

#if 0
#ifdef LOADER
    Restore_LDT_Keeper( p_sys->ldt_fs );
#endif
#endif

    if( p_sys->p_buffer ) free( p_sys->p_buffer );
    free( p_sys );
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function must be fed with ogg packets.
 ****************************************************************************/
static void *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block;
    int i_result;

    DMO_OUTPUT_DATA_BUFFER db;
    CMediaBuffer *p_out;
    block_t block_out;
    uint32_t i_status, i_buffer_out;
    uint8_t *p_buffer_out;

    if( p_sys == NULL )
    {
        if( DecoderOpen( VLC_OBJECT(p_dec) ) )
        {
            msg_Err( p_dec, "DecoderOpen failed" );
            return NULL;
        }
        p_sys = p_dec->p_sys;
    }

    if( !pp_block ) return NULL;

    p_block = *pp_block;

    /* Won't work with streams with B-frames, but do we have any ? */
    if( p_block && p_block->i_pts <= 0 ) p_block->i_pts = p_block->i_dts;

    /* Date management */
    if( p_block && p_block->i_pts > 0 &&
        p_block->i_pts != aout_DateGet( &p_sys->end_date ) )
    {
        aout_DateSet( &p_sys->end_date, p_block->i_pts );
    }

#if 0 /* Breaks the video decoding */
    if( !aout_DateGet( &p_sys->end_date ) )
    {
        /* We've just started the stream, wait for the first PTS. */
        if( p_block ) block_Release( p_block );
        return NULL;
    }
#endif

    /* Feed input to the DMO */
    if( p_block && p_block->i_buffer )
    {
        CMediaBuffer *p_in;

        p_in = CMediaBufferCreate( p_block, p_block->i_buffer, VLC_TRUE );

        i_result = p_sys->p_dmo->vt->ProcessInput( p_sys->p_dmo, 0,
                       (IMediaBuffer *)p_in, DMO_INPUT_DATA_BUFFER_SYNCPOINT,
                       0, 0 );

        p_in->vt->Release( (IUnknown *)p_in );

        if( i_result == S_FALSE )
        {
            /* No output generated */
#ifdef DMO_DEBUG
            msg_Dbg( p_dec, "ProcessInput(): no output generated" );
#endif
            return NULL;
        }
        else if( i_result == DMO_E_NOTACCEPTING )
        {
            /* Need to call ProcessOutput */
            msg_Dbg( p_dec, "ProcessInput(): not accepting" );
        }
        else if( i_result != S_OK )
        {
            msg_Dbg( p_dec, "ProcessInput(): failed" );
            return NULL;
        }
        else
        {
            //msg_Dbg( p_dec, "ProcessInput(): successful" );
            *pp_block = 0;
        }
    }
    else if( p_block && !p_block->i_buffer )
    {
        block_Release( p_block );
        *pp_block = 0;
    }

    /* Get output from the DMO */
    block_out.p_buffer = p_sys->p_buffer;;
    block_out.i_buffer = 0;

    p_out = CMediaBufferCreate( &block_out, p_sys->i_min_output, VLC_FALSE );
    db.rtTimestamp = 0;
    db.rtTimelength = 0;
    db.dwStatus = 0;
    db.pBuffer = (IMediaBuffer *)p_out;

    i_result = p_sys->p_dmo->vt->ProcessOutput( p_sys->p_dmo,
                   DMO_PROCESS_OUTPUT_DISCARD_WHEN_NO_BUFFER,
                   1, &db, &i_status );

    if( i_result != S_OK )
    {
        if( i_result != S_FALSE )
            msg_Dbg( p_dec, "ProcessOutput(): failed" );
#if DMO_DEBUG
        else
            msg_Dbg( p_dec, "ProcessOutput(): no output" );
#endif

        p_out->vt->Release( (IUnknown *)p_out );
        return NULL;
    }

#if DMO_DEBUG
    msg_Dbg( p_dec, "ProcessOutput(): success" );
#endif

    i_result = p_out->vt->GetBufferAndLength( (IMediaBuffer *)p_out,
                                              &p_buffer_out, &i_buffer_out );
    if( i_result != S_OK )
    {
        msg_Dbg( p_dec, "GetBufferAndLength(): failed" );
        p_out->vt->Release( (IUnknown *)p_out );
        return NULL;
    }

    if( !i_buffer_out )
    {
#if DMO_DEBUG
        msg_Dbg( p_dec, "ProcessOutput(): no output (i_buffer_out == 0)" );
#endif
        p_out->vt->Release( (IUnknown *)p_out );
        return NULL;
    }

    if( p_dec->fmt_out.i_cat == VIDEO_ES )
    {
        /* Get a new picture */
        picture_t *p_pic = p_dec->pf_vout_buffer_new( p_dec );
        if( !p_pic ) return NULL;

        CopyPicture( p_dec, p_pic, block_out.p_buffer );

        /* Date management */
        p_pic->date = aout_DateGet( &p_sys->end_date );
        aout_DateIncrement( &p_sys->end_date, 1 );

        p_out->vt->Release( (IUnknown *)p_out );

        return p_pic;
    }
    else
    {
        aout_buffer_t *p_aout_buffer;
        int i_samples = i_buffer_out /
            ( p_dec->fmt_out.audio.i_bitspersample *
              p_dec->fmt_out.audio.i_channels / 8 );

        p_aout_buffer = p_dec->pf_aout_buffer_new( p_dec, i_samples );
        memcpy( p_aout_buffer->p_buffer, p_buffer_out, i_buffer_out );

        /* Date management */
        p_aout_buffer->start_date = aout_DateGet( &p_sys->end_date );
        p_aout_buffer->end_date =
            aout_DateIncrement( &p_sys->end_date, i_samples );

        p_out->vt->Release( (IUnknown *)p_out );

        return p_aout_buffer;
    }

    return NULL;
}

static void CopyPicture( decoder_t *p_dec, picture_t *p_pic, uint8_t *p_in )
{
    int i_plane, i_line, i_width, i_dst_stride;
    uint8_t *p_dst, *p_src = p_in;

    p_dst = p_pic->p[1].p_pixels;
    p_pic->p[1].p_pixels = p_pic->p[2].p_pixels;
    p_pic->p[2].p_pixels = p_dst;

    for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
    {
        p_dst = p_pic->p[i_plane].p_pixels;
        i_width = p_pic->p[i_plane].i_visible_pitch;
        i_dst_stride  = p_pic->p[i_plane].i_pitch;

        for( i_line = 0; i_line < p_pic->p[i_plane].i_lines; i_line++ )
        {
            p_dec->p_vlc->pf_memcpy( p_dst, p_src, i_width );
            p_src += i_width;
            p_dst += i_dst_stride;
        }
    }

    p_dst = p_pic->p[1].p_pixels;
    p_pic->p[1].p_pixels = p_pic->p[2].p_pixels;
    p_pic->p[2].p_pixels = p_dst;
}
