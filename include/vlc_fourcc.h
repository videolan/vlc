/*****************************************************************************
 * vlc_fourcc.h: Definition of various FOURCC and helpers
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ com>
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

#ifndef VLC_FOURCC_H
#define VLC_FOURCC_H 1

#define VLC_CODEC_UNKNOWN         VLC_FOURCC('u','n','d','f')

/* Video codec */
#define VLC_CODEC_MPGV            VLC_FOURCC('m','p','g','v')
#define VLC_CODEC_MP4V            VLC_FOURCC('m','p','4','v')
#define VLC_CODEC_DIV1            VLC_FOURCC('D','I','V','1')
#define VLC_CODEC_DIV2            VLC_FOURCC('D','I','V','2')
#define VLC_CODEC_DIV3            VLC_FOURCC('D','I','V','3')
#define VLC_CODEC_SVQ1            VLC_FOURCC('S','V','Q','1')
#define VLC_CODEC_SVQ3            VLC_FOURCC('S','V','Q','3')
#define VLC_CODEC_H264            VLC_FOURCC('h','2','6','4')
#define VLC_CODEC_H263            VLC_FOURCC('h','2','6','3')
#define VLC_CODEC_H263I           VLC_FOURCC('I','2','6','3')
#define VLC_CODEC_H263P           VLC_FOURCC('I','L','V','R')
#define VLC_CODEC_FLV1            VLC_FOURCC('F','L','V','1')
#define VLC_CODEC_H261            VLC_FOURCC('h','2','6','1')
#define VLC_CODEC_MJPG            VLC_FOURCC('M','J','P','G')
#define VLC_CODEC_MJPGB           VLC_FOURCC('m','j','p','b')
#define VLC_CODEC_LJPG            VLC_FOURCC('L','J','P','G')
#define VLC_CODEC_WMV1            VLC_FOURCC('W','M','V','1')
#define VLC_CODEC_WMV2            VLC_FOURCC('W','M','V','2')
#define VLC_CODEC_WMV3            VLC_FOURCC('W','M','V','3')
#define VLC_CODEC_WMVA            VLC_FOURCC('W','M','V','A')
#define VLC_CODEC_WMVP            VLC_FOURCC('W','M','V','P')
#define VLC_CODEC_WMVP2           VLC_FOURCC('W','V','P','2')
#define VLC_CODEC_VC1             VLC_FOURCC('V','C','-','1')
#define VLC_CODEC_DAALA           VLC_FOURCC('d','a','a','l')
#define VLC_CODEC_THEORA          VLC_FOURCC('t','h','e','o')
#define VLC_CODEC_TARKIN          VLC_FOURCC('t','a','r','k')
#define VLC_CODEC_DIRAC           VLC_FOURCC('d','r','a','c')
#define VLC_CODEC_OGGSPOTS        VLC_FOURCC('S','P','O','T')
#define VLC_CODEC_CAVS            VLC_FOURCC('C','A','V','S')
#define VLC_CODEC_NUV             VLC_FOURCC('N','J','P','G')
#define VLC_CODEC_RV10            VLC_FOURCC('R','V','1','0')
#define VLC_CODEC_RV13            VLC_FOURCC('R','V','1','3')
#define VLC_CODEC_RV20            VLC_FOURCC('R','V','2','0')
#define VLC_CODEC_RV30            VLC_FOURCC('R','V','3','0')
#define VLC_CODEC_RV40            VLC_FOURCC('R','V','4','0')
#define VLC_CODEC_VP3             VLC_FOURCC('V','P','3',' ')
#define VLC_CODEC_VP5             VLC_FOURCC('V','P','5',' ')
#define VLC_CODEC_VP6             VLC_FOURCC('V','P','6','2')
#define VLC_CODEC_VP6F            VLC_FOURCC('V','P','6','F')
#define VLC_CODEC_VP6A            VLC_FOURCC('V','P','6','A')
#define VLC_CODEC_MSVIDEO1        VLC_FOURCC('M','S','V','C')
#define VLC_CODEC_FLIC            VLC_FOURCC('F','L','I','C')
#define VLC_CODEC_SP5X            VLC_FOURCC('S','P','5','X')
#define VLC_CODEC_DV              VLC_FOURCC('d','v',' ',' ')
#define VLC_CODEC_MSRLE           VLC_FOURCC('m','r','l','e')
#define VLC_CODEC_HUFFYUV         VLC_FOURCC('H','F','Y','U')
#define VLC_CODEC_FFVHUFF         VLC_FOURCC('F','F','V','H')
#define VLC_CODEC_ASV1            VLC_FOURCC('A','S','V','1')
#define VLC_CODEC_ASV2            VLC_FOURCC('A','S','V','2')
#define VLC_CODEC_FFV1            VLC_FOURCC('F','F','V','1')
#define VLC_CODEC_VCR1            VLC_FOURCC('V','C','R','1')
#define VLC_CODEC_CLJR            VLC_FOURCC('C','L','J','R')
#define VLC_CODEC_RPZA            VLC_FOURCC('r','p','z','a')
#define VLC_CODEC_SMC             VLC_FOURCC('s','m','c',' ')
#define VLC_CODEC_CINEPAK         VLC_FOURCC('C','V','I','D')
#define VLC_CODEC_TSCC            VLC_FOURCC('T','S','C','C')
#define VLC_CODEC_CSCD            VLC_FOURCC('C','S','C','D')
#define VLC_CODEC_ZMBV            VLC_FOURCC('Z','M','B','V')
#define VLC_CODEC_VMNC            VLC_FOURCC('V','M','n','c')
#define VLC_CODEC_FMVC            VLC_FOURCC('F','M','V','C')
#define VLC_CODEC_FRAPS           VLC_FOURCC('F','P','S','1')
#define VLC_CODEC_TRUEMOTION1     VLC_FOURCC('D','U','C','K')
#define VLC_CODEC_TRUEMOTION2     VLC_FOURCC('T','M','2','0')
#define VLC_CODEC_QTRLE           VLC_FOURCC('r','l','e',' ')
#define VLC_CODEC_QDRAW           VLC_FOURCC('q','d','r','w')
#define VLC_CODEC_QPEG            VLC_FOURCC('Q','P','E','G')
#define VLC_CODEC_ULTI            VLC_FOURCC('U','L','T','I')
#define VLC_CODEC_VIXL            VLC_FOURCC('V','I','X','L')
#define VLC_CODEC_LOCO            VLC_FOURCC('L','O','C','O')
#define VLC_CODEC_WNV1            VLC_FOURCC('W','N','V','1')
#define VLC_CODEC_AASC            VLC_FOURCC('A','A','S','C')
#define VLC_CODEC_INDEO2          VLC_FOURCC('I','V','2','0')
#define VLC_CODEC_INDEO3          VLC_FOURCC('I','V','3','1')
#define VLC_CODEC_INDEO4          VLC_FOURCC('I','V','4','1')
#define VLC_CODEC_INDEO5          VLC_FOURCC('I','V','5','0')
#define VLC_CODEC_FLASHSV         VLC_FOURCC('F','S','V','1')
#define VLC_CODEC_KMVC            VLC_FOURCC('K','M','V','C')
#define VLC_CODEC_SMACKVIDEO      VLC_FOURCC('S','M','K','2')
#define VLC_CODEC_DNXHD           VLC_FOURCC('A','V','d','n')
#define VLC_CODEC_8BPS            VLC_FOURCC('8','B','P','S')
#define VLC_CODEC_MIMIC           VLC_FOURCC('M','L','2','O')
#define VLC_CODEC_INTERPLAY       VLC_FOURCC('i','m','v','e')
#define VLC_CODEC_IDCIN           VLC_FOURCC('I','D','C','I')
#define VLC_CODEC_4XM             VLC_FOURCC('4','X','M','V')
#define VLC_CODEC_ROQ             VLC_FOURCC('R','o','Q','v')
#define VLC_CODEC_MDEC            VLC_FOURCC('M','D','E','C')
#define VLC_CODEC_VMDVIDEO        VLC_FOURCC('V','M','D','V')
#define VLC_CODEC_CDG             VLC_FOURCC('C','D','G',' ')
#define VLC_CODEC_FRWU            VLC_FOURCC('F','R','W','U')
#define VLC_CODEC_AMV             VLC_FOURCC('A','M','V',' ')
#define VLC_CODEC_VP4             VLC_FOURCC('V','P','4','0')
#define VLC_CODEC_VP7             VLC_FOURCC('V','P','7','0')
#define VLC_CODEC_VP8             VLC_FOURCC('V','P','8','0')
#define VLC_CODEC_VP9             VLC_FOURCC('V','P','9','0')
#define VLC_CODEC_VP10            VLC_FOURCC('V','P',':','0')
#define VLC_CODEC_AV1             VLC_FOURCC('a','v','0','1')
#define VLC_CODEC_JPEG2000        VLC_FOURCC('J','P','2','K')
#define VLC_CODEC_LAGARITH        VLC_FOURCC('L','A','G','S')
#define VLC_CODEC_FLASHSV2        VLC_FOURCC('F','S','V','2')
#define VLC_CODEC_PRORES          VLC_FOURCC('a','p','c','n')
#define VLC_CODEC_MXPEG           VLC_FOURCC('M','X','P','G')
#define VLC_CODEC_CDXL            VLC_FOURCC('C','D','X','L')
#define VLC_CODEC_BMVVIDEO        VLC_FOURCC('B','M','V','V')
#define VLC_CODEC_UTVIDEO         VLC_FOURCC('U','L','R','A')
#define VLC_CODEC_VBLE            VLC_FOURCC('V','B','L','E')
#define VLC_CODEC_DXTORY          VLC_FOURCC('x','t','o','r')
#define VLC_CODEC_MSS1            VLC_FOURCC('M','S','S','1')
#define VLC_CODEC_MSS2            VLC_FOURCC('M','S','S','2')
#define VLC_CODEC_MSA1            VLC_FOURCC('M','S','A','1')
#define VLC_CODEC_TSC2            VLC_FOURCC('T','S','C','2')
#define VLC_CODEC_MTS2            VLC_FOURCC('M','T','S','2')
#define VLC_CODEC_HEVC            VLC_FOURCC('h','e','v','c')
#define VLC_CODEC_ICOD            VLC_FOURCC('i','c','o','d')
#define VLC_CODEC_G2M2            VLC_FOURCC('G','2','M','2')
#define VLC_CODEC_G2M3            VLC_FOURCC('G','2','M','3')
#define VLC_CODEC_G2M4            VLC_FOURCC('G','2','M','4')
#define VLC_CODEC_BINKVIDEO       VLC_FOURCC('B','I','K','f')
#define VLC_CODEC_BINKAUDIO_DCT   VLC_FOURCC('B','A','U','1')
#define VLC_CODEC_BINKAUDIO_RDFT  VLC_FOURCC('B','A','U','2')
#define VLC_CODEC_XAN_WC4         VLC_FOURCC('X','x','a','n')
#define VLC_CODEC_LCL_MSZH        VLC_FOURCC('M','S','Z','H')
#define VLC_CODEC_LCL_ZLIB        VLC_FOURCC('Z','L','I','B')
#define VLC_CODEC_THP             VLC_FOURCC('T','H','P','0')
#define VLC_CODEC_ESCAPE124       VLC_FOURCC('E','1','2','4')
#define VLC_CODEC_KGV1            VLC_FOURCC('K','G','V','1')
#define VLC_CODEC_CLLC            VLC_FOURCC('C','L','L','C')
#define VLC_CODEC_AURA            VLC_FOURCC('A','U','R','A')
#define VLC_CODEC_FIC             VLC_FOURCC('F','I','C','V')
#define VLC_CODEC_TMV             VLC_FOURCC('T','M','A','V')
#define VLC_CODEC_XAN_WC3         VLC_FOURCC('X','A','N','3')
#define VLC_CODEC_WS_VQA          VLC_FOURCC('W','V','Q','A')
#define VLC_CODEC_MMVIDEO         VLC_FOURCC('M','M','V','I')
#define VLC_CODEC_AVS             VLC_FOURCC('A','V','S','V')
#define VLC_CODEC_DSICINVIDEO     VLC_FOURCC('D','C','I','V')
#define VLC_CODEC_TIERTEXSEQVIDEO VLC_FOURCC('T','S','E','Q')
#define VLC_CODEC_DXA             VLC_FOURCC('D','E','X','A')
#define VLC_CODEC_C93             VLC_FOURCC('I','C','9','3')
#define VLC_CODEC_BETHSOFTVID     VLC_FOURCC('B','V','I','D')
#define VLC_CODEC_VB              VLC_FOURCC('V','B','V','1')
#define VLC_CODEC_RL2             VLC_FOURCC('R','L','V','2')
#define VLC_CODEC_BFI             VLC_FOURCC('B','F','&','I')
#define VLC_CODEC_CMV             VLC_FOURCC('E','C','M','V')
#define VLC_CODEC_MOTIONPIXELS    VLC_FOURCC('M','P','I','X')
#define VLC_CODEC_TGV             VLC_FOURCC('T','G','V','V')
#define VLC_CODEC_TGQ             VLC_FOURCC('T','G','Q','V')
#define VLC_CODEC_TQI             VLC_FOURCC('T','Q','I','V')
#define VLC_CODEC_MAD             VLC_FOURCC('M','A','D','V')
#define VLC_CODEC_ANM             VLC_FOURCC('A','N','I','M')
#define VLC_CODEC_YOP             VLC_FOURCC('Y','O','P','V')
#define VLC_CODEC_JV              VLC_FOURCC('J','V','0','0')
#define VLC_CODEC_DFA             VLC_FOURCC('D','F','I','A')
#define VLC_CODEC_HNM4_VIDEO      VLC_FOURCC('H','N','M','4')
#define VLC_CODEC_TDSC            VLC_FOURCC('T','D','S','C')
#define VLC_CODEC_HQX             VLC_FOURCC('C','H','Q','X')
#define VLC_CODEC_HQ_HQA          VLC_FOURCC('C','U','V','C')
#define VLC_CODEC_HAP             VLC_FOURCC('H','A','P','1')
#define VLC_CODEC_DXV             VLC_FOURCC('D','X','D','3')
#define VLC_CODEC_CINEFORM        VLC_FOURCC('C','F','H','D')
#define VLC_CODEC_SPEEDHQ         VLC_FOURCC('S','H','Q','2')
#define VLC_CODEC_PIXLET          VLC_FOURCC('p','x','l','t')
#define VLC_CODEC_MAGICYUV        VLC_FOURCC('M','8','Y','0')
#define VLC_CODEC_IMM4            VLC_FOURCC('I','M','M','4')
#define VLC_CODEC_IMM5            VLC_FOURCC('I','M','M','5')
#define VLC_CODEC_AGM             VLC_FOURCC('A','G','M','0')

