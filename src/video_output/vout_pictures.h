/*****************************************************************************
 * vout_pictures.h : picture management definitions
 *****************************************************************************
 * Copyright (C) 2002-2004 VideoLAN
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
 * Fourcc definitions that we can handle internally
 *****************************************************************************/

/* Packed RGB for 8bpp */
#define FOURCC_BI_RGB       0x00000000
#define FOURCC_RGB2         VLC_FOURCC('R','G','B','2')

/* Packed RGB for 16, 24, 32bpp */
#define FOURCC_BI_BITFIELDS 0x00000003

/* Packed RGB 15bpp, usually 0x7c00, 0x03e0, 0x001f */
#define FOURCC_RV15         VLC_FOURCC('R','V','1','5')

/* Packed RGB 16bpp, usually 0xf800, 0x07e0, 0x001f */
#define FOURCC_RV16         VLC_FOURCC('R','V','1','6')

/* Packed RGB 24bpp, usually 0x00ff0000, 0x0000ff00, 0x000000ff */
#define FOURCC_RV24         VLC_FOURCC('R','V','2','4')

/* Packed RGB 32bpp, usually 0x00ff0000, 0x0000ff00, 0x000000ff */
#define FOURCC_RV32         VLC_FOURCC('R','V','3','2')

/* Planar YUV 4:2:0, Y:U:V */
#define FOURCC_I420         VLC_FOURCC('I','4','2','0')
#define FOURCC_IYUV         VLC_FOURCC('I','Y','U','V')
#define FOURCC_J420         VLC_FOURCC('J','4','2','0')

/* Planar YUV 4:2:0, Y:V:U */
#define FOURCC_YV12         VLC_FOURCC('Y','V','1','2')

/* Packed YUV 4:2:2, U:Y:V:Y, interlaced */
#define FOURCC_IUYV         VLC_FOURCC('I','U','Y','V')

/* Packed YUV 4:2:2, U:Y:V:Y */
#define FOURCC_UYVY         VLC_FOURCC('U','Y','V','Y')
#define FOURCC_UYNV         VLC_FOURCC('U','Y','N','V')
#define FOURCC_Y422         VLC_FOURCC('Y','4','2','2')

/* Packed YUV 4:2:2, U:Y:V:Y, reverted */
#define FOURCC_cyuv         VLC_FOURCC('c','y','u','v')

/* Packed YUV 4:2:2, Y:U:Y:V */
#define FOURCC_YUY2         VLC_FOURCC('Y','U','Y','2')
#define FOURCC_YUNV         VLC_FOURCC('Y','U','N','V')

/* Packed YUV 4:2:2, Y:V:Y:U */
#define FOURCC_YVYU         VLC_FOURCC('Y','V','Y','U')

/* Packed YUV 2:1:1, Y:U:Y:V */
#define FOURCC_Y211         VLC_FOURCC('Y','2','1','1')

/* Planar YUV 4:1:1, Y:U:V */
#define FOURCC_I411         VLC_FOURCC('I','4','1','1')

/* Planar YUV 4:1:0, Y:U:V */
#define FOURCC_I410         VLC_FOURCC('I','4','1','0')
#define FOURCC_YVU9         VLC_FOURCC('Y','V','U','9')

/* Planar Y, packed UV, from Matrox */
#define FOURCC_YMGA         VLC_FOURCC('Y','M','G','A')

/* Planar 4:2:2, Y:U:V */
#define FOURCC_I422         VLC_FOURCC('I','4','2','2')
#define FOURCC_J422         VLC_FOURCC('J','4','2','2')

/* Planar 4:4:4, Y:U:V */
#define FOURCC_I444         VLC_FOURCC('I','4','4','4')
#define FOURCC_J444         VLC_FOURCC('J','4','4','4')

/* Planar 4:4:4:4 Y:U:V:A */
#define FOURCC_YUVA         VLC_FOURCC('Y','U','V','A')

/* Palettized YUV with palette element Y:U:V:A */
#define FOURCC_YUVP         VLC_FOURCC('Y','U','V','P')
