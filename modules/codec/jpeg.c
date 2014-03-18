/*****************************************************************************
 * jpeg.c: jpeg decoder module making use of libjpeg.
 *****************************************************************************
 * Copyright (C) 2013-2014 VLC authors and VideoLAN
 *
 * Authors: Maxim Bublis <b@codemonkey.ru>
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
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <jpeglib.h>
#include <setjmp.h>

/* JPEG_SYS_COMMON_MEMBERS:
 * members common to encoder and decoder descriptors
 */
#define JPEG_SYS_COMMON_MEMBERS                             \
/**@{*/                                                     \
    /* libjpeg error handler manager */                     \
    struct jpeg_error_mgr err;                              \
                                                            \
    /* setjmp buffer for internal libjpeg error handling */ \
    jmp_buf setjmp_buffer;                                  \
                                                            \
    vlc_object_t *p_obj;                                    \
                                                            \
/**@}*/                                                     \

#define ENC_CFG_PREFIX "sout-jpeg-"
#define ENC_QUALITY_TEXT N_("Quality level")
#define ENC_QUALITY_LONGTEXT N_("Quality level " \
    "for encoding (this can enlarge or reduce output image size).")


/*
 * jpeg common descriptor
 */
struct jpeg_sys_t
{
    JPEG_SYS_COMMON_MEMBERS
};

typedef struct jpeg_sys_t jpeg_sys_t;

/*
 * jpeg decoder descriptor
 */
struct decoder_sys_t
{
    JPEG_SYS_COMMON_MEMBERS

    struct jpeg_decompress_struct p_jpeg;
};

static int  OpenDecoder(vlc_object_t *);
static void CloseDecoder(vlc_object_t *);

static picture_t *DecodeBlock(decoder_t *, block_t **);

/*
 * jpeg encoder descriptor
 */
struct encoder_sys_t
{
    JPEG_SYS_COMMON_MEMBERS

    struct jpeg_compress_struct p_jpeg;

    int i_blocksize;
    int i_quality;
};

static const char * const ppsz_enc_options[] = {
    "quality",
    NULL
};

static int  OpenEncoder(vlc_object_t *);
static void CloseEncoder(vlc_object_t *);

static block_t *EncodeBlock(encoder_t *, picture_t *);

/*
 * Module descriptor
 */
vlc_module_begin()
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_VCODEC)
    /* decoder main module */
    set_description(N_("JPEG image decoder"))
    set_capability("decoder", 1000)
    set_callbacks(OpenDecoder, CloseDecoder)
    add_shortcut("jpeg")

    /* encoder submodule */
    add_submodule()
    add_shortcut("jpeg")
    set_section(N_("Encoding"), NULL)
    set_description(N_("JPEG image encoder"))
    set_capability("encoder", 1000)
    set_callbacks(OpenEncoder, CloseEncoder)
    add_integer_with_range(ENC_CFG_PREFIX "quality", 95, 0, 100,
                           ENC_QUALITY_TEXT, ENC_QUALITY_LONGTEXT, true)
vlc_module_end()


/*
 * Exit error handler for libjpeg
 */
static void user_error_exit(j_common_ptr p_jpeg)
{
    jpeg_sys_t *p_sys = (jpeg_sys_t *)p_jpeg->err;
    p_sys->err.output_message(p_jpeg);
    longjmp(p_sys->setjmp_buffer, 1);
}

/*
 * Emit message error handler for libjpeg
 */
static void user_error_message(j_common_ptr p_jpeg)
{
    char error_msg[JMSG_LENGTH_MAX];

    jpeg_sys_t *p_sys = (jpeg_sys_t *)p_jpeg->err;
    p_sys->err.format_message(p_jpeg, error_msg);
    msg_Err(p_sys->p_obj, "%s", error_msg);
}

/*
 * Probe the decoder and return score
 */
static int OpenDecoder(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *)p_this;

    if (p_dec->fmt_in.i_codec != VLC_CODEC_JPEG)
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    decoder_sys_t *p_sys = malloc(sizeof(decoder_sys_t));
    if (p_sys == NULL)
    {
        return VLC_ENOMEM;
    }

    p_dec->p_sys = p_sys;

    p_sys->p_obj = p_this;

    p_sys->p_jpeg.err = jpeg_std_error(&p_sys->err);
    p_sys->err.error_exit = user_error_exit;
    p_sys->err.output_message = user_error_message;

    /* Set output properties */
    p_dec->fmt_out.i_cat = VIDEO_ES;

    /* Set callbacks */
    p_dec->pf_decode_video = DecodeBlock;

    return VLC_SUCCESS;
}

