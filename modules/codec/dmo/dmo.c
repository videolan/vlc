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

#include <objbase.h>
#include "codecs.h"
#include "dmo.h"

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
static int  DecoderOpen  ( vlc_object_t * );
static void DecoderClose ( vlc_object_t * );
static void *DecodeBlock ( decoder_t *, block_t ** );

static void CopyPicture( decoder_t *, picture_t *, uint8_t * );

vlc_module_begin();
    set_description( _("DirectMedia Object decoder") );
    add_shortcut( "dmo" );
    set_capability( "decoder", 1 );
    set_callbacks( DecoderOpen, DecoderClose );
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

    audio_date_t end_date;
};

/*****************************************************************************
 * DecoderOpen: open dmo codec
 *****************************************************************************/
static int DecoderOpen( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys = NULL;

    DMO_PARTIAL_MEDIATYPE dmo_partial_type;
    DMO_MEDIA_TYPE dmo_input_type, dmo_output_type;
    IEnumDMO *p_enum_dmo = NULL; 
    IMediaObject *p_dmo = NULL;
    WCHAR *psz_dmo_name;
    GUID clsid_dmo;

    VIDEOINFOHEADER *p_vih = NULL;
    WAVEFORMATEX *p_wf = NULL;

    HRESULT (STDCALL *OurDMOEnum)( const GUID *, uint32_t, uint32_t,
                           const DMO_PARTIAL_MEDIATYPE *,
                           uint32_t, const DMO_PARTIAL_MEDIATYPE *,
                           IEnumDMO ** );

    /* Load msdmo DLL */
    HINSTANCE hmsdmo_dll = LoadLibrary( "msdmo.dll" );
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
        p_wf->cbSize = i_size;

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
        p_dec->fmt_out.audio.i_bitspersample =
            p_dec->fmt_in.audio.i_bitspersample;
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
        p_wf->cbSize = sizeof(WAVEFORMATEX);

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
    block_out.p_buffer = malloc( p_sys->i_min_output );
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
        free( block_out.p_buffer );
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
        free( block_out.p_buffer );
        return NULL;
    }

    if( !i_buffer_out )
    {
#if DMO_DEBUG
        msg_Dbg( p_dec, "ProcessOutput(): no output (i_buffer_out == 0)" );
#endif
        p_out->vt->Release( (IUnknown *)p_out );
        free( block_out.p_buffer );
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
        free( block_out.p_buffer );

        return p_pic;
    }
    else
    {
        aout_buffer_t *p_aout_buffer;
        int i_samples = i_buffer_out /
            ( p_dec->fmt_in.audio.i_bitspersample *
              p_dec->fmt_in.audio.i_channels / 8 );

        p_aout_buffer = p_dec->pf_aout_buffer_new( p_dec, i_samples );
        memcpy( p_aout_buffer->p_buffer, p_buffer_out, i_buffer_out );

        /* Date management */
        p_aout_buffer->start_date = aout_DateGet( &p_sys->end_date );
        p_aout_buffer->end_date =
            aout_DateIncrement( &p_sys->end_date, i_samples );

        p_out->vt->Release( (IUnknown *)p_out );
        free( block_out.p_buffer );

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

        p_src += i_width;

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