/***********
 * Chromas
 ***********/

/* Planar YUV */

/* Planar YUV 4:1:0 Y:V:U */
#define VLC_CODEC_YV9             VLC_FOURCC('Y','V','U','9')
/* Planar YUV 4:1:0 Y:U:V */
#define VLC_CODEC_I410            VLC_FOURCC('I','4','1','0')
/* Planar YUV 4:1:1 Y:U:V */
#define VLC_CODEC_I411            VLC_FOURCC('I','4','1','1')

/* Planar YUV 4:2:0 Y:V:U */
#define VLC_CODEC_YV12            VLC_FOURCC('Y','V','1','2')
/* Planar YUV 4:2:0 Y:U:V 8-bit */
#define VLC_CODEC_I420            VLC_FOURCC('I','4','2','0')
/* Planar YUV 4:2:0 Y:U:V  9-bit stored on 16 bits */
#define VLC_CODEC_I420_9L         VLC_FOURCC('I','0','9','L')
#define VLC_CODEC_I420_9B         VLC_FOURCC('I','0','9','B')
/* Planar YUV 4:2:0 Y:U:V 10-bit stored on 16 bits LSB */
#define VLC_CODEC_I420_10L        VLC_FOURCC('I','0','A','L')
#define VLC_CODEC_I420_10B        VLC_FOURCC('I','0','A','B')
/* Planar YUV 4:2:0 Y:U:V 12-bit stored on 16 bits */
#define VLC_CODEC_I420_12L        VLC_FOURCC('I','0','C','L')
#define VLC_CODEC_I420_12B        VLC_FOURCC('I','0','C','B')

