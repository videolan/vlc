/*****************************************************************************
 * avcodec.h: decoder and encoder using libavcodec
 *****************************************************************************
 * Copyright (C) 2001-2008 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#include "chroma.h"
#include "avcommon.h"

/* VLC <-> avcodec tables */
bool GetFfmpegCodec( enum es_format_category_e cat, vlc_fourcc_t i_fourcc,
                     unsigned *pi_ffmpeg_codec, const char **ppsz_name );
vlc_fourcc_t GetVlcFourcc( unsigned i_ffmpeg_codec );
vlc_fourcc_t GetVlcAudioFormat( int i_sample_fmt );

/* Video encoder module */
int  InitVideoEnc ( vlc_object_t * );
void EndVideoEnc( vlc_object_t * );

/* Video Decoder */
int InitVideoDec( vlc_object_t * );
void EndVideoDec( vlc_object_t * );

/* Audio Decoder */
int InitAudioDec( vlc_object_t * );
void EndAudioDec( vlc_object_t * );

/* Subtitle Decoder */
int InitSubtitleDec( vlc_object_t * );
void EndSubtitleDec( vlc_object_t * );

/* Initialize decoder */
AVCodecContext *ffmpeg_AllocContext( decoder_t *, const AVCodec ** );
int ffmpeg_OpenCodec( decoder_t *p_dec, AVCodecContext *, const AVCodec * );

/*****************************************************************************
 * Module descriptor help strings
 *****************************************************************************/
#define DR_TEXT N_("Direct rendering")
/* FIXME Does somebody who knows what it does, explain */

#define CORRUPTED_TEXT N_("Show corrupted frames")
#define CORRUPTED_LONGTEXT N_("Prefer visual artifacts instead of missing frames")

#define ERROR_TEXT N_("Error resilience")
#define ERROR_LONGTEXT N_( \
    "libavcodec can do error resilience.\n" \
    "However, with a buggy encoder (such as the ISO MPEG-4 encoder from M$) " \
    "this can produce a lot of errors.\n" \
    "Valid values range from 0 to 4 (0 disables all errors resilience).")

#define BUGS_TEXT N_("Workaround bugs")
#define BUGS_LONGTEXT N_( \
    "Try to fix some bugs:\n" \
    "1  autodetect\n" \
    "2  old msmpeg4\n" \
    "4  xvid interlaced\n" \
    "8  ump4 \n" \
    "16 no padding\n" \
    "32 ac vlc\n" \
    "64 Qpel chroma.\n" \
    "This must be the sum of the values. For example, to fix \"ac vlc\" and " \
    "\"ump4\", enter 40.")

#define HURRYUP_TEXT N_("Hurry up")
#define HURRYUP_LONGTEXT N_( \
    "The decoder can partially decode or skip frame(s) " \
    "when there is not enough time. It's useful with low CPU power " \
    "but it can produce distorted pictures.")

#define SKIP_FRAME_TEXT N_("Skip frame (default=0)")
#define SKIP_FRAME_LONGTEXT N_( \
    "Force skipping of frames to speed up decoding " \
    "(-1=None, 0=Default, 1=B-frames, 2=P-frames, 3=B+P frames, 4=all frames)." )

#define SKIP_IDCT_TEXT N_("Skip idct (default=0)")
#define SKIP_IDCT_LONGTEXT N_( \
    "Force skipping of idct to speed up decoding for frame types " \
    "(-1=None, 0=Default, 1=B-frames, 2=P-frames, 3=B+P frames, 4=all frames)." )

#define DEBUG_TEXT N_( "Debug mask" )
#define DEBUG_LONGTEXT N_( "Set FFmpeg debug mask" )

#define CODEC_TEXT N_( "Codec name" )
#define CODEC_LONGTEXT N_( "Internal libavcodec codec name" )

#define SKIPLOOPF_TEXT N_( "Skip the loop filter for H.264 decoding" )
#define SKIPLOOPF_LONGTEXT N_( "Skipping the loop filter (aka deblocking) " \
    "usually has a detrimental effect on quality. However it provides a big " \
    "speedup for high definition streams." )

#define THREADS_TEXT N_( "Threads" )
#define THREADS_LONGTEXT N_( "Number of threads used for decoding, 0 meaning auto" )

/*
 * Encoder options
 */
#define ENC_CFG_PREFIX "sout-avcodec-"

#define ENC_KEYINT_TEXT N_( "Ratio of key frames" )
#define ENC_KEYINT_LONGTEXT N_( "Number of frames " \
  "that will be coded for one key frame." )

#define ENC_BFRAMES_TEXT N_( "Ratio of B frames" )
#define ENC_BFRAMES_LONGTEXT N_( "Number of " \
  "B frames that will be coded between two reference frames." )

#define ENC_VT_TEXT N_( "Video bitrate tolerance" )
#define ENC_VT_LONGTEXT N_( "Video bitrate tolerance in kbit/s." )

#define ENC_INTERLACE_TEXT N_( "Interlaced encoding" )
#define ENC_INTERLACE_LONGTEXT N_( "Enable dedicated " \
  "algorithms for interlaced frames." )

#define ENC_INTERLACE_ME_TEXT N_( "Interlaced motion estimation" )
#define ENC_INTERLACE_ME_LONGTEXT N_( "Enable interlaced " \
  "motion estimation algorithms. This requires more CPU." )

