/*****************************************************************************
 * preparser.c
 *****************************************************************************
 * Copyright © 2017-2017 VLC authors and VideoLAN
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

#include <assert.h>

#include <vlc_common.h>
#include <vlc_preparser.h>
#include <vlc_modules.h>

#include "preparser.h"

vlc_preparser_t *vlc_preparser_New(vlc_object_t *obj,
                                   const struct vlc_preparser_cfg *cfg )
{
    assert(obj != NULL);
    assert(cfg != NULL);

    vlc_preparser_t *preparser = malloc(sizeof(*preparser));
    if (preparser == NULL) {
        return NULL;
    }
#if defined(HAVE_VLC_PROCESS_SPAWN)
    const bool external_process = cfg->external_process;
#else
    if (cfg->external_process) {
        assert(!cfg->external_process);
        msg_Err(obj, "external preparser requested on unsupported platform");
        return NULL;
    }
    const bool external_process = false;
#endif
    if (external_process) {
        preparser->sys = vlc_preparser_external_New(preparser, obj, cfg);
    } else {
        preparser->sys = vlc_preparser_internal_New(preparser, obj, cfg);
    }
    if (preparser->sys == NULL) {
        free(preparser);
        return NULL;
    }
    return preparser;
}

struct vlc_preparser_req *
vlc_preparser_Push(vlc_preparser_t *preparser, input_item_t *item,
                   int option,
                   const struct vlc_preparser_cbs *cbs, void *cbs_userdata)
{
    assert(preparser != NULL);
    assert(preparser->ops != NULL);
    assert(preparser->ops->push != NULL);
    return preparser->ops->push(preparser->sys, item, option, cbs,
                                cbs_userdata);
}

struct vlc_preparser_req *
vlc_preparser_GenerateThumbnail(vlc_preparser_t *preparser, input_item_t *item,
                                const struct vlc_thumbnailer_arg *thumb_arg,
                                const struct vlc_thumbnailer_cbs *cbs,
                                void *cbs_userdata)
{
    assert(preparser != NULL);
    assert(preparser->ops != NULL);
    assert(preparser->ops->generate_thumbnail != NULL);
    return preparser->ops->generate_thumbnail(preparser->sys, item, thumb_arg,
                                              cbs, cbs_userdata);
}


struct vlc_preparser_req *
vlc_preparser_GenerateThumbnailToFiles(vlc_preparser_t *preparser,
                                input_item_t *item,
                                const struct vlc_thumbnailer_arg *thumb_arg,
                                const struct vlc_thumbnailer_output *outputs,
                                size_t output_count,
                                const struct vlc_thumbnailer_to_files_cbs *cbs,
                                void *cbs_userdata)
{
    assert(preparser != NULL);
    assert(preparser->ops != NULL);
    assert(preparser->ops->generate_thumbnail_to_files != NULL);
    return preparser->ops->generate_thumbnail_to_files(preparser->sys, item,
                                                       thumb_arg, outputs,
                                                       output_count, cbs,
                                                       cbs_userdata);
}

size_t vlc_preparser_Cancel(vlc_preparser_t *preparser,
                            struct vlc_preparser_req *req)
{
    assert(preparser != NULL);
    assert(preparser->ops != NULL);
    assert(preparser->ops->cancel != NULL);
    return preparser->ops->cancel(preparser->sys, req);
}

void vlc_preparser_Delete(vlc_preparser_t *preparser)
{
    assert(preparser != NULL);
    assert(preparser->ops != NULL);
    assert(preparser->ops->delete != NULL);
    preparser->ops->delete(preparser->sys);
    free(preparser);
}

void vlc_preparser_SetTimeout(vlc_preparser_t *preparser, vlc_tick_t timeout)
{
    assert(preparser != NULL);
    assert(preparser->ops != NULL);
    assert(preparser->ops->set_timeout != NULL);
    preparser->ops->set_timeout(preparser->sys, timeout);
}

input_item_t *vlc_preparser_req_GetItem(struct vlc_preparser_req *req)
{
    assert(req != NULL);
    assert(req->ops != NULL);
    assert(req->ops->get_item != NULL);
    return req->ops->get_item(req);
}

void vlc_preparser_req_Release(struct vlc_preparser_req *req)
{
    assert(req != NULL);
    assert(req->ops != NULL);
    assert(req->ops->release != NULL);
    req->ops->release(req);
}
