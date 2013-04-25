/*****************************************************************************
 * dirac.c: Dirac encoder module making use of libdirac (dirac-research).
 *          (http://www.bbc.co.uk/rd/projects/dirac/index.shtml)
 *          ##
 *          ## NB, this is a temporary encoder only module until schroedinger
 *          ## offers superior encoding quality than dirac-research
 *          ##
 *****************************************************************************
 * Copyright (C) 2004-2008 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 * Rewritten: David Flynn <davidf at rd.bbc.co.uk>
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

#include <limits.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_sout.h>

#include <libdirac_encoder/dirac_encoder.h>

#ifndef DIRAC_RESEARCH_VERSION_ATLEAST
# define DIRAC_RESEARCH_VERSION_ATLEAST(x,y,z) 0
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenEncoder( vlc_object_t *p_this );
static void CloseEncoder( vlc_object_t *p_this );
static block_t *Encode( encoder_t *p_enc, picture_t *p_pict );

#define ENC_CFG_PREFIX "sout-dirac-"

#define ENC_QUALITY_FACTOR "quality"
#define ENC_QUALITY_FACTOR_TEXT N_("Constant quality factor")
#define ENC_QUALITY_FACTOR_LONGTEXT N_("If bitrate=0, use this value for constant quality")

#define ENC_TARGETRATE "bitrate"
#define ENC_TARGETRATE_TEXT N_("CBR bitrate (kbps)")
#define ENC_TARGETRATE_LONGTEXT N_("A value > 0 enables constant bitrate mode")

#define ENC_LOSSLESS "lossless"
#define ENC_LOSSLESS_TEXT N_("Enable lossless coding")
#define ENC_LOSSLESS_LONGTEXT N_("Lossless coding ignores bitrate and quality settings, " \
                                 "allowing for perfect reproduction of the original")

#define ENC_PREFILTER "prefilter"
#define ENC_PREFILTER_TEXT N_("Prefilter")
#define ENC_PREFILTER_LONGTEXT N_("Enable adaptive prefiltering")
static const char *const enc_prefilter_list[] =
  { "none", "cwm", "rectlp", "diaglp" };
static const char *const enc_prefilter_list_text[] =
  { N_("None"), N_("Centre Weighted Median"),
    N_("Rectangular Linear Phase"), N_("Diagonal Linear Phase") };

#define ENC_PREFILTER_STRENGTH "prefilter-strength"
#define ENC_PREFILTER_STRENGTH_TEXT N_("Amount of prefiltering")
#define ENC_PREFILTER_STRENGTH_LONGTEXT N_("Higher value implies more prefiltering")

#define ENC_CHROMAFMT "chroma-fmt"
#define ENC_CHROMAFMT_TEXT N_("Chroma format")
#define ENC_CHROMAFMT_LONGTEXT N_("Picking chroma format will force a " \
                                  "conversion of the video into that format")
static const char *const enc_chromafmt_list[] =
  { "420", "422", "444" };
static const char *const enc_chromafmt_list_text[] =
  { N_("4:2:0"), N_("4:2:2"), N_("4:4:4") };

#define ENC_L1SEP "l1-sep"
#define ENC_L1SEP_TEXT N_("Distance between 'P' frames")
#define ENC_L1SEP_LONGTEXT ENC_L1SEP_TEXT

#define ENC_L1NUM "num-l1"
#define ENC_L1NUM_TEXT N_("Number of 'P' frames per GOP")
#define ENC_L1NUM_LONGTEXT ENC_L1NUM_TEXT

#define ENC_CODINGMODE "coding-mode"
#define ENC_CODINGMODE_TEXT N_("Picture coding mode")
#define ENC_CODINGMODE_LONGTEXT N_("Field coding is where interlaced fields are coded" \
                                   " separately as opposed to a pseudo-progressive frame")
static const char *const enc_codingmode_list[] =
  { "auto", "progressive", "field" };
static const char *const enc_codingmode_list_text[] =
  { N_("auto - let encoder decide based upon input (Best)"),
    N_("force coding frame as single picture"),
    N_("force coding frame as separate interlaced fields"),
  };

#define ENC_MCBLK_WIDTH "mc-blk-width"
#define ENC_MCBLK_WIDTH_TEXT N_("Width of motion compensation blocks")
#define ENC_MCBLK_WIDTH_LONGTEXT ""

#define ENC_MCBLK_HEIGHT "mc-blk-height"
#define ENC_MCBLK_HEIGHT_TEXT N_("Height of motion compensation blocks")
#define ENC_MCBLK_HEIGHT_LONGTEXT ""

/* also known as XBSEP and YBSEP */
#define ENC_MCBLK_OVERLAP "mc-blk-overlap"
#define ENC_MCBLK_OVERLAP_TEXT N_("Block overlap (%)")
#define ENC_MCBLK_OVERLAP_LONGTEXT N_("Amount that each motion block should " \
                                       "be overlapped by its neighbours")

/* advanced option only */
#define ENC_MCBLK_XBLEN "mc-blk-xblen"
#define ENC_MCBLK_XBLEN_TEXT N_("xblen")
#define ENC_MCBLK_XBLEN_LONGTEXT N_("Total horizontal block length including overlaps")

/* advanded option only */
#define ENC_MCBLK_YBLEN "mc-blk-yblen"
#define ENC_MCBLK_YBLEN_TEXT N_("yblen")
#define ENC_MCBLK_YBLEN_LONGTEXT N_("Total vertical block length including overlaps")

