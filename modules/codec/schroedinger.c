/*****************************************************************************
 * schroedinger.c: Dirac decoder module making use of libschroedinger.
 *          (http://www.bbc.co.uk/rd/projects/dirac/index.shtml)
 *          (http://diracvideo.org)
 *****************************************************************************
 * Copyright (C) 2008-2011 VLC authors and VideoLAN
 *
 * Authors: Jonathan Rosser <jonathan.rosser@gmail.com>
 *          David Flynn <davidf at rd dot bbc.co.uk>
 *          Anuradha Suraparaju <asuraparaju at gmail dot com>
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>

#include <schroedinger/schro.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int        OpenDecoder  ( vlc_object_t * );
static void       CloseDecoder ( vlc_object_t * );
static int        OpenEncoder  ( vlc_object_t * );
static void       CloseEncoder ( vlc_object_t * );

#define ENC_CFG_PREFIX "sout-schro-"

#define ENC_CHROMAFMT "chroma-fmt"
#define ENC_CHROMAFMT_TEXT N_("Chroma format")
#define ENC_CHROMAFMT_LONGTEXT N_("Picking chroma format will force a " \
                                  "conversion of the video into that format")
static const char *const enc_chromafmt_list[] =
  { "420", "422", "444" };
static const char *const enc_chromafmt_list_text[] =
  { N_("4:2:0"), N_("4:2:2"), N_("4:4:4") };

#define ENC_RATE_CONTROL "rate-control"
#define ENC_RATE_CONTROL_TEXT N_("Rate control method")
#define ENC_RATE_CONTROL_LONGTEXT N_("Method used to encode the video sequence")

static const char *enc_rate_control_list[] = {
  "constant_noise_threshold",
  "constant_bitrate",
  "low_delay",
  "lossless",
  "constant_lambda",
  "constant_error",
  "constant_quality"
};

static const char *enc_rate_control_list_text[] = {
  N_("Constant noise threshold mode"),
  N_("Constant bitrate mode (CBR)"),
  N_("Low Delay mode"),
  N_("Lossless mode"),
  N_("Constant lambda mode"),
  N_("Constant error mode"),
  N_("Constant quality mode")
};

#define ENC_GOP_STRUCTURE "gop-structure"
#define ENC_GOP_STRUCTURE_TEXT N_("GOP structure")
#define ENC_GOP_STRUCTURE_LONGTEXT N_("GOP structure used to encode the video sequence")

static const char *enc_gop_structure_list[] = {
  "adaptive",
  "intra_only",
  "backref",
  "chained_backref",
  "biref",
  "chained_biref"
};

static const char *enc_gop_structure_list_text[] = {
  N_("No fixed gop structure. A picture can be intra or inter and refer to previous or future pictures."),
  N_("I-frame only sequence"),
  N_("Inter pictures refere to previous pictures only"),
  N_("Inter pictures refere to previous pictures only"),
  N_("Inter pictures can refer to previous or future pictures"),
  N_("Inter pictures can refer to previous or future pictures")
};

#define ENC_QUALITY "quality"
#define ENC_QUALITY_TEXT N_("Constant quality factor")
#define ENC_QUALITY_LONGTEXT N_("Quality factor to use in constant quality mode")

#define ENC_NOISE_THRESHOLD "noise-threshold"
#define ENC_NOISE_THRESHOLD_TEXT N_("Noise Threshold")
#define ENC_NOISE_THRESHOLD_LONGTEXT N_("Noise threshold to use in constant noise threshold mode")

#define ENC_BITRATE "bitrate"
#define ENC_BITRATE_TEXT N_("CBR bitrate (kbps)")
#define ENC_BITRATE_LONGTEXT N_("Target bitrate in kbps when encoding in constant bitrate mode")

#define ENC_MAX_BITRATE "max-bitrate"
#define ENC_MAX_BITRATE_TEXT N_("Maximum bitrate (kbps)")
#define ENC_MAX_BITRATE_LONGTEXT N_("Maximum bitrate in kbps when encoding in constant bitrate mode")

#define ENC_MIN_BITRATE "min-bitrate"
#define ENC_MIN_BITRATE_TEXT N_("Minimum bitrate (kbps)")
#define ENC_MIN_BITRATE_LONGTEXT N_("Minimum bitrate in kbps when encoding in constant bitrate mode")

#define ENC_AU_DISTANCE "gop-length"
#define ENC_AU_DISTANCE_TEXT N_("GOP length")
#define ENC_AU_DISTANCE_LONGTEXT N_("Number of pictures between successive sequence headers i.e. length of the group of pictures")


#define ENC_PREFILTER "filtering"
#define ENC_PREFILTER_TEXT N_("Prefilter")
#define ENC_PREFILTER_LONGTEXT N_("Enable adaptive prefiltering")

static const char *enc_filtering_list[] = {
  "none",
  "center_weighted_median",
  "gaussian",
  "add_noise",
  "adaptive_gaussian",
  "lowpass"
};

static const char *enc_filtering_list_text[] = {
  N_("No pre-filtering"),
  N_("Centre Weighted Median"),
  N_("Gaussian Low Pass Filter"),
  N_("Add Noise"),
  N_("Gaussian Adaptive Low Pass Filter"),
  N_("Low Pass Filter"),
};

#define ENC_PREFILTER_STRENGTH "filter-value"
#define ENC_PREFILTER_STRENGTH_TEXT N_("Amount of prefiltering")
#define ENC_PREFILTER_STRENGTH_LONGTEXT N_("Higher value implies more prefiltering")

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

/* advanced option only */
#define ENC_MCBLK_SIZE "motion-block-size"
#define ENC_MCBLK_SIZE_TEXT N_("Size of motion compensation blocks")

static const char *enc_block_size_list[] = {
  "automatic",
  "small",
  "medium",
  "large"
};
static const char *const enc_block_size_list_text[] =
  { N_("automatic - let encoder decide based upon input (Best)"),
    N_("small - use small motion compensation blocks"),
    N_("medium - use medium motion compensation blocks"),
    N_("large - use large motion compensation blocks"),
  };

/* advanced option only */
#define ENC_MCBLK_OVERLAP "motion-block-overlap"
#define ENC_MCBLK_OVERLAP_TEXT N_("Overlap of motion compensation blocks")

static const char *enc_block_overlap_list[] = {
  "automatic",
  "none",
  "partial",
  "full"
};
static const char *const enc_block_overlap_list_text[] =
  { N_("automatic - let encoder decide based upon input (Best)"),
    N_("none - Motion compensation blocks do not overlap"),
    N_("partial - Motion compensation blocks only partially overlap"),
    N_("full - Motion compensation blocks fully overlap"),
  };


#define ENC_MVPREC "mv-precision"
#define ENC_MVPREC_TEXT N_("Motion Vector precision")
#define ENC_MVPREC_LONGTEXT N_("Motion Vector precision in pels")
static const char *const enc_mvprec_list[] =
  { "1", "1/2", "1/4", "1/8" };

/* advanced option only */
#define ENC_ME_COMBINED "me-combined"
#define ENC_ME_COMBINED_TEXT N_("Three component motion estimation")
#define ENC_ME_COMBINED_LONGTEXT N_("Use chroma as part of the motion estimation process")

#define ENC_DWTINTRA "intra-wavelet"
#define ENC_DWTINTRA_TEXT N_("Intra picture DWT filter")