#define ENC_PRE_ME_TEXT N_( "Pre-motion estimation" )
#define ENC_PRE_ME_LONGTEXT N_( "Enable the pre-motion " \
  "estimation algorithm.")

#define ENC_RC_BUF_TEXT N_( "Rate control buffer size" )
#define ENC_RC_BUF_LONGTEXT N_( "Rate control " \
  "buffer size (in kbytes). A bigger buffer will allow for better rate " \
  "control, but will cause a delay in the stream." )

#define ENC_RC_BUF_AGGR_TEXT N_( "Rate control buffer aggressiveness" )
#define ENC_RC_BUF_AGGR_LONGTEXT N_( "Rate control "\
  "buffer aggressiveness." )

#define ENC_IQUANT_FACTOR_TEXT N_( "I quantization factor" )
#define ENC_IQUANT_FACTOR_LONGTEXT N_(  \
  "Quantization factor of I frames, compared with P frames (for instance " \
  "1.0 => same qscale for I and P frames)." )

#define ENC_NOISE_RED_TEXT N_( "Noise reduction" )
#define ENC_NOISE_RED_LONGTEXT N_( "Enable a simple noise " \
  "reduction algorithm to lower the encoding length and bitrate, at the " \
  "expense of lower quality frames." )

#define ENC_MPEG4_MATRIX_TEXT N_( "MPEG4 quantization matrix" )
#define ENC_MPEG4_MATRIX_LONGTEXT N_( "Use the MPEG4 " \
  "quantization matrix for MPEG2 encoding. This generally yields a " \
  "better looking picture, while still retaining the compatibility with " \
  "standard MPEG2 decoders.")

#define ENC_HQ_TEXT N_( "Quality level" )
#define ENC_HQ_LONGTEXT N_( "Quality level " \
  "for the encoding of motions vectors (this can slow down the encoding " \
  "very much)." )

#define ENC_HURRYUP_TEXT N_( "Hurry up" )
#define ENC_HURRYUP_LONGTEXT N_( "The encoder " \
  "can make on-the-fly quality tradeoffs if your CPU can't keep up with " \
  "the encoding rate. It will disable trellis quantization, then the rate " \
  "distortion of motion vectors (hq), and raise the noise reduction " \
  "threshold to ease the encoder's task." )

#define ENC_QMIN_TEXT N_( "Minimum video quantizer scale" )
#define ENC_QMIN_LONGTEXT N_( "Minimum video " \
  "quantizer scale." )

#define ENC_QMAX_TEXT N_( "Maximum video quantizer scale" )
#define ENC_QMAX_LONGTEXT N_( "Maximum video " \
  "quantizer scale." )

#define ENC_TRELLIS_TEXT N_( "Trellis quantization" )
#define ENC_TRELLIS_LONGTEXT N_( "Enable trellis " \
  "quantization (rate distortion for block coefficients)." )

#define ENC_QSCALE_TEXT N_( "Fixed quantizer scale" )
#define ENC_QSCALE_LONGTEXT N_( "A fixed video " \
  "quantizer scale for VBR encoding (accepted values: 0.01 to 255.0)." )

#define ENC_STRICT_TEXT N_( "Strict standard compliance" )
#define ENC_STRICT_LONGTEXT N_( "Force a strict standard " \
  "compliance when encoding (accepted values: -2 to 2)." )

#define ENC_LUMI_MASKING_TEXT N_( "Luminance masking" )
#define ENC_LUMI_MASKING_LONGTEXT N_( "Raise the quantizer for " \
  "very bright macroblocks (default: 0.0)." )

#define ENC_DARK_MASKING_TEXT N_( "Darkness masking" )
#define ENC_DARK_MASKING_LONGTEXT N_( "Raise the quantizer for " \
  "very dark macroblocks (default: 0.0)." )

#define ENC_P_MASKING_TEXT N_( "Motion masking" )
#define ENC_P_MASKING_LONGTEXT N_( "Raise the quantizer for " \
  "macroblocks with a high temporal complexity (default: 0.0)." )

#define ENC_BORDER_MASKING_TEXT N_( "Border masking" )
#define ENC_BORDER_MASKING_LONGTEXT N_( "Raise the quantizer " \
  "for macroblocks at the border of the frame (default: 0.0)." )

#define ENC_LUMA_ELIM_TEXT N_( "Luminance elimination" )
#define ENC_LUMA_ELIM_LONGTEXT N_( "Eliminates luminance blocks when " \
  "the PSNR isn't much changed (default: 0.0). The H264 specification " \
  "recommends -4." )

#define ENC_CHROMA_ELIM_TEXT N_( "Chrominance elimination" )
#define ENC_CHROMA_ELIM_LONGTEXT N_( "Eliminates chrominance blocks when " \
  "the PSNR isn't much changed (default: 0.0). The H264 specification " \
  "recommends 7." )

#define ENC_PROFILE_TEXT N_( "Specify AAC audio profile to use" )
#define ENC_PROFILE_LONGTEXT N_( "Specify the AAC audio profile to use " \
   "for encoding the audio bitstream. It takes the following options: " \
   "main, low, ssr (not supported),ltp, hev1, hev2 (default: low). " \
   "hev1 and hev2 are currently supported only with libfdk-aac enabled libavcodec" )

#ifndef AV_VERSION_INT
#   define AV_VERSION_INT(a, b, c) ((a)<<16 | (b)<<8 | (c))
#endif