#define ENC_MVPREC "mv-prec"
#define ENC_MVPREC_TEXT N_("Motion vector precision")
#define ENC_MVPREC_LONGTEXT N_("Motion vector precision in pels.")
static const char *const enc_mvprec_list[] =
  { "1", "1/2", "1/4", "1/8" };

#define ENC_ME_SIMPLESEARCH "me-simple-search"
#define ENC_ME_SIMPLESEARCH_TEXT N_("Simple ME search area x:y")
#define ENC_ME_SIMPLESEARCH_LONGTEXT N_("(Not recommended) Perform a simple (non hierarchical " \
                                        "block matching motion vector search with search range " \
                                        "of +/-x, +/-y")

#define ENC_ME_COMBINED "me-combined"
#define ENC_ME_COMBINED_TEXT N_("Three component motion estimation")
#define ENC_ME_COMBINED_LONGTEXT N_("Use chroma as part of the motion estimation process")

#define ENC_DWTINTRA "dwt-intra"
#define ENC_DWTINTRA_TEXT N_("Intra picture DWT filter")
#define ENC_DWTINTRA_LONGTEXT ENC_DWTINTRA_TEXT

#define ENC_DWTINTER "dwt-inter"
#define ENC_DWTINTER_TEXT N_("Inter picture DWT filter")
#define ENC_DWTINTER_LONGTEXT ENC_DWTINTER_TEXT

#define ENC_DWTDEPTH "dwt-depth"
#define ENC_DWTDEPTH_TEXT N_("Number of DWT iterations")
#define ENC_DWTDEPTH_LONGTEXT N_("Also known as DWT levels")

/* advanced option only */
#define ENC_MULTIQUANT "multi-quant"
#define ENC_MULTIQUANT_TEXT N_("Enable multiple quantizers")
#define ENC_MULTIQUANT_LONGTEXT N_("Enable multiple quantizers per subband (one per codeblock)")

/* advanced option only */
#define ENC_SPARTITION "spartition"
#define ENC_SPARTITION_TEXT N_("Enable spatial partitioning")
#define ENC_SPARTITION_LONGTEXT ENC_SPARTITION_TEXT

#define ENC_NOAC "noac"
#define ENC_NOAC_TEXT N_("Disable arithmetic coding")
#define ENC_NOAC_LONGTEXT N_("Use variable length codes instead, useful for very high bitrates")

/* visual modelling */
/* advanced option only */
#define ENC_CPD "cpd"
#define ENC_CPD_TEXT N_("cycles per degree")
#define ENC_CPD_LONGTEXT ENC_CPD_TEXT

static const char *const ppsz_enc_options[] = {
    ENC_QUALITY_FACTOR, ENC_TARGETRATE, ENC_LOSSLESS, ENC_PREFILTER, ENC_PREFILTER_STRENGTH,
    ENC_CHROMAFMT, ENC_L1SEP, ENC_L1NUM, ENC_CODINGMODE,
    ENC_MCBLK_WIDTH, ENC_MCBLK_HEIGHT, ENC_MCBLK_OVERLAP,
    ENC_MVPREC, ENC_ME_SIMPLESEARCH, ENC_ME_COMBINED,
    ENC_DWTINTRA, ENC_DWTINTER, ENC_DWTDEPTH,
    ENC_MULTIQUANT, ENC_SPARTITION, ENC_NOAC,
    ENC_CPD,
    NULL
};


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    set_description( N_("Dirac video encoder using dirac-research library") )
    set_shortname( "Dirac" )
    set_capability( "encoder", 100 )
    set_callbacks( OpenEncoder, CloseEncoder )

    add_float( ENC_CFG_PREFIX ENC_QUALITY_FACTOR, 5.5,
               ENC_QUALITY_FACTOR_TEXT, ENC_QUALITY_FACTOR_LONGTEXT, false )
    change_float_range(0., 10.);

    add_integer( ENC_CFG_PREFIX ENC_TARGETRATE, -1,
                 ENC_TARGETRATE_TEXT, ENC_TARGETRATE_LONGTEXT, false )
    change_integer_range(-1, INT_MAX);

    add_bool( ENC_CFG_PREFIX ENC_LOSSLESS, false,
              ENC_LOSSLESS_TEXT, ENC_LOSSLESS_LONGTEXT, false )

    add_string( ENC_CFG_PREFIX ENC_PREFILTER, "diaglp",
                ENC_PREFILTER_TEXT, ENC_PREFILTER_LONGTEXT, false )
    change_string_list( enc_prefilter_list, enc_prefilter_list_text );

    add_integer( ENC_CFG_PREFIX ENC_PREFILTER_STRENGTH, 1,
                 ENC_PREFILTER_STRENGTH_TEXT, ENC_PREFILTER_STRENGTH_LONGTEXT, false )
    change_integer_range(0, 10);

    add_string( ENC_CFG_PREFIX ENC_CHROMAFMT, "420",
                ENC_CHROMAFMT_TEXT, ENC_CHROMAFMT_LONGTEXT, false )
    change_string_list( enc_chromafmt_list, enc_chromafmt_list_text );

    add_integer( ENC_CFG_PREFIX ENC_L1SEP, -1,
                 ENC_L1SEP_TEXT, ENC_L1SEP_LONGTEXT, false )
    change_integer_range(-1, INT_MAX);

    add_integer( ENC_CFG_PREFIX ENC_L1NUM, -1,
                 ENC_L1NUM_TEXT, ENC_L1NUM_LONGTEXT, false )
    change_integer_range(-1, INT_MAX);

    add_string( ENC_CFG_PREFIX ENC_CODINGMODE, "auto",
                ENC_CODINGMODE_TEXT, ENC_CODINGMODE_LONGTEXT, false )
    change_string_list( enc_codingmode_list, enc_codingmode_list_text );

    add_string( ENC_CFG_PREFIX ENC_MVPREC, "1/2",
                ENC_MVPREC_TEXT, ENC_MVPREC_LONGTEXT, false )
    change_string_list( enc_mvprec_list, enc_mvprec_list );

    add_integer( ENC_CFG_PREFIX ENC_MCBLK_WIDTH, -1,
                 ENC_MCBLK_WIDTH_TEXT, ENC_MCBLK_WIDTH_LONGTEXT, false )
    change_integer_range(-1, INT_MAX);

    add_integer( ENC_CFG_PREFIX ENC_MCBLK_HEIGHT, -1,
                 ENC_MCBLK_HEIGHT_TEXT, ENC_MCBLK_HEIGHT_LONGTEXT, false )
    change_integer_range(-1, INT_MAX);

    add_integer( ENC_CFG_PREFIX ENC_MCBLK_OVERLAP, -1,
                 ENC_MCBLK_OVERLAP_TEXT, ENC_MCBLK_OVERLAP_LONGTEXT, false )
    change_integer_range(-1, 100);

    /* advanced option only */
    add_integer( ENC_CFG_PREFIX ENC_MCBLK_XBLEN, -1,
                 ENC_MCBLK_XBLEN_TEXT, ENC_MCBLK_XBLEN_LONGTEXT, true )
    change_integer_range(-1, INT_MAX);
    /* advanced option only */
    add_integer( ENC_CFG_PREFIX ENC_MCBLK_YBLEN, -1,
                 ENC_MCBLK_YBLEN_TEXT, ENC_MCBLK_YBLEN_LONGTEXT, true )
    change_integer_range(-1, INT_MAX);

    add_string( ENC_CFG_PREFIX ENC_ME_SIMPLESEARCH, "",
              ENC_ME_SIMPLESEARCH_TEXT, ENC_ME_SIMPLESEARCH_LONGTEXT, false )