#define ENC_DWTINTER "inter-wavelet"
#define ENC_DWTINTER_TEXT N_("Inter picture DWT filter")

static const char *enc_wavelet_list[] = {
  "desl_dubuc_9_7",
  "le_gall_5_3",
  "desl_dubuc_13_7",
  "haar_0",
  "haar_1",
  "fidelity",
  "daub_9_7"
};

static const char *enc_wavelet_list_text[] = {
  "Deslauriers-Dubuc (9,7)",
  "LeGall (5,3)",
  "Deslauriers-Dubuc (13,7)",
  "Haar with no shift",
  "Haar with single shift per level",
  "Fidelity filter",
  "Daubechies (9,7) integer approximation"
};

#define ENC_DWTDEPTH "transform-depth"
#define ENC_DWTDEPTH_TEXT N_("Number of DWT iterations")
#define ENC_DWTDEPTH_LONGTEXT N_("Also known as DWT levels")


/* advanced option only */
#define ENC_MULTIQUANT "enable-multiquant"
#define ENC_MULTIQUANT_TEXT N_("Enable multiple quantizers")
#define ENC_MULTIQUANT_LONGTEXT N_("Enable multiple quantizers per subband (one per codeblock)")

/* advanced option only */
#define ENC_NOAC "enable-noarith"
#define ENC_NOAC_TEXT N_("Disable arithmetic coding")
#define ENC_NOAC_LONGTEXT N_("Use variable length codes instead, useful for very high bitrates")

/* visual modelling */
/* advanced option only */
#define ENC_PWT "perceptual-weighting"
#define ENC_PWT_TEXT N_("perceptual weighting method")

static const char *enc_perceptual_weighting_list[] = {
  "none",
  "ccir959",
  "moo",
  "manos_sakrison"
};

/* advanced option only */
#define ENC_PDIST "perceptual-distance"
#define ENC_PDIST_TEXT N_("perceptual distance")
#define ENC_PDIST_LONGTEXT N_("perceptual distance to calculate perceptual weight")

/* advanced option only */
#define ENC_HSLICES "horiz-slices"
#define ENC_HSLICES_TEXT N_("Horizontal slices per frame")
#define ENC_HSLICES_LONGTEXT N_("Number of horizontal slices per frame in low delay mode")

/* advanced option only */
#define ENC_VSLICES "vert-slices"
#define ENC_VSLICES_TEXT N_("Vertical slices per frame")
#define ENC_VSLICES_LONGTEXT N_("Number of vertical slices per frame in low delay mode")

/* advanced option only */
#define ENC_SCBLK_SIZE "codeblock-size"
#define ENC_SCBLK_SIZE_TEXT N_("Size of code blocks in each subband")

static const char *enc_codeblock_size_list[] = {
  "automatic",
  "small",
  "medium",
  "large",
  "full"
};
static const char *const enc_codeblock_size_list_text[] =
  { N_("automatic - let encoder decide based upon input (Best)"),
    N_("small - use small code blocks"),
    N_("medium - use medium sized code blocks"),
    N_("large - use large code blocks"),
    N_("full - One code block per subband"),
  };

/* advanced option only */
#define ENC_ME_HIERARCHICAL "enable-hierarchical-me"
#define ENC_ME_HIERARCHICAL_TEXT N_("Enable hierarchical Motion Estimation")

/* advanced option only */
#define ENC_ME_DOWNSAMPLE_LEVELS "downsample-levels"
#define ENC_ME_DOWNSAMPLE_LEVELS_TEXT N_("Number of levels of downsampling")
#define ENC_ME_DOWNSAMPLE_LEVELS_LONGTEXT N_("Number of levels of downsampling in hierarchical motion estimation mode")

/* advanced option only */
#define ENC_ME_GLOBAL_MOTION "enable-global-me"
#define ENC_ME_GLOBAL_MOTION_TEXT N_("Enable Global Motion Estimation")

/* advanced option only */
#define ENC_ME_PHASECORR "enable-phasecorr-me"
#define ENC_ME_PHASECORR_TEXT N_("Enable Phase Correlation Estimation")

/* advanced option only */
#define ENC_SCD "enable-scd"
#define ENC_SCD_TEXT N_("Enable Scene Change Detection")

/* advanced option only */
#define ENC_FORCE_PROFILE "force-profile"
#define ENC_FORCE_PROFILE_TEXT N_("Force Profile")

static const char *enc_profile_list[] = {
  "auto",
  "vc2_low_delay",
  "vc2_simple",
  "vc2_main",
  "main"
};

static const char *const enc_profile_list_text[] =
  { N_("automatic - let encoder decide based upon input (Best)"),
    N_("VC2 Low Delay Profile"),
    N_("VC2 Simple Profile"),
    N_("VC2 Main Profile"),
    N_("Main Profile"),
  };

static const char *const ppsz_enc_options[] = {
    ENC_RATE_CONTROL, ENC_GOP_STRUCTURE, ENC_QUALITY, ENC_NOISE_THRESHOLD, ENC_BITRATE,
    ENC_MIN_BITRATE, ENC_MAX_BITRATE, ENC_AU_DISTANCE, ENC_CHROMAFMT,
    ENC_PREFILTER, ENC_PREFILTER_STRENGTH, ENC_CODINGMODE, ENC_MCBLK_SIZE,
    ENC_MCBLK_OVERLAP, ENC_MVPREC, ENC_ME_COMBINED, ENC_DWTINTRA, ENC_DWTINTER,
    ENC_DWTDEPTH, ENC_MULTIQUANT, ENC_NOAC, ENC_PWT, ENC_PDIST, ENC_HSLICES,
    ENC_VSLICES, ENC_SCBLK_SIZE, ENC_ME_HIERARCHICAL, ENC_ME_DOWNSAMPLE_LEVELS,
    ENC_ME_GLOBAL_MOTION, ENC_ME_PHASECORR, ENC_SCD, ENC_FORCE_PROFILE,
    NULL
};


/* Module declaration */

