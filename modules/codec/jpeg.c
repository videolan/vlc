/*****************************************************************************
 * jpeg.c: jpeg decoder module making use of libjpeg.
 *****************************************************************************
 * Copyright (C) 2013-2014,2016 VLC authors and VideoLAN
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
#include <vlc_configuration.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_charset.h>
#include <vlc_ancillary.h>

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
typedef struct
{
    JPEG_SYS_COMMON_MEMBERS

    JSAMPARRAY p_row_pointers;
    struct jpeg_decompress_struct p_jpeg;
} decoder_sys_t;

static int  OpenDecoder(vlc_object_t *);
static void CloseDecoder(vlc_object_t *);

static int DecodeBlock(decoder_t *, block_t *);

/*
 * jpeg encoder descriptor
 */
typedef struct
{
    JPEG_SYS_COMMON_MEMBERS

    struct jpeg_compress_struct p_jpeg;

    int i_blocksize;
    int i_quality;
} encoder_sys_t;

static const char * const ppsz_enc_options[] = {
    "quality",
    NULL
};

static int  OpenEncoder(vlc_object_t *);
static void CloseEncoder(encoder_t *);

static block_t *EncodeBlock(encoder_t *, picture_t *);

/*
 * Module descriptor
 */
vlc_module_begin()
    set_subcategory(SUBCAT_INPUT_VCODEC)
    /* decoder main module */
    set_description(N_("JPEG image decoder"))
    set_capability("video decoder", 1000)
    set_callbacks(OpenDecoder, CloseDecoder)
    add_shortcut("jpeg")

#ifdef ENABLE_SOUT
    /* video encoder submodule */
    add_submodule()
    add_shortcut("jpeg")
    set_section(N_("Encoding"), NULL)
    set_description(N_("JPEG video encoder"))
    set_capability("video encoder", 1000)
    set_callback(OpenEncoder)
    add_integer_with_range(ENC_CFG_PREFIX "quality", 95, 0, 100,
                           ENC_QUALITY_TEXT, ENC_QUALITY_LONGTEXT)
#endif
    /* image encoder submodule */
    add_submodule()
    add_shortcut("jpeg")
    set_section(N_("Encoding"), NULL)
    set_description(N_("JPEG image encoder"))
    set_capability("image encoder", 1000)
    set_callback(OpenEncoder)
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

    if (p_dec->fmt_in->i_codec != VLC_CODEC_JPEG)
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

    /* Set callbacks */
    p_dec->pf_decode = DecodeBlock;

    p_dec->fmt_out.i_codec =
    p_dec->fmt_out.video.i_chroma = VLC_CODEC_RGB24;
    p_dec->fmt_out.video.transfer  = TRANSFER_FUNC_SRGB;
    p_dec->fmt_out.video.space     = COLOR_SPACE_SRGB;
    p_dec->fmt_out.video.primaries = COLOR_PRIMARIES_SRGB;
    p_dec->fmt_out.video.color_range = COLOR_RANGE_FULL;

    return VLC_SUCCESS;
}

/*
 * The following two functions are used to return 16 and 32 bit values from
 * the EXIF tag structure. That structure is borrowed from TIFF files and may be
 * in big endian or little endian format. The boolean b_bigEndian parameter
 * is TRUE if the EXIF data is in big endian format, and FALSE for little endian
 * Case Little Endian EXIF tag / Little Endian machine
 *   - just memcpy the tag structure into the value to return
 * Case Little Endian EXIF tag / Big Endian machine
 *    - memcpy the tag structure into value, and bswap it
 * Case Little Endian EXIF tag / Big Endian machine
 *   - memcpy the tag structure into value, and bswap it
 * Case Big Endian EXIF tag / Big Endian machine
 *   - just memcpy the tag structure into the value to return
 *
 * While there are byte manipulation functions in vlc_common.h, none of them
 * address that format of the data supplied may be either big or little endian.
 *
 * The claim is made that this is the best way to do it. Can you do better?
*/

#define G_LITTLE_ENDIAN     1234
#define G_BIG_ENDIAN        4321

