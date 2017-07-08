/*****************************************************************************
 * noseek.c:
 *****************************************************************************
 * Copyright (C) 2017 Rémi Denis-Courmont
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>

static int Demux(demux_t *demux)
{
    return demux_Demux(demux->p_next);
}

static int Control(demux_t *demux, int query, va_list args)
{
    switch (query)
    {
        case DEMUX_CAN_SEEK:
            *va_arg(args, bool *) = false;
            break;

        case DEMUX_SET_POSITION:
        case DEMUX_SET_TIME:
        case DEMUX_GET_TITLE_INFO:
            return VLC_EGENERIC;

        case DEMUX_TEST_AND_CLEAR_FLAGS:
        {
            unsigned *restrict pf = va_arg(args, unsigned *);

            if (demux_Control(demux->p_next, DEMUX_TEST_AND_CLEAR_FLAGS, pf))
            {
                unsigned update = demux->info.i_update & *pf;
                demux->info.i_update &= ~*pf;
                *pf = update;
            }
            *pf &= ~(INPUT_UPDATE_TITLE|INPUT_UPDATE_SEEKPOINT|
                     INPUT_UPDATE_TITLE_LIST);
            break;
        }

        default:
            return demux_vaControl(demux->p_next, query, args);
    }

    return VLC_SUCCESS;
}

static int Open(vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;

    demux->pf_demux = Demux;
    demux->pf_control = Control;
    return VLC_SUCCESS;
}

vlc_module_begin ()
    set_description(N_("Seek prevention demux filter"))
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_STREAM_FILTER)
    set_capability("demux_filter", 0)
    set_callbacks(Open, NULL)
vlc_module_end()
