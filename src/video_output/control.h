/*****************************************************************************
 * control.h : vout internal control
 *****************************************************************************
 * Copyright (C) 2009-2010 Laurent Aimar
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#ifndef LIBVLC_VOUT_INTERNAL_CONTROL_H
#define LIBVLC_VOUT_INTERNAL_CONTROL_H

#include <vlc_viewpoint.h>

/* */
enum {
    VOUT_CONTROL_TERMINATE,
    VOUT_CONTROL_CHANGE_FILTERS,        /* string */
    VOUT_CONTROL_CHANGE_INTERLACE,      /* boolean */

    VOUT_CONTROL_MOUSE_STATE,           /* vlc_mouse_t */
};

typedef struct {
    int type;

    union {
        bool    boolean;
        char    *string;
        vlc_mouse_t mouse;
    };
} vout_control_cmd_t;

void vout_control_cmd_Init(vout_control_cmd_t *, int type);
void vout_control_cmd_Clean(vout_control_cmd_t *);

typedef struct {
    vlc_mutex_t lock;
    vlc_cond_t  wait_request;
    vlc_cond_t  wait_available;

    /* */
    bool is_dead;
    bool can_sleep;
    bool is_waiting;
    bool is_held;
    DECL_ARRAY(vout_control_cmd_t) cmd;
} vout_control_t;

/* */
void vout_control_Init(vout_control_t *);
void vout_control_Clean(vout_control_t *);

/* controls outside of the vout thread */
void vout_control_WaitEmpty(vout_control_t *);

void vout_control_Push(vout_control_t *, vout_control_cmd_t *);
void vout_control_PushVoid(vout_control_t *, int type);
void vout_control_PushBool(vout_control_t *, int type, bool boolean);
void vout_control_PushString(vout_control_t *, int type, const char *string);
void vout_control_Wake(vout_control_t *);
void vout_control_Hold(vout_control_t *);
void vout_control_Release(vout_control_t *);

/* control inside of the vout thread */
int vout_control_Pop(vout_control_t *, vout_control_cmd_t *, vlc_tick_t deadline);
void vout_control_Dead(vout_control_t *);

#endif
