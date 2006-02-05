/*****************************************************************************
 * i422_yuy2.h : YUV to YUV conversion module for vlc
 *****************************************************************************
 * Copyright (C) 2002 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef MODULE_NAME_IS_i422_yuy2_mmx

#define MMX_YUV422_YUYV "                                                 \n\
movq       (%1), %%mm0  # Load 8 Y            y7 y6 y5 y4 y3 y2 y1 y0     \n\
movd       (%2), %%mm1  # Load 4 Cb           00 00 00 00 u3 u2 u1 u0     \n\
movd       (%3), %%mm2  # Load 4 Cr           00 00 00 00 v3 v2 v1 v0     \n\
punpcklbw %%mm2, %%mm1  #                     v3 u3 v2 u2 v1 u1 v0 u0     \n\
movq      %%mm0, %%mm2  #                     y7 y6 y5 y4 y3 y2 y1 y0     \n\
punpcklbw %%mm1, %%mm2  #                     v1 y3 u1 y2 v0 y1 u0 y0     \n\
movq      %%mm2, (%0)   # Store low YUYV                                  \n\
punpckhbw %%mm1, %%mm0  #                     v3 y7 u3 y6 v2 y5 u2 y4     \n\
movq      %%mm0, 8(%0)  # Store high YUYV                                 \n\
"

#define MMX_YUV422_YVYU "                                                 \n\
movq       (%1), %%mm0  # Load 8 Y            y7 y6 y5 y4 y3 y2 y1 y0     \n\
movd       (%2), %%mm2  # Load 4 Cb           00 00 00 00 u3 u2 u1 u0     \n\
movd       (%3), %%mm1  # Load 4 Cr           00 00 00 00 v3 v2 v1 v0     \n\
punpcklbw %%mm2, %%mm1  #                     u3 v3 u2 v2 u1 v1 u0 v0     \n\
movq      %%mm0, %%mm2  #                     y7 y6 y5 y4 y3 y2 y1 y0     \n\
punpcklbw %%mm1, %%mm2  #                     u1 y3 v1 y2 u0 y1 v0 y0     \n\
movq      %%mm2, (%0)   # Store low YUYV                                  \n\
punpckhbw %%mm1, %%mm0  #                     u3 y7 v3 y6 u2 y5 v2 y4     \n\
movq      %%mm0, 8(%0)  # Store high YUYV                                 \n\
"

#define MMX_YUV422_UYVY "                                                 \n\
movq       (%1), %%mm0  # Load 8 Y            y7 y6 y5 y4 y3 y2 y1 y0     \n\
movd       (%2), %%mm1  # Load 4 Cb           00 00 00 00 u3 u2 u1 u0     \n\
movd       (%3), %%mm2  # Load 4 Cr           00 00 00 00 v3 v2 v1 v0     \n\
punpcklbw %%mm2, %%mm1  #                     v3 u3 v2 u2 v1 u1 v0 u0     \n\
movq      %%mm1, %%mm2  #                     v3 u3 v2 u2 v1 u1 v0 u0     \n\
punpcklbw %%mm0, %%mm2  #                     y3 v1 y2 u1 y1 v0 y0 u0     \n\
movq      %%mm2, (%0)   # Store low UYVY                                  \n\
punpckhbw %%mm0, %%mm1  #                     y7 v3 y6 u3 y5 v2 y4 u2     \n\
movq      %%mm1, 8(%0)  # Store high UYVY                                 \n\
"

#define MMX_YUV422_Y211 "                                                 \n\
"

#else

#define C_YUV422_YUYV( p_line, p_y, p_u, p_v )                              \
    *(p_line)++ = *(p_y)++;                                                 \
    *(p_line)++ = *(p_u)++;                                                 \
    *(p_line)++ = *(p_y)++;                                                 \
    *(p_line)++ = *(p_v)++;                                                 \

#define C_YUV422_YVYU( p_line, p_y, p_u, p_v )                              \
    *(p_line)++ = *(p_y)++;                                                 \
    *(p_line)++ = *(p_v)++;                                                 \
    *(p_line)++ = *(p_y)++;                                                 \
    *(p_line)++ = *(p_u)++;                                                 \

#define C_YUV422_UYVY( p_line, p_y, p_u, p_v )                              \
    *(p_line)++ = *(p_u)++;                                                 \
    *(p_line)++ = *(p_y)++;                                                 \
    *(p_line)++ = *(p_v)++;                                                 \
    *(p_line)++ = *(p_y)++;                                                 \

#define C_YUV422_Y211( p_line, p_y, p_u, p_v )                              \
    *(p_line)++ = *(p_y); p_y += 2;                                         \
    *(p_line)++ = *(p_u) - 0x80; p_u += 2;                                  \
    *(p_line)++ = *(p_y); p_y += 2;                                         \
    *(p_line)++ = *(p_v) - 0x80; p_v += 2;                                  \

#endif