/* Planar YUV 4:2:0 Y:U:V 16-bit stored on 16 bits */
#define VLC_CODEC_I420_16L        VLC_FOURCC('I','0','F','L')
#define VLC_CODEC_I420_16B        VLC_FOURCC('I','0','F','B')

/* Planar YUV 4:2:2 Y:U:V 8-bit */
#define VLC_CODEC_I422            VLC_FOURCC('I','4','2','2')
/* Planar YUV 4:2:2 Y:U:V  9-bit stored on 16 bits */
#define VLC_CODEC_I422_9L         VLC_FOURCC('I','2','9','L')
#define VLC_CODEC_I422_9B         VLC_FOURCC('I','2','9','B')
/* Planar YUV 4:2:2 Y:U:V 10-bit stored on 16 bits */
#define VLC_CODEC_I422_10L        VLC_FOURCC('I','2','A','L')
#define VLC_CODEC_I422_10B        VLC_FOURCC('I','2','A','B')
/* Planar YUV 4:2:2 Y:U:V 12-bit stored on 16 bits */
#define VLC_CODEC_I422_12L        VLC_FOURCC('I','2','C','L')
#define VLC_CODEC_I422_12B        VLC_FOURCC('I','2','C','B')
/* Planar YUV 4:2:2 Y:U:V 16-bit stored on 16 bits */
#define VLC_CODEC_I422_16L        VLC_FOURCC('I','2','F','L')
#define VLC_CODEC_I422_16B        VLC_FOURCC('I','2','F','B')

/* Planar YUV 4:4:0 Y:U:V */
#define VLC_CODEC_I440            VLC_FOURCC('I','4','4','0')
/* Planar YUV 4:4:4 Y:U:V 8-bit */
#define VLC_CODEC_I444            VLC_FOURCC('I','4','4','4')
/* Planar YUV 4:4:4 Y:U:V  9-bit stored on 16 bits */
#define VLC_CODEC_I444_9L         VLC_FOURCC('I','4','9','L')
#define VLC_CODEC_I444_9B         VLC_FOURCC('I','4','9','B')
/* Planar YUV 4:4:4 Y:U:V 10-bit stored on 16 bits */
#define VLC_CODEC_I444_10L        VLC_FOURCC('I','4','A','L')
#define VLC_CODEC_I444_10B        VLC_FOURCC('I','4','A','B')
/* Planar YUV 4:4:4 Y:U:V 12-bit stored on 16 bits */
#define VLC_CODEC_I444_12L        VLC_FOURCC('I','4','C','L')
#define VLC_CODEC_I444_12B        VLC_FOURCC('I','4','C','B')
/* Planar YUV 4:4:4 Y:U:V 16-bit */
#define VLC_CODEC_I444_16L        VLC_FOURCC('I','4','F','L')
#define VLC_CODEC_I444_16B        VLC_FOURCC('I','4','F','B')

/* Planar YUV 4:2:0 Y:U:V full scale */
#define VLC_CODEC_J420            VLC_FOURCC('J','4','2','0')
/* Planar YUV 4:2:2 Y:U:V full scale */
#define VLC_CODEC_J422            VLC_FOURCC('J','4','2','2')
/* Planar YUV 4:4:0 Y:U:V full scale */
#define VLC_CODEC_J440            VLC_FOURCC('J','4','4','0')
/* Planar YUV 4:4:4 Y:U:V full scale */
#define VLC_CODEC_J444            VLC_FOURCC('J','4','4','4')
/* Palettized YUV with palette element Y:U:V:A */
#define VLC_CODEC_YUVP            VLC_FOURCC('Y','U','V','P')

/* Planar YUV 4:4:4 Y:U:V:A */
#define VLC_CODEC_YUVA            VLC_FOURCC('Y','U','V','A')
/* Planar YUV 4:2:2 Y:U:V:A */
#define VLC_CODEC_YUV422A         VLC_FOURCC('I','4','2','A')
/* Planar YUV 4:2:0 Y:U:V:A */
#define VLC_CODEC_YUV420A         VLC_FOURCC('I','4','0','A')

