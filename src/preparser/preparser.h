// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * preparser.h: internal header for preparser
 *****************************************************************************
 * Copyright © 2025 Videolabs, VideoLAN and VLC authors
 *
 * Authors: Gabriel Lafond Thenaille <gabriel@videolabs.io>
 *****************************************************************************/

#ifndef PREPARSER_INTERNAL_H
#define PREPARSER_INTERNAL_H 1

#include <vlc_common.h>
#include <vlc_preparser.h>
#include <vlc_modules.h>

/**
 * Preparser's operations
 */
struct vlc_preparser_operations {
    /** Called by `vlc_preparser_Push`. */
    struct vlc_preparser_req *(*push)(void *opaque, input_item_t *item,
                                      int type_options,
                                      const struct vlc_preparser_cbs *cbs,
                                      void *cbs_userdata);

    /** Called by `vlc_preparser_GenerateThumbnail`. */
    struct vlc_preparser_req *(*generate_thumbnail)
                                  (void *opaque, input_item_t *item,
                                   const struct vlc_thumbnailer_arg *thumb_arg,
                                   const struct vlc_thumbnailer_cbs *cbs,
                                   void *cbs_userdata);

    /** Called by `vlc_preparser_GenerateThumbnailToFiles`. */
    struct vlc_preparser_req *(*generate_thumbnail_to_files)
                               (void *opaque, input_item_t *item,
                                const struct vlc_thumbnailer_arg *thumb_arg,
                                const struct vlc_thumbnailer_output *outputs,
                                size_t output_count,
                                const struct vlc_thumbnailer_to_files_cbs *cbs,
                                void *cbs_userdata);

    /** Called by `vlc_preparser_Cancel`. */
    size_t (*cancel)(void *opaque, struct vlc_preparser_req *req);

    /** Called by `vlc_preparser_Delete`. */
    void (*delete)(void *opaque);

    /** Called by `vlc_preparser_SetTimeout`. */
    void (*set_timeout)(void *opaque, vlc_tick_t timeout);
};

struct vlc_preparser_t {
    const struct vlc_preparser_operations *ops;
    void *sys;
};

void *vlc_preparser_internal_New(vlc_preparser_t *preparser,
                                 vlc_object_t *parent,
                                 const struct vlc_preparser_cfg *cfg);

void *vlc_preparser_external_New(vlc_preparser_t *preparser,
                                 vlc_object_t *parent,
                                 const struct vlc_preparser_cfg *cfg);

/* Preparser Request */
struct vlc_preparser_req_operations {
    /* Called by `vlc_preparser_req_GetItem`. */
    input_item_t *(*get_item)(struct vlc_preparser_req *req);

    /* Called by `vlc_preparser_req_Release`. */
    void (*release)(struct vlc_preparser_req *req);
};

struct vlc_preparser_req {
    const struct vlc_preparser_req_operations *ops;
};

#endif /* PREPARSER_INTERNAL_H */
