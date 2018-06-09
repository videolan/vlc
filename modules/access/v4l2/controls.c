/*****************************************************************************
 * controls.c : Video4Linux2 device controls for vlc
 *****************************************************************************
 * Copyright (C) 2002-2011 VLC authors and VideoLAN
 *
 * Authors: Benjamin Pracht <bigben at videolan dot org>
 *          Richard Hosking <richard at hovis dot net>
 *          Antoine Cellerier <dionoea at videolan d.t org>
 *          Dennis Lou <dlou99 at yahoo dot com>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <vlc_common.h>

#include "v4l2.h"

typedef struct vlc_v4l2_ctrl_name
{
    const char name[28];
    uint32_t cid;
} vlc_v4l2_ctrl_name_t;

/* NOTE: must be sorted by ID */
static const vlc_v4l2_ctrl_name_t controls[] =
{
    { "brightness", V4L2_CID_BRIGHTNESS },
    { "contrast", V4L2_CID_CONTRAST },
    { "saturation", V4L2_CID_SATURATION },
    { "hue", V4L2_CID_HUE },
    { "audio-volume", V4L2_CID_AUDIO_VOLUME },
    { "audio-balance", V4L2_CID_AUDIO_BALANCE },
    { "audio-bass", V4L2_CID_AUDIO_BASS },
    { "audio-treble", V4L2_CID_AUDIO_TREBLE },
    { "audio-mute", V4L2_CID_AUDIO_MUTE },
    { "audio-loudness", V4L2_CID_AUDIO_LOUDNESS },
    { "auto-white-balance", V4L2_CID_AUTO_WHITE_BALANCE },
    { "do-white-balance", V4L2_CID_DO_WHITE_BALANCE },
    { "red-balance", V4L2_CID_RED_BALANCE },
    { "blue-balance", V4L2_CID_BLUE_BALANCE },
    { "gamma", V4L2_CID_GAMMA },
    { "autogain", V4L2_CID_AUTOGAIN },
    { "gain", V4L2_CID_GAIN },
    { "hflip", V4L2_CID_HFLIP },
    { "vflip", V4L2_CID_VFLIP },
    { "power-line-frequency", V4L2_CID_POWER_LINE_FREQUENCY },
    { "hue-auto", V4L2_CID_HUE_AUTO },
    { "white-balance-temperature", V4L2_CID_WHITE_BALANCE_TEMPERATURE },
    { "sharpness", V4L2_CID_SHARPNESS },
    { "backlight-compensation", V4L2_CID_BACKLIGHT_COMPENSATION },
    { "chroma-gain-auto", V4L2_CID_CHROMA_AGC },
    { "color-killer", V4L2_CID_COLOR_KILLER },
    { "color-effect", V4L2_CID_COLORFX },
    { "rotate", V4L2_CID_ROTATE },
    { "bg-color", V4L2_CID_BG_COLOR }, // NOTE: output only
    { "chroma-gain", V4L2_CID_CHROMA_GAIN },
    { "brightness-auto", V4L2_CID_AUTOBRIGHTNESS },
    { "band-stop-filter", V4L2_CID_BAND_STOP_FILTER },

    { "illuminators-1", V4L2_CID_ILLUMINATORS_1 }, // NOTE: don't care?
    { "illuminators-2", V4L2_CID_ILLUMINATORS_2 },
#define CTRL_CID_KNOWN(cid) \
    ((((uint32_t)cid) - V4L2_CID_BRIGHTNESS) \
        <= (V4L2_CID_BAND_STOP_FILTER - V4L2_CID_BRIGHTNESS))
};

struct vlc_v4l2_ctrl
{
    int                   fd;
    uint32_t              id;
    uint8_t               type;
    char                  name[32];
    int32_t               default_value;
    struct vlc_v4l2_ctrl *next;
};

static int ControlSet (const vlc_v4l2_ctrl_t *c, int_fast32_t value)
{
    struct v4l2_control ctrl = {
        .id = c->id,
        .value = value,
    };
    if (v4l2_ioctl (c->fd, VIDIOC_S_CTRL, &ctrl) < 0)
        return -1;
    return 0;
}