/* Planar Y:U:V:A 4:4:4 10bits */
#define VLC_CODEC_YUVA_444_10L    VLC_FOURCC('Y','A','0','L')
#define VLC_CODEC_YUVA_444_10B    VLC_FOURCC('Y','A','0','B')

/* Semi-planar Y/UV */

/* 2 planes Y/UV 4:2:0 */
#define VLC_CODEC_NV12            VLC_FOURCC('N','V','1','2')
/* 2 planes Y/VU 4:2:0 */
#define VLC_CODEC_NV21            VLC_FOURCC('N','V','2','1')
/* 2 planes Y/UV 4:2:2 */
#define VLC_CODEC_NV16            VLC_FOURCC('N','V','1','6')
/* 2 planes Y/VU 4:2:2 */
#define VLC_CODEC_NV61            VLC_FOURCC('N','V','6','1')
/* 2 planes Y/UV 4:4:4 */
#define VLC_CODEC_NV24            VLC_FOURCC('N','V','2','4')
/* 2 planes Y/VU 4:4:4 */
#define VLC_CODEC_NV42            VLC_FOURCC('N','V','4','2')
/* 2 planes Y/UV 4:2:0 10-bit */
#define VLC_CODEC_P010            VLC_FOURCC('P','0','1','0')
/* 2 planes Y/UV 4:2:0 16-bit */
#define VLC_CODEC_P016            VLC_FOURCC('P','0','1','6')

/* Packed YUV */

/* Packed YUV 4:2:0, U:V:Y */
#define VLC_CODEC_YUV4            VLC_FOURCC('y','u','v','4')
/* Packed YUV 4:2:2, U:Y:V:Y */
#define VLC_CODEC_UYVY            VLC_FOURCC('U','Y','V','Y')
/* Packed YUV 4:2:2, V:Y:U:Y */
#define VLC_CODEC_VYUY            VLC_FOURCC('V','Y','U','Y')
/* Packed YUV 4:2:2, Y:U:Y:V */
#define VLC_CODEC_YUYV            VLC_FOURCC('Y','U','Y','2')
/* Packed YUV 4:2:2, Y:U:Y:V, signed */
#define VLC_CODEC_YUV2            VLC_FOURCC('y','u','v','2')
/* Packed YUV 4:2:2, Y:V:Y:U */
#define VLC_CODEC_YVYU            VLC_FOURCC('Y','V','Y','U')
/* Packed YUV 2:1:1, Y:U:Y:V */
#define VLC_CODEC_Y211            VLC_FOURCC('Y','2','1','1')
/* Packed YUV 4:2:2, U:Y:V:Y, reverted */
#define VLC_CODEC_CYUV            VLC_FOURCC('c','y','u','v')
/* Packed YUV 4:2:2 10-bit U10:Y10:V10:Y10:X2 (12 on 4*32bits) */
#define VLC_CODEC_V210            VLC_FOURCC('v','2','1','0')
/* Packed YUV 4:4:4 */
#define VLC_CODEC_V308            VLC_FOURCC('v','3','0','8')
/* Packed YUVA 4:4:4:4 */
#define VLC_CODEC_V408            VLC_FOURCC('v','4','0','8')
/* Packed YUV 4:4:4 10-bit X2:U10:Y10:V10:Y10 */
#define VLC_CODEC_V410            VLC_FOURCC('v','4','1','0')
/* I420 packed for RTP (RFC 4175) */
#define VLC_CODEC_R420            VLC_FOURCC('r','4','2','0')
/* Packed YUV 4:2:2 10-bit V10:U10:Y10:A2 */
#define VLC_CODEC_Y210            VLC_FOURCC('Y','2','1','0')
/* Packed YUV 4:4:4 10-bit V10:U10:Y10:A2 */
#define VLC_CODEC_Y410            VLC_FOURCC('Y','4','1','0')
/* Packed YUV 4:4:4 V:U:Y:A */
#define VLC_CODEC_VUYA            VLC_FOURCC('V','U','Y','A')

/* RGB */

/* Palettized RGB with palette element R:G:B */
#define VLC_CODEC_RGBP            VLC_FOURCC('R','G','B','P')
/* 8 bits RGB */
#define VLC_CODEC_RGB8            VLC_FOURCC('R','G','B','8')
/* 12 bits RGB padded to 16 bits */
#define VLC_CODEC_RGB12           VLC_FOURCC('R','V','1','2')
/* 15 bits RGB padded to 16 bits */
#define VLC_CODEC_RGB15           VLC_FOURCC('R','V','1','5')
/* 16 bits RGB */
#define VLC_CODEC_RGB16           VLC_FOURCC('R','V','1','6')
/* 24 bits RGB */
#define VLC_CODEC_RGB24           VLC_FOURCC('R','V','2','4')
/* 24 bits RGB padded to 32 bits */
#define VLC_CODEC_RGB32           VLC_FOURCC('R','V','3','2')
/* 32 bits RGBA */
#define VLC_CODEC_RGBA            VLC_FOURCC('R','G','B','A')
/* 32 bits ARGB */
#define VLC_CODEC_ARGB            VLC_FOURCC('A','R','G','B')
/* 32 bits BGRA */
#define VLC_CODEC_BGRA            VLC_FOURCC('B','G','R','A')
/* 32 bits BGRA 10:10:10:2 */
#define VLC_CODEC_RGBA10          VLC_FOURCC('R','G','A','0')
/* 64 bits RGBA */
#define VLC_CODEC_RGBA64          VLC_FOURCC('R','G','A','4')

/* Planar GBR 4:4:4 8 bits */
#define VLC_CODEC_GBR_PLANAR      VLC_FOURCC('G','B','R','8')
#define VLC_CODEC_GBR_PLANAR_9B   VLC_FOURCC('G','B','9','B')
#define VLC_CODEC_GBR_PLANAR_9L   VLC_FOURCC('G','B','9','L')
#define VLC_CODEC_GBR_PLANAR_10B  VLC_FOURCC('G','B','A','B')
#define VLC_CODEC_GBR_PLANAR_10L  VLC_FOURCC('G','B','A','L')
#define VLC_CODEC_GBR_PLANAR_12B  VLC_FOURCC('G','B','B','B')
#define VLC_CODEC_GBR_PLANAR_12L  VLC_FOURCC('G','B','B','L')
#define VLC_CODEC_GBR_PLANAR_14B  VLC_FOURCC('G','B','D','B')
#define VLC_CODEC_GBR_PLANAR_14L  VLC_FOURCC('G','B','D','L')
#define VLC_CODEC_GBR_PLANAR_16L  VLC_FOURCC('G','B','F','L')
#define VLC_CODEC_GBR_PLANAR_16B  VLC_FOURCC('G','B','F','B')
#define VLC_CODEC_GBRA_PLANAR     VLC_FOURCC('G','B','0','8')
#define VLC_CODEC_GBRA_PLANAR_10B VLC_FOURCC('G','B','0','B')
#define VLC_CODEC_GBRA_PLANAR_10L VLC_FOURCC('G','B','0','L')
#define VLC_CODEC_GBRA_PLANAR_12B VLC_FOURCC('G','B','C','B')
#define VLC_CODEC_GBRA_PLANAR_12L VLC_FOURCC('G','B','C','L')
#define VLC_CODEC_GBRA_PLANAR_16L VLC_FOURCC('G','B','E','L')
#define VLC_CODEC_GBRA_PLANAR_16B VLC_FOURCC('G','B','E','B')

