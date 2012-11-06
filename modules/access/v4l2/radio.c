/*****************************************************************************
 * radio.c : V4L2 analog radio receiver
 *****************************************************************************
 * Copyright (C) 2012 Rémi Denis-Courmont
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

#include <fcntl.h>

#include <vlc_common.h>
#include <vlc_demux.h>
#include <vlc_fs.h>

#include "v4l2.h"

struct demux_sys_t
{
    int fd;
    vlc_v4l2_ctrl_t *controls;
    mtime_t start;
};

static int RadioControl (demux_t *demux, int query, va_list args)
{
    demux_sys_t *sys = demux->p_sys;

    switch (query)
    {
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_SEEK:
        case DEMUX_CAN_CONTROL_PACE:
            *va_arg (args, bool *) = false;
            break;

        case DEMUX_GET_PTS_DELAY:
            *va_arg (args,int64_t *) = INT64_C(1000)
                * var_InheritInteger (demux, "live-caching");
            break;

        case DEMUX_GET_TIME:
            *va_arg (args, int64_t *) = mdate () - sys->start;
            break;

        /* TODO implement others */
        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

int RadioOpen (vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;

    /* Parse MRL */
    size_t pathlen = strcspn (demux->psz_location, ":;");
    char *path = (pathlen != 0) ? strndup (demux->psz_location, pathlen)
                              : var_InheritString (obj, CFG_PREFIX"radio-dev");
    if (unlikely(path == NULL))
        return VLC_ENOMEM;
    if (demux->psz_location[pathlen] != '\0')
        var_LocationParse (obj, demux->psz_location + pathlen + 1, CFG_PREFIX);

    /* Open device */
    uint32_t caps;
    int fd = OpenDevice (obj, path, &caps);
    free (path);
    if (fd == -1)
        return VLC_EGENERIC;
    if (!(caps & V4L2_CAP_TUNER))
    {
        msg_Err (obj, "not a radio tuner device");
        goto error;
    }

    if (SetupTuner (obj, fd, 0))
        goto error;

    demux_sys_t *sys = malloc (sizeof (*sys));
    if (unlikely(sys == NULL))
        goto error;

    sys->fd = fd;
    sys->controls = ControlsInit (VLC_OBJECT(demux), fd);
    sys->start = mdate ();

    demux->p_sys = sys;
    demux->pf_demux = NULL;
    demux->pf_control = RadioControl;
    demux->info.i_update = 0;
    demux->info.i_title = 0;
    demux->info.i_seekpoint = 0;
    return VLC_SUCCESS;

error:
    v4l2_close (fd);
    return VLC_EGENERIC;
}

void RadioClose (vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;
    demux_sys_t *sys = demux->p_sys;

    ControlsDeinit (obj, sys->controls);
    v4l2_close (sys->fd);
    free (sys);
}