static int ControlSet64 (const vlc_v4l2_ctrl_t *c, int64_t value)
{
    struct v4l2_ext_control ext_ctrl = {
        .id = c->id,
        .size = 0,
    };
    ext_ctrl.value64 = value;
    struct v4l2_ext_controls ext_ctrls = {
        .ctrl_class = V4L2_CTRL_ID2CLASS(c->id),
        .count = 1,
        .error_idx = 0,
        .controls = &ext_ctrl,
    };

    if (v4l2_ioctl (c->fd, VIDIOC_S_EXT_CTRLS, &ext_ctrls) < 0)
        return -1;
    return 0;
}

static int ControlSetStr (const vlc_v4l2_ctrl_t *c, const char *restrict value)
{
    struct v4l2_ext_control ext_ctrl = {
        .id = c->id,
        .size = strlen (value) + 1,
    };
    ext_ctrl.string = (char *)value;
    struct v4l2_ext_controls ext_ctrls = {
        .ctrl_class = V4L2_CTRL_ID2CLASS(c->id),
        .count = 1,
        .error_idx = 0,
        .controls = &ext_ctrl,
    };

    if (v4l2_ioctl (c->fd, VIDIOC_S_EXT_CTRLS, &ext_ctrls) < 0)
        return -1;
    return 0;
}

static int ControlSetCallback (vlc_object_t *obj, const char *var,
                               vlc_value_t old, vlc_value_t cur, void *data)
{
    const vlc_v4l2_ctrl_t *ctrl = data;
    int ret;

    switch (ctrl->type)
    {
        case V4L2_CTRL_TYPE_INTEGER:
        case V4L2_CTRL_TYPE_MENU:
        case V4L2_CTRL_TYPE_BITMASK:
        case V4L2_CTRL_TYPE_INTEGER_MENU:
            ret = ControlSet (ctrl, cur.i_int);
            break;
        case V4L2_CTRL_TYPE_BOOLEAN:
            ret = ControlSet (ctrl, cur.b_bool);
            break;
        case V4L2_CTRL_TYPE_BUTTON:
            ret = ControlSet (ctrl, 0);
            break;
        case V4L2_CTRL_TYPE_INTEGER64:
            ret = ControlSet64 (ctrl, cur.i_int);
            break;
        case V4L2_CTRL_TYPE_STRING:
            ret = ControlSetStr (ctrl, cur.psz_string);
            break;
        default:
            vlc_assert_unreachable ();
    }

    if (ret)
    {
        msg_Err (obj, "cannot set control %s: %s", var, vlc_strerror_c(errno));
        return VLC_EGENERIC;
    }
    (void) old;
    return VLC_SUCCESS;
}

static void ControlsReset (vlc_object_t *obj, vlc_v4l2_ctrl_t *list)
{
    while (list != NULL)
    {
        switch (list->type)
        {
            case V4L2_CTRL_TYPE_INTEGER:
            case V4L2_CTRL_TYPE_MENU:
            case V4L2_CTRL_TYPE_INTEGER_MENU:
                var_SetInteger (obj, list->name, list->default_value);
                break;
            case V4L2_CTRL_TYPE_BOOLEAN:
                var_SetBool (obj, list->name, list->default_value);
                break;
            default:;
        }
        list = list->next;
    }
}

static int ControlsResetCallback (vlc_object_t *obj, const char *var,
                                  vlc_value_t old, vlc_value_t cur, void *data)
{
    ControlsReset (obj, data);
    (void) var; (void) old; (void) cur;
    return VLC_SUCCESS;
}