/* 8 bits grey */
#define VLC_CODEC_GREY            VLC_FOURCC('G','R','E','Y')
/* 10 bits grey */
#define VLC_CODEC_GREY_10L        VLC_FOURCC('G','0','F','L')
#define VLC_CODEC_GREY_10B        VLC_FOURCC('G','0','F','B')
/* 12 bits grey */
#define VLC_CODEC_GREY_12L        VLC_FOURCC('G','2','F','L')
#define VLC_CODEC_GREY_12B        VLC_FOURCC('G','2','F','B')
/* 16 bits grey */
#define VLC_CODEC_GREY_16L        VLC_FOURCC('G','R','F','L')
#define VLC_CODEC_GREY_16B        VLC_FOURCC('G','R','F','B')

/* VDPAU video surface YCbCr 4:2:0 */
#define VLC_CODEC_VDPAU_VIDEO_420 VLC_FOURCC('V','D','V','0')
/* VDPAU video surface YCbCr 4:2:2 */
#define VLC_CODEC_VDPAU_VIDEO_422 VLC_FOURCC('V','D','V','2')
/* VDPAU video surface YCbCr 4:4:4 */
#define VLC_CODEC_VDPAU_VIDEO_444 VLC_FOURCC('V','D','V','4')
/* VDPAU output surface RGBA */
#define VLC_CODEC_VDPAU_OUTPUT    VLC_FOURCC('V','D','O','R')

/* VAAPI opaque surface */
#define VLC_CODEC_VAAPI_420 VLC_FOURCC('V','A','O','P') /* 4:2:0  8 bpc */
#define VLC_CODEC_VAAPI_420_10BPP VLC_FOURCC('V','A','O','0') /* 4:2:0 10 bpc */

/* MediaCodec/IOMX opaque buffer type */
#define VLC_CODEC_ANDROID_OPAQUE  VLC_FOURCC('A','N','O','P')

/* Broadcom MMAL opaque buffer type */
#define VLC_CODEC_MMAL_OPAQUE     VLC_FOURCC('M','M','A','L')

/* DXVA2 opaque video surface for use with D3D9 */
#define VLC_CODEC_D3D9_OPAQUE     VLC_FOURCC('D','X','A','9') /* 4:2:0  8 bpc */
#define VLC_CODEC_D3D9_OPAQUE_10B VLC_FOURCC('D','X','A','0') /* 4:2:0 10 bpc */

/* D3D11VA opaque video surface for use with D3D11 */
#define VLC_CODEC_D3D11_OPAQUE          VLC_FOURCC('D','X','1','1') /* 4:2:0  8 bpc */
#define VLC_CODEC_D3D11_OPAQUE_10B      VLC_FOURCC('D','X','1','0') /* 4:2:0 10 bpc */
#define VLC_CODEC_D3D11_OPAQUE_RGBA     VLC_FOURCC('D','X','R','G')
#define VLC_CODEC_D3D11_OPAQUE_BGRA     VLC_FOURCC('D','A','G','R')

/* NVDEC opaque video format for use the NVDec API */
#define VLC_CODEC_NVDEC_OPAQUE          VLC_FOURCC('N','V','D','8') /* 4:2:0  8 bpc */
#define VLC_CODEC_NVDEC_OPAQUE_10B      VLC_FOURCC('N','V','D','0') /* 4:2:0 10 bpc */
#define VLC_CODEC_NVDEC_OPAQUE_16B      VLC_FOURCC('N','V','D','6') /* 4:2:0 16 bpc */
#define VLC_CODEC_NVDEC_OPAQUE_444      VLC_FOURCC('N','V','4','8') /* 4:4:4  8 bpc */
#define VLC_CODEC_NVDEC_OPAQUE_444_16B  VLC_FOURCC('N','V','4','6') /* 4:4:4 16 bpc */

/* CVPixelBuffer opaque buffer type */
#define VLC_CODEC_CVPX_NV12       VLC_FOURCC('C','V','P','N')
#define VLC_CODEC_CVPX_UYVY       VLC_FOURCC('C','V','P','Y')
#define VLC_CODEC_CVPX_I420       VLC_FOURCC('C','V','P','I')
#define VLC_CODEC_CVPX_BGRA       VLC_FOURCC('C','V','P','B')
#define VLC_CODEC_CVPX_P010       VLC_FOURCC('C','V','P','P')

/* Image codec (video) */
#define VLC_CODEC_PNG             VLC_FOURCC('p','n','g',' ')
#define VLC_CODEC_PPM             VLC_FOURCC('p','p','m',' ')
#define VLC_CODEC_PGM             VLC_FOURCC('p','g','m',' ')
#define VLC_CODEC_PGMYUV          VLC_FOURCC('p','g','m','y')
#define VLC_CODEC_PAM             VLC_FOURCC('p','a','m',' ')
#define VLC_CODEC_JPEG            VLC_FOURCC('j','p','e','g')
#define VLC_CODEC_BPG             VLC_FOURCC('B','P','G',0xFB)
#define VLC_CODEC_JPEGLS          VLC_FOURCC('M','J','L','S')
#define VLC_CODEC_BMP             VLC_FOURCC('b','m','p',' ')
#define VLC_CODEC_TIFF            VLC_FOURCC('t','i','f','f')
#define VLC_CODEC_GIF             VLC_FOURCC('g','i','f',' ')
#define VLC_CODEC_TARGA           VLC_FOURCC('t','g','a',' ')
#define VLC_CODEC_SVG             VLC_FOURCC('s','v','g',' ')
#define VLC_CODEC_SGI             VLC_FOURCC('s','g','i',' ')
#define VLC_CODEC_PNM             VLC_FOURCC('p','n','m',' ')
#define VLC_CODEC_PCX             VLC_FOURCC('p','c','x',' ')
#define VLC_CODEC_XWD             VLC_FOURCC('X','W','D',' ')
#define VLC_CODEC_TXD             VLC_FOURCC('T','X','D',' ')
#define VLC_CODEC_WEBP            VLC_FOURCC('W','E','B','P')


