/*****************************************************************************
 * dmo.c : DirectMedia Object decoder module for vlc
 *****************************************************************************
 * Copyright (C) 2002, 2003 VLC authors and VideoLAN
 *
 * Author: Gildas Bazin <gbazin@videolan.org>
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

#include <assert.h>

#define COBJMACROS
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>

#include <objbase.h>
#include <strmif.h>
#include <amvideo.h>
#include <mmreg.h>

#include <vlc_codecs.h> // fourcc_to_wf_tag
#include "dmo.h"
#include "../../video_chroma/copy.h"

#ifndef NDEBUG
# define DMO_DEBUG 1
#endif

typedef long (STDCALL *GETCLASS) ( const GUID*, const GUID*, void** );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  DecoderOpen  ( vlc_object_t * );
static void DecoderClose ( vlc_object_t * );
static int DecodeBlock ( decoder_t *, block_t * );
static void *DecoderThread( void * );
#ifdef ENABLE_SOUT
static void EncoderClose ( encoder_t * );
static block_t *EncodeBlock( encoder_t *, void * );

static int  EncOpen  ( vlc_object_t * );
#endif // !ENABLE_SOUT

static int LoadDMO( vlc_object_t *, HINSTANCE *, IMediaObject **,
                    const es_format_t *, bool );
static void CopyPicture( picture_t *, uint8_t * );

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/****************************************************************************
 * Decoder descriptor declaration
 ****************************************************************************/
typedef struct
{
    HINSTANCE hmsdmo_dll;
    IMediaObject *p_dmo;

    int i_min_output;
    uint8_t *p_buffer;

    date_t end_date;

    vlc_thread_t thread;
    vlc_mutex_t  lock;
    vlc_cond_t   wait_input, wait_output;
    bool         b_ready, b_works;
    block_t     *p_input;
} decoder_sys_t;

