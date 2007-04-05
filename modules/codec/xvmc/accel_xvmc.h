/*****************************************************************************
 * xvmc.c : Common acceleration definitions for XvMC
 *****************************************************************************
 * Copyright (C) 2006 VideoLAN
 * $Id$
 *
 * Authors: Christophe Burgalat <c _dot_ burgalat _at_ broadcastavenue _dot_ com>
 *          Jean-Paul Saman <jpsaman _at_ videolan _dot_ org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

/*
 * Common acceleration definitions for XvMC
 *
 *
 */

#ifndef HAVE_VLC_ACCEL_H
#define HAVE_VLC_ACCEL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vlc_macroblock_s
{
    short  *blockptr;          /* pointer to current dct block */
    short  *blockbaseptr;      /* pointer to base of dct block array in blocks */
    short   xvmc_accel;        /* type of acceleration supported */
} vlc_macroblocks_t;

typedef struct vlc_vld_frame_s
{
    int version;              /* Backward compatibility */
    int mv_ranges[2][2];
    int picture_structure;
    int picture_coding_type;
    int intra_dc_precision;
    int mpeg_coding;
    int progressive_sequence;
    int scan;
    int pred_dct_frame;
    int concealment_motion_vectors;
    int q_scale_type;
    int intra_vlc_format;
    int second_field;
    int load_intra_quantizer_matrix;
    int load_non_intra_quantizer_matrix;
    uint8_t intra_quantizer_matrix[64];
    uint8_t non_intra_quantizer_matrix[64];
    picture_t *backward_reference_picture;
    picture_t *forward_reference_picture;
} vlc_vld_frame_t;


typedef struct vlc_xvmc_s
{
    vlc_macroblocks_t *macroblocks;
    void (*proc_macro_block)(int x,int y,int mb_type,
    int motion_type,int (*mv_field_sel)[2],
    int *dmvector,int cbp,int dct_type,
    picture_t *current_picture,picture_t *forward_ref_picture,
    picture_t *backward_ref_picture,int picture_structure,
    int second_field,int (*f_mot_pmv)[2],int (*b_mot_pmv)[2]);
} vlc_xvmc_t ;

typedef struct vlc_xxmc_s
{
    /*
    * We inherit the xine_xvmc_t properties.
    */
    vlc_xvmc_t xvmc;

    unsigned mpeg;
    unsigned acceleration;
    vlc_fourcc_t fallback_format;
    vlc_vld_frame_t vld_frame;
    uint8_t *slice_data;
    unsigned slice_data_size;
    unsigned slice_code;
    int result;
    int decoded;
    float sleep;
    void (*proc_xxmc_update_frame) (picture_t *picture_gen,
                uint32_t width, uint32_t height, double ratio,
                int format, int flags);
    void (*proc_xxmc_begin) (picture_t *vo_img);
    void (*proc_xxmc_slice) (picture_t *vo_img);
    void (*proc_xxmc_flush) (picture_t *vo_img);
    void (*proc_xxmc_flushsync) (picture_t *vo_img);
} vlc_xxmc_t;

#define VLC_IMGFMT_XXMC VLC_FOURCC('X','x','M','C')

  /*
   * Register XvMC stream types here.
   */
#define VLC_XVMC_MPEG_1 0x00000001
#define VLC_XVMC_MPEG_2 0x00000002
#define VLC_XVMC_MPEG_4 0x00000004

  /*
   * Register XvMC acceleration levels here.
   */
#define VLC_XVMC_ACCEL_MOCOMP 0x00000001
#define VLC_XVMC_ACCEL_IDCT   0x00000002
#define VLC_XVMC_ACCEL_VLD    0x00000004

/* xvmc acceleration types */
#define VLC_VO_MOTION_ACCEL   1
#define VLC_VO_IDCT_ACCEL     2
#define VLC_VO_SIGNED_INTRA   4

/* motion types */
#define VLC_MC_FIELD 1
#define VLC_MC_FRAME 2
#define VLC_MC_16X8  2
#define VLC_MC_DMV   3

/* picture coding type */
#define VLC_PICT_I_TYPE 1
#define VLC_PICT_P_TYPE 2
#define VLC_PICT_B_TYPE 3
#define VLC_PICT_D_TYPE 4

/* macroblock modes */
#define VLC_MACROBLOCK_INTRA 1
#define VLC_MACROBLOCK_PATTERN 2
#define VLC_MACROBLOCK_MOTION_BACKWARD 4
#define VLC_MACROBLOCK_MOTION_FORWARD 8
#define VLC_MACROBLOCK_QUANT 16
#define VLC_MACROBLOCK_DCT_TYPE_INTERLACED 32

#ifdef __cplusplus
}
#endif

#endif
