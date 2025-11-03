// SPDX-License-Identifier: LGPL-2.1-or-later

// openapv.c : APV decoder using openapv library
// Copyright © 2025 VideoLabs, VLC authors and VideoLAN

// Authors: Steve Lhomme <robux4@videolabs.io>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_modules.h>
#include <vlc_bits.h>

#include "../packetizer/iso_color_tables.h"

#include <oapv.h>
#include <limits.h>

#define THREAD_TILES_TEXT N_("Tile Threads")
#define THREAD_TILES_LONGTEXT N_( "Max number of threads used for decoding, default 0=auto" )

static int OpenAPVDecoder(vlc_object_t *);
static void CloseAPVDecoder(vlc_object_t *);

vlc_module_begin ()
    set_description(N_("OpenAPV video decoder"))
    set_capability("video decoder", 10)
    set_callbacks(OpenAPVDecoder, CloseAPVDecoder)
    set_subcategory(SUBCAT_INPUT_VCODEC)

    add_integer_with_range( "openapv-threads", 0, 0, INT_MAX,
                 THREAD_TILES_TEXT, THREAD_TILES_LONGTEXT)
vlc_module_end ()

struct oapv_sys
{
    oapvd_t        decoder;
    int            cs;
};

static const struct {
    vlc_fourcc_t vlc;
    int          oapv;
} vlc_oapv_chromas[] = {
    { VLC_CODEC_I422_10L, OAPV_CS_SET(OAPV_CF_YCBCR422, 10, 0) },
    { VLC_CODEC_I422_12L, OAPV_CS_SET(OAPV_CF_YCBCR422, 12, 0) },
    { VLC_CODEC_I422_16L, OAPV_CS_SET(OAPV_CF_YCBCR422, 16, 0) },
    { VLC_CODEC_I444_10L, OAPV_CS_SET(OAPV_CF_YCBCR444, 10, 0) },
    { VLC_CODEC_I444_12L, OAPV_CS_SET(OAPV_CF_YCBCR444, 12, 0) },
    { VLC_CODEC_I444_16L, OAPV_CS_SET(OAPV_CF_YCBCR444, 16, 0) },
    { VLC_CODEC_GREY_10L, OAPV_CS_SET(OAPV_CF_YCBCR400, 10, 0) },
    { VLC_CODEC_GREY_12L, OAPV_CS_SET(OAPV_CF_YCBCR400, 12, 0) },
    { VLC_CODEC_GREY_16L, OAPV_CS_SET(OAPV_CF_YCBCR400, 16, 0) },
};

static inline vlc_fourcc_t FindVlcChroma(uint8_t profile_idc, uint8_t bit_depth_minus8, uint8_t chroma_format_idc)
{
    if (profile_idc == 33) // 422-10 profile
    {
        assert(chroma_format_idc == 2);
        assert(bit_depth_minus8 == 2);
        return VLC_CODEC_I422_10L;
    }
    if (profile_idc == 44) // 422-12 profile
    {
        assert(chroma_format_idc == 2);
        assert(bit_depth_minus8 >= 2 && bit_depth_minus8 <= 4);
        return VLC_CODEC_I422_12L;
    }
    if (profile_idc == 55) // 444-10 profile
    {
        assert(chroma_format_idc == 2 || chroma_format_idc == 3);
        assert(bit_depth_minus8 == 2);
        return VLC_CODEC_I444_10L;
    }
    if (profile_idc == 66) // 444-12 profile
    {
        assert(chroma_format_idc == 2 || chroma_format_idc == 3);
        assert(bit_depth_minus8 >= 2 && bit_depth_minus8 <= 4);
        return VLC_CODEC_I444_12L;
    }
    // if (profile_idc == 77) // 4444-10 profile
    // {
    //     assert(chroma_format_idc >= 2 && chroma_format_idc <= 4);
    //     assert(bit_depth_minus8 == 2);
    //     return VLC_CODEC_V410;
    // }
    // if (profile_idc == 88) // 4444-12 profile
    // {
    //     assert(chroma_format_idc >= 2 && chroma_format_idc <= 4);
    //     assert(bit_depth_minus8 >= 2 && bit_depth_minus8 <= 4);
    //     return VLC_CODEC_V410;
    // }
    if (bit_depth_minus8 == 2) // 10-bit
    {
        switch(chroma_format_idc)
        {
            case 0: return VLC_CODEC_GREY_10L;
            case 2: return VLC_CODEC_I422_10L;
            case 3: return VLC_CODEC_I444_10L;
            // case 4: return VLC_CODEC_I4444_10L;
            default: return 0;
        }
    }
    if (bit_depth_minus8 == 4) // 12-bit
    {
        switch(chroma_format_idc)
        {
            case 0: return VLC_CODEC_GREY_12L;
            case 2: return VLC_CODEC_I422_12L;
            case 3: return VLC_CODEC_I444_12L;
            // case 4: return VLC_CODEC_I4444_12L;
            default: return 0;
        }
    }
    // if (bit_depth_minus8 == 6) // 14-bit
    // {
    //     switch(chroma_format_idc)
    //     {
    //         case 0: return VLC_CODEC_GREY_14L;
    //         case 2: return VLC_CODEC_I422_14L;
    //         case 3: return VLC_CODEC_I444_14L;
    // //        case 4: return VLC_CODEC_I4444_14L;
    //         default: return 0;
    //     }
    // }
    if (bit_depth_minus8 == 8) // 16-bit
    {
        switch(chroma_format_idc)
        {
            case 0: return VLC_CODEC_GREY_16L;
            case 2: return VLC_CODEC_I422_16L;
            case 3: return VLC_CODEC_I444_16L;
            // case 4: return VLC_CODEC_I4444_16L;
            default: return 0;
        }
    }
    return 0;
}

