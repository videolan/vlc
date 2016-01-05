/*****************************************************************************
 * h2output.h: HTTP/2 send queue declarations
 *****************************************************************************
 * Copyright (C) 2015 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/**
 * \defgroup h2_output HTTP/2 output
 * \ingroup h2
 * @{
 */

struct vlc_h2_output;
struct vlc_h2_frame;
struct vlc_tls;

int vlc_h2_output_send_prio(struct vlc_h2_output *, struct vlc_h2_frame *);
int vlc_h2_output_send(struct vlc_h2_output *, struct vlc_h2_frame *);

struct vlc_h2_output *vlc_h2_output_create(struct vlc_tls *, bool client);
void vlc_h2_output_destroy(struct vlc_h2_output *);

/** @} */
