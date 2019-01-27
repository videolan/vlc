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
    VOUT_CONTROL_CLEAN,
    VOUT_CONTROL_REINIT,                /* cfg */

#if 0
    /* */
    VOUT_CONTROL_START,
    VOUT_CONTROL_STOP,
#endif
    VOUT_CONTROL_SUBPICTURE,            /* subpicture */
    VOUT_CONTROL_FLUSH_SUBPICTURE,      /* integer */
    VOUT_CONTROL_CHANGE_FILTERS,        /* string */
    VOUT_CONTROL_CHANGE_INTERLACE,      /* boolean */

    VOUT_CONTROL_STEP,                  /* time_ptr */

    VOUT_CONTROL_MOUSE_STATE,           /* vlc_mouse_t */
    VOUT_CONTROL_DISPLAY_SIZE,          /* window */
    VOUT_CONTROL_DISPLAY_FILLED,        /* bool */
    VOUT_CONTROL_ZOOM,                  /* pair */

    VOUT_CONTROL_ASPECT_RATIO,          /* pair */
    VOUT_CONTROL_CROP_BORDER,           /* border */
    VOUT_CONTROL_CROP_RATIO,            /* pair */
    VOUT_CONTROL_CROP_WINDOW,           /* window */
    VOUT_CONTROL_VIEWPOINT,             /* viewpoint */
};

typedef struct {
    int type;

    union {
        bool    boolean;
        vlc_tick_t *time_ptr;
        char    *string;
        int     integer;
        struct {
            int a;
            int b;
        } pair;
        struct {
            unsigned left;
            unsigned top;
            unsigned right;
            unsigned bottom;
        } border;
        struct {
            unsigned x;
            unsigned y;
            unsigned width;
            unsigned height;
        } window;
        vlc_mouse_t mouse;
        const vout_configuration_t *cfg;
        subpicture_t *subpicture;
        vlc_viewpoint_t viewpoint;
    };
} vout_control_cmd_t;

void vout_control_cmd_Init(vout_control_cmd_t *, int type);
void vout_control_cmd_Clean(vout_control_cmd_t *);

typedef struct {
    vlc_mutex_t lock;
    vlc_cond_t  wait_request;
    vlc_cond_t  wait_acknowledge;

    /* */
    bool is_dead;
    bool can_sleep;
    bool is_processing;
    bool is_waiting;
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
void vout_control_PushInteger(vout_control_t *, int type, int integer);
void vout_control_PushPair(vout_control_t *, int type, int a, int b);
void vout_control_PushString(vout_control_t *, int type, const char *string);
void vout_control_Wake(vout_control_t *);
void vout_control_Hold(vout_control_t *);
void vout_control_Release(vout_control_t *);

/* control inside of the vout thread */
int vout_control_Pop(vout_control_t *, vout_control_cmd_t *, vlc_tick_t deadline);
void vout_control_Dead(vout_control_t *);

#endif
