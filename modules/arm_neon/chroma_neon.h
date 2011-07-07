/*****************************************************************************
 * chroma_neon.h
 *****************************************************************************
 * Copyright (C) 2011 Rémi Denis-Courmont
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

/* Planes must start on a 16-bytes boundary. Pitches must be multiples of 16
 * bytes even for subsampled components. */

/* Planar picture buffer.
 * Pitch corresponds to luminance component in bytes. Chrominance pitches are
 * inferred from the color subsampling ratio. */
struct yuv_planes
{
	void *y, *u, *v;
	size_t pitch;
};

/* Packed picture buffer. Pitch is in bytes (_not_ pixels). */
struct yuv_pack
{
	void *yuv;
	size_t pitch;
};

/* I420 to YUYV conversion. */
void i420_yuyv_neon (struct yuv_pack *const out,
                     const struct yuv_planes *const in,
                     int width, int height);

/* I420 to UYVY conversion. */
void i420_uyvy_neon (struct yuv_pack *const out,
                     const struct yuv_planes *const in,
                     int width, int height);

/* I422 to YUYV conversion. */
void i422_yuyv_neon (struct yuv_pack *const out,
                     const struct yuv_planes *const in,
                     int width, int height);

/* I422 to UYVY conversion. */
void i422_uyvy_neon (struct yuv_pack *const out,
                     const struct yuv_planes *const in,
                     int width, int height);