typedef unsigned int uint;
typedef unsigned short ushort;

static uint16_t
de_get16( const void * ptr, uint endian ) {
    return (endian == G_BIG_ENDIAN) ? GetWBE(ptr) : GetWLE(ptr);
}

static uint32_t
de_get32( const void * ptr, uint endian ) {
    return (endian == G_BIG_ENDIAN) ? GetDWBE(ptr) : GetDWLE(ptr);
}

static bool getRDFFloat(const char *psz_rdf, float *out, const char *psz_var)
{
    char *p_start = strcasestr(psz_rdf, psz_var);
    if (p_start == NULL)
        return false;

    size_t varlen = strlen(psz_var);
    p_start += varlen;
    char *p_end = NULL;
    /* XML style */
    if (p_start[0] == '>')
    {
        p_start += 1;
        p_end = strchr(p_start, '<');
    }
    else if (p_start[0] == '=' && p_start[1] == '"')
    {
        p_start += 2;
        p_end = strchr(p_start, '"');
    }
    if (unlikely(p_end == NULL || p_end == p_start + 1))
        return false;

    *out = vlc_strtof_c(p_start, NULL);
    return true;
}

#define EXIF_JPEG_MARKER    0xE1
#define EXIF_XMP_STRING     "http://ns.adobe.com/xap/1.0/\000"

/* read XMP metadata for projection according to
 * https://developers.google.com/streetview/spherical-metadata */
static bool jpeg_ParseXMP(const uint8_t *p_buf, size_t i_buf,
                          video_format_t *fmt)
{
    if(i_buf < 29)
        return false;

    /* Allocate temp, zero terminated buffer */
    /* Fixme: custom memicmp */
    /* or Fixme: Who's not compliant to XML attributes case ? */
    char *psz_rdf = malloc(i_buf - 29 + 1);
    if (unlikely(psz_rdf == NULL))
        return false;
    memcpy(psz_rdf, p_buf + 29, i_buf - 29);
    psz_rdf[i_buf - 29] = '\0';

    if (!strcasestr(psz_rdf, "ProjectionType=\"equirectangular\"") &&
        !strcasestr(psz_rdf, "ProjectionType>equirectangular"))
    return false;

    fmt->projection_mode = PROJECTION_MODE_EQUIRECTANGULAR;

    /* pose handling */
    float value;
    if (getRDFFloat(psz_rdf, &value, "PoseHeadingDegrees") &&
        value >= 0.f && value <= 360.f)
        fmt->pose.yaw = value;

    if (getRDFFloat(psz_rdf, &value, "PosePitchDegrees"))
        fmt->pose.pitch = value;

    if (getRDFFloat(psz_rdf, &value, "PoseRollDegrees"))
        fmt->pose.roll = value;

    /* initial view */
    if (getRDFFloat(psz_rdf, &value, "InitialViewHeadingDegrees"))
        fmt->pose.yaw = value;

    if (getRDFFloat(psz_rdf, &value, "InitialViewPitchDegrees"))
        fmt->pose.pitch = value;

    if (getRDFFloat(psz_rdf, &value, "InitialViewRollDegrees"))
        fmt->pose.roll = value;

    if (getRDFFloat(psz_rdf, &value, "InitialHorizontalFOVDegrees") &&
        value >= FIELD_OF_VIEW_DEGREES_MIN && value <= FIELD_OF_VIEW_DEGREES_MAX)
        fmt->pose.fov = value;

    free(psz_rdf);

    return true;
}

static void jpeg_FillProjection(j_decompress_ptr cinfo, video_format_t *fmt)
{
    for (jpeg_saved_marker_ptr cmarker = cinfo->marker_list;
                               cmarker != NULL;
                               cmarker = cmarker->next)
    {
        if(cmarker->marker == EXIF_JPEG_MARKER &&
           cmarker->data_length >= 32 &&
           !memcmp(cmarker->data, EXIF_XMP_STRING, 29) &&
           jpeg_ParseXMP(cmarker->data, cmarker->data_length, fmt))
           break; /* found projection marker */
    }
}

