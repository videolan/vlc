/*****************************************************************************
 * postprocessing.c
 *****************************************************************************
 * Copyright (C) 2010 Laurent Aimar
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <assert.h>

#include <vlc_common.h>
#include <vlc_vout.h>

#include "postprocessing.h"

static bool PostProcessIsPresent(const char *filter)
{
    const char  *pp        = "postproc";
    const size_t pp_length = strlen(pp);
    return filter &&
           !strncmp(filter, pp, pp_length) &&
           (filter[pp_length] == '\0' || filter[pp_length] == ':');
}

static int PostProcessCallback(vlc_object_t *object, char const *cmd,
                               vlc_value_t oldval, vlc_value_t newval, void *data)
{
    vout_thread_t *vout = (vout_thread_t *)object;
    VLC_UNUSED(cmd); VLC_UNUSED(oldval); VLC_UNUSED(data);

    static const char *pp = "postproc";

    char *filters = var_GetString(vout, "video-filter");

    if (newval.i_int <= 0) {
        if (PostProcessIsPresent(filters)) {
            strcpy(filters, &filters[strlen(pp)]);
            if (*filters == ':')
                strcpy(filters, &filters[1]);
        }
    } else {
        if (!PostProcessIsPresent(filters)) {
            if (filters) {
                char *tmp = filters;
                if (asprintf(&filters, "%s:%s", pp, tmp) < 0)
                    filters = tmp;
                else
                    free(tmp);
            } else {
                filters = strdup(pp);
            }
        }
    }
    if (newval.i_int > 0)
        var_SetInteger(vout, "postproc-q", newval.i_int);
    if (filters) {
        var_SetString(vout, "video-filter", filters);
        free(filters);
    } else if (newval.i_int > 0) {
        var_TriggerCallback(vout, "video-filter");
    }
    return VLC_SUCCESS;
}
static void PostProcessEnable(vout_thread_t *vout)
{
    vlc_value_t text;
    msg_Dbg(vout, "Post-processing available");
    var_Create(vout, "postprocess", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE);
    text.psz_string = _("Post processing");
    var_Change(vout, "postprocess", VLC_VAR_SETTEXT, &text, NULL);

    for (int i = 0; i <= 6; i++) {
        vlc_value_t val;
        vlc_value_t text;
        char tmp[1+1];

        val.i_int = i;
        snprintf(tmp, sizeof(tmp), "%d", i);
        if (i == 0)
            text.psz_string = _("Disable");
        else
            text.psz_string = tmp;
        var_Change(vout, "postprocess", VLC_VAR_ADDCHOICE, &val, &text);
    }
    var_AddCallback(vout, "postprocess", PostProcessCallback, NULL);

    /* */
    char *filters = var_GetNonEmptyString(vout, "video-filter");
    int postproc_q = 0;
    if (PostProcessIsPresent(filters))
        postproc_q = var_CreateGetInteger(vout, "postproc-q");

    var_SetInteger(vout, "postprocess", postproc_q);

    free(filters);
}
static void PostProcessDisable(vout_thread_t *vout)
{
    msg_Dbg(vout, "Post-processing no more available");
    var_Destroy(vout, "postprocess");
}

void vout_SetPostProcessingState(vout_thread_t *vout, vout_postprocessing_support_t *state, int qtype)
{
    const int postproc_change = (qtype != QTYPE_NONE) - (state->qtype != QTYPE_NONE);
    if (postproc_change == 1)
        PostProcessEnable(vout);
    else if (postproc_change == -1)
        PostProcessDisable(vout);
    if (postproc_change)
        state->qtype = qtype;
}