static void ControlsSetFromString (vlc_object_t *obj,
                                   const vlc_v4l2_ctrl_t *list)
{
    char *buf = var_InheritString (obj, CFG_PREFIX"set-ctrls");
    if (buf == NULL)
        return;

    char *p = buf;
    if (*p == '{')
        p++;

    char *end = strchr (p, '}');
    if (end != NULL)
        *end = '\0';
next:
    while (p != NULL && *p)
    {
        const char *name, *value;

        p += strspn (p, ", ");
        name = p;
        end = strchr (p, ',');
        if (end != NULL)
            *(end++) = '\0';
        p = end; /* next name/value pair */

        end = strchr (name, '=');
        if (end == NULL)
        {
            /* TODO? support button controls that way? */
            msg_Err (obj, "syntax error in \"%s\": missing '='", name);
            continue;
        }
        *(end++) = '\0';
        value = end;

        for (const vlc_v4l2_ctrl_t *c = list; c != NULL; c = c->next)
            if (!strcasecmp (name, c->name))
                switch (c->type)
                {
                    case V4L2_CTRL_TYPE_INTEGER:
                    case V4L2_CTRL_TYPE_BOOLEAN:
                    case V4L2_CTRL_TYPE_MENU:
                    case V4L2_CTRL_TYPE_INTEGER_MENU:
                    {
                        long val = strtol (value, &end, 0);
                        if (*end)
                        {
                            msg_Err (obj, "syntax error in \"%s\": "
                                     " not an integer", value);
                            goto next;
                        }
                        ControlSet (c, val);
                        break;
                    }

                    case V4L2_CTRL_TYPE_INTEGER64:
                    {
                        long long val = strtoll (value, &end, 0);
                        if (*end)
                        {
                            msg_Err (obj, "syntax error in \"%s\": "
                                     " not an integer", value);
                            goto next;
                        }
                        ControlSet64 (c, val);
                        break;
                    }

                    case V4L2_CTRL_TYPE_STRING:
                        ControlSetStr (c, value);
                        break;

                    case V4L2_CTRL_TYPE_BITMASK:
                    {
                        unsigned long val = strtoul (value, &end, 0);
                        if (*end)
                        {
                            msg_Err (obj, "syntax error in \"%s\": "
                                     " not an integer", value);
                            goto next;
                        }
                        ControlSet (c, val);
                        break;
                    }

                    default:
                        msg_Err (obj, "setting \"%s\" not supported", name);
                        goto next;
                }

        msg_Err (obj, "control \"%s\" not available", name);
    }
    free (buf);
}

static int cidcmp (const void *a, const void *b)
{
    const uint32_t *id = a;
    const vlc_v4l2_ctrl_name_t *name = b;

    return (int32_t)(*id - name->cid);
}

/**
 * Creates a VLC-V4L2 control structure:
 * In particular, determines a name suitable for a VLC object variable.
 * \param query V4L2 control query structure [IN]
 * \return NULL on error
 */
static vlc_v4l2_ctrl_t *ControlCreate (int fd,
                                       const struct v4l2_queryctrl *query)
{
    vlc_v4l2_ctrl_t *ctrl = malloc (sizeof (*ctrl));
    if (unlikely(ctrl == NULL))
        return NULL;

    ctrl->fd = fd;
    ctrl->id = query->id;
    ctrl->type = query->type;

    /* Search for a well-known control */
    const vlc_v4l2_ctrl_name_t *known;
    known = bsearch (&query->id, controls, sizeof (controls) / sizeof (*known),
                     sizeof (*known), cidcmp);
    if (known != NULL)
        strcpy (ctrl->name, known->name);
    else
    /* Fallback to automatically-generated control name */
    {
        size_t i;
        for (i = 0; query->name[i]; i++)
        {
            unsigned char c = query->name[i];
            if (c == ' ' || c == ',')
                c = '_';
            if (c < 128)
                c = tolower (c);
            ctrl->name[i] = c;
        }
        ctrl->name[i] = '\0';
    }

    ctrl->default_value = query->default_value;
    return ctrl;
}


#define CTRL_FLAGS_IGNORE \
    (V4L2_CTRL_FLAG_DISABLED /* not implemented at all */ \
    |V4L2_CTRL_FLAG_READ_ONLY /* value is constant */ \
    |V4L2_CTRL_FLAG_VOLATILE /* value is (variable but) read-only */)