vlc_module_begin ()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    set_shortname( "Schroedinger" )
    set_description( N_("Dirac video decoder using libschroedinger") )
    set_capability( "video decoder", 200 )
    set_callbacks( OpenDecoder, CloseDecoder )
    add_shortcut( "schroedinger" )

    /* encoder */
    add_submodule()
    set_section( N_("Encoding") , NULL )
    set_description( N_("Dirac video encoder using libschroedinger") )
    set_capability( "encoder", 110 )
    set_callbacks( OpenEncoder, CloseEncoder )
    add_shortcut( "schroedinger", "schro" )

    add_string( ENC_CFG_PREFIX ENC_RATE_CONTROL, NULL,
                 ENC_RATE_CONTROL_TEXT, ENC_RATE_CONTROL_LONGTEXT, false )
    change_string_list( enc_rate_control_list, enc_rate_control_list_text )

    add_float( ENC_CFG_PREFIX ENC_QUALITY, -1.,
               ENC_QUALITY_TEXT, ENC_QUALITY_LONGTEXT, false )
    change_float_range(-1., 10.);

    add_float( ENC_CFG_PREFIX ENC_NOISE_THRESHOLD, -1.,
               ENC_NOISE_THRESHOLD_TEXT, ENC_NOISE_THRESHOLD_LONGTEXT, false )
    change_float_range(-1., 100.);

    add_integer( ENC_CFG_PREFIX ENC_BITRATE, -1,
                 ENC_BITRATE_TEXT, ENC_BITRATE_LONGTEXT, false )
    change_integer_range(-1, INT_MAX);

    add_integer( ENC_CFG_PREFIX ENC_MAX_BITRATE, -1,
                 ENC_MAX_BITRATE_TEXT, ENC_MAX_BITRATE_LONGTEXT, false )
    change_integer_range(-1, INT_MAX);

    add_integer( ENC_CFG_PREFIX ENC_MIN_BITRATE, -1,
                 ENC_MIN_BITRATE_TEXT, ENC_MIN_BITRATE_LONGTEXT, false )
    change_integer_range(-1, INT_MAX);

    add_string( ENC_CFG_PREFIX ENC_GOP_STRUCTURE, NULL,
                 ENC_GOP_STRUCTURE_TEXT, ENC_GOP_STRUCTURE_LONGTEXT, false )
    change_string_list( enc_gop_structure_list, enc_gop_structure_list_text )

    add_integer( ENC_CFG_PREFIX ENC_AU_DISTANCE, -1,
                 ENC_AU_DISTANCE_TEXT, ENC_AU_DISTANCE_LONGTEXT, false )
    change_integer_range(-1, INT_MAX);

    add_string( ENC_CFG_PREFIX ENC_CHROMAFMT, "420",
                ENC_CHROMAFMT_TEXT, ENC_CHROMAFMT_LONGTEXT, false )
    change_string_list( enc_chromafmt_list, enc_chromafmt_list_text )

    add_string( ENC_CFG_PREFIX ENC_CODINGMODE, "auto",
                ENC_CODINGMODE_TEXT, ENC_CODINGMODE_LONGTEXT, false )
    change_string_list( enc_codingmode_list, enc_codingmode_list_text )

    add_string( ENC_CFG_PREFIX ENC_MVPREC, NULL,
                ENC_MVPREC_TEXT, ENC_MVPREC_LONGTEXT, false )
    change_string_list( enc_mvprec_list, enc_mvprec_list )

    /* advanced option only */
    add_string( ENC_CFG_PREFIX ENC_MCBLK_SIZE, NULL,
                ENC_MCBLK_SIZE_TEXT, ENC_MCBLK_SIZE_TEXT, true )
    change_string_list( enc_block_size_list, enc_block_size_list_text )


    /* advanced option only */
    add_string( ENC_CFG_PREFIX ENC_MCBLK_OVERLAP, NULL,
                ENC_MCBLK_OVERLAP_TEXT, ENC_MCBLK_OVERLAP_TEXT, true )
    change_string_list( enc_block_overlap_list, enc_block_overlap_list_text )

    /* advanced option only */
    add_integer( ENC_CFG_PREFIX ENC_ME_COMBINED, -1,
              ENC_ME_COMBINED_TEXT, ENC_ME_COMBINED_LONGTEXT, true )
    change_integer_range(-1, 1 );

    /* advanced option only */
    add_integer( ENC_CFG_PREFIX ENC_ME_HIERARCHICAL, -1,
                 ENC_ME_HIERARCHICAL_TEXT, ENC_ME_HIERARCHICAL_TEXT, true )
    change_integer_range(-1, 1 );

    /* advanced option only */
    add_integer( ENC_CFG_PREFIX ENC_ME_DOWNSAMPLE_LEVELS, -1,
                 ENC_ME_DOWNSAMPLE_LEVELS_TEXT, ENC_ME_DOWNSAMPLE_LEVELS_LONGTEXT, true )
    change_integer_range(-1, 8 );

    /* advanced option only */
    add_integer( ENC_CFG_PREFIX ENC_ME_GLOBAL_MOTION, -1,
                 ENC_ME_GLOBAL_MOTION_TEXT, ENC_ME_GLOBAL_MOTION_TEXT, true )
    change_integer_range(-1, 1 );

    /* advanced option only */
    add_integer( ENC_CFG_PREFIX ENC_ME_PHASECORR, -1,
                 ENC_ME_PHASECORR_TEXT, ENC_ME_PHASECORR_TEXT, true )
    change_integer_range(-1, 1 );

    add_string( ENC_CFG_PREFIX ENC_DWTINTRA, NULL,
                ENC_DWTINTRA_TEXT, ENC_DWTINTRA_TEXT, false )
    change_string_list( enc_wavelet_list, enc_wavelet_list_text )

    add_string( ENC_CFG_PREFIX ENC_DWTINTER, NULL,
                ENC_DWTINTER_TEXT, ENC_DWTINTER_TEXT, false )
    change_string_list( enc_wavelet_list, enc_wavelet_list_text )

    add_integer( ENC_CFG_PREFIX ENC_DWTDEPTH, -1,
                 ENC_DWTDEPTH_TEXT, ENC_DWTDEPTH_LONGTEXT, false )
    change_integer_range(-1, SCHRO_LIMIT_ENCODER_TRANSFORM_DEPTH );

    /* advanced option only */
    add_integer( ENC_CFG_PREFIX ENC_MULTIQUANT, -1,
                 ENC_MULTIQUANT_TEXT, ENC_MULTIQUANT_LONGTEXT, true )
    change_integer_range(-1, 1 );

    /* advanced option only */
    add_string( ENC_CFG_PREFIX ENC_SCBLK_SIZE, NULL,
                ENC_SCBLK_SIZE_TEXT, ENC_SCBLK_SIZE_TEXT, true )
    change_string_list( enc_codeblock_size_list, enc_codeblock_size_list_text )

    add_string( ENC_CFG_PREFIX ENC_PREFILTER, NULL,
                ENC_PREFILTER_TEXT, ENC_PREFILTER_LONGTEXT, false )
    change_string_list( enc_filtering_list, enc_filtering_list_text )

    add_float( ENC_CFG_PREFIX ENC_PREFILTER_STRENGTH, -1.,
                 ENC_PREFILTER_STRENGTH_TEXT, ENC_PREFILTER_STRENGTH_LONGTEXT, false )
    change_float_range(-1., 100.0);

    /* advanced option only */
    add_integer( ENC_CFG_PREFIX ENC_SCD, -1,
                 ENC_SCD_TEXT, ENC_SCD_TEXT, true )
    change_integer_range(-1, 1 );

    /* advanced option only */
    add_string( ENC_CFG_PREFIX ENC_PWT, NULL,
                ENC_PWT_TEXT, ENC_PWT_TEXT, true )
    change_string_list( enc_perceptual_weighting_list, enc_perceptual_weighting_list )

    /* advanced option only */
    add_float( ENC_CFG_PREFIX ENC_PDIST, -1,
               ENC_PDIST_TEXT, ENC_PDIST_LONGTEXT, true )
    change_float_range(-1., 100.);

    /* advanced option only */
    add_integer( ENC_CFG_PREFIX ENC_NOAC, -1,
              ENC_NOAC_TEXT, ENC_NOAC_LONGTEXT, true )
    change_integer_range(-1, 1 );

    /* advanced option only */
    add_integer( ENC_CFG_PREFIX ENC_HSLICES, -1,
                 ENC_HSLICES_TEXT, ENC_HSLICES_LONGTEXT, true )
    change_integer_range(-1, INT_MAX );

    /* advanced option only */
    add_integer( ENC_CFG_PREFIX ENC_VSLICES, -1,
                 ENC_VSLICES_TEXT, ENC_VSLICES_LONGTEXT, true )
    change_integer_range(-1, INT_MAX );

    /* advanced option only */
    add_string( ENC_CFG_PREFIX ENC_FORCE_PROFILE, NULL,
                ENC_FORCE_PROFILE_TEXT, ENC_FORCE_PROFILE_TEXT, true )
    change_string_list( enc_profile_list, enc_profile_list_text )

vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int DecodeBlock  ( decoder_t *p_dec, block_t *p_block );
static void Flush( decoder_t * );

struct picture_free_t
{
   picture_t *p_pic;
   decoder_t *p_dec;
};

/*****************************************************************************
 * decoder_sys_t : Schroedinger decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /*
     * Dirac properties
     */
    mtime_t i_lastpts;
    mtime_t i_frame_pts_delta;
    SchroDecoder *p_schro;
    SchroVideoFormat *p_format;
};

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;
    SchroDecoder *p_schro;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_DIRAC )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    p_sys = malloc(sizeof(decoder_sys_t));
    if( p_sys == NULL )
        return VLC_ENOMEM;

    /* Initialise the schroedinger (and hence liboil libraries */
    /* This does no allocation and is safe to call */
    schro_init();

    /* Initialise the schroedinger decoder */
    if( !(p_schro = schro_decoder_new()) )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_dec->p_sys = p_sys;
    p_sys->p_schro = p_schro;
    p_sys->p_format = NULL;
    p_sys->i_lastpts = VLC_TS_INVALID;
    p_sys->i_frame_pts_delta = 0;

    /* Set output properties */
    p_dec->fmt_out.i_codec = VLC_CODEC_I420;

    /* Set callbacks */
    p_dec->pf_decode = DecodeBlock;
    p_dec->pf_flush  = Flush;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SetPictureFormat: Set the decoded picture params to the ones from the stream
 *****************************************************************************/
static void SetVideoFormat( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    p_sys->p_format = schro_decoder_get_video_format(p_sys->p_schro);
    if( p_sys->p_format == NULL ) return;

    p_sys->i_frame_pts_delta = CLOCK_FREQ
                            * p_sys->p_format->frame_rate_denominator
                            / p_sys->p_format->frame_rate_numerator;

    switch( p_sys->p_format->chroma_format )
    {
    case SCHRO_CHROMA_420: p_dec->fmt_out.i_codec = VLC_CODEC_I420; break;
    case SCHRO_CHROMA_422: p_dec->fmt_out.i_codec = VLC_CODEC_I422; break;
    case SCHRO_CHROMA_444: p_dec->fmt_out.i_codec = VLC_CODEC_I444; break;
    default:
        p_dec->fmt_out.i_codec = 0;
        break;
    }

    p_dec->fmt_out.video.i_visible_width = p_sys->p_format->clean_width;
    p_dec->fmt_out.video.i_x_offset = p_sys->p_format->left_offset;
    p_dec->fmt_out.video.i_width = p_sys->p_format->width;

    p_dec->fmt_out.video.i_visible_height = p_sys->p_format->clean_height;
    p_dec->fmt_out.video.i_y_offset = p_sys->p_format->top_offset;
    p_dec->fmt_out.video.i_height = p_sys->p_format->height;

    /* aspect_ratio_[numerator|denominator] describes the pixel aspect ratio */
    p_dec->fmt_out.video.i_sar_num = p_sys->p_format->aspect_ratio_numerator;
    p_dec->fmt_out.video.i_sar_den = p_sys->p_format->aspect_ratio_denominator;

    p_dec->fmt_out.video.i_frame_rate =
        p_sys->p_format->frame_rate_numerator;
    p_dec->fmt_out.video.i_frame_rate_base =
        p_sys->p_format->frame_rate_denominator;
}

/*****************************************************************************
 * SchroFrameFree: schro_frame callback to release the associated picture_t
 * When schro_decoder_reset() is called there will be pictures in the
 * decoding pipeline that need to be released rather than displayed.
 *****************************************************************************/
static void SchroFrameFree( SchroFrame *frame, void *priv)
{
    struct picture_free_t *p_free = priv;

    if( !p_free )
        return;

    picture_Release( p_free->p_pic );
    free(p_free);
    (void)frame;
}

/*****************************************************************************
 * CreateSchroFrameFromPic: wrap a picture_t in a SchroFrame
 *****************************************************************************/
static SchroFrame *CreateSchroFrameFromPic( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    SchroFrame *p_schroframe = schro_frame_new();
    picture_t *p_pic = NULL;
    struct picture_free_t *p_free;

    if( !p_schroframe )
        return NULL;

    if( decoder_UpdateVideoFormat( p_dec ) )
        return NULL;
    p_pic = decoder_NewPicture( p_dec );

    if( !p_pic )
        return NULL;

    p_schroframe->format = SCHRO_FRAME_FORMAT_U8_420;
    if( p_sys->p_format->chroma_format == SCHRO_CHROMA_422 )
    {
        p_schroframe->format = SCHRO_FRAME_FORMAT_U8_422;
    }
    else if( p_sys->p_format->chroma_format == SCHRO_CHROMA_444 )
    {
        p_schroframe->format = SCHRO_FRAME_FORMAT_U8_444;
    }

    p_schroframe->width = p_sys->p_format->width;
    p_schroframe->height = p_sys->p_format->height;

    p_free = malloc( sizeof( *p_free ) );
    p_free->p_pic = p_pic;
    p_free->p_dec = p_dec;
    schro_frame_set_free_callback( p_schroframe, SchroFrameFree, p_free );

    for( int i=0; i<3; i++ )
    {
        p_schroframe->components[i].width = p_pic->p[i].i_visible_pitch;
        p_schroframe->components[i].stride = p_pic->p[i].i_pitch;
        p_schroframe->components[i].height = p_pic->p[i].i_visible_lines;
        p_schroframe->components[i].length =
            p_pic->p[i].i_pitch * p_pic->p[i].i_lines;
        p_schroframe->components[i].data = p_pic->p[i].p_pixels;

        if(i!=0)
        {
            p_schroframe->components[i].v_shift =
                SCHRO_FRAME_FORMAT_V_SHIFT( p_schroframe->format );
            p_schroframe->components[i].h_shift =
                SCHRO_FRAME_FORMAT_H_SHIFT( p_schroframe->format );
        }
    }

    p_pic->b_progressive = !p_sys->p_format->interlaced;
    p_pic->b_top_field_first = p_sys->p_format->top_field_first;
    p_pic->i_nb_fields = 2;

    return p_schroframe;
}

/*****************************************************************************
 * SchroBufferFree: schro_buffer callback to release the associated block_t
 *****************************************************************************/
static void SchroBufferFree( SchroBuffer *buf, void *priv )
{
    block_t *p_block = priv;

    if( !p_block )
        return;

    block_Release( p_block );
    (void)buf;
}

/*****************************************************************************
 * CloseDecoder: decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    schro_decoder_free( p_sys->p_schro );
    free( p_sys );
}

/*****************************************************************************
 * Flush:
 *****************************************************************************/
