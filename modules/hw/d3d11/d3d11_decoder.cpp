/*****************************************************************************
 * d3d11_decoder.cpp : D3D11 GPU block to picture decoder
 *****************************************************************************
 * Copyright © 2018-2022 VLC authors, VideoLAN and VideoLabs
 *
 * Authors: Steve Lhomme <robux4@gmail.com>
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

#include <vlc_common.h>
#include <vlc_codec.h>
#include <vlc_picture.h>
#include <vlc_modules.h>

#include "d3d11_filters.h"
#include <dxgi1_2.h>
#include <d3d11_1.h>

#include <new>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

struct d3d11_dec_sys {
    date_t             pts;
    const d3d_format_t *output_format = nullptr;
    vlc_video_context  *vctx = nullptr;
    vlc_decoder_device *dec_dev = nullptr;

    ~d3d11_dec_sys()
    {
        if (vctx)
            vlc_video_context_Release(vctx);
        if (dec_dev)
            vlc_decoder_device_Release(dec_dev);
    }
};

static block_t *DecodeBlock( decoder_t *p_dec, block_t *p_block )
{
    d3d11_dec_sys *p_sys = static_cast<d3d11_dec_sys*>(p_dec->p_sys);

    if( p_block->i_flags & (BLOCK_FLAG_CORRUPTED|BLOCK_FLAG_DISCONTINUITY) )
    {
        date_Set( &p_sys->pts, p_block->i_dts );
        if( p_block->i_flags & BLOCK_FLAG_CORRUPTED )
        {
            block_Release( p_block );
            return nullptr;
        }
    }

    if( p_block->i_pts <= VLC_TICK_INVALID && p_block->i_dts <= VLC_TICK_INVALID &&
        !date_Get( &p_sys->pts ) )
    {
        /* We've just started the stream, wait for the first PTS. */
        block_Release( p_block );
        return nullptr;
    }

    /* Date management: If there is a pts avaliable, use that. */
    if( p_block->i_pts > VLC_TICK_INVALID )
    {
        date_Set( &p_sys->pts, p_block->i_pts );
    }
    else if( p_block->i_dts > VLC_TICK_INVALID )
    {
        /* NB, davidf doesn't quite agree with this in general, it is ok
         * for rawvideo since it is in order (ie pts=dts), however, it
         * may not be ok for an out-of-order codec, so don't copy this
         * without thinking */
        date_Set( &p_sys->pts, p_block->i_dts );
    }

    return p_block;
}

static int DecodeFrame( decoder_t *p_dec, block_t *p_block )
{
    if( p_block == nullptr ) // No Drain needed
        return VLCDEC_SUCCESS;

    p_block = DecodeBlock( p_dec, p_block );
    if( p_block == nullptr )
        return VLCDEC_SUCCESS;

    HRESULT hr;
    ComPtr<ID3D11Resource> d3d11res;
    d3d11_dec_sys *p_sys = static_cast<d3d11_dec_sys*>(p_dec->p_sys);
    picture_t *p_pic = D3D11BLOCK_FROM_BLOCK(p_block)->d3d11_pic;
    assert( p_pic != nullptr );
    picture_sys_d3d11_t *src_sys = ActiveD3D11PictureSys(p_pic);

    if (unlikely(p_sys->output_format == nullptr))
    {
        D3D11_TEXTURE2D_DESC outDesc;
        src_sys->texture[0]->GetDesc(&outDesc);

        for (p_sys->output_format = DxgiGetRenderFormatList();
            p_sys->output_format->name != nullptr; ++p_sys->output_format)
        {
            if (p_sys->output_format->formatTexture == outDesc.Format &&
                is_d3d11_opaque(p_sys->output_format->fourcc))
                break;
        }
        if (unlikely(!p_sys->output_format->name))
        {
            msg_Err(p_dec, "Unknown texture format %d", outDesc.Format);
            block_Release( p_block );
            return VLC_EGENERIC;
        }
    }

    if (unlikely(p_sys->vctx == nullptr))
    {
        p_sys->vctx =
            D3D11CreateVideoContext(p_sys->dec_dev, p_sys->output_format->formatTexture);
        if (!p_sys->vctx)
        {
            block_Release( p_block );
            return VLC_EGENERIC;
        }

        if( decoder_UpdateVideoOutput( p_dec, p_sys->vctx ) )
        {
            block_Release( p_block );
            return VLCDEC_SUCCESS;
        }
    }

    p_pic->date = date_Get( &p_sys->pts );
    date_Increment( &p_sys->pts, 1 );

    if( p_block->i_flags & BLOCK_FLAG_INTERLACED_MASK )
    {
        p_pic->b_progressive = false;
        p_pic->i_nb_fields = (p_block->i_flags & BLOCK_FLAG_SINGLE_FIELD) ? 1 : 2;
        if( p_block->i_flags & BLOCK_FLAG_TOP_FIELD_FIRST )
            p_pic->b_top_field_first = true;
        else
            p_pic->b_top_field_first = false;
    }
    else
        p_pic->b_progressive = true;

    // replace the texture shared the DXGI device by a texture for our device
    assert(src_sys->sharedHandle != INVALID_HANDLE_VALUE);
    assert(src_sys->processorInput == nullptr);
    assert(src_sys->processorOutput == nullptr);
    assert(src_sys->texture[1] == nullptr);

    d3d11_decoder_device_t *dev_sys = GetD3D11OpaqueDevice(p_sys->dec_dev);
    ComPtr<ID3D11Device1> d3d11VLC1;
    dev_sys->d3d_dev.d3ddevice->QueryInterface(IID_GRAPHICS_PPV_ARGS(&d3d11VLC1));

    picture_sys_d3d11_t *pic_sys = ActiveD3D11PictureSys(p_pic);

    ID3D11Texture2D* sharedTex;
    hr = d3d11VLC1->OpenSharedResource1(pic_sys->sharedHandle, IID_GRAPHICS_PPV_ARGS(&sharedTex));
    if (unlikely(FAILED(hr)))
    {
        block_Release( p_block );
        return VLCDEC_SUCCESS;
    }

    ID3D11Texture2D *srcTex = src_sys->texture[0];
    auto *srcVctx = p_pic->context->vctx;
    src_sys->texture[0] = sharedTex;
    p_pic->context->vctx = vlc_video_context_Hold(p_sys->vctx);
    srcTex->Release();
    vlc_video_context_Release(srcVctx);

    for (int j = 0; j < DXGI_MAX_SHADER_VIEW; j++)
    {
        if (src_sys->renderSrc[j])
            src_sys->renderSrc[j]->Release();
    }
    D3D11_AllocateResourceView(vlc_object_logger(p_dec), dev_sys->d3d_dev.d3ddevice, p_sys->output_format,
                               src_sys->texture, src_sys->slice_index, src_sys->renderSrc);

    picture_Hold( p_pic ); // hold the picture we got from the block
    block_Release( p_block );
    decoder_QueueVideo( p_dec, p_pic );
    return VLCDEC_SUCCESS;
}