#if DIRAC_RESEARCH_VERSION_ATLEAST(1,0,1)
    add_bool( ENC_CFG_PREFIX ENC_ME_COMBINED, true,
              ENC_ME_COMBINED_TEXT, ENC_ME_COMBINED_LONGTEXT, false )
#endif

    add_integer( ENC_CFG_PREFIX ENC_DWTINTRA, -1,
                 ENC_DWTINTRA_TEXT, ENC_DWTINTRA_LONGTEXT, false )
    change_integer_range(-1, 6);

    add_integer( ENC_CFG_PREFIX ENC_DWTINTER, -1,
                 ENC_DWTINTER_TEXT, ENC_DWTINTER_LONGTEXT, false )
    change_integer_range(-1, 6);

    add_integer( ENC_CFG_PREFIX ENC_DWTDEPTH, -1,
                 ENC_DWTDEPTH_TEXT, ENC_DWTDEPTH_LONGTEXT, false )
    change_integer_range(-1, 4);

    /* advanced option only */
    /* NB, unforunately vlc doesn't have a concept of 'don't care' */
    add_integer( ENC_CFG_PREFIX ENC_MULTIQUANT, -1,
                 ENC_MULTIQUANT_TEXT, ENC_MULTIQUANT_LONGTEXT, true )
    change_integer_range(-1, 1);

    /* advanced option only */
    /* NB, unforunately vlc doesn't have a concept of 'don't care' */
    add_integer( ENC_CFG_PREFIX ENC_SPARTITION, -1,
                 ENC_SPARTITION_TEXT, ENC_SPARTITION_LONGTEXT, true )
    change_integer_range(-1, 1);

    add_bool( ENC_CFG_PREFIX ENC_NOAC, false,
              ENC_NOAC_TEXT, ENC_NOAC_LONGTEXT, false )

    /* advanced option only */
    add_float( ENC_CFG_PREFIX ENC_CPD, -1,
               ENC_CPD_TEXT, ENC_CPD_LONGTEXT, true )
    change_integer_range(-1, INT_MAX);
vlc_module_end()

/*****************************************************************************
 * picture_pts_t : store pts alongside picture number, not carried through
 * encoder
 *****************************************************************************/
struct picture_pts_t
{
   bool b_empty;      /* entry is invalid */
   uint32_t u_pnum;  /* dirac picture number */
   mtime_t i_pts;    /* associated pts */
};

/*****************************************************************************
 * encoder_sys_t : dirac encoder descriptor
 *****************************************************************************/
#define PTS_TLB_SIZE 256
struct encoder_sys_t
{
    dirac_encoder_t *p_dirac;
    dirac_encoder_context_t ctx;
    bool b_auto_field_coding;

    uint8_t *p_buffer_in;
    int i_buffer_in;
    uint32_t i_input_picnum;
    block_fifo_t *p_dts_fifo;

    int i_buffer_out;
    uint8_t *p_buffer_out;
    block_t *p_chain;

    struct picture_pts_t pts_tlb[PTS_TLB_SIZE];
    mtime_t i_pts_offset;
    mtime_t i_field_time;
};