static void Flush( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    schro_decoder_reset( p_sys->p_schro );
    p_sys->i_lastpts = VLC_TS_INVALID;
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * Blocks need not be Dirac dataunit aligned.
 * If a block has a PTS signaled, it applies to the first picture at or after p_block
 ****************************************************************************/
static int DecodeBlock( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( !p_block ) /* No Drain */
        return VLCDEC_SUCCESS;
    else {

        /* reset the decoder when seeking as the decode in progress is invalid */
        /* discard the block as it is just a null magic block */
        if( p_block->i_flags & (BLOCK_FLAG_CORRUPTED|BLOCK_FLAG_DISCONTINUITY) )
        {
            Flush( p_dec );
            if( p_block->i_flags & BLOCK_FLAG_CORRUPTED )
            {
                block_Release( p_block );
                return VLCDEC_SUCCESS;
            }
        }

        SchroBuffer *p_schrobuffer;
        p_schrobuffer = schro_buffer_new_with_data( p_block->p_buffer, p_block->i_buffer );
        p_schrobuffer->free = SchroBufferFree;
        p_schrobuffer->priv = p_block;
        if( p_block->i_pts > VLC_TS_INVALID ) {
            mtime_t *p_pts = malloc( sizeof(*p_pts) );
            if( p_pts ) {
                *p_pts = p_block->i_pts;
                /* if this call fails, p_pts is freed automatically */
                p_schrobuffer->tag = schro_tag_new( p_pts, free );
            }
        }

        /* this stops the same block being fed back into this function if
         * we were on the next iteration of this loop to output a picture */
        schro_decoder_autoparse_push( p_sys->p_schro, p_schrobuffer );
        /* DO NOT refer to p_block after this point, it may have been freed */
    }

    while( 1 )
    {
        SchroFrame *p_schroframe;
        picture_t *p_pic;
        int state = schro_decoder_autoparse_wait( p_sys->p_schro );

        switch( state )
        {
        case SCHRO_DECODER_FIRST_ACCESS_UNIT:
            SetVideoFormat( p_dec );
            break;

        case SCHRO_DECODER_NEED_BITS:
            return VLCDEC_SUCCESS;

        case SCHRO_DECODER_NEED_FRAME:
            p_schroframe = CreateSchroFrameFromPic( p_dec );

            if( !p_schroframe )
            {
                msg_Err( p_dec, "Could not allocate picture for decoder");
                return VLCDEC_SUCCESS;
            }

            schro_decoder_add_output_picture( p_sys->p_schro, p_schroframe);
            break;

        case SCHRO_DECODER_OK: {
            SchroTag *p_tag = schro_decoder_get_picture_tag( p_sys->p_schro );
            p_schroframe = schro_decoder_pull( p_sys->p_schro );
            if( !p_schroframe || !p_schroframe->priv )
            {
                /* frame can't be one that was allocated by us
                 *   -- no private data: discard */
                if( p_tag ) schro_tag_free( p_tag );
                if( p_schroframe ) schro_frame_unref( p_schroframe );
                break;
            }
            p_pic = ((struct picture_free_t*) p_schroframe->priv)->p_pic;
            p_schroframe->priv = NULL;

            if( p_tag )
            {
                /* free is handled by schro_frame_unref */
                p_pic->date = *(mtime_t*) p_tag->value;
                schro_tag_free( p_tag );
            }
            else if( p_sys->i_lastpts > VLC_TS_INVALID )
            {
                /* NB, this shouldn't happen since the packetizer does a
                 * very thorough job of inventing timestamps.  The
                 * following is just a very rough fall back incase packetizer
                 * is missing. */
                /* maybe it would be better to set p_pic->b_force ? */
                p_pic->date = p_sys->i_lastpts + p_sys->i_frame_pts_delta;
            }
            p_sys->i_lastpts = p_pic->date;

            schro_frame_unref( p_schroframe );
            decoder_QueueVideo( p_dec, p_pic );
            return VLCDEC_SUCCESS;
        }
        case SCHRO_DECODER_EOS:
            /* NB, the new api will not emit _EOS, it handles the reset internally */
            break;

        case SCHRO_DECODER_ERROR:
            msg_Err( p_dec, "SCHRO_DECODER_ERROR");
            return VLCDEC_SUCCESS;
        }
    }
}

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static block_t *Encode( encoder_t *p_enc, picture_t *p_pict );


/*****************************************************************************
 * picture_pts_t : store pts alongside picture number, not carried through
 * encoder
 *****************************************************************************/
struct picture_pts_t
{
   mtime_t i_pts;    /* associated pts */
   uint32_t u_pnum;  /* dirac picture number */
   bool b_empty;     /* entry is invalid */
};

/*****************************************************************************
 * encoder_sys_t : Schroedinger encoder descriptor
 *****************************************************************************/
#define SCHRO_PTS_TLB_SIZE 256
struct encoder_sys_t
{
    /*
     * Schro properties
     */
    SchroEncoder *p_schro;
    SchroVideoFormat *p_format;
    int started;
    bool b_auto_field_coding;

    uint32_t i_input_picnum;
    block_fifo_t *p_dts_fifo;

    block_t *p_chain;

    struct picture_pts_t pts_tlb[SCHRO_PTS_TLB_SIZE];
    mtime_t i_pts_offset;
    mtime_t i_field_time;

    bool b_eos_signalled;
    bool b_eos_pulled;
};

static struct
{
    unsigned int i_height;
    int i_approx_fps;
    SchroVideoFormatEnum i_vf;
} schro_format_guess[] = {
    /* Important: Keep this list ordered in ascending picture height */
    {1, 0, SCHRO_VIDEO_FORMAT_CUSTOM},
    {120, 15, SCHRO_VIDEO_FORMAT_QSIF},
    {144, 12, SCHRO_VIDEO_FORMAT_QCIF},
    {240, 15, SCHRO_VIDEO_FORMAT_SIF},
    {288, 12, SCHRO_VIDEO_FORMAT_CIF},
    {480, 30, SCHRO_VIDEO_FORMAT_SD480I_60},
    {480, 15, SCHRO_VIDEO_FORMAT_4SIF},
    {576, 12, SCHRO_VIDEO_FORMAT_4CIF},
    {576, 25, SCHRO_VIDEO_FORMAT_SD576I_50},
    {720, 50, SCHRO_VIDEO_FORMAT_HD720P_50},
    {720, 60, SCHRO_VIDEO_FORMAT_HD720P_60},
    {1080, 24, SCHRO_VIDEO_FORMAT_DC2K_24},
    {1080, 25, SCHRO_VIDEO_FORMAT_HD1080I_50},
    {1080, 30, SCHRO_VIDEO_FORMAT_HD1080I_60},
    {1080, 50, SCHRO_VIDEO_FORMAT_HD1080P_50},
    {1080, 60, SCHRO_VIDEO_FORMAT_HD1080P_60},
    {2160, 24, SCHRO_VIDEO_FORMAT_DC4K_24},
    {0, 0, 0},
};

/*****************************************************************************
 * ResetPTStlb: Purge all entries in @p_enc@'s PTS-tlb
 *****************************************************************************/
static void ResetPTStlb( encoder_t *p_enc )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    for( int i = 0; i < SCHRO_PTS_TLB_SIZE; i++ )
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

    for( int i = 0; i<SCHRO_PTS_TLB_SIZE; i++ )
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

    for( int i = 0; i < SCHRO_PTS_TLB_SIZE; i++ )
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

static inline bool SchroSetEnum( const encoder_t *p_enc, int i_list_size, const char *list[],
                  const char *psz_name,  const char *psz_name_text,  const char *psz_value)
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    if( list && psz_name_text && psz_name && psz_value ) {
        for( int i = 0; i < i_list_size; ++i ) {
            if( strcmp( list[i], psz_value ) )
                continue;
            schro_encoder_setting_set_double( p_sys->p_schro, psz_name, i );
            return true;
        }
        msg_Err( p_enc, "Invalid %s: %s", psz_name_text, psz_value );
    }
    return false;
}

