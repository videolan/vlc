/*****************************************************************************
 * pacer.h: decoder picture output pacing
 *****************************************************************************
 * Copyright © 2017-2022 VideoLabs, VideoLAN and VLC authors
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
#ifndef VIDEOTOOLBOX_PACER_H
#define VIDEOTOOLBOX_PACER_H

struct pic_pacer
{
    vlc_mutex_t lock;
    vlc_cond_t  wait;
    uint8_t     nb_fields_out;
    uint8_t     allocated_fields_max;
    uint8_t     allocated_fields_next;
    uint8_t     queued_fields_for_decode;
};

void pic_pacer_Clean(struct pic_pacer *);

void pic_pacer_Init(struct pic_pacer *);

void pic_pacer_AccountAllocation(struct pic_pacer *, bool b_field);

void pic_pacer_AccountScheduledDecode(struct pic_pacer *, bool b_field);

void pic_pacer_AccountFinishedDecode(struct pic_pacer *, bool b_field);

void pic_pacer_WaitAllocatableSlot(struct pic_pacer *, bool b_field);

void pic_pacer_AccountDeallocation(struct pic_pacer *, bool b_field);

void pic_pacer_UpdateMaxBuffering(struct pic_pacer *, uint8_t);

#endif // VIDEOTOOLBOX_PACER_H