static struct
{
    unsigned int i_height;
    int i_approx_fps;
    VideoFormat i_vf;
} dirac_format_guess[] = {
    /* Important: Keep this list ordered in ascending picture height */
    {1, 0, VIDEO_FORMAT_CUSTOM},
    {120, 15, VIDEO_FORMAT_QSIF525},
    {144, 12, VIDEO_FORMAT_QCIF},
    {240, 15, VIDEO_FORMAT_SIF525},
    {288, 12, VIDEO_FORMAT_CIF},
    {480, 30, VIDEO_FORMAT_SD_480I60},
    {480, 15, VIDEO_FORMAT_4SIF525},
    {576, 12, VIDEO_FORMAT_4CIF},
    {576, 25, VIDEO_FORMAT_SD_576I50},
    {720, 50, VIDEO_FORMAT_HD_720P50},
    {720, 60, VIDEO_FORMAT_HD_720P60},
    {1080, 24, VIDEO_FORMAT_DIGI_CINEMA_2K24},
    {1080, 25, VIDEO_FORMAT_HD_1080I50},
    {1080, 30, VIDEO_FORMAT_HD_1080I60},
    {1080, 50, VIDEO_FORMAT_HD_1080P50},
    {1080, 60, VIDEO_FORMAT_HD_1080P60},
    {2160, 24, VIDEO_FORMAT_DIGI_CINEMA_4K24},
    {2160, 50, VIDEO_FORMAT_UHDTV_4K50},
    {2160, 60, VIDEO_FORMAT_UHDTV_4K60},
    {3840, 50, VIDEO_FORMAT_UHDTV_8K50},
    {3840, 60, VIDEO_FORMAT_UHDTV_8K60},
    {0, 0, 0},
};

/*****************************************************************************
 * ResetPTStlb: Purge all entries in @p_dec@'s PTS-tlb
 *****************************************************************************/
static void ResetPTStlb( encoder_t *p_enc )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    for( int i=0; i<PTS_TLB_SIZE; i++)
    {
        p_sys->pts_tlb[i].b_empty = true;
    }
}

/*****************************************************************************
 * StorePicturePTS: Store the PTS value for a particular picture number
 *****************************************************************************/
static void StorePicturePTS( encoder_t *p_enc, uint32_t u_pnum, mtime_t i_pts )
{
    encoder_sys_t *p_sys = p_enc->p_sys;

    for( int i=0; i<PTS_TLB_SIZE; i++ )
    {
        if( p_sys->pts_tlb[i].b_empty )
        {
            p_sys->pts_tlb[i].u_pnum = u_pnum;
            p_sys->pts_tlb[i].i_pts = i_pts;
            p_sys->pts_tlb[i].b_empty = false;

            return;
        }
    }

    msg_Err( p_enc, "Could not store PTS %"PRId64" for frame %u", i_pts, u_pnum );
}

/*****************************************************************************
 * GetPicturePTS: Retrieve the PTS value for a particular picture number
 *****************************************************************************/
static mtime_t GetPicturePTS( encoder_t *p_enc, uint32_t u_pnum )
{
    encoder_sys_t *p_sys = p_enc->p_sys;

    for( int i=0; i<PTS_TLB_SIZE; i++ )
    {
        if( !p_sys->pts_tlb[i].b_empty &&
            p_sys->pts_tlb[i].u_pnum == u_pnum )
        {
             p_sys->pts_tlb[i].b_empty = true;
             return p_sys->pts_tlb[i].i_pts;
        }
    }

    msg_Err( p_enc, "Could not retrieve PTS for picture %u", u_pnum );
    return 0;
}

/*****************************************************************************
 * OpenEncoder: probe the encoder and return score
 *****************************************************************************/