/* Audio codec */
#define VLC_CODEC_MPGA                       VLC_FOURCC('m','p','g','a')
#define VLC_CODEC_MP4A                       VLC_FOURCC('m','p','4','a')
#define VLC_CODEC_ALS                        VLC_FOURCC('a','l','s',' ')
#define VLC_CODEC_A52                        VLC_FOURCC('a','5','2',' ')
#define VLC_CODEC_EAC3                       VLC_FOURCC('e','a','c','3')
#define VLC_CODEC_AC4                        VLC_FOURCC('a','c','-','4')
#define VLC_CODEC_DTS                        VLC_FOURCC('d','t','s',' ')
/* Only used by outputs and filters */
#define VLC_CODEC_DTSHD                      VLC_FOURCC('d','t','s','h')
#define VLC_CODEC_WMA1                       VLC_FOURCC('W','M','A','1')
#define VLC_CODEC_WMA2                       VLC_FOURCC('W','M','A','2')
#define VLC_CODEC_WMAP                       VLC_FOURCC('W','M','A','P')
#define VLC_CODEC_WMAL                       VLC_FOURCC('W','M','A','L')
#define VLC_CODEC_WMAS                       VLC_FOURCC('W','M','A','S')
#define VLC_CODEC_FLAC                       VLC_FOURCC('f','l','a','c')
#define VLC_CODEC_MLP                        VLC_FOURCC('m','l','p',' ')
#define VLC_CODEC_TRUEHD                     VLC_FOURCC('m','l','p','a')
#define VLC_CODEC_DVAUDIO                    VLC_FOURCC('d','v','a','u')
#define VLC_CODEC_SPEEX                      VLC_FOURCC('s','p','x',' ')
#define VLC_CODEC_OPUS                       VLC_FOURCC('O','p','u','s')
#define VLC_CODEC_VORBIS                     VLC_FOURCC('v','o','r','b')
#define VLC_CODEC_MACE3                      VLC_FOURCC('M','A','C','3')
#define VLC_CODEC_MACE6                      VLC_FOURCC('M','A','C','6')
#define VLC_CODEC_MUSEPACK7                  VLC_FOURCC('M','P','C',' ')
#define VLC_CODEC_MUSEPACK8                  VLC_FOURCC('M','P','C','K')
#define VLC_CODEC_RA_144                     VLC_FOURCC('1','4','_','4')
#define VLC_CODEC_RA_288                     VLC_FOURCC('2','8','_','8')
#define VLC_CODEC_INTERPLAY_DPCM             VLC_FOURCC('i','d','p','c')
#define VLC_CODEC_ROQ_DPCM                   VLC_FOURCC('R','o','Q','a')
#define VLC_CODEC_DSICINAUDIO                VLC_FOURCC('D','C','I','A')
#define VLC_CODEC_ADPCM_4XM                  VLC_FOURCC('4','x','m','a')
#define VLC_CODEC_ADPCM_EA                   VLC_FOURCC('A','D','E','A')
#define VLC_CODEC_ADPCM_XA                   VLC_FOURCC('x','a',' ',' ')
#define VLC_CODEC_ADPCM_ADX                  VLC_FOURCC('a','d','x',' ')
#define VLC_CODEC_ADPCM_IMA_WS               VLC_FOURCC('A','I','W','S')
#define VLC_CODEC_ADPCM_G722                 VLC_FOURCC('g','7','2','2')
#define VLC_CODEC_ADPCM_G726                 VLC_FOURCC('g','7','2','6')
#define VLC_CODEC_ADPCM_SWF                  VLC_FOURCC('S','W','F','a')
#define VLC_CODEC_ADPCM_MS                   VLC_FOURCC('m','s',0x00,0x02)
#define VLC_CODEC_ADPCM_IMA_WAV              VLC_FOURCC('m','s',0x00,0x11)
#define VLC_CODEC_ADPCM_IMA_AMV              VLC_FOURCC('i','m','a','v')
#define VLC_CODEC_ADPCM_IMA_QT               VLC_FOURCC('i','m','a','4')
#define VLC_CODEC_ADPCM_YAMAHA               VLC_FOURCC('m','s',0x00,0x20)
#define VLC_CODEC_ADPCM_DK3                  VLC_FOURCC('m','s',0x00,0x62)
#define VLC_CODEC_ADPCM_DK4                  VLC_FOURCC('m','s',0x00,0x61)
#define VLC_CODEC_ADPCM_CREATIVE             VLC_FOURCC('m','s',0x00,0xC0)
#define VLC_CODEC_ADPCM_SBPRO_2              VLC_FOURCC('m','s',0x00,0xC2)
#define VLC_CODEC_ADPCM_SBPRO_3              VLC_FOURCC('m','s',0x00,0xC3)
#define VLC_CODEC_ADPCM_SBPRO_4              VLC_FOURCC('m','s',0x00,0xC4)
#define VLC_CODEC_ADPCM_THP                  VLC_FOURCC('T','H','P','A')
#define VLC_CODEC_ADPCM_XA_EA                VLC_FOURCC('X','A','J', 0)
#define VLC_CODEC_G723_1                     VLC_FOURCC('g','7','2', 0x31)
#define VLC_CODEC_G729                       VLC_FOURCC('g','7','2','9')
#define VLC_CODEC_VMDAUDIO                   VLC_FOURCC('v','m','d','a')
#define VLC_CODEC_AMR_NB                     VLC_FOURCC('s','a','m','r')
#define VLC_CODEC_AMR_WB                     VLC_FOURCC('s','a','w','b')
#define VLC_CODEC_ALAC                       VLC_FOURCC('a','l','a','c')
#define VLC_CODEC_QDM2                       VLC_FOURCC('Q','D','M','2')
#define VLC_CODEC_QDMC                       VLC_FOURCC('Q','D','M','C')
#define VLC_CODEC_COOK                       VLC_FOURCC('c','o','o','k')
#define VLC_CODEC_SIPR                       VLC_FOURCC('s','i','p','r')
#define VLC_CODEC_TTA                        VLC_FOURCC('T','T','A','1')
#define VLC_CODEC_SHORTEN                    VLC_FOURCC('s','h','n',' ')
#define VLC_CODEC_WAVPACK                    VLC_FOURCC('W','V','P','K')
#define VLC_CODEC_GSM                        VLC_FOURCC('g','s','m',' ')
#define VLC_CODEC_GSM_MS                     VLC_FOURCC('a','g','s','m')
#define VLC_CODEC_ATRAC1                     VLC_FOURCC('a','t','r','1')
#define VLC_CODEC_ATRAC3                     VLC_FOURCC('a','t','r','c')
#define VLC_CODEC_ATRAC3P                    VLC_FOURCC('a','t','r','p')
#define VLC_CODEC_IMC                        VLC_FOURCC(0x1,0x4,0x0,0x0)
#define VLC_CODEC_TRUESPEECH                 VLC_FOURCC(0x22,0x0,0x0,0x0)
#define VLC_CODEC_NELLYMOSER                 VLC_FOURCC('N','E','L','L')
#define VLC_CODEC_APE                        VLC_FOURCC('A','P','E',' ')
#define VLC_CODEC_QCELP                      VLC_FOURCC('Q','c','l','p')
#define VLC_CODEC_302M                       VLC_FOURCC('3','0','2','m')
#define VLC_CODEC_DVD_LPCM                   VLC_FOURCC('l','p','c','m')
#define VLC_CODEC_DVDA_LPCM                  VLC_FOURCC('a','p','c','m')
#define VLC_CODEC_BD_LPCM                    VLC_FOURCC('b','p','c','m')
#define VLC_CODEC_WIDI_LPCM                  VLC_FOURCC('w','p','c','m')
#define VLC_CODEC_SDDS                       VLC_FOURCC('s','d','d','s')
#define VLC_CODEC_MIDI                       VLC_FOURCC('M','I','D','I')
#define VLC_CODEC_RALF                       VLC_FOURCC('R','A','L','F')