static bool SetEncChromaFormat( encoder_t *p_enc, uint32_t i_codec )
{
    encoder_sys_t *p_sys = p_enc->p_sys;

    switch( i_codec ) {
    case VLC_CODEC_I420:
        p_enc->fmt_in.i_codec = i_codec;
        p_enc->fmt_in.video.i_bits_per_pixel = 12;
        p_sys->p_format->chroma_format = SCHRO_CHROMA_420;
           break;
    case VLC_CODEC_I422:
        p_enc->fmt_in.i_codec = i_codec;
        p_enc->fmt_in.video.i_bits_per_pixel = 16;
        p_sys->p_format->chroma_format = SCHRO_CHROMA_422;
        break;
    case VLC_CODEC_I444:
        p_enc->fmt_in.i_codec = i_codec;
        p_enc->fmt_in.video.i_bits_per_pixel = 24;
        p_sys->p_format->chroma_format = SCHRO_CHROMA_444;
        break;
    default:
        return false;
    }

    return true;
}

#define SCHRO_SET_FLOAT(psz_name, pschro_name) \
    f_tmp = var_GetFloat( p_enc, ENC_CFG_PREFIX psz_name ); \
    if( f_tmp >= 0.0 ) \
        schro_encoder_setting_set_double( p_sys->p_schro, pschro_name, f_tmp );

#define SCHRO_SET_INTEGER(psz_name, pschro_name, ignore_val) \
    i_tmp = var_GetInteger( p_enc, ENC_CFG_PREFIX psz_name ); \
    if( i_tmp > ignore_val ) \
        schro_encoder_setting_set_double( p_sys->p_schro, pschro_name, i_tmp );

#define SCHRO_SET_ENUM(list, psz_name, psz_name_text, pschro_name) \
    psz_tmp = var_GetString( p_enc, ENC_CFG_PREFIX psz_name ); \
    if( !psz_tmp ) \
        goto error; \
    else if ( *psz_tmp != '\0' ) { \
        int i_list_size = ARRAY_SIZE(list); \
        if( !SchroSetEnum( p_enc, i_list_size, list, pschro_name, psz_name_text, psz_tmp ) ) { \
            free( psz_tmp ); \
            goto error; \
        } \
    } \
    free( psz_tmp );

/*****************************************************************************
 * OpenEncoder: probe the encoder and return score
 *****************************************************************************/