/*
 * Look through the meta data in the libjpeg decompress structure to determine
 * if an EXIF Orientation tag is present. If so return true and sets orientation
 *  value (1-8), otherwise return false
 *
 * This function is based on the function get_orientation in io-jpeg.c, part of
 * the GdkPixbuf library, licensed under LGPLv2+.
 *   Copyright (C) 1999 Michael Zucchi, The Free Software Foundation
*/
#define EXIF_IDENT_STRING   "Exif\0" /* Exif\000\000 */
static bool jpeg_ParseExifApp1( const uint8_t *p_buf, size_t i_buf, int *orientation )
{

    uint i;                    /* index into working buffer */
    ushort tag_type;           /* endianed tag type extracted from tiff header */
    uint ret;                  /* Return value */
    uint offset;               /* de-endianed offset in various situations */
    uint tags;                 /* number of tags in current ifd */
    uint type;                 /* de-endianed type of tag */
    uint count;                /* de-endianed count of elements in a tag */
    uint tiff = 0;             /* offset to active tiff header */
    uint endian = 0;           /* detected endian of data */

    const char leth[] = { 0x49, 0x49, 0x2a, 0x00 }; /* Little endian TIFF header */
    const char beth[] = { 0x4d, 0x4d, 0x00, 0x2a }; /* Big endian TIFF header */

    #define EXIF_ORIENT_TAGID   0x112

    if (i_buf < 32)
        return false;

    /* Check for TIFF header and catch endianness */
    i = 0;

    /* Just skip data until TIFF header - it should be within 16 bytes from marker start.
       Normal structure relative to APP1 marker -
            0x0000: APP1 marker entry = 2 bytes
            0x0002: APP1 length entry = 2 bytes
            0x0004: Exif Identifier entry = 6 bytes
            0x000A: Start of TIFF header (Byte order entry) - 4 bytes
                    - This is what we look for, to determine endianness.
            0x000E: 0th IFD offset pointer - 4 bytes

            exif_marker->data points to the first data after the APP1 marker
            and length entries, which is the exif identification string.
            The TIFF header should thus normally be found at i=6, below,
            and the pointer to IFD0 will be at 6+4 = 10.
    */

    while ( i < 16 )
    {
        /* Little endian TIFF header */
        if ( memcmp( &p_buf[i], leth, 4 ) == 0 )
        {
            endian = G_LITTLE_ENDIAN;
        }
        /* Big endian TIFF header */
        else
        if ( memcmp( &p_buf[i], beth, 4 ) == 0 )
        {
            endian = G_BIG_ENDIAN;
        }
        /* Keep looking through buffer */
        else
        {
            i++;
            continue;
        }
        /* We have found either big or little endian TIFF header */
        tiff = i;
        break;
    }

    /* So did we find a TIFF header or did we just hit end of buffer? */
    if ( tiff == 0 )
        return false;

    /* Read out the offset pointer to IFD0 */
    offset = de_get32( &p_buf[i] + 4, endian );
    i = i + offset;

    /* Check that we still are within the buffer and can read the tag count */

    if ( i > i_buf - 2 )
        return false;

    /* Find out how many tags we have in IFD0. As per the TIFF spec, the first
       two bytes of the IFD contain a count of the number of tags. */
    tags = de_get16( &p_buf[i], endian );
    i = i + 2;

    /* Check that we still have enough data for all tags to check. The tags
       are listed in consecutive 12-byte blocks. The tag ID, type, size, and
       a pointer to the actual value, are packed into these 12 byte entries. */
    if ( tags * 12U > i_buf - i )
        return false;

    /* Check through IFD0 for tags of interest */
    while ( tags-- )
    {
        tag_type = de_get16( &p_buf[i], endian );
        /* Is this the orientation tag? */
        if ( tag_type == EXIF_ORIENT_TAGID )
        {
            type = de_get16( &p_buf[i + 2], endian );
            count = de_get32( &p_buf[i + 4], endian );

            /* Check that type and count fields are OK. The orientation field
               will consist of a single (count=1) 2-byte integer (type=3). */
            if ( type != 3 || count != 1 )
                return false;

            /* Return the orientation value. Within the 12-byte block, the
               pointer to the actual data is at offset 8. */
            ret = de_get16( &p_buf[i + 8], endian );
            *orientation = ( ret <= 8 ) ? ret : 0;
            return true;
        }
        /* move the pointer to the next 12-byte tag field. */
        i = i + 12;
    }

    return false; /* No EXIF Orientation tag found */
}