#define VLC_CODEC_S8                         VLC_FOURCC('s','8',' ',' ')
#define VLC_CODEC_U8                         VLC_FOURCC('u','8',' ',' ')
#define VLC_CODEC_S16L                       VLC_FOURCC('s','1','6','l')
#define VLC_CODEC_S16L_PLANAR                VLC_FOURCC('s','1','l','p')
#define VLC_CODEC_S16B                       VLC_FOURCC('s','1','6','b')
#define VLC_CODEC_U16L                       VLC_FOURCC('u','1','6','l')
#define VLC_CODEC_U16B                       VLC_FOURCC('u','1','6','b')
#define VLC_CODEC_S20B                       VLC_FOURCC('s','2','0','b')
#define VLC_CODEC_S24L                       VLC_FOURCC('s','2','4','l')
#define VLC_CODEC_S24B                       VLC_FOURCC('s','2','4','b')
#define VLC_CODEC_U24L                       VLC_FOURCC('u','2','4','l')
#define VLC_CODEC_U24B                       VLC_FOURCC('u','2','4','b')
#define VLC_CODEC_S24L32                     VLC_FOURCC('s','2','4','4')
#define VLC_CODEC_S24B32                     VLC_FOURCC('S','2','4','4')
#define VLC_CODEC_S32L                       VLC_FOURCC('s','3','2','l')
#define VLC_CODEC_S32B                       VLC_FOURCC('s','3','2','b')
#define VLC_CODEC_U32L                       VLC_FOURCC('u','3','2','l')
#define VLC_CODEC_U32B                       VLC_FOURCC('u','3','2','b')
#define VLC_CODEC_F32L                       VLC_FOURCC('f','3','2','l')
#define VLC_CODEC_F32B                       VLC_FOURCC('f','3','2','b')
#define VLC_CODEC_F64L                       VLC_FOURCC('f','6','4','l')
#define VLC_CODEC_F64B                       VLC_FOURCC('f','6','4','b')

#define VLC_CODEC_ALAW                       VLC_FOURCC('a','l','a','w')
#define VLC_CODEC_MULAW                      VLC_FOURCC('m','l','a','w')
#define VLC_CODEC_DAT12                      VLC_FOURCC('L','P','1','2')
#define VLC_CODEC_S24DAUD                    VLC_FOURCC('d','a','u','d')
#define VLC_CODEC_TWINVQ                     VLC_FOURCC('T','W','I','N')
#define VLC_CODEC_BMVAUDIO                   VLC_FOURCC('B','M','V','A')
#define VLC_CODEC_ULEAD_DV_AUDIO_NTSC        VLC_FOURCC('m','s',0x02,0x15)
#define VLC_CODEC_ULEAD_DV_AUDIO_PAL         VLC_FOURCC('m','s',0x02,0x16)
#define VLC_CODEC_INDEO_AUDIO                VLC_FOURCC('m','s',0x04,0x02)
#define VLC_CODEC_METASOUND                  VLC_FOURCC('m','s',0x00,0x75)
#define VLC_CODEC_ON2AVC                     VLC_FOURCC('m','s',0x05,0x00)
#define VLC_CODEC_TAK                        VLC_FOURCC('t','a','k',' ')
#define VLC_CODEC_SMACKAUDIO                 VLC_FOURCC('S','M','K','A')
#define VLC_CODEC_ADPCM_IMA_EA_SEAD          VLC_FOURCC('S','E','A','D')
#define VLC_CODEC_ADPCM_EA_R1                VLC_FOURCC('E','A','R','1')
#define VLC_CODEC_ADPCM_IMA_APC              VLC_FOURCC('A','I','P','C')
#define VLC_CODEC_DSD_LSBF                   VLC_FOURCC('D','S','D','l')
#define VLC_CODEC_DSD_LSBF_PLANAR            VLC_FOURCC('D','S','F','l')
#define VLC_CODEC_DSD_MSBF                   VLC_FOURCC('D','S','D',' ')
#define VLC_CODEC_DSD_MSBF_PLANAR            VLC_FOURCC('D','S','F','m')

/* Subtitle */
#define VLC_CODEC_SPU       VLC_FOURCC('s','p','u',' ')
#define VLC_CODEC_DVBS      VLC_FOURCC('d','v','b','s')
#define VLC_CODEC_SUBT      VLC_FOURCC('s','u','b','t')
#define VLC_CODEC_XSUB      VLC_FOURCC('X','S','U','B')
#define VLC_CODEC_SSA       VLC_FOURCC('s','s','a',' ')
#define VLC_CODEC_TEXT      VLC_FOURCC('T','E','X','T')
#define VLC_CODEC_TELETEXT  VLC_FOURCC('t','e','l','x')
#define VLC_CODEC_KATE      VLC_FOURCC('k','a','t','e')
#define VLC_CODEC_CMML      VLC_FOURCC('c','m','m','l')
#define VLC_CODEC_ITU_T140  VLC_FOURCC('t','1','4','0')
#define VLC_CODEC_USF       VLC_FOURCC('u','s','f',' ')
#define VLC_CODEC_OGT       VLC_FOURCC('o','g','t',' ')
#define VLC_CODEC_CVD       VLC_FOURCC('c','v','d',' ')
#define VLC_CODEC_QTXT      VLC_FOURCC('q','t','x','t')
#define VLC_CODEC_TX3G      VLC_FOURCC('t','x','3','g')
#define VLC_CODEC_ARIB_A    VLC_FOURCC('a','r','b','a')
#define VLC_CODEC_ARIB_C    VLC_FOURCC('a','r','b','c')
/* Blu-ray Presentation Graphics */
#define VLC_CODEC_BD_PG     VLC_FOURCC('b','d','p','g')
#define VLC_CODEC_BD_TEXT   VLC_FOURCC('b','d','t','x')
/* EBU STL (TECH. 3264-E) */
#define VLC_CODEC_EBU_STL   VLC_FOURCC('S','T','L',' ')
#define VLC_CODEC_SCTE_18   VLC_FOURCC('S','C','1','8')
#define VLC_CODEC_SCTE_27   VLC_FOURCC('S','C','2','7')
/* EIA/CEA-608/708 */
#define VLC_CODEC_CEA608    VLC_FOURCC('c','6','0','8')
#define VLC_CODEC_CEA708    VLC_FOURCC('c','7','0','8')
#define VLC_CODEC_TTML      VLC_FOURCC('s','t','p','p')
#define VLC_CODEC_TTML_TS   VLC_FOURCC('s','t','p','P') /* special for EN.303.560 */
#define VLC_CODEC_WEBVTT    VLC_FOURCC('w','v','t','t')

