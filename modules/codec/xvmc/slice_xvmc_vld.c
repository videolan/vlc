/* $Id$
 * Copyright (c) 2004 The Unichrome project. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTIES OR REPRESENTATIONS; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_vout.h>
#include <vlc_codec.h>

#include "mpeg2.h"
#include "mpeg2_internal.h"
#include "xvmc_vld.h"

static uint8_t zig_zag_scan[64] ATTR_ALIGN(16) =
{
    /* Zig-Zag scan pattern */
     0, 1, 8,16, 9, 2, 3,10,
    17,24,32,25,18,11, 4, 5,
    12,19,26,33,40,48,41,34,
    27,20,13, 6, 7,14,21,28,
    35,42,49,56,57,50,43,36,
    29,22,15,23,30,37,44,51,
    58,59,52,45,38,31,39,46,
    53,60,61,54,47,55,62,63
};

static uint8_t alternate_scan [64] ATTR_ALIGN(16) =
{
    /* Alternate scan pattern */
    0,8,16,24,1,9,2,10,17,25,32,40,48,56,57,49,
    41,33,26,18,3,11,4,12,19,27,34,42,50,58,35,43,
    51,59,20,28,5,13,6,14,21,29,36,44,52,60,37,45,
    53,61,22,30,7,15,23,31,38,46,54,62,39,47,55,63
};

void mpeg2_xxmc_choose_coding(decoder_t *p_dec,
        mpeg2_decoder_t * const decoder, picture_t *picture,
        double aspect_ratio, int flags)
{
    if (picture)
    {
        //vlc_fourcc_t decoder_format = picture->format.i_chroma;
        //if (decoder_format == VLC_FOURCC('X','x','M','C')) {
        vlc_xxmc_t *xxmc = (vlc_xxmc_t *) picture->p_data;

        /*
         * Make a request for acceleration type and mpeg coding from
         * the output plugin.
         */

        xxmc->fallback_format = VLC_FOURCC('Y','V','1','2');
        xxmc->acceleration = VLC_XVMC_ACCEL_VLD;//| VLC_XVMC_ACCEL_IDCT| VLC_XVMC_ACCEL_MOCOMP ;

        //msg_Dbg(p_dec, "mpeg2_xxmc_choose_coding 2");
        /*
         * Standard MOCOMP / IDCT XvMC implementation for interlaced streams
         * is buggy. The bug is inherited from the old XvMC driver. Don't use it until
         * it has been fixed. (A volunteer ?)
         */

        //if ( decoder->picture_structure != 3 ) {
        //xxmc->acceleration &= ~( VLC_XVMC_ACCEL_IDCT | VLC_XVMC_ACCEL_MOCOMP );
        //}

        xxmc->mpeg = (decoder->mpeg1) ? VLC_XVMC_MPEG_1:VLC_XVMC_MPEG_2;
        xxmc->proc_xxmc_update_frame( picture,
                                      decoder->width,
                                      decoder->height,
                                      aspect_ratio,
                                      VLC_IMGFMT_XXMC, flags );
        //}
  }
}