static int jpeg_GetOrientation( j_decompress_ptr cinfo )
{
    int orientation = 0;
    for (jpeg_saved_marker_ptr cmarker = cinfo->marker_list;
                               cmarker != NULL;
                               cmarker = cmarker->next)
    {
        if(cmarker->marker == EXIF_JPEG_MARKER &&
           cmarker->data_length >= 32 &&
           !memcmp(cmarker->data, EXIF_IDENT_STRING, sizeof(EXIF_IDENT_STRING)) &&
           jpeg_ParseExifApp1(cmarker->data, cmarker->data_length, &orientation))
           break; /* found orientation marker */
    }
    return orientation;
}

/*
 * This function must be fed with a complete compressed frame.
 */
static int DecodeBlock(decoder_t *p_dec, block_t *p_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    picture_t *p_pic = 0;

    p_sys->p_row_pointers = NULL;

    if (!p_block) /* No Drain */
        return VLCDEC_SUCCESS;

    if (p_block->i_flags & BLOCK_FLAG_CORRUPTED )
    {
        block_Release(p_block);
        return VLCDEC_SUCCESS;
    }

    /* libjpeg longjmp's there in case of error */
    if (setjmp(p_sys->setjmp_buffer))
    {
        goto error;
    }

#define ICC_JPEG_MARKER (JPEG_APP0+2)

    jpeg_create_decompress(&p_sys->p_jpeg);
    jpeg_mem_src(&p_sys->p_jpeg, p_block->p_buffer, p_block->i_buffer);
    jpeg_save_markers( &p_sys->p_jpeg, EXIF_JPEG_MARKER, 0xffff );
    jpeg_save_markers( &p_sys->p_jpeg, ICC_JPEG_MARKER, 0xffff );
    jpeg_read_header(&p_sys->p_jpeg, TRUE);

    p_sys->p_jpeg.out_color_space = JCS_RGB;

    jpeg_start_decompress(&p_sys->p_jpeg);

    /* Set output properties */
    p_dec->fmt_out.video.i_visible_width  = p_dec->fmt_out.video.i_width  = p_sys->p_jpeg.output_width;
    p_dec->fmt_out.video.i_visible_height = p_dec->fmt_out.video.i_height = p_sys->p_jpeg.output_height;
    p_dec->fmt_out.video.i_sar_num = 1;
    p_dec->fmt_out.video.i_sar_den = 1;

    int i_otag; /* Orientation tag has valid range of 1-8. 1 is normal orientation, 0 = unspecified = normal */
    i_otag = jpeg_GetOrientation( &p_sys->p_jpeg );
    if ( i_otag > 1 )
    {
        msg_Dbg( p_dec, "Jpeg orientation is %d", i_otag );
        p_dec->fmt_out.video.orientation = ORIENT_FROM_EXIF( i_otag );
    }
    jpeg_FillProjection(&p_sys->p_jpeg, &p_dec->fmt_out.video);

    /* Get a new picture */
    if (decoder_UpdateVideoFormat(p_dec))
    {
        goto error;
    }
    p_pic = decoder_NewPicture(p_dec);
    if (!p_pic)
    {
        goto error;
    }

    /* Read ICC profile */
#if defined(LIBJPEG_TURBO_VERSION_NUMBER) && LIBJPEG_TURBO_VERSION_NUMBER >= 1005090
    JOCTET *p_icc;
    unsigned int icc_len;
    if (jpeg_read_icc_profile(&p_sys->p_jpeg, &p_icc, &icc_len))
    {
        vlc_icc_profile_t *icc;
        icc = picture_AttachNewAncillary(p_pic, VLC_ANCILLARY_ID_ICC, sizeof(*icc) + icc_len);
        if (!icc) {
            free(p_icc);
            goto error;
        }
        memcpy(icc->data, p_icc, icc_len);
        icc->size = icc_len;
        free(p_icc);
    }
#endif /* LIBJPEG_TURBO_VERSION_NUMBER */

    /* Decode picture */
    p_sys->p_row_pointers = vlc_alloc(p_sys->p_jpeg.output_height, sizeof(JSAMPROW));
    if (!p_sys->p_row_pointers)
    {
        goto error;
    }
    for (unsigned i = 0; i < p_sys->p_jpeg.output_height; i++) {
        p_sys->p_row_pointers[i] = p_pic->p->p_pixels + p_pic->p->i_pitch * i;
    }

    while (p_sys->p_jpeg.output_scanline < p_sys->p_jpeg.output_height)
    {
        jpeg_read_scanlines(&p_sys->p_jpeg,
                p_sys->p_row_pointers + p_sys->p_jpeg.output_scanline,
                p_sys->p_jpeg.output_height - p_sys->p_jpeg.output_scanline);
    }

    jpeg_finish_decompress(&p_sys->p_jpeg);
    jpeg_destroy_decompress(&p_sys->p_jpeg);
    free(p_sys->p_row_pointers);

    p_pic->date = p_block->i_pts != VLC_TICK_INVALID ? p_block->i_pts : p_block->i_dts;

    block_Release(p_block);
    decoder_QueueVideo( p_dec, p_pic );
    return VLCDEC_SUCCESS;

error:

    if (p_pic)
        picture_Release(p_pic);
    jpeg_destroy_decompress(&p_sys->p_jpeg);
    free(p_sys->p_row_pointers);

    block_Release(p_block);
    return VLCDEC_SUCCESS;
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

    p_enc->fmt_in.i_codec = p_enc->fmt_in.video.i_chroma = VLC_CODEC_I420;
    p_enc->fmt_in.video.color_range = COLOR_RANGE_FULL;

    static const struct vlc_encoder_operations ops =
    {
        .close = CloseEncoder,
        .encode_video = EncodeBlock,
    };
    p_enc->ops = &ops;

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
    unsigned long size = p_block->i_buffer;

    /* libjpeg longjmp's there in case of error */
    if (setjmp(p_sys->setjmp_buffer))
    {
        goto error;
    }

    jpeg_create_compress(&p_sys->p_jpeg);
    jpeg_mem_dest(&p_sys->p_jpeg, &p_block->p_buffer, &size);

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

    unsigned char exif[] = "Exif\x00\x00"
                           "\x4d\x4d\x00\x2a" /* TIFF BE header */
                           "\x00\x00\x00\x08" /* IFD0 offset */
                           "\x00\x01" /* IFD0 tags count */
                           "\x01\x12" /* tagtype */
                           "\x00\x03" /* value type (x03 == short) */
                           "\x00\x00\x00\x01" /* value count */
                           "\xFF\xFF\x00\x00" /* value if <= 4 bytes or value offset */
                           "\x00\x00\x00\x00" /* 0 last IFD */
                           "\x00\x00\x00\x00" /* 0 last IFD offset */
            ;
    exif[24] = 0x00;
    exif[25] = ORIENT_TO_EXIF(p_pic->format.orientation);
    jpeg_write_marker(&p_sys->p_jpeg, JPEG_APP0 + 1, exif, sizeof(exif) - 1);

    /* Encode picture */
    p_row_pointers = vlc_alloc(p_pic->i_planes, sizeof(JSAMPARRAY));
    if (p_row_pointers == NULL)
    {
        goto error;
    }

    for (int i = 0; i < p_pic->i_planes; i++)
    {
        p_row_pointers[i] = vlc_alloc(p_sys->p_jpeg.comp_info[i].v_samp_factor, sizeof(JSAMPROW) * DCTSIZE);
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

    p_block->i_buffer = size;
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
static void CloseEncoder(encoder_t *p_enc)
{
    encoder_sys_t *p_sys = p_enc->p_sys;

    free(p_sys);
}