/* XYZ colorspace 12 bits packed in 16 bits, organisation |XXX0|YYY0|ZZZ0| */
#define VLC_CODEC_XYZ12     VLC_FOURCC('X','Y','1','2')


/* Special endian dependent values
 * The suffic N means Native
 * The suffix I means Inverted (ie non native) */
#ifdef WORDS_BIGENDIAN
#   define VLC_CODEC_S16N VLC_CODEC_S16B
#   define VLC_CODEC_U16N VLC_CODEC_U16B
#   define VLC_CODEC_S24N VLC_CODEC_S24B
#   define VLC_CODEC_U24N VLC_CODEC_U24B
#   define VLC_CODEC_S32N VLC_CODEC_S32B
#   define VLC_CODEC_U32N VLC_CODEC_U32B
#   define VLC_CODEC_FL32 VLC_CODEC_F32B
#   define VLC_CODEC_FL64 VLC_CODEC_F64B

#   define VLC_CODEC_S16I VLC_CODEC_S16L
#   define VLC_CODEC_U16I VLC_CODEC_U16L
#   define VLC_CODEC_S24I VLC_CODEC_S24L
#   define VLC_CODEC_U24I VLC_CODEC_U24L
#   define VLC_CODEC_S32I VLC_CODEC_S32L
#   define VLC_CODEC_U32I VLC_CODEC_U32L

#else
#   define VLC_CODEC_S16N VLC_CODEC_S16L
#   define VLC_CODEC_U16N VLC_CODEC_U16L
#   define VLC_CODEC_S24N VLC_CODEC_S24L
#   define VLC_CODEC_U24N VLC_CODEC_U24L
#   define VLC_CODEC_S32N VLC_CODEC_S32L
#   define VLC_CODEC_U32N VLC_CODEC_U32L
#   define VLC_CODEC_FL32 VLC_CODEC_F32L
#   define VLC_CODEC_FL64 VLC_CODEC_F64L

#   define VLC_CODEC_S16I VLC_CODEC_S16B
#   define VLC_CODEC_U16I VLC_CODEC_U16B
#   define VLC_CODEC_S24I VLC_CODEC_S24B
#   define VLC_CODEC_U24I VLC_CODEC_U24B
#   define VLC_CODEC_S32I VLC_CODEC_S32B
#   define VLC_CODEC_U32I VLC_CODEC_U32B
#endif

/* Non official codecs, used to force a profile in an encoder */
/* MPEG-1 video */
#define VLC_CODEC_MP1V      VLC_FOURCC('m','p','1','v')
/* MPEG-2 video */
#define VLC_CODEC_MP2V      VLC_FOURCC('m','p','2','v')
/* MPEG-I/II layer 2 audio */
#define VLC_CODEC_MP2       VLC_FOURCC('m','p','2',' ')
/* MPEG-I/II layer 3 audio */
#define VLC_CODEC_MP3       VLC_FOURCC('m','p','3',' ')

/**
 * It returns the codec associated to a fourcc within an ES category.
 *
 * If not found, it will return the given fourcc.
 * If found, it will always be one of the VLC_CODEC_ defined above.
 *
 * You may use UNKNOWN_ES for the ES category if you don't have the information.
 */
VLC_API vlc_fourcc_t vlc_fourcc_GetCodec( int i_cat, vlc_fourcc_t i_fourcc );

/**
 * It returns the codec associated to a fourcc stored in a zero terminated
 * string.
 *
 * If the string is NULL or does not have exactly 4 characters, it will
 * return 0, otherwise it behaves like vlc_fourcc_GetCodec.
 *
 * Provided for convenience.
 */
VLC_API vlc_fourcc_t vlc_fourcc_GetCodecFromString( int i_cat, const char * );

/**
 * It converts the given fourcc to an audio codec when possible.
 *
 * The fourccs converted are aflt, araw/pcm , twos, sowt. When an incompatible i_bits
 * is detected, 0 is returned.
 * The other fourccs go through vlc_fourcc_GetCodec and i_bits is not checked.
 */
VLC_API vlc_fourcc_t vlc_fourcc_GetCodecAudio( vlc_fourcc_t i_fourcc, int i_bits );

/**
 * It returns the description of the given fourcc or NULL if not found.
 *
 * You may use UNKNOWN_ES for the ES category if you don't have the information.
 */
VLC_API const char * vlc_fourcc_GetDescription( int i_cat, vlc_fourcc_t i_fourcc );

/**
 * It returns a list (terminated with the value 0) of YUV fourccs in
 * decreasing priority order for the given chroma.
 *
 * It will always return a non NULL pointer that must not be freed.
 */
VLC_API const vlc_fourcc_t * vlc_fourcc_GetYUVFallback( vlc_fourcc_t );

/**
 * It returns a list (terminated with the value 0) of RGB fourccs in
 * decreasing priority order for the given chroma.
 *
 * It will always return a non NULL pointer that must not be freed.
 */
VLC_API const vlc_fourcc_t * vlc_fourcc_GetRGBFallback( vlc_fourcc_t );

/**
 * It returns a list (terminated with the value 0) of fourccs in decreasing
 * priority order for the given chroma. It will return either YUV or RGB
 * fallbacks depending on whether or not the fourcc given is YUV.
 *
 * It will always return a non NULL pointer that must not be freed.
 */
VLC_API const vlc_fourcc_t * vlc_fourcc_GetFallback( vlc_fourcc_t );

/**
 * It returns true if the given fourcc is YUV and false otherwise.
 */
VLC_API bool vlc_fourcc_IsYUV( vlc_fourcc_t );

/**
 * It returns true if the two fourccs are equivalent if their U&V planes are
 * swapped.
 */
VLC_API bool vlc_fourcc_AreUVPlanesSwapped(vlc_fourcc_t , vlc_fourcc_t );

/**
 * Chroma related information.
 */
typedef struct {
    unsigned plane_count;
    struct {
        vlc_rational_t w;
        vlc_rational_t h;
    } p[4];
    unsigned pixel_size;        /* Number of bytes per pixel for a plane */
    unsigned pixel_bits;        /* Number of bits actually used bits per pixel for a plane */
} vlc_chroma_description_t;

/**
 * It returns a vlc_chroma_description_t describing the requested fourcc or NULL
 * if not found.
 */
VLC_API const vlc_chroma_description_t * vlc_fourcc_GetChromaDescription( vlc_fourcc_t fourcc ) VLC_USED;

#endif /* _VLC_FOURCC_H */