static int OpenEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;
    int i_tmp;
    float f_tmp;
    char *psz_tmp;

    if( p_enc->fmt_out.i_codec != VLC_CODEC_DIRAC &&
        !p_enc->b_force )
    {
        return VLC_EGENERIC;
    }

    if( !p_enc->fmt_in.video.i_frame_rate || !p_enc->fmt_in.video.i_frame_rate_base ||
        !p_enc->fmt_in.video.i_height || !p_enc->fmt_in.video.i_width )
    {
        msg_Err( p_enc, "Framerate and picture dimensions must be non-zero" );
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_sys = calloc( 1, sizeof(*p_sys) ) ) == NULL )
        return VLC_ENOMEM;

    p_enc->p_sys = p_sys;
    p_enc->pf_encode_video = Encode;
    p_enc->fmt_out.i_codec = VLC_CODEC_DIRAC;
    p_enc->fmt_out.i_cat = VIDEO_ES;

    if( ( p_sys->p_dts_fifo = block_FifoNew() ) == NULL )
    {
        CloseEncoder( p_this );
        return VLC_ENOMEM;
    }

    ResetPTStlb( p_enc );

    /* guess the video format based upon number of lines and picture height */
    int i = 0;
    VideoFormat guessed_video_fmt = VIDEO_FORMAT_CUSTOM;
    /* Pick the dirac_video_format in this order of preference:
     *  1. an exact match in frame height and an approximate fps match
     *  2. the previous preset with a smaller number of lines.
     */
    do
    {
        if( dirac_format_guess[i].i_height > p_enc->fmt_in.video.i_height )
        {
            guessed_video_fmt = dirac_format_guess[i-1].i_vf;
            break;
        }
        if( dirac_format_guess[i].i_height != p_enc->fmt_in.video.i_height )
            continue;
        int src_fps = p_enc->fmt_in.video.i_frame_rate / p_enc->fmt_in.video.i_frame_rate_base;
        int delta_fps = abs( dirac_format_guess[i].i_approx_fps - src_fps );
        if( delta_fps > 2 )
            continue;

        guessed_video_fmt = dirac_format_guess[i].i_vf;
        break;
    } while( dirac_format_guess[++i].i_height );

    dirac_encoder_context_init( &p_sys->ctx, guessed_video_fmt );

    /* constants set from the input video format */
    p_sys->ctx.src_params.width = p_enc->fmt_in.video.i_width;
    p_sys->ctx.src_params.height = p_enc->fmt_in.video.i_height;
    p_sys->ctx.src_params.frame_rate.numerator = p_enc->fmt_in.video.i_frame_rate;
    p_sys->ctx.src_params.frame_rate.denominator = p_enc->fmt_in.video.i_frame_rate_base;
    unsigned u_asr_num, u_asr_den;
    vlc_ureduce( &u_asr_num, &u_asr_den,
                 p_enc->fmt_in.video.i_sar_num,
                 p_enc->fmt_in.video.i_sar_den, 0 );
    p_sys->ctx.src_params.pix_asr.numerator = u_asr_num;
    p_sys->ctx.src_params.pix_asr.denominator = u_asr_den;

    config_ChainParse( p_enc, ENC_CFG_PREFIX, ppsz_enc_options, p_enc->p_cfg );

    psz_tmp = var_GetString( p_enc, ENC_CFG_PREFIX ENC_CHROMAFMT );
    if( !psz_tmp )
        goto error;
    else if( !strcmp( psz_tmp, "420" ) ) {
        p_enc->fmt_in.i_codec = VLC_CODEC_I420;
        p_enc->fmt_in.video.i_bits_per_pixel = 12;
        p_sys->ctx.src_params.chroma = format420;
        p_sys->i_buffer_in = p_enc->fmt_in.video.i_width * p_enc->fmt_in.video.i_height * 3 / 2;
    }
    else if( !strcmp( psz_tmp, "422" ) ) {
        p_enc->fmt_in.i_codec = VLC_CODEC_I422;
        p_enc->fmt_in.video.i_bits_per_pixel = 16;
        p_sys->ctx.src_params.chroma = format422;
        p_sys->i_buffer_in = p_enc->fmt_in.video.i_width * p_enc->fmt_in.video.i_height * 2;
    }
    else if( !strcmp( psz_tmp, "444" ) ) {
        p_enc->fmt_in.i_codec = VLC_CODEC_I444;
        p_enc->fmt_in.video.i_bits_per_pixel = 24;
        p_sys->ctx.src_params.chroma = format444;
        p_sys->i_buffer_in = p_enc->fmt_in.video.i_width * p_enc->fmt_in.video.i_height * 3;
    }
    else {
        msg_Err( p_enc, "Invalid chroma format: %s", psz_tmp );
        free( psz_tmp );
        goto error;
    }
    free( psz_tmp );

    p_sys->ctx.enc_params.qf = var_GetFloat( p_enc, ENC_CFG_PREFIX ENC_QUALITY_FACTOR );

    /* use bitrate from sout-transcode-vb in kbps */
    p_sys->ctx.enc_params.trate = p_enc->fmt_out.i_bitrate / 1000;
    i_tmp = var_GetInteger( p_enc, ENC_CFG_PREFIX ENC_TARGETRATE );
    if( i_tmp > -1 )
        p_sys->ctx.enc_params.trate = i_tmp;
    p_enc->fmt_out.i_bitrate = p_sys->ctx.enc_params.trate * 1000;

    p_sys->ctx.enc_params.lossless = var_GetBool( p_enc, ENC_CFG_PREFIX ENC_LOSSLESS );

    psz_tmp = var_GetString( p_enc, ENC_CFG_PREFIX ENC_PREFILTER );
    if( !psz_tmp )
        goto error;
    else if( !strcmp( psz_tmp, "none" ) ) {
        p_sys->ctx.enc_params.prefilter = NO_PF;
    }
    else if( !strcmp( psz_tmp, "cwm" ) ) {
        p_sys->ctx.enc_params.prefilter = CWM;
    }
    else if( !strcmp( psz_tmp, "rectlp" ) ) {
        p_sys->ctx.enc_params.prefilter = RECTLP;
    }
    else if( !strcmp( psz_tmp, "diaglp" ) ) {
        p_sys->ctx.enc_params.prefilter = DIAGLP;
    }
    else {
        msg_Err( p_enc, "Invalid prefilter: %s", psz_tmp );
        free( psz_tmp );
        goto error;
    }
    free( psz_tmp );

    p_sys->ctx.enc_params.prefilter_strength =
        var_GetInteger( p_enc, ENC_CFG_PREFIX ENC_PREFILTER_STRENGTH );

    i_tmp = var_GetInteger( p_enc, ENC_CFG_PREFIX ENC_L1SEP );
    if( i_tmp > -1 )
        p_sys->ctx.enc_params.L1_sep = i_tmp;

    i_tmp = var_GetInteger( p_enc, ENC_CFG_PREFIX ENC_L1NUM );
    if( i_tmp > -1 )
        p_sys->ctx.enc_params.num_L1 = i_tmp;

    psz_tmp = var_GetString( p_enc, ENC_CFG_PREFIX ENC_CODINGMODE );
    if( !psz_tmp )
        goto error;
    else if( !strcmp( psz_tmp, "auto" ) ) {
        p_sys->b_auto_field_coding = true;
    }
    else if( !strcmp( psz_tmp, "progressive" ) ) {
        p_sys->b_auto_field_coding = false;
        p_sys->ctx.enc_params.picture_coding_mode = 0;
    }
    else if( !strcmp( psz_tmp, "field" ) ) {
        p_sys->b_auto_field_coding = false;
        p_sys->ctx.enc_params.picture_coding_mode = 1;
    }
    else {
        msg_Err( p_enc, "Invalid codingmode: %s", psz_tmp );
        free( psz_tmp );
        goto error;
    }
    free( psz_tmp );

    psz_tmp = var_GetString( p_enc, ENC_CFG_PREFIX ENC_MVPREC );
    if( !psz_tmp )
        goto error;
    else if( !strcmp( psz_tmp, "1" ) ) {
        p_sys->ctx.enc_params.mv_precision = MV_PRECISION_PIXEL;
    }
    else if( !strcmp( psz_tmp, "1/2" ) ) {
        p_sys->ctx.enc_params.mv_precision = MV_PRECISION_HALF_PIXEL;
    }
    else if( !strcmp( psz_tmp, "1/4" ) ) {
        p_sys->ctx.enc_params.mv_precision = MV_PRECISION_QUARTER_PIXEL;
    }
    else if( !strcmp( psz_tmp, "1/8" ) ) {
        p_sys->ctx.enc_params.mv_precision = MV_PRECISION_EIGHTH_PIXEL;
    }
    else {
        msg_Err( p_enc, "Invalid mv-prec: %s", psz_tmp );
        free( psz_tmp );
        goto error;
    }
    free( psz_tmp );

    /*
     * {x,y}b{len,sep} must be multiples of 4
     */
    i_tmp = var_GetInteger( p_enc, ENC_CFG_PREFIX ENC_MCBLK_WIDTH );
    if( i_tmp > -1 )
        p_sys->ctx.enc_params.xbsep = i_tmp / 4 * 4;

    i_tmp = var_GetInteger( p_enc, ENC_CFG_PREFIX ENC_MCBLK_HEIGHT );
    if( i_tmp > -1 )
        p_sys->ctx.enc_params.ybsep = i_tmp / 4 * 4;

    i_tmp = var_GetInteger( p_enc, ENC_CFG_PREFIX ENC_MCBLK_OVERLAP );
    if( i_tmp > -1 ) {
        p_sys->ctx.enc_params.xblen = p_sys->ctx.enc_params.xbsep * (100 + i_tmp) / 400 * 4;
        p_sys->ctx.enc_params.yblen = p_sys->ctx.enc_params.ybsep * (100 + i_tmp) / 400 * 4;
    }

    /*
     * {x,y}blen >= {x,y}bsep
     * {x,y}blen <= 2* {x,y}bsep
     */
    i_tmp = var_GetInteger( p_enc, ENC_CFG_PREFIX ENC_MCBLK_XBLEN );
    if( i_tmp > -1 ) {
        int xblen = __MAX( i_tmp, p_sys->ctx.enc_params.xbsep );
        xblen = __MIN( xblen, 2 * p_sys->ctx.enc_params.xbsep );
        p_sys->ctx.enc_params.xblen = xblen;
    }

    i_tmp = var_GetInteger( p_enc, ENC_CFG_PREFIX ENC_MCBLK_YBLEN );
    if( i_tmp > -1 ) {
        int yblen = __MAX( i_tmp, p_sys->ctx.enc_params.ybsep );
        yblen = __MIN( yblen, 2 * p_sys->ctx.enc_params.ybsep );
        p_sys->ctx.enc_params.yblen = yblen;
    }

    psz_tmp = var_GetString( p_enc, ENC_CFG_PREFIX ENC_ME_SIMPLESEARCH );
    if( !psz_tmp )
        goto error;
    if( *psz_tmp != '\0' ) {
        /* of the form [0-9]+:[0-9]+ */
        char *psz_start = psz_tmp;
        char *psz_end = psz_tmp;
        p_sys->ctx.enc_params.x_range_me = strtol(psz_start, &psz_end, 10);
        if( *psz_end != ':'  || psz_end == psz_start ) {
            msg_Err( p_enc, "Invalid simple search range: %s", psz_tmp );
            free( psz_tmp );
            goto error;
        }
        psz_start = ++psz_end;
        p_sys->ctx.enc_params.y_range_me = strtol(psz_start, &psz_end, 10);
        if( *psz_end != '\0'  || psz_end == psz_start ) {
            msg_Err( p_enc, "Invalid simple search range: %s", psz_tmp );
            free( psz_tmp );
            goto error;
        }
        if( p_sys->ctx.enc_params.x_range_me < 0 ||
            p_sys->ctx.enc_params.y_range_me < 0 )
        {
            msg_Err( p_enc, "Invalid negative simple search range: %s", psz_tmp );
            free( psz_tmp );
            goto error;
        }
        p_sys->ctx.enc_params.full_search = 1;
    }
    free( psz_tmp );