void mpeg2_xxmc_slice( mpeg2dec_t *mpeg2dec, picture_t *picture,
                        int code, uint8_t *buffer, int size)
{
    mpeg2_decoder_t * const decoder = &(mpeg2dec->decoder);
    picture = (picture_t *)mpeg2dec->fbuf[0]->id;
    vlc_xxmc_t *xxmc = (vlc_xxmc_t *) picture->p_data;
    vlc_vld_frame_t *vft = &xxmc->vld_frame;
    unsigned mb_frame_height;
    int i;
    const uint8_t *scan_pattern;

    if (1 == code)
    {
        //mpeg2_skip(mpeg2dec, 1);
        //frame->bad_frame = 1;

        /*
         * Check that first field went through OK. Otherwise,
         * indicate bad frame.
         */ 
        if (decoder->second_field)
        {
            mpeg2dec->xvmc_last_slice_code = (xxmc->decoded) ? 0 : -1;
            xxmc->decoded = 0;
        }
        else
        {
            mpeg2dec->xvmc_last_slice_code = 0;
        }

        mb_frame_height =
                //(!(decoder->mpeg1) && (decoder->progressive_sequence)) ?
                //2*((decoder->height+31) >> 5) :
                (decoder->height+15) >> 4;
        mpeg2dec->xxmc_mb_pic_height = (decoder->picture_structure == FRAME_PICTURE ) ?
                                        mb_frame_height : mb_frame_height >> 1;

        if (decoder->mpeg1)
        {
            vft->mv_ranges[0][0] = decoder->b_motion.f_code[0];
            vft->mv_ranges[0][1] = decoder->b_motion.f_code[0];
            vft->mv_ranges[1][0] = decoder->f_motion.f_code[0];
            vft->mv_ranges[1][1] = decoder->f_motion.f_code[0];
        }
        else
        {
            vft->mv_ranges[0][0] = decoder->b_motion.f_code[0];
            vft->mv_ranges[0][1] = decoder->b_motion.f_code[1];
            vft->mv_ranges[1][0] = decoder->f_motion.f_code[0];
            vft->mv_ranges[1][1] = decoder->f_motion.f_code[1];
        }

        vft->picture_structure = decoder->picture_structure;
        vft->picture_coding_type = decoder->coding_type;
        vft->mpeg_coding = (decoder->mpeg1) ? 0 : 1;
        vft->progressive_sequence = decoder->progressive_sequence;
        vft->scan = (decoder->scan == mpeg2_scan_alt);
        vft->pred_dct_frame = decoder->frame_pred_frame_dct;
        vft->concealment_motion_vectors =
        decoder->concealment_motion_vectors;
        vft->q_scale_type = decoder->q_scale_type;
        vft->intra_vlc_format = decoder->intra_vlc_format;
        vft->intra_dc_precision = 7 - decoder->intra_dc_precision;
        vft->second_field = decoder->second_field;

        /*
         * Translation of libmpeg2's Q-matrix layout to VLD XvMC's.
         * Errors here will give
         * blocky artifacts and sometimes wrong colors.
         */

        scan_pattern = (vft->scan) ? alternate_scan : zig_zag_scan;
        if( (vft->load_intra_quantizer_matrix = decoder->load_intra_quantizer_matrix) )
        {
            for (i=0; i<64; ++i)
            {
                vft->intra_quantizer_matrix[scan_pattern[i]] =
                mpeg2dec->quantizer_matrix[0][decoder->scan[i]];
            }
        }

        if( (vft->load_non_intra_quantizer_matrix = decoder->load_non_intra_quantizer_matrix) )
        {
            for (i=0; i<64; ++i)
            {
                vft->non_intra_quantizer_matrix[scan_pattern[i]] =
                                        mpeg2dec->quantizer_matrix[1][decoder->scan[i]];
            }
        }
        decoder->load_intra_quantizer_matrix = 0;
        decoder->load_non_intra_quantizer_matrix = 0;

        vft->forward_reference_picture = (picture_t *)mpeg2dec->ptr_forward_ref_picture;
        vft->backward_reference_picture = (picture_t *)mpeg2dec->ptr_backward_ref_picture;

#if 0
    printf("\nSLICE DATA !!!! size=%d", size-4);
    int i=0;
    if ( vft->forward_reference_picture != NULL && ((vlc_xxmc_t *)
         vft->forward_reference_picture->p_data)->slice_data_size > 10)
    {
        printf("\nFORWARD SLICE DATA !!!! size=%d\n", ((vlc_xxmc_t *)
               vft->forward_reference_picture->p_data)->slice_data_size);
        for (i=0;i<10;i++)
        {
            printf("%d ", *(((vlc_xxmc_t *) vft->forward_reference_picture->p_data)->slice_data+i));
        }
        printf("\nFORWARD SLICE DATA END!!!!\n");
    }
    if ( vft->backward_reference_picture != NULL && ((vlc_xxmc_t *)
         vft->backward_reference_picture->p_data)->slice_data_size > 10)
    {
        printf("\nBACKWARD SLICE DATA !!!! size=%d\n", ((vlc_xxmc_t *)
               vft->backward_reference_picture->p_data)->slice_data_size);
        for (i=0;i<10;i++)
        {
            printf("%d ", *(((vlc_xxmc_t *) vft->backward_reference_picture->p_data)->slice_data+i));
        }
        printf("\nBACKWARD SLICE DATA END!!!!\n");
    }
#endif

        xxmc->proc_xxmc_begin( picture );
        if (xxmc->result != 0)
        {
            /* "mpeg2_xxmc_slice begin failed" */
            /* xmc->proc_xxmc_flushsync( picture ); */
            xxmc->proc_xxmc_flush( picture );
            mpeg2dec->xvmc_last_slice_code=-1;
        }
    }

    if( ((code == mpeg2dec->xvmc_last_slice_code + 1 ||
        code == mpeg2dec->xvmc_last_slice_code)) &&
        (unsigned int)code <= mpeg2dec->xxmc_mb_pic_height )
    {
        /*
         * Send this slice to the output plugin. May stall for a long
         * time in proc_slice;
         */
        //mpeg2_skip(mpeg2dec, 1);

        //frame->bad_frame = 1;
        //size = mpeg2dec->chunk_ptr-mpeg2dec->chunk_start;
        xxmc->slice_data_size = size;//mpeg2dec->buf_end - mpeg2dec->buf_start;
        xxmc->slice_data = mpeg2dec->chunk_start;//buffer;
        xxmc->slice_code = code;
        xxmc->proc_xxmc_slice( picture );

        if (xxmc->result != 0)
        {
            //xxmc->proc_xxmc_flushsync( picture );
            xxmc->proc_xxmc_flush( picture );	
            mpeg2dec->xvmc_last_slice_code=-1;
            return;
        }
        if ( (unsigned int)code == mpeg2dec->xxmc_mb_pic_height)
        {
            /*
             * We've encountered the last slice of this frame.
             * Release the decoder for a new frame and, if all
             * went well, tell libmpeg2 that we are ready.
             */

            mpeg2_xxmc_vld_frame_complete(mpeg2dec,picture,code);
            return;
        }
        else if (code == mpeg2dec->xvmc_last_slice_code + 1)
        {
            //xxmc->proc_xxmc_flush( picture );

            /*
             * Keep track of slices.
             */
            mpeg2dec->xvmc_last_slice_code++;
        }
    }
    else
    {
        /*
         * An error has occurred.
         */

        //printf("VLD XvMC: Slice error: code=%d\tlast slice code=%d\tmb_pic_height=%d\n", code, mpeg2dec->xvmc_last_slice_code,mpeg2dec->xxmc_mb_pic_height);
        mpeg2dec->xvmc_last_slice_code = -1;
        xxmc->proc_xxmc_flush( picture );
        return;
    }
}

void mpeg2_xxmc_vld_frame_complete(mpeg2dec_t *mpeg2dec, picture_t *picture, int code) 
{
    vlc_xxmc_t *xxmc = (vlc_xxmc_t *) picture->p_data;
    vlc_vld_frame_t *vft = &xxmc->vld_frame;

    if (xxmc->decoded)
        return;

    if (mpeg2dec->xvmc_last_slice_code >= 1)
    {
        xxmc->proc_xxmc_flush( picture );
        if (xxmc->result)
        {
            mpeg2dec->xvmc_last_slice_code=-1;
            return;
        }
        xxmc->decoded = 1;
        mpeg2dec->xvmc_last_slice_code++;
        if (vft->picture_structure == 3 || vft->second_field)
        {
            if (xxmc->result == 0)
                mpeg2_skip(mpeg2dec, 0);
            //frame->bad_frame = 0;
        }
    }
}

