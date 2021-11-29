/*****************************************************************************
 * video_window.h: window management for VLC video output
 *****************************************************************************
 * Copyright © 2014 Rémi Denis-Courmont
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

struct vout_crop;

vout_window_t *vout_display_window_New(vout_thread_t *);
void vout_display_window_Delete(vout_window_t *);

void vout_display_SizeWindow(unsigned *restrict width,
                             unsigned *restrict height,
                             const video_format_t *restrict original,
                             const vlc_rational_t *restrict dar,
                             const struct vout_crop *restrict crop,
                             const vout_display_cfg_t *restrict cfg);