#if DIRAC_RESEARCH_VERSION_ATLEAST(1,0,1)
    p_sys->ctx.enc_params.combined_me = var_GetBool( p_enc, ENC_CFG_PREFIX ENC_ME_COMBINED );
#endif

    i_tmp = var_GetInteger( p_enc, ENC_CFG_PREFIX ENC_DWTINTRA );
    if( i_tmp > -1 )
        p_sys->ctx.enc_params.intra_wlt_filter = i_tmp;

    i_tmp = var_GetInteger( p_enc, ENC_CFG_PREFIX ENC_DWTINTER );
    if( i_tmp > -1 )
        p_sys->ctx.enc_params.inter_wlt_filter = i_tmp;

    i_tmp = var_GetInteger( p_enc, ENC_CFG_PREFIX ENC_DWTDEPTH );
    if( i_tmp > -1 )
        p_sys->ctx.enc_params.wlt_depth = i_tmp;

    i_tmp = var_GetInteger( p_enc, ENC_CFG_PREFIX ENC_MULTIQUANT );
    if( i_tmp > -1 )
        p_sys->ctx.enc_params.multi_quants = i_tmp;

    i_tmp = var_GetInteger( p_enc, ENC_CFG_PREFIX ENC_SPARTITION );
    if( i_tmp > -1 )
        p_sys->ctx.enc_params.spatial_partition = i_tmp;

    p_sys->ctx.enc_params.using_ac = !var_GetBool( p_enc, ENC_CFG_PREFIX ENC_NOAC );

    f_tmp = var_GetFloat( p_enc, ENC_CFG_PREFIX ENC_CPD );
    if( f_tmp > -1 )
        p_sys->ctx.enc_params.cpd = f_tmp;

    /* Allocate the buffer for inputing frames into the encoder */
    if( ( p_sys->p_buffer_in = malloc( p_sys->i_buffer_in ) ) == NULL )
    {
        CloseEncoder( p_this );
        return VLC_ENOMEM;
    }

    /* Set up output buffer */
    /* Unfortunately it isn't possible to determine if the buffer
     * is too small (and then reallocate it) */
    p_sys->i_buffer_out = 4096 + p_sys->i_buffer_in;
    if( ( p_sys->p_buffer_out = malloc( p_sys->i_buffer_out ) ) == NULL )
    {
        CloseEncoder( p_this );
        return VLC_ENOMEM;
    }

    return VLC_SUCCESS;