static int imgb_addref(oapv_imgb_t *imgb)
{
    return imgb->refcnt++;
}

static int imgb_getref(oapv_imgb_t *imgb)
{
    return imgb->refcnt;
}

static int imgb_release(oapv_imgb_t *imgb)
{
    if (--imgb->refcnt == 0)
    {
        picture_Release(imgb->pdata[0]);
        free(imgb);
        return 0;
    }
    return imgb->refcnt;
}

static oapv_imgb_t *GetImage( decoder_t *p_dec )
{
    struct oapv_sys *sys = p_dec->p_sys;

    oapv_imgb_t *imgb = calloc(1, sizeof(*imgb));
    if (imgb == NULL)
        return NULL;

    picture_t *pic = decoder_NewPicture( p_dec );
    if (pic == NULL)
        goto fail;

    imgb->refcnt = 1;
    imgb->cs = sys->cs;
    imgb->pdata[0] = pic;
    imgb->np = pic->i_planes;
    for (int i = 0; i < pic->i_planes; i++)
    {
        imgb->w[i] = pic->p[i].i_visible_pitch / pic->p[i].i_pixel_pitch;
        imgb->h[i] = pic->p[i].i_visible_lines / pic->p[i].i_pixel_pitch;

        imgb->aw[i] = pic->p[i].i_pitch / pic->p[i].i_pixel_pitch;
        imgb->s[i] = pic->p[i].i_pitch;
        imgb->ah[i] = pic->p[i].i_lines / pic->p[i].i_pixel_pitch;
        imgb->e[i] = pic->p[i].i_lines;
        imgb->bsize[i] = imgb->s[i] * imgb->e[i];
        imgb->a[i] = pic->p[i].p_pixels;
    }

    imgb->addref = imgb_addref;
    imgb->getref = imgb_getref;
    imgb->release = imgb_release;

    return imgb;
fail:
    free(imgb);
    return NULL;
}

static int Decode( decoder_t *p_dec, block_t *p_block )
{
    struct oapv_sys *sys = p_dec->p_sys;

    if (p_block == NULL) // drain nothing to do
        return VLCDEC_SUCCESS;

    oapv_frms_t  ofrms = { 0 };
    oapvm_t      mid = { 0 };
    oapv_bitb_t  bitb = { 0 };
    oapvd_stat_t stats = { 0 };
    int err;

    ofrms.num_frms = 1;
    ofrms.frm[0].imgb = GetImage(p_dec);
    if (ofrms.frm[0].imgb == NULL)
        return VLCDEC_ECRITICAL; // no more memory ?

    bitb.addr = p_block->p_buffer;
    bitb.ssize = p_block->i_buffer;

    err = oapvd_decode(sys->decoder, &bitb, &ofrms, &mid, &stats);
    if (unlikely(err != OAPV_OK))
    {
        msg_Err( p_dec, "decoding error %d", err );
        ofrms.frm[0].imgb->release(ofrms.frm[0].imgb);
        return VLCDEC_ECRITICAL;
    }

    assert(stats.aui.num_frms == 1 || stats.aui.num_frms == 0);
    if (stats.aui.num_frms == 1)
    {
        picture_t *decoded = ofrms.frm[0].imgb->pdata[0];
        picture_Hold(decoded);
        decoded->date = p_block->i_pts != VLC_TICK_INVALID ? p_block->i_pts : p_block->i_dts;

        decoder_QueueVideo( p_dec, decoded );
    }
    for (int i=0; i<stats.aui.num_frms; i++)
    {
        ofrms.frm[i].imgb->release(ofrms.frm[i].imgb);
    }

    return VLCDEC_SUCCESS;
}