static int OpenEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys;
    int i_tmp;
    float f_tmp;
    char *psz_tmp;

    if( p_enc->fmt_out.i_codec != VLC_CODEC_DIRAC &&
        !p_enc->obj.force )
    {
        return VLC_EGENERIC;
    }

    if( !p_enc->fmt_in.video.i_frame_rate || !p_enc->fmt_in.video.i_frame_rate_base ||
        !p_enc->fmt_in.video.i_visible_height || !p_enc->fmt_in.video.i_visible_width )
    {
        msg_Err( p_enc, "Framerate and picture dimensions must be non-zero" );
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_sys = calloc( 1, sizeof( *p_sys ) ) ) == NULL )
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
    SchroVideoFormatEnum guessed_video_fmt = SCHRO_VIDEO_FORMAT_CUSTOM;
    /* Pick the dirac_video_format in this order of preference:
     *  1. an exact match in frame height and an approximate fps match
     *  2. the previous preset with a smaller number of lines.
     */
    do
    {
        if( schro_format_guess[i].i_height > p_enc->fmt_in.video.i_height )
        {
            guessed_video_fmt = schro_format_guess[i-1].i_vf;
            break;
        }
        if( schro_format_guess[i].i_height != p_enc->fmt_in.video.i_height )
            continue;
        int src_fps = p_enc->fmt_in.video.i_frame_rate / p_enc->fmt_in.video.i_frame_rate_base;
        int delta_fps = abs( schro_format_guess[i].i_approx_fps - src_fps );
        if( delta_fps > 2 )
            continue;

        guessed_video_fmt = schro_format_guess[i].i_vf;
        break;
    } while( schro_format_guess[++i].i_height );

    schro_init();
    p_sys->p_schro = schro_encoder_new();
    if( !p_sys->p_schro ) {
        msg_Err( p_enc, "Failed to initialize libschroedinger encoder" );
        return VLC_EGENERIC;
    }
    schro_encoder_set_packet_assembly( p_sys->p_schro, true );

    if( !( p_sys->p_format = schro_encoder_get_video_format( p_sys->p_schro ) ) ) {
        msg_Err( p_enc, "Failed to get Schroedigner video format" );
        schro_encoder_free( p_sys->p_schro );
        return VLC_EGENERIC;
    }

    /* initialise the video format parameters to the guessed format */
    schro_video_format_set_std_video_format( p_sys->p_format, guessed_video_fmt );

    /* constants set from the input video format */
    p_sys->p_format->width                  = p_enc->fmt_in.video.i_visible_width;
    p_sys->p_format->height                 = p_enc->fmt_in.video.i_visible_height;
    p_sys->p_format->frame_rate_numerator   = p_enc->fmt_in.video.i_frame_rate;
    p_sys->p_format->frame_rate_denominator = p_enc->fmt_in.video.i_frame_rate_base;
    unsigned u_asr_num, u_asr_den;
    vlc_ureduce( &u_asr_num, &u_asr_den,
                 p_enc->fmt_in.video.i_sar_num,
                 p_enc->fmt_in.video.i_sar_den, 0 );
    p_sys->p_format->aspect_ratio_numerator   = u_asr_num;
    p_sys->p_format->aspect_ratio_denominator = u_asr_den;

    config_ChainParse( p_enc, ENC_CFG_PREFIX, ppsz_enc_options, p_enc->p_cfg );

    SCHRO_SET_ENUM(enc_rate_control_list, ENC_RATE_CONTROL, ENC_RATE_CONTROL_TEXT, "rate_control")

    SCHRO_SET_ENUM(enc_gop_structure_list, ENC_GOP_STRUCTURE, ENC_GOP_STRUCTURE_TEXT, "gop_structure")

    psz_tmp = var_GetString( p_enc, ENC_CFG_PREFIX ENC_CHROMAFMT );
    if( !psz_tmp )
        goto error;
    else {
        uint32_t i_codec;
        if( !strcmp( psz_tmp, "420" ) ) {
            i_codec = VLC_CODEC_I420;
        }
        else if( !strcmp( psz_tmp, "422" ) ) {
            i_codec = VLC_CODEC_I422;
        }
        else if( !strcmp( psz_tmp, "444" ) ) {
            i_codec = VLC_CODEC_I444;
        }
        else {
            msg_Err( p_enc, "Invalid chroma format: %s", psz_tmp );
            free( psz_tmp );
            goto error;
        }
        SetEncChromaFormat( p_enc, i_codec );
    }
    free( psz_tmp );

    SCHRO_SET_FLOAT(ENC_QUALITY, "quality")

    SCHRO_SET_FLOAT(ENC_NOISE_THRESHOLD, "noise_threshold")

    /* use bitrate from sout-transcode-vb in kbps */
    i_tmp = var_GetInteger( p_enc, ENC_CFG_PREFIX ENC_BITRATE );
    if( i_tmp > -1 )
        schro_encoder_setting_set_double( p_sys->p_schro, "bitrate", i_tmp * 1000 );
    else
        schro_encoder_setting_set_double( p_sys->p_schro, "bitrate", p_enc->fmt_out.i_bitrate );

       p_enc->fmt_out.i_bitrate = schro_encoder_setting_get_double( p_sys->p_schro, "bitrate" );

    i_tmp = var_GetInteger( p_enc, ENC_CFG_PREFIX ENC_MIN_BITRATE );
    if( i_tmp > -1 )
        schro_encoder_setting_set_double( p_sys->p_schro, "min_bitrate", i_tmp * 1000 );

    i_tmp = var_GetInteger( p_enc, ENC_CFG_PREFIX ENC_MAX_BITRATE );
    if( i_tmp > -1 )
        schro_encoder_setting_set_double( p_sys->p_schro, "max_bitrate", i_tmp * 1000 );

    SCHRO_SET_INTEGER(ENC_AU_DISTANCE, "au_distance", -1)

    SCHRO_SET_ENUM(enc_filtering_list, ENC_PREFILTER, ENC_PREFILTER_TEXT, "filtering")

    SCHRO_SET_FLOAT(ENC_PREFILTER_STRENGTH, "filter_value")


    psz_tmp = var_GetString( p_enc, ENC_CFG_PREFIX ENC_CODINGMODE );
    if( !psz_tmp )
        goto error;
    else if( !strcmp( psz_tmp, "auto" ) ) {
        p_sys->b_auto_field_coding = true;
    }
    else if( !strcmp( psz_tmp, "progressive" ) ) {
        p_sys->b_auto_field_coding = false;
        schro_encoder_setting_set_double( p_sys->p_schro, "interlaced_coding", false);
    }
    else if( !strcmp( psz_tmp, "field" ) ) {
        p_sys->b_auto_field_coding = false;
        schro_encoder_setting_set_double( p_sys->p_schro, "interlaced_coding", true);
    }
    else {
        msg_Err( p_enc, "Invalid codingmode: %s", psz_tmp );
        free( psz_tmp );
        goto error;
    }
    free( psz_tmp );

    SCHRO_SET_ENUM(enc_block_size_list, ENC_MCBLK_SIZE, ENC_MCBLK_SIZE_TEXT, "motion_block_size")

    SCHRO_SET_ENUM(enc_block_overlap_list, ENC_MCBLK_OVERLAP, ENC_MCBLK_OVERLAP_TEXT, "motion_block_overlap")

    psz_tmp = var_GetString( p_enc, ENC_CFG_PREFIX ENC_MVPREC );
    if( !psz_tmp )
        goto error;
    else if( *psz_tmp != '\0') {
        if( !strcmp( psz_tmp, "1" ) ) {
            schro_encoder_setting_set_double( p_sys->p_schro, "mv_precision", 0 );
        }
        else if( !strcmp( psz_tmp, "1/2" ) ) {
            schro_encoder_setting_set_double( p_sys->p_schro, "mv_precision", 1 );
        }
        else if( !strcmp( psz_tmp, "1/4" ) ) {
            schro_encoder_setting_set_double( p_sys->p_schro, "mv_precision", 2 );
        }
        else if( !strcmp( psz_tmp, "1/8" ) ) {
            schro_encoder_setting_set_double( p_sys->p_schro, "mv_precision", 3 );
        }
        else {
            msg_Err( p_enc, "Invalid mv_precision: %s", psz_tmp );
            free( psz_tmp );
            goto error;
        }
    }
    free( psz_tmp );

    SCHRO_SET_INTEGER(ENC_ME_COMBINED, "enable_chroma_me", -1)

    SCHRO_SET_ENUM(enc_wavelet_list, ENC_DWTINTRA, ENC_DWTINTRA_TEXT, "intra_wavelet")

    SCHRO_SET_ENUM(enc_wavelet_list, ENC_DWTINTER, ENC_DWTINTER_TEXT, "inter_wavelet")

    SCHRO_SET_INTEGER(ENC_DWTDEPTH, "transform_depth", -1)

    SCHRO_SET_INTEGER(ENC_MULTIQUANT, "enable_multiquant", -1)

    SCHRO_SET_INTEGER(ENC_NOAC, "enable_noarith", -1)

    SCHRO_SET_ENUM(enc_perceptual_weighting_list, ENC_PWT, ENC_PWT_TEXT, "perceptual_weighting")

    SCHRO_SET_FLOAT(ENC_PDIST, "perceptual_distance")

    SCHRO_SET_INTEGER(ENC_HSLICES, "horiz_slices", -1)

    SCHRO_SET_INTEGER(ENC_VSLICES, "vert_slices", -1)

    SCHRO_SET_ENUM(enc_codeblock_size_list, ENC_SCBLK_SIZE, ENC_SCBLK_SIZE_TEXT, "codeblock_size")

    SCHRO_SET_INTEGER(ENC_ME_HIERARCHICAL, "enable_hierarchical_estimation", -1)

    SCHRO_SET_INTEGER(ENC_ME_DOWNSAMPLE_LEVELS, "downsample_levels", 1)

    SCHRO_SET_INTEGER(ENC_ME_GLOBAL_MOTION, "enable_global_motion", -1)

    SCHRO_SET_INTEGER(ENC_ME_PHASECORR, "enable_phasecorr_estimation", -1)

    SCHRO_SET_INTEGER(ENC_SCD, "enable_scene_change_detection", -1)

    SCHRO_SET_ENUM(enc_profile_list, ENC_FORCE_PROFILE, ENC_FORCE_PROFILE_TEXT, "force_profile")

    p_sys->started = 0;

    return VLC_SUCCESS;
error:
    CloseEncoder( p_this );
    return VLC_EGENERIC;
}


struct enc_picture_free_t
{
   picture_t *p_pic;
   encoder_t *p_enc;
};

/*****************************************************************************
 * EncSchroFrameFree: schro_frame callback to release the associated picture_t
 * When schro_encoder_reset() is called there will be pictures in the
 * encoding pipeline that need to be released rather than displayed.
 *****************************************************************************/
static void EncSchroFrameFree( SchroFrame *frame, void *priv )
{
    struct enc_picture_free_t *p_free = priv;

    if( !p_free )
        return;

    picture_Release( p_free->p_pic );
    free( p_free );
    (void)frame;
}

/*****************************************************************************
 * CreateSchroFrameFromPic: wrap a picture_t in a SchroFrame
 *****************************************************************************/
