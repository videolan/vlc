/*****************************************************************************
 * ffmpeg.h: decoder using the ffmpeg library
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#include "codecs.h"                                      /* BITMAPINFOHEADER */

#if LIBAVCODEC_BUILD >= 4663
#   define LIBAVCODEC_PP
#else
#   undef  LIBAVCODEC_PP
#endif

struct picture_t;
struct AVFrame;
struct AVCodecContext;
struct AVCodec;

void E_(InitLibavcodec)( vlc_object_t * );
int E_(GetFfmpegCodec) ( vlc_fourcc_t, int *, int *, char ** );
int E_(GetVlcFourcc)   ( int, int *, vlc_fourcc_t *, char ** );
int E_(GetFfmpegChroma)( vlc_fourcc_t );

/* Video decoder module */
int  E_( InitVideoDec )( decoder_t *, AVCodecContext *, AVCodec *,
                         int, char * );
void E_( EndVideoDec ) ( decoder_t * );
picture_t *E_( DecodeVideo ) ( decoder_t *, block_t ** );

/* Audio decoder module */
int  E_( InitAudioDec )( decoder_t *, AVCodecContext *, AVCodec *,
                         int, char * );
void E_( EndAudioDec ) ( decoder_t * );
aout_buffer_t *E_( DecodeAudio ) ( decoder_t *, block_t ** );

/* Chroma conversion module */
int  E_(OpenChroma)( vlc_object_t * );
void E_(CloseChroma)( vlc_object_t * );

/* Video encoder module */
int  E_(OpenEncoder) ( vlc_object_t * );
void E_(CloseEncoder)( vlc_object_t * );

/* Audio encoder module */
int  E_(OpenAudioEncoder) ( vlc_object_t * );
void E_(CloseAudioEncoder)( vlc_object_t * );

/* Demux module */
int  E_(OpenDemux) ( vlc_object_t * );
void E_(CloseDemux)( vlc_object_t * );

/* Postprocessing module */
void *E_(OpenPostproc)( decoder_t *, vlc_bool_t * );
int E_(InitPostproc)( decoder_t *, void *, int, int, int );
int E_(PostprocPict)( decoder_t *, void *, picture_t *, AVFrame * );
void E_(ClosePostproc)( decoder_t *, void * );

/*****************************************************************************
 * Module descriptor help strings
 *****************************************************************************/
#define DR_TEXT N_("Direct rendering")

#define ERROR_TEXT N_("Error resilience")
#define ERROR_LONGTEXT N_( \
    "ffmpeg can make error resiliences.          \n" \
    "Nevertheless, with a buggy encoder (like ISO MPEG-4 encoder from M$) " \
    "this will produce a lot of errors.\n" \
    "Valid range is -1 to 99 (-1 disables all errors resiliences).")

#define BUGS_TEXT N_("Workaround bugs")
#define BUGS_LONGTEXT N_( \
    "Try to fix some bugs\n" \
    "1  autodetect\n" \
    "2  old msmpeg4\n" \
    "4  xvid interlaced\n" \
    "8  ump4 \n" \
    "16 no padding\n" \
    "32 ac vlc\n" \
    "64 Qpel chroma")

#define HURRYUP_TEXT N_("Hurry up")
#define HURRYUP_LONGTEXT N_( \
    "Allow the decoder to partially decode or skip frame(s) " \
    "when there is not enough time. It's useful with low CPU power " \
    "but it can produce distorted pictures.")

#define PP_Q_TEXT N_("Post processing quality")
#define PP_Q_LONGTEXT N_( \
    "Quality of post processing. Valid range is 0 to 6\n" \
    "Higher levels require considerable more CPU power, but produce " \
    "better looking pictures." )

#define DEBUG_TEST N_( "Debug mask" )
#define DEBUG_LONGTEST N_( "Set ffmpeg debug mask" )

#define LIBAVCODEC_PP_TEXT N_("ffmpeg postproc filter chains")
/* FIXME (cut/past from ffmpeg */
#define LIBAVCODEC_PP_LONGTEXT \
"<filterName>[:<option>[:<option>...]][[,|/][-]<filterName>[:<option>...]]...\n" \
"long form example:\n" \
"vdeblock:autoq/hdeblock:autoq/linblenddeint    default,-vdeblock\n" \
"short form example:\n" \
"vb:a/hb:a/lb de,-vb\n" \
"more examples:\n" \
"tn:64:128:256\n" \
"Filters                        Options\n" \
"short  long name       short   long option     Description\n" \
"*      *               a       autoq           cpu power dependant enabler\n" \
"                       c       chrom           chrominance filtring enabled\n" \
"                       y       nochrom         chrominance filtring disabled\n" \
"hb     hdeblock        (2 Threshold)           horizontal deblocking filter\n" \
"       1. difference factor: default=64, higher -> more deblocking\n" \
"       2. flatness threshold: default=40, lower -> more deblocking\n" \
"                       the h & v deblocking filters share these\n" \
"                       so u cant set different thresholds for h / v\n" \
"vb     vdeblock        (2 Threshold)           vertical deblocking filter\n" \
"h1     x1hdeblock                              Experimental h deblock filter 1\n" \
"v1     x1vdeblock                              Experimental v deblock filter 1\n" \
"dr     dering                                  Deringing filter\n" \
"al     autolevels                              automatic brightness / contrast\n" \
"                       f       fullyrange      stretch luminance to (0..255)\n" \
"lb     linblenddeint                           linear blend deinterlacer\n" \
"li     linipoldeint                            linear interpolating deinterlace\n" \
"ci     cubicipoldeint                          cubic interpolating deinterlacer\n" \
"md     mediandeint                             median deinterlacer\n" \
"fd     ffmpegdeint                             ffmpeg deinterlacer\n" \
"de     default                                 hb:a,vb:a,dr:a,al\n" \
"fa     fast                                    h1:a,v1:a,dr:a,al\n" \
"tn     tmpnoise        (3 Thresholds)          Temporal Noise Reducer\n" \
"                       1. <= 2. <= 3.          larger -> stronger filtering\n" \
"fq     forceQuant      <quantizer>             Force quantizer\n"
