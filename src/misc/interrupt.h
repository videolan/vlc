/*****************************************************************************
 * interrupt.h:
 *****************************************************************************
 * Copyright (C) 2015 Rémi Denis-Courmont
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

/** @ingroup interrupt */
#ifndef LIBVLC_INPUT_SIGNAL_H
# define LIBVLC_INPUT_SIGNAL_H 1

# include <stdatomic.h>

# include <vlc_interrupt.h>

void vlc_interrupt_init(vlc_interrupt_t *);
void vlc_interrupt_deinit(vlc_interrupt_t *);

struct vlc_interrupt
{
    vlc_mutex_t lock;
    bool interrupted;
    atomic_bool killed;
    void (*callback)(void *);
    void *data;
};
#endif