static SchroFrame *CreateSchroFrameFromInputPic( encoder_t *p_enc,  picture_t *p_pic )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    SchroFrame *p_schroframe = schro_frame_new();
    struct enc_picture_free_t *p_free;

    if( !p_schroframe )
        return NULL;

    if( !p_pic )
        return NULL;

    p_schroframe->format = SCHRO_FRAME_FORMAT_U8_420;
    if( p_sys->p_format->chroma_format == SCHRO_CHROMA_422 )
    {
        p_schroframe->format = SCHRO_FRAME_FORMAT_U8_422;
    }
    else if( p_sys->p_format->chroma_format == SCHRO_CHROMA_444 )
    {
        p_schroframe->format = SCHRO_FRAME_FORMAT_U8_444;
    }

    p_schroframe->width  = p_sys->p_format->width;
    p_schroframe->height = p_sys->p_format->height;

    p_free = malloc( sizeof( *p_free ) );
    if( unlikely( p_free == NULL ) ) {
        schro_frame_unref( p_schroframe );
        return NULL;
    }

    p_free->p_pic = p_pic;
    p_free->p_enc = p_enc;
    schro_frame_set_free_callback( p_schroframe, EncSchroFrameFree, p_free );

    for( int i=0; i<3; i++ )
    {
        p_schroframe->components[i].width  = p_pic->p[i].i_visible_pitch;
        p_schroframe->components[i].stride = p_pic->p[i].i_pitch;
        p_schroframe->components[i].height = p_pic->p[i].i_visible_lines;
        p_schroframe->components[i].length =
            p_pic->p[i].i_pitch * p_pic->p[i].i_lines;
        p_schroframe->components[i].data   = p_pic->p[i].p_pixels;

        if( i!=0 )
        {
            p_schroframe->components[i].v_shift =
                SCHRO_FRAME_FORMAT_V_SHIFT( p_schroframe->format );
            p_schroframe->components[i].h_shift =
                SCHRO_FRAME_FORMAT_H_SHIFT( p_schroframe->format );
        }
    }

    return p_schroframe;
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


static block_t *Encode( encoder_t *p_enc, picture_t *p_pic )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    block_t *p_block, *p_output_chain = NULL;
    SchroFrame *p_frame;
    bool b_go = true;

    if( !p_pic ) {
        if( !p_sys->started || p_sys->b_eos_pulled )
            return NULL;

        if( !p_sys->b_eos_signalled ) {
            p_sys->b_eos_signalled = 1;
            schro_encoder_end_of_stream( p_sys->p_schro );
        }
    } else {
        /* we only know if the sequence is interlaced when the first
         * picture arrives, so final setup is done here */
        /* XXX todo, detect change of interlace */
        p_sys->p_format->interlaced = !p_pic->b_progressive;
        p_sys->p_format->top_field_first = p_pic->b_top_field_first;

        if( p_sys->b_auto_field_coding )
            schro_encoder_setting_set_double( p_sys->p_schro, "interlaced_coding", !p_pic->b_progressive );
    }

    if( !p_sys->started ) {
        date_t date;

        if( p_pic->format.i_chroma != p_enc->fmt_in.i_codec ) {
            char chroma_in[5], chroma_out[5];
            vlc_fourcc_to_char( p_pic->format.i_chroma, chroma_in );
            chroma_in[4]  = '\0';
            chroma_out[4] = '\0';
            vlc_fourcc_to_char( p_enc->fmt_in.i_codec, chroma_out );
            msg_Warn( p_enc, "Resetting chroma from %s to %s", chroma_out, chroma_in );
            if( !SetEncChromaFormat( p_enc, p_pic->format.i_chroma ) ) {
                msg_Err( p_enc, "Could not reset chroma format to %s", chroma_in );
                return NULL;
            }
        }

        date_Init( &date, p_enc->fmt_in.video.i_frame_rate, p_enc->fmt_in.video.i_frame_rate_base );
        /* FIXME - Unlike dirac-research codec Schro doesn't have a function that returns the delay in pics yet.
         *   Use a default of 1
         */
        date_Increment( &date, 1 );
        p_sys->i_pts_offset = date_Get( &date );
        if( schro_encoder_setting_get_double( p_sys->p_schro, "interlaced_coding" ) > 0.0 ) {
            date_Set( &date, 0 );
            date_Increment( &date, 1);
            p_sys->i_field_time = date_Get( &date ) / 2;
        }

        schro_video_format_set_std_signal_range( p_sys->p_format, SCHRO_SIGNAL_RANGE_8BIT_VIDEO );
        schro_encoder_set_video_format( p_sys->p_schro, p_sys->p_format );
        schro_encoder_start( p_sys->p_schro );
        p_sys->started = 1;
    }

    if( !p_sys->b_eos_signalled ) {
        /* create a schro frame from the input pic and load */
        /* Increase ref count by 1 so that the picture is not freed until
           Schro finishes with it */
        picture_Hold( p_pic );

        p_frame = CreateSchroFrameFromInputPic( p_enc, p_pic );
        if( !p_frame )
            return NULL;
        schro_encoder_push_frame( p_sys->p_schro, p_frame );


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
        if( schro_encoder_setting_get_double( p_sys->p_schro, "interlaced_coding" ) > 0.0 ) {
            StorePicturePTS( p_enc, p_sys->i_input_picnum, p_pic->date + p_sys->i_field_time );
            p_sys->i_input_picnum++;

            p_block = block_Alloc( 1 );
            if( !p_block )
                return NULL;
            p_block->i_dts = p_pic->date - p_sys->i_pts_offset + p_sys->i_field_time;
            block_FifoPut( p_sys->p_dts_fifo, p_block );
            p_block = NULL;
        }
    }

    do
    {
        SchroStateEnum state;
        state = schro_encoder_wait( p_sys->p_schro );
        switch( state )
        {
        case SCHRO_STATE_NEED_FRAME:
            b_go = false;
            break;
        case SCHRO_STATE_AGAIN:
            break;
        case SCHRO_STATE_END_OF_STREAM:
            p_sys->b_eos_pulled = 1;
            b_go = false;
            break;
        case SCHRO_STATE_HAVE_BUFFER:
        {
            SchroBuffer *p_enc_buf;
            uint32_t u_pic_num;
            int i_presentation_frame;
            p_enc_buf = schro_encoder_pull( p_sys->p_schro, &i_presentation_frame );
            p_block = block_Alloc( p_enc_buf->length );
            if( !p_block )
                return NULL;

            memcpy( p_block->p_buffer, p_enc_buf->data, p_enc_buf->length );
            schro_buffer_unref( p_enc_buf );

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

                    if( len > UINT32_MAX - sizeof( eos ) )
                        return NULL;

                    p_enc->fmt_out.p_extra = malloc( len + sizeof( eos ) );
                    if( !p_enc->fmt_out.p_extra )
                        return NULL;
                    memcpy( p_enc->fmt_out.p_extra, p_block->p_buffer, len );
                    memcpy( (uint8_t*)p_enc->fmt_out.p_extra + len, eos, sizeof( eos ) );
                    SetDWBE( (uint8_t*)p_enc->fmt_out.p_extra + len + sizeof(eos) - 4, len );
                    p_enc->fmt_out.i_extra = len + sizeof( eos );
                }
            }

            if( ReadDiracPictureNumber( &u_pic_num, p_block ) ) {
                block_t *p_dts_block = block_FifoGet( p_sys->p_dts_fifo );
                p_block->i_dts = p_dts_block->i_dts;
                   p_block->i_pts = GetPicturePTS( p_enc, u_pic_num );
                block_Release( p_dts_block );
                block_ChainAppend( &p_output_chain, p_block );
            } else {
                /* End of sequence */
                block_ChainAppend( &p_output_chain, p_block );
            }
            break;
        }
        default:
            break;
        }
    } while( b_go );

    return p_output_chain;
}

/*****************************************************************************
 * CloseEncoder: Schro encoder destruction
 *****************************************************************************/
static void CloseEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;

    /* Free the encoder resources */
    if( p_sys->p_schro )
        schro_encoder_free( p_sys->p_schro );

    free( p_sys->p_format );

    if( p_sys->p_dts_fifo )
        block_FifoRelease( p_sys->p_dts_fifo );

    block_ChainRelease( p_sys->p_chain );

    free( p_sys );
}