static void Flush( decoder_t *p_dec )
{
    d3d11_dec_sys *p_sys = static_cast<d3d11_dec_sys*>(p_dec->p_sys);

    date_Set( &p_sys->pts, VLC_TICK_INVALID );
}

void D3D11CloseBlockDecoder( vlc_object_t *obj )
{
    decoder_t *p_dec = (decoder_t *)obj;
    d3d11_dec_sys *p_sys = static_cast<d3d11_dec_sys*>(p_dec->p_sys);
    delete p_sys;
}

int D3D11OpenBlockDecoder( vlc_object_t *obj )
{
    decoder_t *p_dec = (decoder_t *)obj;

    if ( !is_d3d11_opaque(p_dec->p_fmt_in->video.i_chroma) )
        return VLC_EGENERIC;
    if( p_dec->p_fmt_in->video.i_width <= 0 || p_dec->p_fmt_in->video.i_height == 0 )
    {
        msg_Err( p_dec, "invalid display size %dx%d",
                 p_dec->p_fmt_in->video.i_width, p_dec->p_fmt_in->video.i_height );
        return VLC_EGENERIC;
    }

    vlc_decoder_device *dec_dev = decoder_GetDecoderDevice(p_dec);
    if (dec_dev == nullptr)
        return VLC_ENOTSUP;
    auto dev_sys = GetD3D11OpaqueDevice(dec_dev);
    if (dev_sys == nullptr)
    {
        vlc_decoder_device_Release(dec_dev);
        return VLC_ENOTSUP;
    }

    d3d11_dec_sys *p_sys = new (std::nothrow) d3d11_dec_sys();
    if (unlikely(!p_sys))
    {
        vlc_decoder_device_Release(dec_dev);
        return VLC_ENOMEM;
    }
    p_sys->dec_dev = dec_dev;

    es_format_Copy( &p_dec->fmt_out, p_dec->p_fmt_in );

    if( !p_dec->fmt_out.video.i_visible_width )
        p_dec->fmt_out.video.i_visible_width = p_dec->fmt_out.video.i_width;
    if( !p_dec->fmt_out.video.i_visible_height )
        p_dec->fmt_out.video.i_visible_height = p_dec->fmt_out.video.i_height;

    if( p_dec->fmt_out.video.i_frame_rate == 0 ||
        p_dec->fmt_out.video.i_frame_rate_base == 0)
    {
        msg_Warn( p_dec, "invalid frame rate %d/%d, using 25 fps instead",
                  p_dec->fmt_out.video.i_frame_rate,
                  p_dec->fmt_out.video.i_frame_rate_base);
        date_Init( &p_sys->pts, 25, 1 );
    }
    else
        date_Init( &p_sys->pts, p_dec->fmt_out.video.i_frame_rate,
                    p_dec->fmt_out.video.i_frame_rate_base );

    if (p_dec->p_fmt_in->video.i_chroma == VLC_CODEC_D3D11_OPAQUE_BGRA)
    {
        // there's only one possible value so we don't have to wait for the
        // DXGI_FORMAT
        for (p_sys->output_format = DxgiGetRenderFormatList();
            p_sys->output_format->name != nullptr; ++p_sys->output_format)
        {
            if (p_sys->output_format->fourcc == p_dec->p_fmt_in->video.i_chroma &&
                is_d3d11_opaque(p_sys->output_format->fourcc))
                break;
        }

        p_sys->vctx =
            D3D11CreateVideoContext(p_sys->dec_dev, p_sys->output_format->formatTexture);
        if (!p_sys->vctx)
        {
            vlc_decoder_device_Release(dec_dev);
            return VLC_EGENERIC;
        }

        if( decoder_UpdateVideoOutput( p_dec, p_sys->vctx ) )
        {
            vlc_video_context_Release(p_sys->vctx);
            vlc_decoder_device_Release(dec_dev);
            return VLC_EGENERIC;
        }
    }
    else
    {
        // we can't do a decoder_UpdateVideoOutput() here as we don't have the video context
        // we assume the output will handle the D3D11 output
    }

    p_dec->pf_decode = DecodeFrame;
    p_dec->pf_flush  = Flush;
    p_dec->p_sys = p_sys;

    return VLC_SUCCESS;
}