/*
 * This function must be fed with a complete compressed frame.
 */
static picture_t *DecodeBlock(decoder_t *p_dec, block_t **pp_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block;
    picture_t *p_pic = 0;

    JSAMPARRAY p_row_pointers = NULL;

    if (!pp_block || !*pp_block)
    {
        return NULL;
    }

    p_block = *pp_block;

    if (p_block->i_flags & BLOCK_FLAG_DISCONTINUITY)
    {
        block_Release(p_block);
        *pp_block = NULL;
        return NULL;
    }

    /* libjpeg longjmp's there in case of error */
    if (setjmp(p_sys->setjmp_buffer))
    {
        goto error;
    }

    jpeg_create_decompress(&p_sys->p_jpeg);
    jpeg_mem_src(&p_sys->p_jpeg, p_block->p_buffer, p_block->i_buffer);
    jpeg_read_header(&p_sys->p_jpeg, TRUE);

    p_sys->p_jpeg.out_color_space = JCS_RGB;

    jpeg_start_decompress(&p_sys->p_jpeg);

    /* Set output properties */
    p_dec->fmt_out.i_codec = VLC_CODEC_RGB24;
    p_dec->fmt_out.video.i_visible_width  = p_dec->fmt_out.video.i_width  = p_sys->p_jpeg.output_width;
    p_dec->fmt_out.video.i_visible_height = p_dec->fmt_out.video.i_height = p_sys->p_jpeg.output_height;
    p_dec->fmt_out.video.i_sar_num = 1;
    p_dec->fmt_out.video.i_sar_den = 1;
    p_dec->fmt_out.video.i_rmask = 0x000000ff;
    p_dec->fmt_out.video.i_gmask = 0x0000ff00;
    p_dec->fmt_out.video.i_bmask = 0x00ff0000;

    /* Get a new picture */
    p_pic = decoder_NewPicture(p_dec);
    if (!p_pic)
    {
        goto error;
    }

    /* Decode picture */
    p_row_pointers = malloc(sizeof(JSAMPROW) * p_sys->p_jpeg.output_height);
    if (!p_row_pointers)
    {
        goto error;
    }
    for (unsigned i = 0; i < p_sys->p_jpeg.output_height; i++) {
        p_row_pointers[i] = p_pic->p->p_pixels + p_pic->p->i_pitch * i;
    }

    while (p_sys->p_jpeg.output_scanline < p_sys->p_jpeg.output_height)
    {
        jpeg_read_scanlines(&p_sys->p_jpeg,
                p_row_pointers + p_sys->p_jpeg.output_scanline,
                p_sys->p_jpeg.output_height - p_sys->p_jpeg.output_scanline);
    }

    jpeg_finish_decompress(&p_sys->p_jpeg);
    jpeg_destroy_decompress(&p_sys->p_jpeg);
    free(p_row_pointers);

    p_pic->date = p_block->i_pts > VLC_TS_INVALID ? p_block->i_pts : p_block->i_dts;

    block_Release(p_block);
    *pp_block = NULL;

    return p_pic;

error:

    jpeg_destroy_decompress(&p_sys->p_jpeg);
    free(p_row_pointers);

    block_Release(p_block);
    *pp_block = NULL;

    return NULL;
}

/*
 * jpeg decoder destruction
 */
static void CloseDecoder(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    free(p_sys);
}

/*
 * Probe the encoder and return score
 */
static int OpenEncoder(vlc_object_t *p_this)
{
    encoder_t *p_enc = (encoder_t *)p_this;

    config_ChainParse(p_enc, ENC_CFG_PREFIX, ppsz_enc_options, p_enc->p_cfg);

    if (p_enc->fmt_out.i_codec != VLC_CODEC_JPEG)
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store encoder's structure */
    encoder_sys_t *p_sys = malloc(sizeof(encoder_sys_t));
    if (p_sys == NULL)
    {
        return VLC_ENOMEM;
    }

    p_enc->p_sys = p_sys;

    p_sys->p_obj = p_this;

    p_sys->p_jpeg.err = jpeg_std_error(&p_sys->err);
    p_sys->err.error_exit = user_error_exit;
    p_sys->err.output_message = user_error_message;

    p_sys->i_quality = var_GetInteger(p_enc, ENC_CFG_PREFIX "quality");
    p_sys->i_blocksize = 3 * p_enc->fmt_in.video.i_visible_width * p_enc->fmt_in.video.i_visible_height;

    p_enc->fmt_in.i_codec = VLC_CODEC_J420;
    p_enc->pf_encode_video = EncodeBlock;

    return VLC_SUCCESS;
}

