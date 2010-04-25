/*****************************************************************************
 * control.h : vout internal control
 *****************************************************************************
 * Copyright (C) 2009-2010 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#if defined(__PLUGIN__) || defined(__BUILTIN__) || !defined(__LIBVLC__)
# error This header file can only be included from LibVLC.
#endif

#ifndef _VOUT_INTERNAL_CONTROL_H
#define _VOUT_INTERNAL_CONTROL_H

/* */
enum {
#if 0
    VOUT_CONTROL_INIT,
    VOUT_CONTROL_EXIT,

    /* */
    VOUT_CONTROL_START,
    VOUT_CONTROL_STOP,

    /* Controls */
    VOUT_CONTROL_FULLSCREEN,
    VOUT_CONTROL_DISPLAY_FILLED,
    VOUT_CONTROL_ZOOM,
    VOUT_CONTROL_ON_TOP,

    VOUT_CONTROL_SOURCE_ASPECT,
    VOUT_CONTROL_SOURCE_CROP_BORDER,
    VOUT_CONTROL_SOURCE_CROP_RATIO,
    VOUT_CONTROL_SOURCE_CROP_WINDOW,

    /* OSD */
    VOUT_CONTROL_OSD_MESSAGE,
    VOUT_CONTROL_OSD_TEXT,
    VOUT_CONTROL_OSD_SLIDER,
    VOUT_CONTROL_OSD_ICON,
    VOUT_CONTROL_OSD_SUBPICTURE,
#endif
    VOUT_CONTROL_OSD_TITLE,             /* string */
    VOUT_CONTROL_CHANGE_FILTERS,        /* string */

    VOUT_CONTROL_PAUSE,
    VOUT_CONTROL_RESET,
    VOUT_CONTROL_FLUSH,                 /* time */
    VOUT_CONTROL_STEP,                  /* time_ptr */
};

typedef struct {
    int type;

    union {
        bool    boolean;
        mtime_t time;
        mtime_t *time_ptr;
        char    *string;
        struct {
            int a;
            int b;
        } pair;
        struct {
            bool is_on;
            mtime_t date;
        } pause;
        struct {
            int channel;
            char *string;
        } message;
#if 0
        struct {
            int channel;
            char *string;
            text_style_t *style;
            int flags;
            int hmargin;
            int vmargin;
            mtime_t start;
            mtime_t stop;
        } text;
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
        struct {
            int   channel;
            int   type;
            float position;
        } slider;
        struct {
            int channel;
            int icon;
        } icon;
        subpicture_t *subpicture;
#endif
    } u;
} vout_control_cmd_t;

void vout_control_cmd_Init(vout_control_cmd_t *, int type);
void vout_control_cmd_Clean(vout_control_cmd_t *);

typedef struct {
    vlc_mutex_t lock;
    vlc_cond_t  wait_request;
    vlc_cond_t  wait_acknowledge;

    /* */
    bool is_dead;
    bool is_sleeping;
    bool can_sleep;
    bool is_processing;
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
void vout_control_PushTime(vout_control_t *, int type, mtime_t time);
void vout_control_PushMessage(vout_control_t *, int type, int channel, const char *string);
void vout_control_PushPair(vout_control_t *, int type, int a, int b);
void vout_control_PushString(vout_control_t *, int type, const char *string);
void vout_control_Wake(vout_control_t *);

/* control inside of the vout thread */
int vout_control_Pop(vout_control_t *, vout_control_cmd_t *, mtime_t deadline, mtime_t timeout);
void vout_control_Dead(vout_control_t *);

#endif