static vlc_v4l2_ctrl_t *ControlAddInteger (vlc_object_t *obj, int fd,
                                           const struct v4l2_queryctrl *query)
{
    msg_Dbg (obj, " integer  %s (%08"PRIX32")", query->name, query->id);
    if (query->flags & CTRL_FLAGS_IGNORE)
        return NULL;

    vlc_v4l2_ctrl_t *c = ControlCreate (fd, query);
    if (unlikely(c == NULL))
        return NULL;

    if (var_Create (obj, c->name, VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND))
    {
        free (c);
        return NULL;
    }

    vlc_value_t val;
    struct v4l2_control ctrl = { .id = query->id };

    if (v4l2_ioctl (fd, VIDIOC_G_CTRL, &ctrl) >= 0)
    {
        msg_Dbg (obj, "  current: %3"PRId32", default: %3"PRId32,
                 ctrl.value, query->default_value);
        val.i_int = ctrl.value;
        var_Change(obj, c->name, VLC_VAR_SETVALUE, val);
    }
    var_Change (obj, c->name, VLC_VAR_SETMINMAX,
        (vlc_value_t){ .i_int = query->minimum },
        (vlc_value_t){ .i_int = query->maximum } );
    if (query->step != 1)
    {
        val.i_int = query->step;
        var_Change(obj, c->name, VLC_VAR_SETSTEP, val);
    }
    return c;
}

static vlc_v4l2_ctrl_t *ControlAddBoolean (vlc_object_t *obj, int fd,
                                           const struct v4l2_queryctrl *query)
{
    msg_Dbg (obj, " boolean  %s (%08"PRIX32")", query->name, query->id);
    if (query->flags & CTRL_FLAGS_IGNORE)
        return NULL;

    vlc_v4l2_ctrl_t *c = ControlCreate (fd, query);
    if (unlikely(c == NULL))
        return NULL;

    if (var_Create (obj, c->name, VLC_VAR_BOOL | VLC_VAR_ISCOMMAND))
    {
        free (c);
        return NULL;
    }

    vlc_value_t val;
    struct v4l2_control ctrl = { .id = query->id };

    if (v4l2_ioctl (fd, VIDIOC_G_CTRL, &ctrl) >= 0)
    {
        msg_Dbg (obj, "  current: %s, default: %s",
                 ctrl.value ? " true" : "false",
                 query->default_value ? " true" : "false");
        val.b_bool = ctrl.value;
        var_Change(obj, c->name, VLC_VAR_SETVALUE, val);
    }
    return c;
}

static vlc_v4l2_ctrl_t *ControlAddMenu (vlc_object_t *obj, int fd,
                                        const struct v4l2_queryctrl *query)
{
    msg_Dbg (obj, " menu     %s (%08"PRIX32")", query->name, query->id);
    if (query->flags & CTRL_FLAGS_IGNORE)
        return NULL;

    vlc_v4l2_ctrl_t *c = ControlCreate (fd, query);
    if (unlikely(c == NULL))
        return NULL;

    if (var_Create (obj, c->name, VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND))
    {
        free (c);
        return NULL;
    }

    vlc_value_t val;
    struct v4l2_control ctrl = { .id = query->id };

    if (v4l2_ioctl (fd, VIDIOC_G_CTRL, &ctrl) >= 0)
    {
        msg_Dbg (obj, "  current: %"PRId32", default: %"PRId32,
                 ctrl.value, query->default_value);
        val.i_int = ctrl.value;
        var_Change(obj, c->name, VLC_VAR_SETVALUE, val);
    }
    var_Change (obj, c->name, VLC_VAR_SETMINMAX,
        (vlc_value_t){ .i_int = query->minimum },
        (vlc_value_t){ .i_int = query->maximum } );

    /* Import menu choices */
    for (uint_fast32_t idx = query->minimum;
         idx <= (uint_fast32_t)query->maximum;
         idx++)
    {
        struct v4l2_querymenu menu = { .id = query->id, .index = idx };

        if (v4l2_ioctl (fd, VIDIOC_QUERYMENU, &menu) < 0)
            continue;
        msg_Dbg (obj, "  choice %"PRIu32") %s", menu.index, menu.name);

        val.i_int = menu.index;
        var_Change(obj, c->name, VLC_VAR_ADDCHOICE, val,
                   (const char *)menu.name);
    }
    return c;
}