const GUID IID_IWMCodecPrivateData = {0x73f0be8e, 0x57f7, 0x4f01, {0xaa, 0x66, 0x9f, 0x57, 0x34, 0xc, 0xfe, 0xe}};
const GUID IID_IMediaObject = {0xd8ad0f58, 0x5494, 0x4102, {0x97, 0xc5, 0xec, 0x79, 0x8e, 0x59, 0xbc, 0xf4}};
const GUID IID_IMediaBuffer = {0x59eff8b9, 0x938c, 0x4a26, {0x82, 0xf2, 0x95, 0xcb, 0x84, 0xcd, 0xc8, 0x37}};
const GUID MEDIATYPE_Video = {0x73646976, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID MEDIATYPE_Audio = {0x73647561, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID MEDIASUBTYPE_PCM = {0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID MEDIASUBTYPE_IEEE_FLOAT = {0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID FORMAT_VideoInfo = {0x05589f80, 0xc356, 0x11ce, {0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a}};
const GUID FORMAT_WaveFormatEx = {0x05589f81, 0xc356, 0x11ce, {0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a}};
const GUID GUID_NULL = {0x0000, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
const GUID MEDIASUBTYPE_I420 = {0x30323449, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID MEDIASUBTYPE_YV12 = {0x32315659, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID MEDIASUBTYPE_RGB24 = {0xe436eb7d, 0x524f, 0x11ce, {0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}};
const GUID MEDIASUBTYPE_RGB565 = {0xe436eb7b, 0x524f, 0x11ce, {0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}};


static const GUID guid_wvc1 = { 0xc9bfbccf, 0xe60e, 0x4588, { 0xa3, 0xdf, 0x5a, 0x03, 0xb1, 0xfd, 0x95, 0x85 } };
static const GUID guid_wmv9 = { 0x724bb6a4, 0xe526, 0x450f, { 0xaf, 0xfa, 0xab, 0x9b, 0x45, 0x12, 0x91, 0x11 } };

static const GUID guid_wmv = { 0x82d353df, 0x90bd, 0x4382, { 0x8b, 0xc2, 0x3f, 0x61, 0x92, 0xb7, 0x6e, 0x34 } };
static const GUID guid_wms = { 0x7bafb3b1, 0xd8f4, 0x4279, { 0x92, 0x53, 0x27, 0xda, 0x42, 0x31, 0x08, 0xde } };
static const GUID guid_wmva ={ 0x03be3ac4, 0x84b7, 0x4e0e, { 0xa7, 0x8d, 0xd3, 0x52, 0x4e, 0x60, 0x39, 0x5a } };

static const GUID guid_wma = { 0x874131cb, 0x4ecc, 0x443b, { 0x89, 0x48, 0x74, 0x6b, 0x89, 0x59, 0x5d, 0x20 } };
static const GUID guid_wma9 = { 0x27ca0808, 0x01f5, 0x4e7a, { 0x8b, 0x05, 0x87, 0xf8, 0x07, 0xa2, 0x33, 0xd1 } };

static const GUID guid_wmv_enc2 = { 0x96b57cdd, 0x8966, 0x410c,{ 0xbb, 0x1f, 0xc9, 0x7e, 0xea, 0x76, 0x5c, 0x04 } };
static const GUID guid_wma_enc = { 0x70f598e9, 0xf4ab, 0x495a, { 0x99, 0xe2, 0xa7, 0xc4, 0xd3, 0xd8, 0x9a, 0xbf } };
static const GUID guid_wmv8_enc = { 0x7e320092, 0x596a, 0x41b2,{ 0xbb, 0xeb, 0x17, 0x5d, 0x10, 0x50, 0x4e, 0xb6 } };
static const GUID guid_wmv9_enc = { 0xd23b90d0, 0x144f, 0x46bd,{ 0x84, 0x1d, 0x59, 0xe4, 0xeb, 0x19, 0xdc, 0x59 } };

#ifndef BI_RGB
# define BI_RGB 0x0
#endif

typedef struct
{
    vlc_fourcc_t i_fourcc;
    const WCHAR  *psz_dll;
    const GUID   *p_guid;

} codec_dll;

static const codec_dll decoders_table[] =
{
    /* WVC1 */
    { VLC_CODEC_VC1,    TEXT("wvc1dmod.dll"), &guid_wvc1 },
    /* WMV3 */
    { VLC_CODEC_WMV3,   TEXT("wmv9dmod.dll"), &guid_wmv9 },
    /* WMV2 */
    { VLC_CODEC_WMV2,   TEXT("wmvdmod.dll"), &guid_wmv },
    /* WMV1 */
    { VLC_CODEC_WMV1,   TEXT("wmvdmod.dll"), &guid_wmv },
    /* Screen codecs */
    { VLC_CODEC_MSS2,   TEXT("WMVSDECD.DLL"), &guid_wms },
    { VLC_CODEC_MSS2,   TEXT("wmsdmod.dll"),  &guid_wms },
    { VLC_CODEC_MSS1,   TEXT("WMVSDECD.DLL"), &guid_wms },
    { VLC_CODEC_MSS1,   TEXT("wmsdmod.dll"),  &guid_wms },
    /* Windows Media Video Adv */
    { VLC_CODEC_WMVA,   TEXT("wmvadvd.dll"), &guid_wmva },

    /* WMA 3 */
    { VLC_CODEC_WMAP,   TEXT("wma9dmod.dll"), &guid_wma9 },
    { VLC_CODEC_WMAL,   TEXT("wma9dmod.dll"), &guid_wma9 },

    /* WMA 2 */
    { VLC_CODEC_WMA2,   TEXT("wma9dmod.dll"), &guid_wma9 },

    /* WMA Speech */
    { VLC_CODEC_WMAS,   TEXT("wmspdmod.dll"), &guid_wma },

    /* */
    { 0, NULL, NULL }
};

static const codec_dll encoders_table[] =
{
    /* WMV3 */
    { VLC_CODEC_WMV3, TEXT("wmvdmoe2.dll"), &guid_wmv_enc2 },
    /* WMV2 */
    { VLC_CODEC_WMV2, TEXT("wmvdmoe2.dll"), &guid_wmv_enc2 },
    /* WMV1 */
    { VLC_CODEC_WMV1, TEXT("wmvdmoe2.dll"), &guid_wmv_enc2 },

    /* WMA 3 */
    { VLC_CODEC_WMAP, TEXT("wmadmoe.dll"), &guid_wma_enc },
    /* WMA 2 */
    { VLC_CODEC_WMA2, TEXT("wmadmoe.dll"), &guid_wma_enc },

    /* WMV3 v11 */
    { VLC_CODEC_WMV3, TEXT("wmvencod.dll"), &guid_wmv9_enc },
    /* WMV2 v11 */
    { VLC_CODEC_WMV2, TEXT("wmvxencd.dll"), &guid_wmv8_enc },
    /* WMV1 v11 */
    { VLC_CODEC_WMV1, TEXT("wmvxencd.dll"), &guid_wmv8_enc },

    /* */
    { 0, NULL, NULL }
};

static void WINAPI DMOFreeMediaType( DMO_MEDIA_TYPE *mt )
{
    if( mt->cbFormat != 0 ) CoTaskMemFree( (PVOID)mt->pbFormat );
    if( mt->pUnk != NULL ) IUnknown_Release( mt->pUnk );
    mt->cbFormat = 0;
    mt->pbFormat = NULL;
    mt->pUnk = NULL;
}

/*****************************************************************************
 * DecoderOpen: open dmo codec
 *****************************************************************************/
static int DecoderOpen( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    /* We can't open it now, because of ldt_keeper or something
     * Open/Decode/Close has to be done in the same thread */

    /* Probe if we support it */
    for( unsigned i = 0; decoders_table[i].i_fourcc != 0; i++ )
    {
        if( decoders_table[i].i_fourcc == p_dec->fmt_in->i_codec )
        {
            msg_Dbg( p_dec, "DMO codec for %4.4s may work with dll=%ls",
                     (char*)&p_dec->fmt_in->i_codec, decoders_table[i].psz_dll);
            goto found;
        }
    }
    return VLC_EGENERIC;

found:
    p_sys = p_dec->p_sys = malloc(sizeof(*p_sys));
    if( !p_sys )
        return VLC_ENOMEM;

    /* Set callbacks */
    p_dec->pf_decode = DecodeBlock;

    vlc_mutex_init( &p_sys->lock );
    vlc_cond_init( &p_sys->wait_input );
    vlc_cond_init( &p_sys->wait_output );
    p_sys->b_works =
    p_sys->b_ready = false;
    p_sys->p_input = NULL;

    if( vlc_clone( &p_sys->thread, DecoderThread, p_dec ) )
        goto error;

    vlc_mutex_lock( &p_sys->lock );
    while( !p_sys->b_ready )
        vlc_cond_wait( &p_sys->wait_output, &p_sys->lock );
    vlc_mutex_unlock( &p_sys->lock );

    if( p_sys->b_works )
        return VLC_SUCCESS;

    vlc_join( p_sys->thread, NULL );
error:
    free( p_sys );
    return VLC_ENOMEM;
}

/*****************************************************************************
 * DecoderClose: close codec
 *****************************************************************************/
static void DecoderClose( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    vlc_mutex_lock( &p_sys->lock );
    p_sys->b_ready = false;
    vlc_cond_signal( &p_sys->wait_input );
    vlc_mutex_unlock( &p_sys->lock );

    vlc_join( p_sys->thread, NULL );
    free( p_sys );
}

static int DecodeBlock( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_block == NULL ) /* No Drain */
        return VLCDEC_SUCCESS;

    vlc_mutex_lock( &p_sys->lock );
    while( p_sys->p_input )
        vlc_cond_wait( &p_sys->wait_output, &p_sys->lock );
    p_sys->p_input = p_block;
    vlc_cond_signal( &p_sys->wait_input );
    vlc_mutex_unlock( &p_sys->lock );

    return VLCDEC_SUCCESS;
}

/*****************************************************************************
 * DecOpen: open dmo codec
 *****************************************************************************/
static int DecOpen( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    DMO_MEDIA_TYPE dmo_input_type, dmo_output_type;
    IMediaObject *p_dmo = NULL;
    HINSTANCE hmsdmo_dll = NULL;

    VIDEOINFOHEADER *p_vih = NULL;
    WAVEFORMATEX *p_wf = NULL;

    int i_ret = VLC_EGENERIC;

    /* Initialize OLE/COM */
    if( FAILED(CoInitializeEx( NULL, COINIT_MULTITHREADED )) )
        vlc_assert_unreachable();

    if( LoadDMO( VLC_OBJECT(p_dec), &hmsdmo_dll, &p_dmo, p_dec->fmt_in, false )
        != VLC_SUCCESS )
    {
        hmsdmo_dll = 0;
        p_dmo = NULL;
        goto error;
    }

    /* Setup input format */
    memset( &dmo_input_type, 0, sizeof(dmo_input_type) );
    dmo_input_type.pUnk = 0;

    if( p_dec->fmt_in->i_cat == AUDIO_ES )
    {
        uint16_t i_tag;
        int i_size = sizeof(WAVEFORMATEX) + p_dec->fmt_in->i_extra;
        p_wf = malloc( i_size );
        if (p_wf == NULL)
            goto allocation_failure;

        memset( p_wf, 0, sizeof(WAVEFORMATEX) );
        if( p_dec->fmt_in->i_extra )
            memcpy( &p_wf[1], p_dec->fmt_in->p_extra, p_dec->fmt_in->i_extra );

        dmo_input_type.majortype  = MEDIATYPE_Audio;
        dmo_input_type.subtype    = dmo_input_type.majortype;
        dmo_input_type.subtype.Data1 = p_dec->fmt_in->i_original_fourcc ?
                                p_dec->fmt_in->i_original_fourcc : p_dec->fmt_in->i_codec;
        fourcc_to_wf_tag( p_dec->fmt_in->i_codec, &i_tag );
        if( i_tag ) dmo_input_type.subtype.Data1 = i_tag;

        p_wf->wFormatTag = dmo_input_type.subtype.Data1;
        p_wf->nSamplesPerSec = p_dec->fmt_in->audio.i_rate;
        p_wf->nChannels = p_dec->fmt_in->audio.i_channels;
        p_wf->wBitsPerSample = p_dec->fmt_in->audio.i_bitspersample;
        p_wf->nBlockAlign = p_dec->fmt_in->audio.i_blockalign;
        p_wf->nAvgBytesPerSec = p_dec->fmt_in->i_bitrate / 8;
        p_wf->cbSize = p_dec->fmt_in->i_extra;

        dmo_input_type.formattype = FORMAT_WaveFormatEx;
        dmo_input_type.cbFormat   = i_size;
        dmo_input_type.pbFormat   = (BYTE *)p_wf;
        dmo_input_type.bFixedSizeSamples = 1;
        dmo_input_type.bTemporalCompression = 0;
        dmo_input_type.lSampleSize = p_wf->nBlockAlign;
    }
    else
    {
        BITMAPINFOHEADER *p_bih;

        int i_size = sizeof(VIDEOINFOHEADER) + p_dec->fmt_in->i_extra;
        p_vih = malloc( i_size );
        if (p_vih == NULL)
            goto allocation_failure;

        memset( p_vih, 0, sizeof(VIDEOINFOHEADER) );
        if( p_dec->fmt_in->i_extra )
            memcpy( &p_vih[1], p_dec->fmt_in->p_extra, p_dec->fmt_in->i_extra );

        vlc_fourcc_t fcc = p_dec->fmt_in->i_original_fourcc ?
                           p_dec->fmt_in->i_original_fourcc : p_dec->fmt_in->i_codec;

        p_bih = &p_vih->bmiHeader;
        p_bih->biCompression = fcc;
        p_bih->biWidth = p_dec->fmt_in->video.i_width;
        p_bih->biHeight = p_dec->fmt_in->video.i_height;
        p_bih->biBitCount = vlc_fourcc_GetChromaBPP(fcc);
        if ( p_bih->biBitCount == 0 &&
             p_dec->fmt_in->i_profile == -1 && p_dec->fmt_in->i_level != -1 )
            // HACK: the bits per sample was stored in the i_level
            p_bih->biBitCount = p_dec->fmt_in->i_level;
        p_bih->biPlanes = 1;
        p_bih->biSize = i_size - sizeof(VIDEOINFOHEADER) + sizeof(*p_bih);

        p_vih->rcSource.left = p_vih->rcSource.top = 0;
        p_vih->rcSource.right = p_dec->fmt_in->video.i_width;
        p_vih->rcSource.bottom = p_dec->fmt_in->video.i_height;
        p_vih->rcTarget = p_vih->rcSource;

        dmo_input_type.majortype  = MEDIATYPE_Video;
        dmo_input_type.subtype    = dmo_input_type.majortype;
        dmo_input_type.subtype.Data1 = fcc;
        dmo_input_type.formattype = FORMAT_VideoInfo;
        dmo_input_type.bFixedSizeSamples = 0;
        dmo_input_type.bTemporalCompression = 1;
        dmo_input_type.cbFormat = i_size;
        dmo_input_type.pbFormat = (BYTE *)p_vih;
    }

    if( IMediaObject_SetInputType( p_dmo, 0, &dmo_input_type, 0 ) )
    {
        msg_Err( p_dec, "can't set DMO input type" );
        goto error;
    }
    msg_Dbg( p_dec, "DMO input type set" );

    /* Setup output format */
    memset( &dmo_output_type, 0, sizeof(dmo_output_type) );
    dmo_output_type.pUnk = 0;

    if( p_dec->fmt_in->i_cat == AUDIO_ES )
    {
        /* Setup the format */
        p_dec->fmt_out.i_codec = VLC_CODEC_S16N;
        p_dec->fmt_out.audio.i_rate     = p_dec->fmt_in->audio.i_rate;
        p_dec->fmt_out.audio.i_channels = p_dec->fmt_in->audio.i_channels;
        p_dec->fmt_out.audio.i_bitspersample = 16;//p_dec->fmt_in->audio.i_bitspersample; We request 16
        if( p_dec->fmt_in->audio.i_channels > 8 )
            goto error;
        p_dec->fmt_out.audio.i_physical_channels =
            vlc_chan_maps[p_dec->fmt_out.audio.i_channels];

        p_wf->wFormatTag = WAVE_FORMAT_PCM;
        p_wf->nSamplesPerSec = p_dec->fmt_out.audio.i_rate;
        p_wf->nChannels = p_dec->fmt_out.audio.i_channels;
        p_wf->wBitsPerSample = p_dec->fmt_out.audio.i_bitspersample;
        p_wf->nBlockAlign =
            p_wf->wBitsPerSample / 8 * p_wf->nChannels;
        p_wf->nAvgBytesPerSec =
            p_wf->nSamplesPerSec * p_wf->nBlockAlign;
        p_wf->cbSize = 0;

        dmo_output_type.majortype  = MEDIATYPE_Audio;
        dmo_output_type.formattype = FORMAT_WaveFormatEx;
        dmo_output_type.subtype    = MEDIASUBTYPE_PCM;
        dmo_output_type.cbFormat   = sizeof(WAVEFORMATEX);
        dmo_output_type.pbFormat   = (BYTE *)p_wf;
        dmo_output_type.bFixedSizeSamples = 1;
        dmo_output_type.bTemporalCompression = 0;
        dmo_output_type.lSampleSize = p_wf->nBlockAlign;
    }
    else
    {
        BITMAPINFOHEADER *p_bih;
        DMO_MEDIA_TYPE mt;
        unsigned i_chroma = VLC_CODEC_YUYV;
        int i = 0;

        /* Find out which chroma to use */
        while( !IMediaObject_GetOutputType( p_dmo, 0, i++, &mt ) )
        {
            if( mt.subtype.Data1 == VLC_CODEC_YV12 )
            {
                i_chroma = mt.subtype.Data1;
                DMOFreeMediaType( &mt );
                break;
            }
            else if( (p_dec->fmt_in->i_codec == VLC_CODEC_MSS1 ||
                      p_dec->fmt_in->i_codec == VLC_CODEC_MSS2 ) &&
                      IsEqualGUID( &mt.subtype, &MEDIASUBTYPE_RGB24 ) )
            {
                i_chroma = VLC_CODEC_BGR24;
            }

            DMOFreeMediaType( &mt );
        }

        p_dec->fmt_out.i_codec = i_chroma == VLC_CODEC_YV12 ? VLC_CODEC_I420 : i_chroma;
        p_dec->fmt_out.video.i_width = p_dec->fmt_in->video.i_width;
        p_dec->fmt_out.video.i_height = p_dec->fmt_in->video.i_height;

        /* If an aspect-ratio was specified in the input format then force it */
        if( p_dec->fmt_in->video.i_sar_num > 0 &&
            p_dec->fmt_in->video.i_sar_den > 0 )
        {
            p_dec->fmt_out.video.i_sar_num = p_dec->fmt_in->video.i_sar_num;
            p_dec->fmt_out.video.i_sar_den = p_dec->fmt_in->video.i_sar_den;
        }
        else
        {
            p_dec->fmt_out.video.i_sar_num = 1;
            p_dec->fmt_out.video.i_sar_den = 1;
        }

        p_bih = &p_vih->bmiHeader;
        p_bih->biCompression = i_chroma == VLC_CODEC_RGB24 ? BI_RGB : i_chroma;
        p_bih->biHeight *= -1;
        p_bih->biBitCount = vlc_fourcc_GetChromaBPP(i_chroma);
        p_bih->biSizeImage = p_dec->fmt_in->video.i_width *
            p_dec->fmt_in->video.i_height *
            ((p_bih->biBitCount + 7) / 8);

        p_bih->biPlanes = 1; /* http://msdn.microsoft.com/en-us/library/dd183376%28v=vs.85%29.aspx */
        p_bih->biSize = sizeof(*p_bih);

        dmo_output_type.majortype = MEDIATYPE_Video;
        dmo_output_type.formattype = FORMAT_VideoInfo;
        if( i_chroma == VLC_CODEC_BGR24 )
        {
            dmo_output_type.subtype = MEDIASUBTYPE_RGB24;
        }
        else
        {
            dmo_output_type.subtype = dmo_output_type.majortype;
            dmo_output_type.subtype.Data1 = p_bih->biCompression;
        }
        dmo_output_type.bFixedSizeSamples = true;
        dmo_output_type.bTemporalCompression = 0;
        dmo_output_type.lSampleSize = p_bih->biSizeImage;
        dmo_output_type.cbFormat = sizeof(VIDEOINFOHEADER);
        dmo_output_type.pbFormat = (BYTE *)p_vih;
    }

#ifdef DMO_DEBUG
    /* Enumerate output types */
    if( p_dec->fmt_in->i_cat == VIDEO_ES )
    {
        int i = 0;
        DMO_MEDIA_TYPE mt;

        while( !IMediaObject_GetOutputType( p_dmo, 0, i++, &mt ) )
        {
            msg_Dbg( p_dec, "available output chroma: %4.4s", (char *)&mt.subtype.Data1 );
            DMOFreeMediaType( &mt );
        }
    }
#endif

    unsigned i_err = IMediaObject_SetOutputType( p_dmo, 0, &dmo_output_type, 0 );
    if( i_err )
    {
        msg_Err( p_dec, "can't set DMO output type for decoder: 0x%x", i_err );
        goto error;
    }
    msg_Dbg( p_dec, "DMO output type set for decoder" );

    /* Allocate the memory needed to store the decoder's structure */
    p_sys->hmsdmo_dll = hmsdmo_dll;
    p_sys->p_dmo = p_dmo;

    /* Find out some properties of the output */
    {
        DWORD i_size, i_align;

        p_sys->i_min_output = 0;
        if( IMediaObject_GetOutputSizeInfo( p_dmo, 0, &i_size, &i_align ) )
        {
            msg_Err( p_dec, "GetOutputSizeInfo() failed" );
            goto error;
        }
        else
        {
            msg_Dbg( p_dec, "GetOutputSizeInfo(): bytes %ld align %ld",
                     i_size, i_align );
            p_sys->i_min_output = i_size;
            p_sys->p_buffer = malloc( i_size );
            if( !p_sys->p_buffer ) goto error;
        }
    }

    /* Set output properties */
    p_dec->fmt_out.i_cat = p_dec->fmt_in->i_cat;
    if( p_dec->fmt_out.i_cat == AUDIO_ES )
        date_Init( &p_sys->end_date, p_dec->fmt_in->audio.i_rate, 1 );
    else
        date_Init( &p_sys->end_date, 25 /* FIXME */, 1 );
    date_Set( &p_sys->end_date, VLC_TICK_0 );

    free( p_vih );
    free( p_wf );

    vlc_mutex_lock( &p_sys->lock );
    p_sys->b_ready =
    p_sys->b_works = true;
    vlc_cond_signal( &p_sys->wait_output );
    vlc_mutex_unlock( &p_sys->lock );

    return VLC_SUCCESS;

allocation_failure:
    i_ret = VLC_ENOMEM;

error:
    if( p_dmo ) IMediaObject_Release( p_dmo );
    if( hmsdmo_dll ) FreeLibrary( hmsdmo_dll );

    /* Uninitialize OLE/COM */
    CoUninitialize();

    free( p_vih );
    free( p_wf );

    vlc_mutex_lock( &p_sys->lock );
    p_sys->b_ready = true;
    vlc_cond_signal( &p_sys->wait_output );
    vlc_mutex_unlock( &p_sys->lock );
    return i_ret;
}

/*****************************************************************************
 * LoadDMO: Load the DMO object
 *****************************************************************************/
static int LoadDMO( vlc_object_t *p_this, HINSTANCE *p_hmsdmo_dll,
                    IMediaObject **pp_dmo, const es_format_t *p_fmt,
                    bool b_out )
{
    DMO_PARTIAL_MEDIATYPE dmo_partial_type;
    int i_err;

    long (STDCALL *OurDMOEnum)( const GUID *, DWORD, DWORD,
                               const DMO_PARTIAL_MEDIATYPE *,
                               DWORD, const DMO_PARTIAL_MEDIATYPE *,
                               IEnumDMO ** );

    IEnumDMO *p_enum_dmo = NULL;
    WCHAR *psz_dmo_name;
    GUID clsid_dmo;
    DWORD i_dummy;

    GETCLASS GetClass;
    void *pv;
    IClassFactory *cFactory = NULL;
    IUnknown *cObject = NULL;
    const codec_dll *codecs_table = b_out ? encoders_table : decoders_table;
    int i_codec;

    /* Look for a DMO which can handle the requested codec */
    if( p_fmt->i_cat == AUDIO_ES )
    {
        uint16_t i_tag;
        dmo_partial_type.type = MEDIATYPE_Audio;
        dmo_partial_type.subtype = dmo_partial_type.type;
        dmo_partial_type.subtype.Data1 = p_fmt->i_original_fourcc ?
                                        p_fmt->i_original_fourcc : p_fmt->i_codec;
        fourcc_to_wf_tag( p_fmt->i_codec, &i_tag );
        if( i_tag ) dmo_partial_type.subtype.Data1 = i_tag;
    }
    else
    {
        dmo_partial_type.type = MEDIATYPE_Video;
        dmo_partial_type.subtype = dmo_partial_type.type;
        dmo_partial_type.subtype.Data1 = p_fmt->i_original_fourcc ?
                                        p_fmt->i_original_fourcc : p_fmt->i_codec;
    }

    /* Load msdmo DLL */
    *p_hmsdmo_dll = LoadLibrary( TEXT( "msdmo.dll" ) );
    if( *p_hmsdmo_dll == NULL )
    {
        msg_Dbg( p_this, "failed loading msdmo.dll" );
        return VLC_EGENERIC;
    }
    OurDMOEnum = (void *)GetProcAddress( *p_hmsdmo_dll, "DMOEnum" );
    if( OurDMOEnum == NULL )
    {
        msg_Dbg( p_this, "GetProcAddress failed to find DMOEnum()" );
        FreeLibrary( *p_hmsdmo_dll );
        return VLC_EGENERIC;
    }

    if( !b_out )
    {
        i_err = OurDMOEnum( &GUID_NULL, 1 /*DMO_ENUMF_INCLUDE_KEYED*/,
                            1, &dmo_partial_type, 0, NULL, &p_enum_dmo );
    }
    else
    {
        i_err = OurDMOEnum( &GUID_NULL, 1 /*DMO_ENUMF_INCLUDE_KEYED*/,
                            0, NULL, 1, &dmo_partial_type, &p_enum_dmo );
    }
    if( i_err )
    {
        FreeLibrary( *p_hmsdmo_dll );
        /* return VLC_EGENERIC; */
        /* Try loading the dll directly */
        goto loader;
    }

    /* Pickup the first available codec */
    *pp_dmo = NULL;
    while( ( S_OK == IEnumDMO_Next( p_enum_dmo, 1, &clsid_dmo,
                     &psz_dmo_name, &i_dummy /* NULL doesn't work */ ) ) )
    {
        msg_Dbg( p_this, "found DMO: %ls", psz_dmo_name );
        CoTaskMemFree( psz_dmo_name );

        /* Create DMO */
        if( CoCreateInstance( &clsid_dmo, NULL, CLSCTX_INPROC,
                              &IID_IMediaObject, &pv ) )
        {
            msg_Warn( p_this, "can't create DMO" );
        }
        else
        {
            *pp_dmo = pv;
            break;
        }
    }

    IEnumDMO_Release( p_enum_dmo );

    if( !*pp_dmo )
    {
        FreeLibrary( *p_hmsdmo_dll );
        /* return VLC_EGENERIC; */
        /* Try loading the dll directly */
        goto loader;
    }

    return VLC_SUCCESS;

loader:

    for( i_codec = 0; codecs_table[i_codec].i_fourcc != 0; i_codec++ )
    {
        if( codecs_table[i_codec].i_fourcc == p_fmt->i_codec )
            break;
    }
    if( codecs_table[i_codec].i_fourcc == 0 )
        return VLC_EGENERIC;    /* Can't happen */

    *p_hmsdmo_dll = LoadLibrary( codecs_table[i_codec].psz_dll );
    if( *p_hmsdmo_dll == NULL )
    {
        msg_Dbg( p_this, "failed loading '%ls'", codecs_table[i_codec].psz_dll );
        return VLC_EGENERIC;
    }

    GetClass = (GETCLASS)GetProcAddress( *p_hmsdmo_dll, "DllGetClassObject" );
    if (!GetClass)
    {
        msg_Dbg( p_this, "GetProcAddress failed to find DllGetClassObject()" );
        FreeLibrary( *p_hmsdmo_dll );
        return VLC_EGENERIC;
    }

    i_err = GetClass( codecs_table[i_codec].p_guid, &IID_IClassFactory, &pv );

    if( i_err || pv == NULL )
    {
        msg_Dbg( p_this, "no such class object" );
        FreeLibrary( *p_hmsdmo_dll );
        return VLC_EGENERIC;
    }
    cFactory = pv;

    i_err = IClassFactory_CreateInstance( cFactory, 0, &IID_IUnknown, &pv );
    IClassFactory_Release( cFactory );
    if( i_err || !pv )
    {
        msg_Dbg( p_this, "class factory failure" );
        FreeLibrary( *p_hmsdmo_dll );
        return VLC_EGENERIC;
    }
    i_err = IUnknown_QueryInterface( cObject, &IID_IMediaObject, &pv );
    IUnknown_Release( cObject );
    if( i_err || !*pp_dmo )
    {
        msg_Dbg( p_this, "QueryInterface failure" );
        FreeLibrary( *p_hmsdmo_dll );
        return VLC_EGENERIC;
    }
    *pp_dmo = pv;

    return VLC_SUCCESS;
}

static void DecClose( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_dmo ) IMediaObject_Release( p_sys->p_dmo );
    FreeLibrary( p_sys->hmsdmo_dll );

    CoUninitialize();

    free( p_sys->p_buffer );
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function must be fed with packets.
 ****************************************************************************/
static int DecBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block;
    int i_result;

    DMO_OUTPUT_DATA_BUFFER db;
    CMediaBuffer *p_out;
    block_t block_out;
    DWORD i_status;

    p_block = *pp_block;

    /* Won't work with streams with B-frames, but do we have any ? */
    if( p_block && p_block->i_pts == VLC_TICK_INVALID )
        p_block->i_pts = p_block->i_dts;

    /* Date management */
    if( p_block && p_block->i_pts != VLC_TICK_INVALID &&
        p_block->i_pts != date_Get( &p_sys->end_date ) )
    {
        date_Set( &p_sys->end_date, p_block->i_pts );
    }

#if 0 /* Breaks the video decoding */
    if( date_Get( &p_sys->end_date ) == VLC_TICK_INVALID )
    {
        /* We've just started the stream, wait for the first PTS. */
        if( p_block ) block_Release( p_block );
        return -1;
    }
#endif

    /* Feed input to the DMO */
    if( p_block && p_block->i_buffer )
    {
        CMediaBuffer *p_in;

        p_in = CMediaBufferCreate( p_block, p_block->i_buffer, true );

        i_result = IMediaObject_ProcessInput( p_sys->p_dmo, 0,
                       &p_in->intf, DMO_INPUT_DATA_BUFFERF_SYNCPOINT,
                       0, 0 );

        *pp_block = NULL;
        IMediaBuffer_Release( &p_in->intf );

        if( i_result == S_FALSE )
        {
            /* No output generated */
#ifdef DMO_DEBUG
            msg_Dbg( p_dec, "ProcessInput(): no output generated" );
#endif
            return -1;
        }
        else if( i_result == (int)DMO_E_NOTACCEPTING )
        {
            /* Need to call ProcessOutput */
            msg_Dbg( p_dec, "ProcessInput(): not accepting" );
        }
        else if( i_result != S_OK )
        {
            msg_Dbg( p_dec, "ProcessInput(): failed" );
            return -1;
        }
        else
        {
#ifdef DMO_DEBUG
            msg_Dbg( p_dec, "ProcessInput(): successful" );
#endif
        }
    }

    /* Get output from the DMO */
    block_out.p_buffer = p_sys->p_buffer;
    block_out.i_buffer = 0;

    p_out = CMediaBufferCreate( &block_out, p_sys->i_min_output, false );
    memset( &db, 0, sizeof(db) );
    db.pBuffer = &p_out->intf;

    i_result = IMediaObject_ProcessOutput( p_sys->p_dmo,
                   DMO_PROCESS_OUTPUT_DISCARD_WHEN_NO_BUFFER,
                   1, &db, &i_status );

    if( i_result != S_OK )
    {
        if( i_result != S_FALSE )
            msg_Dbg( p_dec, "ProcessOutput(): failed" );
#ifdef DMO_DEBUG
        else
            msg_Dbg( p_dec, "ProcessOutput(): no output" );
#endif

        IMediaBuffer_Release( &p_out->intf );
        return -1;
    }

#ifdef DMO_DEBUG
    msg_Dbg( p_dec, "ProcessOutput(): success" );
#endif

    if( !block_out.i_buffer )
    {
#ifdef DMO_DEBUG
        msg_Dbg( p_dec, "ProcessOutput(): no output (i_buffer_out == 0)" );
#endif
        IMediaBuffer_Release( &p_out->intf );
        return -1;
    }

    if( p_dec->fmt_out.i_cat == VIDEO_ES )
    {
        /* Get a new picture */
        if( decoder_UpdateVideoFormat( p_dec ) )
            return -1;
        picture_t *p_pic = decoder_NewPicture( p_dec );
        if( !p_pic ) return -1;

        CopyPicture( p_pic, block_out.p_buffer );

        /* Date management */
        p_pic->date = date_Get( &p_sys->end_date );
        date_Increment( &p_sys->end_date, 1 );

        IMediaBuffer_Release( &p_out->intf );

        decoder_QueueVideo( p_dec, p_pic );
        return 0;
    }
    else
    {
        if( decoder_UpdateAudioFormat( p_dec ) )
        {
            IMediaBuffer_Release( &p_out->intf );
            return -1;
        }

        block_t *p_aout_buffer;
        int i_samples = block_out.i_buffer /
            ( p_dec->fmt_out.audio.i_bitspersample *
              p_dec->fmt_out.audio.i_channels / 8 );

        p_aout_buffer = decoder_NewAudioBuffer( p_dec, i_samples );
        if( p_aout_buffer )
        {
            memcpy( p_aout_buffer->p_buffer,
                    block_out.p_buffer, block_out.i_buffer );
            /* Date management */
            p_aout_buffer->i_pts = date_Get( &p_sys->end_date );
            p_aout_buffer->i_length =
                date_Increment( &p_sys->end_date, i_samples )
                - p_aout_buffer->i_pts;
        }
        IMediaBuffer_Release( &p_out->intf );

        decoder_QueueAudio( p_dec, p_aout_buffer );
        return 0;
    }
}

static void CopyPicture( picture_t *p_pic, uint8_t *p_in )
{
    int i_plane, i_line, i_width, i_dst_stride;
    uint8_t *p_dst, *p_src = p_in;

    picture_SwapUV( p_pic );

    for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
    {
        p_dst = p_pic->p[i_plane].p_pixels;
        i_width = p_pic->p[i_plane].i_visible_pitch;
        i_dst_stride  = p_pic->p[i_plane].i_pitch;

        for( i_line = 0; i_line < p_pic->p[i_plane].i_visible_lines; i_line++ )
        {
            memcpy( p_dst, p_src, i_width );
            p_src += i_width;
            p_dst += i_dst_stride;
        }
    }

    picture_SwapUV( p_pic );
}

static void *DecoderThread( void *data )
{
    decoder_t *p_dec = data;
    decoder_sys_t *p_sys = p_dec->p_sys;

    vlc_thread_set_name("vlc-dmo");

    if( DecOpen( p_dec ) )
        return NULL; /* failed */

    vlc_mutex_lock( &p_sys->lock );
    for( ;; )
    {
        while( p_sys->b_ready && !p_sys->p_input )
            vlc_cond_wait( &p_sys->wait_input, &p_sys->lock );
        if( !p_sys->b_ready )
            break;

        while( DecBlock( p_dec, &p_sys->p_input ) == 0 );

        if( p_sys->p_input != NULL )
            block_Release( p_sys->p_input );
        p_sys->p_input = NULL;

        vlc_cond_signal( &p_sys->wait_output );
    }
    vlc_mutex_unlock( &p_sys->lock );

    DecClose( p_dec );
    return NULL;
}


#ifdef ENABLE_SOUT
/****************************************************************************
 * Encoder descriptor declaration
 ****************************************************************************/
typedef struct
{
    HINSTANCE hmsdmo_dll;
    IMediaObject *p_dmo;

    int i_min_output;

    date_t end_date;

} encoder_sys_t;

static block_t *EncodeVideo( encoder_t *p_enc, picture_t *pic )
    { return EncodeBlock( p_enc, pic ); }

static block_t *EncodeAudio( encoder_t *p_enc, block_t *samples )
    { return EncodeBlock( p_enc, samples ); }

/*****************************************************************************
 * EncoderOpenAudio: open audio dmo codec
 *****************************************************************************/
static int EncoderOpenAudio(vlc_object_t *p_this)
{
    encoder_t *p_enc = (encoder_t*)p_this;

    int i_ret = EncOpen( p_this );
    if( i_ret != VLC_SUCCESS ) return i_ret;


    static const struct vlc_encoder_operations audio_ops =
    {
        .close = EncoderClose,
        .encode_audio = EncodeAudio,
    };
    p_enc->ops = &audio_ops;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * EncoderOpenVideo: open video dmo codec
 *****************************************************************************/
static int EncoderOpenVideo(vlc_object_t *p_this)
{
    encoder_t *p_enc = (encoder_t*)p_this;

    int i_ret = EncOpen( p_this );
    if( i_ret != VLC_SUCCESS ) return i_ret;

    static const struct vlc_encoder_operations video_ops =
    {
        .close = EncoderClose,
        .encode_video = EncodeVideo,
    };
    p_enc->ops = &video_ops;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * EncoderSetVideoType: configures the input and output types of the dmo
 *****************************************************************************/
static int EncoderSetVideoType( encoder_t *p_enc, IMediaObject *p_dmo )
{
    int i, i_selected, i_err;
    DMO_MEDIA_TYPE dmo_type;
    VIDEOINFOHEADER vih, *p_vih;
    BITMAPINFOHEADER *p_bih;

    /* Enumerate input format (for debug output) */
    i = 0;
    while( !IMediaObject_GetInputType( p_dmo, 0, i++, &dmo_type ) )
    {
        p_vih = (VIDEOINFOHEADER *)dmo_type.pbFormat;

        msg_Dbg( p_enc, "available input chroma: %4.4s",
                 (char *)&dmo_type.subtype.Data1 );
        if( !memcmp( &dmo_type.subtype, &MEDIASUBTYPE_RGB565, 16 ) )
            msg_Dbg( p_enc, "-> MEDIASUBTYPE_RGB565" );
        if( !memcmp( &dmo_type.subtype, &MEDIASUBTYPE_RGB24, 16 ) )
            msg_Dbg( p_enc, "-> MEDIASUBTYPE_RGB24" );

        DMOFreeMediaType( &dmo_type );
    }

    /* Setup input format */
    memset( &dmo_type, 0, sizeof(dmo_type) );
    memset( &vih, 0, sizeof(VIDEOINFOHEADER) );

    p_bih = &vih.bmiHeader;
    p_bih->biCompression = VLC_CODEC_I420;
    p_bih->biWidth = p_enc->fmt_in.video.i_visible_width;
    p_bih->biHeight = p_enc->fmt_in.video.i_visible_height;
    p_bih->biBitCount = vlc_fourcc_GetChromaBPP(VLC_CODEC_I420);
    p_bih->biSizeImage = p_enc->fmt_in.video.i_visible_width *
        p_enc->fmt_in.video.i_visible_height * ((p_bih->biBitCount + 7) /8);
    p_bih->biPlanes = 3;
    p_bih->biSize = sizeof(*p_bih);

    vih.rcSource.left = vih.rcSource.top = 0;
    vih.rcSource.right = p_enc->fmt_in.video.i_visible_width;
    vih.rcSource.bottom = p_enc->fmt_in.video.i_visible_height;
    vih.rcTarget = vih.rcSource;

    vih.AvgTimePerFrame = INT64_C(10000000) / 25; //FIXME

    dmo_type.majortype = MEDIATYPE_Video;
    //dmo_type.subtype = MEDIASUBTYPE_RGB24;
    dmo_type.subtype = MEDIASUBTYPE_I420;
    //dmo_type.subtype.Data1 = p_bih->biCompression;
    dmo_type.formattype = FORMAT_VideoInfo;
    dmo_type.bFixedSizeSamples = TRUE;
    dmo_type.bTemporalCompression = FALSE;
    dmo_type.lSampleSize = p_bih->biSizeImage;
    dmo_type.cbFormat = sizeof(VIDEOINFOHEADER);
    dmo_type.pbFormat = (BYTE *)&vih;

    if( ( i_err = IMediaObject_SetInputType( p_dmo, 0, &dmo_type, 0 ) ) )
    {
        msg_Err( p_enc, "can't set DMO input type: %x", i_err );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_enc, "successfully set input type" );

    /* Setup output format */
    memset( &dmo_type, 0, sizeof(dmo_type) );
    dmo_type.pUnk = 0;

    /* Enumerate output types */
    i = 0, i_selected = -1;
    while( !IMediaObject_GetOutputType( p_dmo, 0, i++, &dmo_type ) )
    {
        p_vih = (VIDEOINFOHEADER *)dmo_type.pbFormat;

        msg_Dbg( p_enc, "available output codec: %4.4s",
                 (char *)&dmo_type.subtype.Data1 );

        if( p_vih->bmiHeader.biCompression == p_enc->fmt_out.i_codec )
            i_selected = i - 1;

        DMOFreeMediaType( &dmo_type );
    }

    if( i_selected < 0 )
    {
        msg_Err( p_enc, "couldn't find codec: %4.4s",
                 (char *)&p_enc->fmt_out.i_codec );
        return VLC_EGENERIC;
    }

    IMediaObject_GetOutputType( p_dmo, 0, i_selected, &dmo_type );
    ((VIDEOINFOHEADER *)dmo_type.pbFormat)->dwBitRate =
        p_enc->fmt_out.i_bitrate;

    /* Get the private data for the codec */
    while( 1 )
    {
        void *pv;
        IWMCodecPrivateData *p_privdata;
        uint8_t *p_data = 0;
        uint32_t i_data = 0, i_vih;

        i_err = IMediaObject_QueryInterface( p_dmo,
                                           &IID_IWMCodecPrivateData, &pv );
        if( i_err ) break;
        p_privdata = pv;

        i_err = p_privdata->vt->SetPartialOutputType( p_privdata, &dmo_type );
        if( i_err )
        {
            msg_Err( p_enc, "SetPartialOutputType() failed" );
            p_privdata->vt->Release( p_privdata );
            break;
        }

        i_err = p_privdata->vt->GetPrivateData( p_privdata, NULL, &i_data );
        if( i_err )
        {
            msg_Err( p_enc, "GetPrivateData() failed" );
            p_privdata->vt->Release( p_privdata );
            break;
        }

        p_data = malloc( i_data );
        if (p_data == NULL)
        {
            DMOFreeMediaType( &dmo_type );
            return VLC_ENOMEM;
        }
        i_err = p_privdata->vt->GetPrivateData( p_privdata, p_data, &i_data );

        /* Update the media type with the private data */
        i_vih = dmo_type.cbFormat + i_data;
        p_vih = CoTaskMemAlloc( i_vih );
        memcpy( p_vih, dmo_type.pbFormat, dmo_type.cbFormat );
        memcpy( ((uint8_t *)p_vih) + dmo_type.cbFormat, p_data, i_data );
        DMOFreeMediaType( &dmo_type );
        dmo_type.pbFormat = (BYTE*)p_vih;
        dmo_type.cbFormat = i_vih;

        msg_Dbg( p_enc, "found extra data: %i", i_data );
        p_enc->fmt_out.i_extra = i_data;
        p_enc->fmt_out.p_extra = p_data;
        break;
    }

    i_err = IMediaObject_SetOutputType( p_dmo, 0, &dmo_type, 0 );

    p_enc->fmt_in.video.i_chroma = p_enc->fmt_in.i_codec = VLC_CODEC_I420;

    DMOFreeMediaType( &dmo_type );
    if( i_err )
    {
        msg_Err( p_enc, "can't set DMO output type for encoder: 0x%x", i_err );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_enc, "successfully set output type for encoder" );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * EncoderSetAudioType: configures the input and output types of the dmo
 *****************************************************************************/
static int EncoderSetAudioType( encoder_t *p_enc, IMediaObject *p_dmo )
{
    int i, i_selected, i_err;
    unsigned int i_last_byterate;
    uint16_t i_tag;
    DMO_MEDIA_TYPE dmo_type;
    WAVEFORMATEX *p_wf;

    /* Setup the format structure */
    fourcc_to_wf_tag( p_enc->fmt_out.i_codec, &i_tag );
    if( i_tag == 0 ) return VLC_EGENERIC;

    p_enc->fmt_in.i_codec = VLC_CODEC_S16N;
    p_enc->fmt_in.audio.i_bitspersample = 16;

    /* We first need to choose an output type from the predefined
     * list of choices (we cycle through the list to select the best match) */
    i = 0; i_selected = -1; i_last_byterate = 0;
    while( !IMediaObject_GetOutputType( p_dmo, 0, i++, &dmo_type ) )
    {
        p_wf = (WAVEFORMATEX *)dmo_type.pbFormat;
        msg_Dbg( p_enc, "available format :%i, sample rate: %i, channels: %i, "
                 "bits per sample: %i, bitrate: %i, blockalign: %i",
                 (int) p_wf->wFormatTag, (int)p_wf->nSamplesPerSec,
                 (int)p_wf->nChannels, (int)p_wf->wBitsPerSample,
                 (int)p_wf->nAvgBytesPerSec * 8, (int)p_wf->nBlockAlign );

        if( p_wf->wFormatTag == i_tag &&
            p_wf->nSamplesPerSec == p_enc->fmt_in.audio.i_rate &&
            p_wf->nChannels == p_enc->fmt_in.audio.i_channels &&
            p_wf->wBitsPerSample == p_enc->fmt_in.audio.i_bitspersample )
        {
            if( p_wf->nAvgBytesPerSec <
                p_enc->fmt_out.i_bitrate * 110 / 800 /* + 10% */ &&
                p_wf->nAvgBytesPerSec > i_last_byterate )
            {
                i_selected = i - 1;
                i_last_byterate = p_wf->nAvgBytesPerSec;
                msg_Dbg( p_enc, "selected entry %i (bitrate: %lu)",
                         i_selected, p_wf->nAvgBytesPerSec * 8 );
            }
        }

        DMOFreeMediaType( &dmo_type );
    }

    if( i_selected < 0 )
    {
        msg_Err( p_enc, "couldn't find a matching output" );
        return VLC_EGENERIC;
    }

    IMediaObject_GetOutputType( p_dmo, 0, i_selected, &dmo_type );
    p_wf = (WAVEFORMATEX *)dmo_type.pbFormat;

    msg_Dbg( p_enc, "selected format: %i, sample rate:%i, "
             "channels: %i, bits per sample: %i, bitrate: %i, blockalign: %i",
             (int)p_wf->wFormatTag, (int)p_wf->nSamplesPerSec,
             (int)p_wf->nChannels, (int)p_wf->wBitsPerSample,
             (int)p_wf->nAvgBytesPerSec * 8, (int)p_wf->nBlockAlign );

    p_enc->fmt_out.audio.i_rate = p_wf->nSamplesPerSec;
    p_enc->fmt_out.audio.i_channels = p_wf->nChannels;
    p_enc->fmt_out.audio.i_bitspersample = p_wf->wBitsPerSample;
    p_enc->fmt_out.audio.i_blockalign = p_wf->nBlockAlign;
    p_enc->fmt_out.i_bitrate = p_wf->nAvgBytesPerSec * 8;

    if( p_wf->cbSize )
    {
        msg_Dbg( p_enc, "found cbSize: %i", p_wf->cbSize );
        p_enc->fmt_out.i_extra = p_wf->cbSize;
        p_enc->fmt_out.p_extra = malloc( p_enc->fmt_out.i_extra );
        if( p_enc->fmt_out.p_extra == NULL)
            return VLC_EGENERIC;

        memcpy( p_enc->fmt_out.p_extra, &p_wf[1], p_enc->fmt_out.i_extra );
    }

    i_err = IMediaObject_SetOutputType( p_dmo, 0, &dmo_type, 0 );
    DMOFreeMediaType( &dmo_type );

    if( i_err )
    {
        msg_Err( p_enc, "can't set DMO output type: %i", i_err );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_enc, "successfully set output type" );

    /* Setup the input type */
    i = 0; i_selected = -1;
    while( !IMediaObject_GetInputType( p_dmo, 0, i++, &dmo_type ) )
    {
        p_wf = (WAVEFORMATEX *)dmo_type.pbFormat;
        msg_Dbg( p_enc, "available format :%i, sample rate: %i, channels: %i, "
                 "bits per sample: %i, bitrate: %i, blockalign: %i",
                 (int) p_wf->wFormatTag, (int)p_wf->nSamplesPerSec,
                 (int)p_wf->nChannels, (int)p_wf->wBitsPerSample,
                 (int)p_wf->nAvgBytesPerSec * 8, (int)p_wf->nBlockAlign );

        if( p_wf->wFormatTag == WAVE_FORMAT_PCM &&
            p_wf->nSamplesPerSec == p_enc->fmt_in.audio.i_rate &&
            p_wf->nChannels == p_enc->fmt_in.audio.i_channels &&
            p_wf->wBitsPerSample == p_enc->fmt_in.audio.i_bitspersample )
        {
            i_selected = i - 1;
        }

        DMOFreeMediaType( &dmo_type );
    }

    if( i_selected < 0 )
    {
        msg_Err( p_enc, "couldn't find a matching input" );
        return VLC_EGENERIC;
    }

    IMediaObject_GetInputType( p_dmo, 0, i_selected, &dmo_type );
    i_err = IMediaObject_SetInputType( p_dmo, 0, &dmo_type, 0 );
    DMOFreeMediaType( &dmo_type );
    if( i_err )
    {
        msg_Err( p_enc, "can't set DMO input type: 0x%x", i_err );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_enc, "successfully set input type" );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * EncOpen: open dmo codec
 *****************************************************************************/
static int EncOpen( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t*)p_this;
    encoder_sys_t *p_sys = NULL;
    IMediaObject *p_dmo = NULL;
    HINSTANCE hmsdmo_dll = NULL;

    /* Initialize OLE/COM */
    if( FAILED(CoInitializeEx( NULL, COINIT_MULTITHREADED )) )
        vlc_assert_unreachable();

    if( LoadDMO( p_this, &hmsdmo_dll, &p_dmo, &p_enc->fmt_out, true )
        != VLC_SUCCESS )
    {
        hmsdmo_dll = 0;
        p_dmo = 0;
        goto error;
    }

    if( p_enc->fmt_in.i_cat == VIDEO_ES )
    {
        if( EncoderSetVideoType( p_enc, p_dmo ) != VLC_SUCCESS ) goto error;
    }
    else
    {
        if( EncoderSetAudioType( p_enc, p_dmo ) != VLC_SUCCESS ) goto error;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_enc->p_sys = p_sys = malloc(sizeof(*p_sys)) ) == NULL )
    {
        goto error;
    }

    p_sys->hmsdmo_dll = hmsdmo_dll;
    p_sys->p_dmo = p_dmo;

    /* Find out some properties of the inputput */
    {
        DWORD i_size, i_align, dum;

        if( IMediaObject_GetInputSizeInfo( p_dmo, 0, &i_size, &i_align, &dum ) )
            msg_Err( p_enc, "GetInputSizeInfo() failed" );
        else
            msg_Dbg( p_enc, "GetInputSizeInfo(): bytes %ld, align %ld, %ld",
                     i_size, i_align, dum );
    }

    /* Find out some properties of the output */
    {
        DWORD i_size, i_align;

        p_sys->i_min_output = 0;
        if( IMediaObject_GetOutputSizeInfo( p_dmo, 0, &i_size, &i_align ) )
        {
            msg_Err( p_enc, "GetOutputSizeInfo() failed" );
            goto error;
        }
        else
        {
            msg_Dbg( p_enc, "GetOutputSizeInfo(): bytes %ld, align %ld",
                     i_size, i_align );
            p_sys->i_min_output = i_size;
        }
    }

    /* Set output properties */
    p_enc->fmt_out.i_cat = p_enc->fmt_in.i_cat;
    if( p_enc->fmt_out.i_cat == AUDIO_ES )
        date_Init( &p_sys->end_date, p_enc->fmt_out.audio.i_rate, 1 );
    else
        date_Init( &p_sys->end_date, 25 /* FIXME */, 1 );

    return VLC_SUCCESS;

 error:

    if( p_dmo ) IMediaObject_Release( (IUnknown *)p_dmo );
    if( hmsdmo_dll ) FreeLibrary( hmsdmo_dll );

    /* Uninitialize OLE/COM */
    CoUninitialize();

    free( p_sys );

    return VLC_EGENERIC;
}

/****************************************************************************
 * Encode: the whole thing
 ****************************************************************************/
static block_t *EncodeBlock( encoder_t *p_enc, void *p_data )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    CMediaBuffer *p_in;
    block_t *p_chain = NULL;
    block_t *p_block_in;
    DWORD i_status;
    int i_result;
    vlc_tick_t i_pts;

    if( !p_data ) return NULL;

    if( p_enc->fmt_out.i_cat == VIDEO_ES )
    {
        /* Get picture data */
        picture_t *p_pic = (picture_t *)p_data;
        uint8_t *p_dst;

        const vlc_chroma_description_t *dsc = vlc_fourcc_GetChromaDescription(p_pic->format.i_chroma);
        assert(dsc != NULL);
        size_t i_buffer = 0;
        plane_t dst_planes[PICTURE_PLANE_MAX];
        for (unsigned plane=0; plane < dsc->plane_count; plane++)
        {
            dst_planes[plane].i_pitch = dst_planes[plane].i_visible_pitch =
                (p_enc->fmt_in.video.i_visible_width * dsc->pixel_size *
                 dsc->p[plane].w.num * dsc->p[plane].h.num) /
                (dsc->p[plane].w.den * dsc->p[plane].h.den);
            dst_planes[plane].i_lines = dst_planes[plane].i_visible_lines = p_enc->fmt_in.video.i_visible_height;
            i_buffer += dst_planes[plane].i_pitch * dst_planes[plane].i_lines;
        }

        p_block_in = block_Alloc( i_buffer );

        /* Copy picture stride by stride */
        p_dst = p_block_in->p_buffer;
        for(int i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
        {
            dst_planes[i_plane].p_pixels = p_dst;
            plane_CopyPixels(&dst_planes[i_plane], &p_pic->p[i_plane]);
            p_dst += dst_planes[i_plane].i_pitch * dst_planes[i_plane].i_lines;
        }

        i_pts = p_pic->date;
    }
    else
    {
        block_t *p_aout_buffer = (block_t *)p_data;
        p_block_in = block_Alloc( p_aout_buffer->i_buffer );
        memcpy( p_block_in->p_buffer, p_aout_buffer->p_buffer,
                p_block_in->i_buffer );

        i_pts = p_aout_buffer->i_pts;
    }

    /* Feed input to the DMO */
    p_in = CMediaBufferCreate( p_block_in, p_block_in->i_buffer, true );
    i_result = IMediaObject_ProcessInput( p_sys->p_dmo, 0,
       &p_in->intf, DMO_INPUT_DATA_BUFFERF_TIME, MSFTIME_FROM_VLC_TICK(i_pts), 0 );

    IMediaBuffer_Release( &p_in->intf );
    if( i_result == S_FALSE )
    {
        /* No output generated */
#ifdef DMO_DEBUG
        msg_Dbg( p_enc, "ProcessInput(): no output generated %"PRId64, i_pts );
#endif
        return NULL;
    }
    else if( i_result == (int)DMO_E_NOTACCEPTING )
    {
        /* Need to call ProcessOutput */
        msg_Dbg( p_enc, "ProcessInput(): not accepting" );
    }
    else if( i_result != S_OK )
    {
        msg_Dbg( p_enc, "ProcessInput(): failed: %x", i_result );
        return NULL;
    }

#ifdef DMO_DEBUG
    msg_Dbg( p_enc, "ProcessInput(): success" );
#endif

    /* Get output from the DMO */
    while( 1 )
    {
        DMO_OUTPUT_DATA_BUFFER db;
        block_t *p_block_out;
        CMediaBuffer *p_out;

        p_block_out = block_Alloc( p_sys->i_min_output );
        p_block_out->i_buffer = 0;
        p_out = CMediaBufferCreate(p_block_out, p_sys->i_min_output, false);
        memset( &db, 0, sizeof(db) );
        db.pBuffer = &p_out->intf;

        i_result = IMediaObject_ProcessOutput( p_sys->p_dmo,
                                                    0, 1, &db, &i_status );

        if( i_result != S_OK )
        {
            if( i_result != S_FALSE )
                msg_Dbg( p_enc, "ProcessOutput(): failed: %x", i_result );
#ifdef DMO_DEBUG
            else
                msg_Dbg( p_enc, "ProcessOutput(): no output" );
#endif

            IMediaBuffer_Release( &p_out->intf );
            block_Release( p_block_out );
            return p_chain;
        }

        if( !p_block_out->i_buffer )
        {
#ifdef DMO_DEBUG
            msg_Dbg( p_enc, "ProcessOutput(): no output (i_buffer_out == 0)" );
#endif
            IMediaBuffer_Release( &p_out->intf );
            block_Release( p_block_out );
            return p_chain;
        }

        if( db.dwStatus & DMO_OUTPUT_DATA_BUFFERF_TIME )
        {
#ifdef DMO_DEBUG
            msg_Dbg( p_enc, "ProcessOutput(): pts: %"PRId64", %"PRId64,
                     i_pts, VLC_TICK_FROM_MSFTIME(db.rtTimestamp) );
#endif
            i_pts = VLC_TICK_FROM_MSFTIME(db.rtTimestamp);
        }

        if( db.dwStatus & DMO_OUTPUT_DATA_BUFFERF_TIMELENGTH )
        {
            p_block_out->i_length = VLC_TICK_FROM_MSFTIME(db.rtTimelength);
#ifdef DMO_DEBUG
            msg_Dbg( p_enc, "ProcessOutput(): length: %"PRId64,
                     p_block_out->i_length );
#endif
        }

        if( p_enc->fmt_out.i_cat == VIDEO_ES )
        {
            if( db.dwStatus & DMO_OUTPUT_DATA_BUFFERF_SYNCPOINT )
                p_block_out->i_flags |= BLOCK_FLAG_TYPE_I;
            else
                p_block_out->i_flags |= BLOCK_FLAG_TYPE_P;
        }

        p_block_out->i_dts = p_block_out->i_pts = i_pts;
        block_ChainAppend( &p_chain, p_block_out );
    }
}

/*****************************************************************************
 * EncoderClose: close codec
 *****************************************************************************/
void EncoderClose( encoder_t *p_enc )
{
    encoder_sys_t *p_sys = p_enc->p_sys;

    if( !p_sys ) return;

    if( p_sys->p_dmo ) IMediaObject_Release( p_sys->p_dmo );
    FreeLibrary( p_sys->hmsdmo_dll );

    /* Uninitialize OLE/COM */
    CoUninitialize();

    free( p_sys );
}
#endif // !ENABLE_SOUT

vlc_module_begin ()
    set_description( N_("DirectMedia Object decoder") )
    add_shortcut( "dmo" )
    set_capability( "video decoder", 1 )
    set_callbacks( DecoderOpen, DecoderClose )
    set_subcategory( SUBCAT_INPUT_VCODEC )

    add_submodule()
    add_shortcut("dmo")
    set_capability( "audio decoder", 1 )
    set_callbacks(DecoderOpen, DecoderClose)

#ifdef ENABLE_SOUT
    add_submodule ()
    set_description( N_("DirectMedia Object encoder") )
    add_shortcut( "dmo" )
    set_capability( "video encoder", 10 )
    set_callback( EncoderOpenVideo )

    add_submodule ()
    set_description( N_("DirectMedia Object encoder") )
    add_shortcut( "dmo" )
    set_capability( "audio encoder", 10 )
    set_callback( EncoderOpenAudio )
#endif
vlc_module_end ()