int OpenAPVDecoder(vlc_object_t *o)
{
    decoder_t *dec = container_of(o, decoder_t, obj);
    if (dec->fmt_in->i_codec != VLC_CODEC_APV)
        return VLC_ENOTSUP;

    uint8_t color_primaries = 2;
    uint8_t transfer_characteristics = 2;
    uint8_t matrix_coefficients = 2;
    uint8_t profile_idc = 0;
    uint8_t chroma_format_idc = 0;
    uint8_t bit_depth_minus8 = 2;
    bool full_range_flag = false;
    uint32_t frame_width = dec->fmt_in->video.i_width;
    uint32_t frame_height = dec->fmt_in->video.i_height;

    // parse extradata to get stream info
    bs_t extra;
    bs_init(&extra, dec->fmt_in->p_extra, dec->fmt_in->i_extra);
    uint8_t configurationVersion = bs_read(&extra, 8);
    if (configurationVersion != 1)
    {
        msg_Dbg(o, "unknown configuration %" PRIu8, configurationVersion);
        return VLC_ENOTSUP;
    }

    uint32_t number_of_configuration_entry = bs_read(&extra, 8);
    if (number_of_configuration_entry != 1)
    {
        msg_Err(o, "unsupported number of configurations (%u)", (unsigned)number_of_configuration_entry);
        return VLC_EGENERIC;
    }
    bs_skip(&extra, 8); // pbu_type
    uint8_t number_of_frame_info = bs_read(&extra, 8);
    if (number_of_frame_info != 1)
    {
        msg_Err(o, "unsupported number of frames (%u)", (unsigned)number_of_frame_info);
        return VLC_EGENERIC;
    }
    bs_skip(&extra, 6);
    bool color_description_present_flag = bs_read1(&extra);
    bs_skip(&extra, 1); // capture_time_distance_ignored
    profile_idc = bs_read(&extra, 8);
    bs_skip(&extra, 8); // level_idc
    bs_skip(&extra, 8); // band_idc
    frame_width = bs_read(&extra, 32);
    frame_height = bs_read(&extra, 32);
    chroma_format_idc = bs_read(&extra, 4);
    bit_depth_minus8 = bs_read(&extra, 4);
    bs_skip(&extra, 8); // capture_time_distance
    if (color_description_present_flag)
    {
        color_primaries = bs_read(&extra, 8);
        transfer_characteristics = bs_read(&extra, 8);
        matrix_coefficients = bs_read(&extra, 8);
        full_range_flag = bs_read1(&extra);
        bs_skip(&extra, 7); // reserved
    }

    struct oapv_sys *sys = vlc_obj_malloc(o, sizeof(*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    oapvd_cdesc_t desc = { 0 };
    desc.threads = var_InheritInteger( o, "openapv-threads" );
    sys->decoder = oapvd_create(&desc, NULL);
    if (sys->decoder == NULL)
    {
        msg_Err(o, "failed to create decoder");
        return VLC_EGENERIC;
    }

    if (dec->fmt_in->video.primaries == COLOR_PRIMARIES_UNDEF)
        dec->fmt_out.video.primaries = iso_23001_8_cp_to_vlc_primaries(color_primaries);
    else
        dec->fmt_out.video.primaries = dec->fmt_in->video.primaries;
    if (dec->fmt_in->video.transfer == TRANSFER_FUNC_UNDEF)
        dec->fmt_out.video.transfer = iso_23001_8_tc_to_vlc_xfer(transfer_characteristics);
    else
        dec->fmt_out.video.transfer = dec->fmt_in->video.transfer;
    if (dec->fmt_in->video.space == COLOR_SPACE_UNDEF)
        dec->fmt_out.video.space = iso_23001_8_mc_to_vlc_coeffs(matrix_coefficients);
    else
        dec->fmt_out.video.space = dec->fmt_in->video.space;
    if (dec->fmt_in->video.color_range == COLOR_RANGE_UNDEF)
        dec->fmt_out.video.color_range = full_range_flag ? COLOR_RANGE_FULL : COLOR_RANGE_LIMITED;
    else
        dec->fmt_out.video.color_range = dec->fmt_in->video.color_range;

    dec->fmt_out.video.i_width = frame_width;
    dec->fmt_out.video.i_height = frame_height;

    dec->fmt_out.i_codec = FindVlcChroma(profile_idc, bit_depth_minus8, chroma_format_idc);
    dec->fmt_out.video.i_chroma = dec->fmt_out.i_codec;

    if (decoder_UpdateVideoOutput(dec, NULL) != 0)
    {
        msg_Err(o, "decoder_UpdateVideoOutput failed");
        CloseAPVDecoder(o);
        return VLC_EGENERIC;
    }
    for (size_t i=0; i < ARRAY_SIZE(vlc_oapv_chromas); i++)
    {
        if (dec->fmt_out.video.i_chroma == vlc_oapv_chromas[i].vlc)
        {
            sys->cs = vlc_oapv_chromas[i].oapv;
            break;
        }
    }

    dec->p_sys = sys;

    dec->pf_decode = Decode;

    return VLC_SUCCESS;
}

void CloseAPVDecoder(vlc_object_t *o)
{
    decoder_t *dec = container_of(o, decoder_t, obj);
    struct oapv_sys *sys = dec->p_sys;
    oapvd_delete(sys->decoder);
}