static vlc_v4l2_ctrl_t *ControlAddButton (vlc_object_t *obj, int fd,
                                          const struct v4l2_queryctrl *query)
{
    msg_Dbg (obj, " button   %s (%08"PRIX32")", query->name, query->id);
    if (query->flags & CTRL_FLAGS_IGNORE)
        return NULL;

    vlc_v4l2_ctrl_t *c = ControlCreate (fd, query);
    if (unlikely(c == NULL))
        return NULL;

    if (var_Create (obj, c->name, VLC_VAR_VOID | VLC_VAR_ISCOMMAND))
    {
        free (c);
        return NULL;
    }
    return c;
}

static vlc_v4l2_ctrl_t *ControlAddInteger64 (vlc_object_t *obj, int fd,
                                            const struct v4l2_queryctrl *query)
{
    msg_Dbg (obj, " 64-bits  %s (%08"PRIX32")", query->name, query->id);
    if (query->flags & CTRL_FLAGS_IGNORE)
        return NULL;

    vlc_v4l2_ctrl_t *c = ControlCreate (fd, query);
    if (unlikely(c == NULL))
        return NULL;

    if (var_Create (obj, c->name, VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND))
    {
        free (c);
        return NULL;
    }

    struct v4l2_ext_control ext_ctrl = { .id = c->id, .size = 0, };
    struct v4l2_ext_controls ext_ctrls = {
        .ctrl_class = V4L2_CTRL_ID2CLASS(c->id),
        .count = 1,
        .error_idx = 0,
        .controls = &ext_ctrl,
    };

    if (v4l2_ioctl (c->fd, VIDIOC_G_EXT_CTRLS, &ext_ctrls) >= 0)
    {
        vlc_value_t val = { .i_int = ext_ctrl.value64 };

        msg_Dbg (obj, "  current: %"PRId64, val.i_int);
        var_Change(obj, c->name, VLC_VAR_SETVALUE, val);
    }

    return c;
}

static vlc_v4l2_ctrl_t *ControlAddClass (vlc_object_t *obj, int fd,
                                         const struct v4l2_queryctrl *query)
{
    msg_Dbg (obj, "control class %s:", query->name);
    (void) fd;
    return NULL;
}

static vlc_v4l2_ctrl_t *ControlAddString (vlc_object_t *obj, int fd,
                                          const struct v4l2_queryctrl *query)
{
    msg_Dbg (obj, " string   %s (%08"PRIX32")", query->name, query->id);
    if ((query->flags & CTRL_FLAGS_IGNORE) || query->maximum > 65535)
        return NULL;

    vlc_v4l2_ctrl_t *c = ControlCreate (fd, query);
    if (unlikely(c == NULL))
        return NULL;

    if (var_Create (obj, c->name, VLC_VAR_STRING | VLC_VAR_ISCOMMAND))
    {
        free (c);
        return NULL;
    }

    /* Get current value */
    char *buf = malloc (query->maximum + 1);
    if (likely(buf != NULL))
    {
        struct v4l2_ext_control ext_ctrl = {
            .id = c->id,
            .size = query->maximum + 1,
        };
        ext_ctrl.string = buf;
        struct v4l2_ext_controls ext_ctrls = {
            .ctrl_class = V4L2_CTRL_ID2CLASS(c->id),
            .count = 1,
            .error_idx = 0,
            .controls = &ext_ctrl,
        };

        if (v4l2_ioctl (c->fd, VIDIOC_G_EXT_CTRLS, &ext_ctrls) >= 0)
        {
            vlc_value_t val = { .psz_string = buf };

            msg_Dbg (obj, "  current: \"%s\"", buf);
            var_Change(obj, c->name, VLC_VAR_SETVALUE, val);
        }
        free (buf);
    }

    return c;
}