/*
 * EncodeBlock
 */
static block_t *EncodeBlock(encoder_t *p_enc, picture_t *p_pic)
{
    encoder_sys_t *p_sys = p_enc->p_sys;

    if (unlikely(!p_pic))
    {
        return NULL;
    }
    block_t *p_block = block_Alloc(p_sys->i_blocksize);
    if (p_block == NULL)
    {
        return NULL;
    }

    JSAMPIMAGE p_row_pointers = NULL;

    /* libjpeg longjmp's there in case of error */
    if (setjmp(p_sys->setjmp_buffer))
    {
        goto error;
    }

    jpeg_create_compress(&p_sys->p_jpeg);
    jpeg_mem_dest(&p_sys->p_jpeg, &p_block->p_buffer, &p_block->i_buffer);

    p_sys->p_jpeg.image_width = p_enc->fmt_in.video.i_visible_width;
    p_sys->p_jpeg.image_height = p_enc->fmt_in.video.i_visible_height;
    p_sys->p_jpeg.input_components = 3;
    p_sys->p_jpeg.in_color_space = JCS_YCbCr;

    jpeg_set_defaults(&p_sys->p_jpeg);
    jpeg_set_colorspace(&p_sys->p_jpeg, JCS_YCbCr);

    p_sys->p_jpeg.raw_data_in = TRUE;
#if JPEG_LIB_VERSION >= 70
    p_sys->p_jpeg.do_fancy_downsampling = FALSE;
#endif

    jpeg_set_quality(&p_sys->p_jpeg, p_sys->i_quality, TRUE);

    jpeg_start_compress(&p_sys->p_jpeg, TRUE);

    /* Encode picture */
    p_row_pointers = malloc(sizeof(JSAMPARRAY) * p_pic->i_planes);
    if (p_row_pointers == NULL)
    {
        goto error;
    }

    for (int i = 0; i < p_pic->i_planes; i++)
    {
        p_row_pointers[i] = malloc(sizeof(JSAMPROW) * p_sys->p_jpeg.comp_info[i].v_samp_factor * DCTSIZE);
    }

    while (p_sys->p_jpeg.next_scanline < p_sys->p_jpeg.image_height)
    {
        for (int i = 0; i < p_pic->i_planes; i++)
        {
            int i_offset = p_sys->p_jpeg.next_scanline * p_sys->p_jpeg.comp_info[i].v_samp_factor / p_sys->p_jpeg.max_v_samp_factor;

            for (int j = 0; j < p_sys->p_jpeg.comp_info[i].v_samp_factor * DCTSIZE; j++)
            {
                p_row_pointers[i][j] = p_pic->p[i].p_pixels + p_pic->p[i].i_pitch * (i_offset + j);
            }
        }
        jpeg_write_raw_data(&p_sys->p_jpeg, p_row_pointers, p_sys->p_jpeg.max_v_samp_factor * DCTSIZE);
    }

    jpeg_finish_compress(&p_sys->p_jpeg);
    jpeg_destroy_compress(&p_sys->p_jpeg);

    for (int i = 0; i < p_pic->i_planes; i++)
    {
        free(p_row_pointers[i]);
    }
    free(p_row_pointers);

    p_block->i_dts = p_block->i_pts = p_pic->date;

    return p_block;

error:
    jpeg_destroy_compress(&p_sys->p_jpeg);

    if (p_row_pointers != NULL)
    {
        for (int i = 0; i < p_pic->i_planes; i++)
        {
            free(p_row_pointers[i]);
        }
    }
    free(p_row_pointers);

    block_Release(p_block);

    return NULL;
}

/*
 * jpeg encoder destruction
 */
static void CloseEncoder(vlc_object_t *p_this)
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;

    free(p_sys);
}