error:
    CloseEncoder( p_this );
    return VLC_EGENERIC;
}

/* Attempt to find dirac picture number in an encapsulation unit */
static int ReadDiracPictureNumber( uint32_t *p_picnum, block_t *p_block )
{
    uint32_t u_pos = 4;
    /* protect against falling off the edge */
    while( u_pos + 13 < p_block->i_buffer )
    {
        /* find the picture startcode */
        if( p_block->p_buffer[u_pos] & 0x08 )
        {
            *p_picnum = GetDWBE( p_block->p_buffer + u_pos + 9 );
            return 1;
        }
        /* skip to the next dirac data unit */
        uint32_t u_npo = GetDWBE( p_block->p_buffer + u_pos + 1 );
        assert( u_npo <= UINT32_MAX - u_pos );
        if( u_npo == 0 )
            u_npo = 13;
        u_pos += u_npo;
    }
    return 0;
}

/****************************************************************************
 * Encode: the whole thing
 ****************************************************************************
 * This function spits out encapsulation units.
 ****************************************************************************/
static block_t *Encode( encoder_t *p_enc, picture_t *p_pic )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    block_t *p_block, *p_output_chain = NULL;
    int i_plane, i_line, i_width, i_src_stride;
    uint8_t *p_dst;

    if( !p_pic ) return NULL;
    /* we only know if the sequence is interlaced when the first
     * picture arrives, so final setup is done here */
    /* XXX todo, detect change of interlace */
    p_sys->ctx.src_params.topfieldfirst = p_pic->b_top_field_first;
    p_sys->ctx.src_params.source_sampling = !p_pic->b_progressive;

    if( p_sys->b_auto_field_coding )
        p_sys->ctx.enc_params.picture_coding_mode = !p_pic->b_progressive;

    if( !p_sys->p_dirac )
    {
        date_t date;
        /* Initialise the encoder with the encoder context */
        p_sys->p_dirac = dirac_encoder_init( &p_sys->ctx, 0 );
        if( !p_sys->p_dirac )
        {
            msg_Err( p_enc, "Failed to initialize dirac encoder" );
            return NULL;
        }
        date_Init( &date, p_enc->fmt_in.video.i_frame_rate, p_enc->fmt_in.video.i_frame_rate_base );
#if DIRAC_RESEARCH_VERSION_ATLEAST(1,0,2)
        int i_delayinpics = dirac_encoder_pts_offset( p_sys->p_dirac );
        i_delayinpics /= p_sys->ctx.enc_params.picture_coding_mode + 1;
        date_Increment( &date, i_delayinpics );
#else
        date_Increment( &date, 1 );
#endif
        p_sys->i_pts_offset = date_Get( &date );

        /* picture_coding_mode = 1 == FIELD_CODING, two pictures are produced
         * for each frame input. Calculate time between fields for offsetting
         * the second field later. */
        if( 1 == p_sys->ctx.enc_params.picture_coding_mode )
        {
            date_Set( &date, 0 );
            date_Increment( &date, 1 );
            p_sys->i_field_time = date_Get( &date ) / 2;
        }
    }

    /* Copy input picture into encoder input buffer (stride by stride) */
    /* Would be lovely to just pass the picture in, but there is noway for the
     * library to free it */
    p_dst = p_sys->p_buffer_in;
    for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
    {
        uint8_t *p_src = p_pic->p[i_plane].p_pixels;
        i_width = p_pic->p[i_plane].i_visible_pitch;
        i_src_stride = p_pic->p[i_plane].i_pitch;

        for( i_line = 0; i_line < p_pic->p[i_plane].i_visible_lines; i_line++ )
        {
            memcpy( p_dst, p_src, i_width );
            p_dst += i_width;
            p_src += i_src_stride;
        }
    }

    /* Load one frame of data into encoder */
    if( dirac_encoder_load( p_sys->p_dirac, p_sys->p_buffer_in,
                            p_sys->i_buffer_in ) < 0 )
    {
        msg_Dbg( p_enc, "dirac_encoder_load() error" );
        return NULL;
    }

    /* store pts in a lookaside buffer, so that the same pts may
     * be used for the picture in coded order */
    StorePicturePTS( p_enc, p_sys->i_input_picnum, p_pic->date );
    p_sys->i_input_picnum++;

    /* store dts in a queue, so that they appear in order in
     * coded order */
    p_block = block_Alloc( 1 );
    if( !p_block )
        return NULL;
    p_block->i_dts = p_pic->date - p_sys->i_pts_offset;
    block_FifoPut( p_sys->p_dts_fifo, p_block );
    p_block = NULL;

    /* for field coding mode, insert an extra value into both the
     * pts lookaside buffer and dts queue, offset to correspond
     * to a one field delay. */
    if( 1 == p_sys->ctx.enc_params.picture_coding_mode )
    {
        StorePicturePTS( p_enc, p_sys->i_input_picnum, p_pic->date + p_sys->i_field_time );
        p_sys->i_input_picnum++;

        p_block = block_Alloc( 1 );
        if( !p_block )
            return NULL;
        p_block->i_dts = p_pic->date - p_sys->i_pts_offset + p_sys->i_field_time;
        block_FifoPut( p_sys->p_dts_fifo, p_block );
        p_block = NULL;
    }

    dirac_encoder_state_t state;
    /* Retrieve encoded frames from encoder */
    do
    {
        p_sys->p_dirac->enc_buf.buffer = p_sys->p_buffer_out;
        p_sys->p_dirac->enc_buf.size = p_sys->i_buffer_out;
        state = dirac_encoder_output( p_sys->p_dirac );
        switch( state )
        {
        case ENC_STATE_AVAIL: {
            uint32_t pic_num;

            /* extract data from encoder temporary buffer. */
            p_block = block_Alloc( p_sys->p_dirac->enc_buf.size );
            if( !p_block )
                return NULL;
            memcpy( p_block->p_buffer, p_sys->p_dirac->enc_buf.buffer,
                    p_sys->p_dirac->enc_buf.size );

            /* if some flags were set for a previous block, prevent
             * them from getting lost */
            if( p_sys->p_chain )
                p_block->i_flags |= p_sys->p_chain->i_flags;

            /* store all extracted blocks in a chain and gather up when an
             * entire encapsulation unit is avaliable (ends with a picture) */
            block_ChainAppend( &p_sys->p_chain, p_block );

            /* Presence of a Sequence header indicates a seek point */
            if( 0 == p_block->p_buffer[4] )
            {
                p_block->i_flags |= BLOCK_FLAG_TYPE_I;

                if( !p_enc->fmt_out.p_extra ) {
                    const uint8_t eos[] = { 'B','B','C','D',0x10,0,0,0,13,0,0,0,0 };
                    uint32_t len = GetDWBE( p_block->p_buffer + 5 );
                    /* if it hasn't been done so far, stash a copy of the
                     * sequence header for muxers such as ogg */
                    /* The OggDirac spec advises that a Dirac EOS DataUnit
                     * is appended to the sequence header to allow guard
                     * against poor streaming servers */
                    /* XXX, should this be done using the packetizer ? */
                    p_enc->fmt_out.p_extra = malloc( len + sizeof(eos) );
                    if( !p_enc->fmt_out.p_extra )
                        return NULL;
                    memcpy( p_enc->fmt_out.p_extra, p_block->p_buffer, len);
                    memcpy( (uint8_t*)p_enc->fmt_out.p_extra + len, eos, sizeof(eos) );
                    SetDWBE( (uint8_t*)p_enc->fmt_out.p_extra + len + 10, len );
                    p_enc->fmt_out.i_extra = len + sizeof(eos);
                }
            }

            if( ReadDiracPictureNumber( &pic_num, p_block ) )
            {
                /* Finding a picture terminates an ecapsulation unit, gather
                 * all data and output; use the next dts value queued up
                 * and find correct pts in the tlb */
                p_block = block_FifoGet( p_sys->p_dts_fifo );
                p_sys->p_chain->i_dts = p_block->i_dts;
                p_sys->p_chain->i_pts = GetPicturePTS( p_enc, pic_num );
                block_Release( p_block );
                block_ChainAppend( &p_output_chain, block_ChainGather( p_sys->p_chain ) );
                p_sys->p_chain = NULL;
            } else {
                p_block = NULL;
            }
            break;
            }

        case ENC_STATE_BUFFER:
            break;
        case ENC_STATE_INVALID:
        default:
            break;
        }
    } while( state == ENC_STATE_AVAIL );

    return p_output_chain;
}

/*****************************************************************************
 * CloseEncoder: dirac encoder destruction
 *****************************************************************************/
static void CloseEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;

    /* Free the encoder resources */
    if( p_sys->p_dirac )
        dirac_encoder_close( p_sys->p_dirac );

    free( p_sys->p_buffer_in );
    free( p_sys->p_buffer_out );

    if( p_sys->p_dts_fifo )
        block_FifoRelease( p_sys->p_dts_fifo );
    block_ChainRelease( p_sys->p_chain );

    free( p_sys );
}