static vlc_v4l2_ctrl_t *ControlAddBitMask (vlc_object_t *obj, int fd,
                                           const struct v4l2_queryctrl *query)
{
    msg_Dbg (obj, " bit mask %s (%08"PRIX32")", query->name, query->id);
    if (query->flags & CTRL_FLAGS_IGNORE)
        return NULL;

    vlc_v4l2_ctrl_t *c = ControlCreate (fd, query);
    if (unlikely(c == NULL))
        return NULL;

    if (var_Create (obj, c->name, VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND))
    {
        free (c);
        return NULL;
    }

    vlc_value_t val;
    struct v4l2_control ctrl = { .id = query->id };

    if (v4l2_ioctl (fd, VIDIOC_G_CTRL, &ctrl) >= 0)
    {
        msg_Dbg (obj, "  current: 0x%08"PRIX32", default: 0x%08"PRIX32,
                 ctrl.value, query->default_value);
        val.i_int = ctrl.value;
        var_Change(obj, c->name, VLC_VAR_SETVALUE, val);
    }
    var_Change (obj, c->name, VLC_VAR_SETMINMAX,
        (vlc_value_t){ .i_int = 0 },
        (vlc_value_t){ .i_int = (uint32_t)query->maximum } );
    return c;
}

static vlc_v4l2_ctrl_t *ControlAddIntMenu (vlc_object_t *obj, int fd,
                                           const struct v4l2_queryctrl *query)
{
    msg_Dbg (obj, " int menu %s (%08"PRIX32")", query->name, query->id);
    if (query->flags & CTRL_FLAGS_IGNORE)
        return NULL;

    vlc_v4l2_ctrl_t *c = ControlCreate (fd, query);
    if (unlikely(c == NULL))
        return NULL;

    if (var_Create (obj, c->name, VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND))
    {
        free (c);
        return NULL;
    }

    vlc_value_t val;
    struct v4l2_control ctrl = { .id = query->id };

    if (v4l2_ioctl (fd, VIDIOC_G_CTRL, &ctrl) >= 0)
    {
        msg_Dbg (obj, "  current: %"PRId32", default: %"PRId32,
                 ctrl.value, query->default_value);
        val.i_int = ctrl.value;
        var_Change(obj, c->name, VLC_VAR_SETVALUE, &val);
    }
    var_Change (obj, c->name, VLC_VAR_SETMINMAX,
        (vlc_value_t){ .i_int = query->minimum },
        (vlc_value_t){ .i_int = query->maximum } );

    /* Import menu choices */
    for (uint_fast32_t idx = query->minimum;
         idx <= (uint_fast32_t)query->maximum;
         idx++)
    {
        struct v4l2_querymenu menu = { .id = query->id, .index = idx };
        char name[sizeof ("-9223372036854775808")];

        if (v4l2_ioctl (fd, VIDIOC_QUERYMENU, &menu) < 0)
            continue;
        msg_Dbg (obj, "  choice %"PRIu32") %"PRId64, menu.index,
                 (uint64_t)menu.value);

        val.i_int = menu.index;
        sprintf(name, "%"PRId64, (int64_t)menu.value);
        var_Change(obj, c->name, VLC_VAR_ADDCHOICE, val,
                   (const char *)name);
    }
    return c;
}

static vlc_v4l2_ctrl_t *ControlAddUnknown (vlc_object_t *obj, int fd,
                                           const struct v4l2_queryctrl *query)
{
    msg_Dbg (obj, " unknown %s (%08"PRIX32")", query->name, query->id);
    msg_Warn (obj, "  unknown control type %u", (unsigned)query->type);
    (void) fd;
    return NULL;
}

typedef vlc_v4l2_ctrl_t *(*ctrl_type_cb) (vlc_object_t *, int,
                                          const struct v4l2_queryctrl *);

/**
 * Lists all user-class v4l2 controls, sets them to the user specified
 * value and create the relevant variables to enable run-time changes.
 */
vlc_v4l2_ctrl_t *ControlsInit (vlc_object_t *obj, int fd)
{
    /* A list of controls that can be modified at run-time is stored in the
     * "controls" variable. The V4L2 controls dialog can be built from this. */
    var_Create (obj, "controls", VLC_VAR_INTEGER);

    static const ctrl_type_cb handlers[] =
    {
        [V4L2_CTRL_TYPE_INTEGER] = ControlAddInteger,
        [V4L2_CTRL_TYPE_BOOLEAN] = ControlAddBoolean,
        [V4L2_CTRL_TYPE_MENU] = ControlAddMenu,
        [V4L2_CTRL_TYPE_BUTTON] = ControlAddButton,
        [V4L2_CTRL_TYPE_INTEGER64] = ControlAddInteger64,
        [V4L2_CTRL_TYPE_CTRL_CLASS] = ControlAddClass,
        [V4L2_CTRL_TYPE_STRING] = ControlAddString,
        [V4L2_CTRL_TYPE_BITMASK] = ControlAddBitMask,
        [V4L2_CTRL_TYPE_INTEGER_MENU] = ControlAddIntMenu,
    };

    vlc_v4l2_ctrl_t *list = NULL;
    struct v4l2_queryctrl query;

    query.id = V4L2_CTRL_FLAG_NEXT_CTRL;
    while (v4l2_ioctl (fd, VIDIOC_QUERYCTRL, &query) >= 0)
    {
        ctrl_type_cb handler = NULL;
        if (query.type < (sizeof (handlers) / sizeof (handlers[0])))
            handler = handlers[query.type];
        if (handler == NULL)
            handler = ControlAddUnknown;

        vlc_v4l2_ctrl_t *c = handler (obj, fd, &query);
        if (c != NULL)
        {
            vlc_value_t val;

            var_AddCallback (obj, c->name, ControlSetCallback, c);
            var_Change(obj, c->name, VLC_VAR_SETTEXT,
                       (const char *)query.name);
            val.i_int = query.id;
            var_Change(obj, "controls", VLC_VAR_ADDCHOICE, val,
                       (const char *)c->name);

            c->next = list;
            list = c;
        }
        query.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
    }

    /* Set well-known controls from VLC configuration */
    for (vlc_v4l2_ctrl_t *ctrl = list; ctrl != NULL; ctrl = ctrl->next)
    {
        if (!CTRL_CID_KNOWN (ctrl->id))
            continue;

        char varname[sizeof (CFG_PREFIX) + sizeof (ctrl->name) - 1];
        sprintf (varname, CFG_PREFIX"%s", ctrl->name);

        int64_t val = var_InheritInteger (obj, varname);
        if (val == -1)
            continue; /* the VLC default value: "do not modify" */
        ControlSet (ctrl, val); /* NOTE: all known are integers or booleans */
    }

    /* Set any control from the VLC configuration control string */
    ControlsSetFromString (obj, list);

    /* Add a control to reset all controls to their default values */
    {
        vlc_value_t val;

        var_Create (obj, "reset", VLC_VAR_VOID | VLC_VAR_ISCOMMAND);
        var_Change(obj, "reset", VLC_VAR_SETTEXT, _("Reset defaults"));
        val.i_int = -1;

        var_Change(obj, "controls", VLC_VAR_ADDCHOICE, val, "reset");
        var_AddCallback (obj, "reset", ControlsResetCallback, list);
    }
    if (var_InheritBool (obj, CFG_PREFIX"controls-reset"))
        ControlsReset (obj, list);

    return list;
}

void ControlsDeinit (vlc_object_t *obj, vlc_v4l2_ctrl_t *list)
{
    var_DelCallback (obj, "reset", ControlsResetCallback, list);
    var_Destroy (obj, "reset");

    while (list != NULL)
    {
        vlc_v4l2_ctrl_t *next = list->next;

        var_DelCallback (obj, list->name, ControlSetCallback, list);
        var_Destroy (obj, list->name);
        free (list);
        list = next;
    }

    var_Destroy (obj, "controls");
}
