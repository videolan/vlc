// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * preparser.c: preparse a media and return it's informations
 *****************************************************************************
 * Copyright © 2025 Videolabs, VideoLAN and VLC authors
 *
 * Authors: Gabriel Lafond Thenaille <gabriel@videolabs.io>
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <vlc/vlc.h>
#include <vlc_common.h>
#include <vlc_preparser.h>
#include <vlc_preparser_ipc.h>
#include <vlc_vector.h>
#include <vlc_process.h>
#include <vlc_threads.h>
#include <vlc_fs.h>
#include <vlc_url.h>
#include <vlc_tls.h>

#include "preparser/cmdline.h"

#include "../lib/libvlc_internal.h"

struct preparser {
    vlc_sem_t sem;
    struct vlc_preparser_msg res_msg;
    struct vlc_preparser_msg req_msg;
    struct vlc_preparser_msg_serdes *serdes;
    bool daemon;
    int status;

    struct vlc_tls *tls_in;
    struct vlc_tls *tls_out;
};


/****************************************************************************
 * parse Callbacks
 *****************************************************************************/

static void parse_OnEnded(struct vlc_preparser_req *req, int status, void *userdata)
{
    struct preparser *pp = userdata;
    assert(pp != NULL);
    assert(pp->serdes != NULL);
    assert(pp->res_msg.type == VLC_PREPARSER_MSG_TYPE_RES);
    assert(pp->res_msg.req_type == VLC_PREPARSER_MSG_REQ_TYPE_PARSE);
    assert(req != NULL);
    input_item_t *item = vlc_preparser_req_GetItem(req);
    assert(item != NULL && item == pp->res_msg.res.item);

    pp->res_msg.res.status = status;
    pp->status = status;
    vlc_preparser_msg_serdes_Serialize(pp->serdes, &pp->res_msg, pp->tls_out);
    vlc_sem_post(&pp->sem);
}

static void parse_OnAttachmentsAdded(struct vlc_preparser_req *req,
                                     input_attachment_t *const *array,
                                     size_t count, void *userdata)
{
    struct preparser *pp = userdata;
    assert(pp != NULL);
    assert(pp->res_msg.type == VLC_PREPARSER_MSG_TYPE_RES);
    assert(pp->res_msg.req_type == VLC_PREPARSER_MSG_REQ_TYPE_PARSE);
    assert(req != NULL);
    input_item_t *item = vlc_preparser_req_GetItem(req);
    assert(item != NULL && item == pp->res_msg.res.item);
    assert(array != NULL);

    for (size_t i = 0; i < count; i++) {
        assert(array[i] != NULL);
        input_attachment_t *a = vlc_input_attachment_Hold(array[i]);
        vlc_vector_push(&pp->res_msg.res.attachments, a);
    }
}

static void parse_OnSubtreeAdded(struct vlc_preparser_req *req,
                                 input_item_node_t *subtree,
                                 void *userdata)
{
    struct preparser *pp = userdata;
    assert(pp != NULL);
    assert(pp->res_msg.type == VLC_PREPARSER_MSG_TYPE_RES);
    assert(pp->res_msg.req_type == VLC_PREPARSER_MSG_REQ_TYPE_PARSE);
    assert(req != NULL);
    input_item_t *item = vlc_preparser_req_GetItem(req);
    assert(item != NULL && item == pp->res_msg.res.item);

    pp->res_msg.res.subtree = subtree;
}

/****************************************************************************
 * thumbnailer Callback
 *****************************************************************************/

static void thumbnailer_OnEnded(struct vlc_preparser_req *req, int status,
                                picture_t* thumbnail, void *userdata)
{
    struct preparser *pp = userdata;
    assert(pp != NULL);
    assert(pp->serdes != NULL);
    assert(pp->res_msg.type == VLC_PREPARSER_MSG_TYPE_RES);
    assert(pp->res_msg.req_type == VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL);
    assert(req != NULL);
    input_item_t *item = vlc_preparser_req_GetItem(req);
    assert(item != NULL && item == pp->res_msg.res.item);

    pp->res_msg.res.status = status;
    if (status == 0) {
        assert(thumbnail != NULL);
        pp->res_msg.res.pic = picture_Hold(thumbnail);
    }

    vlc_preparser_msg_serdes_Serialize(pp->serdes, &pp->res_msg, NULL);
    vlc_sem_post(&pp->sem);
}

/****************************************************************************
 * thumbnailer_to_files Callback
 *****************************************************************************/

static void thumbnailer_to_files_OnEnded(struct vlc_preparser_req *req,
                                         int status, const bool *result_array,
                                         size_t result_count, void *userdata)
{
    struct preparser *pp = userdata;
    assert(pp != NULL);
    assert(pp->serdes != NULL);
    assert(pp->res_msg.type == VLC_PREPARSER_MSG_TYPE_RES);
    assert(pp->res_msg.req_type == VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL_TO_FILES);
    assert(req != NULL);
    input_item_t *item = vlc_preparser_req_GetItem(req);
    assert(item != NULL && item == pp->res_msg.res.item);

    pp->res_msg.res.status = status;
    vlc_vector_init(&pp->res_msg.res.result);

    if (result_count != 0) {
        assert(result_array != NULL);
        vlc_vector_push_all(&pp->res_msg.res.result, result_array, result_count);
    }

    vlc_preparser_msg_serdes_Serialize(pp->serdes, &pp->res_msg, NULL);
    vlc_sem_post(&pp->sem);
}

/****************************************************************************
 * Preparser engine
 *****************************************************************************/

static struct vlc_preparser_req *
preparser_req_PreparsePush(vlc_preparser_t *preparser,
                          struct preparser *pp)
{
    assert(pp->req_msg.req_type == VLC_PREPARSER_MSG_REQ_TYPE_PARSE);

    static const struct vlc_preparser_cbs cbs = {
        .on_ended = parse_OnEnded,
        .on_attachments_added = parse_OnAttachmentsAdded,
        .on_subtree_added = parse_OnSubtreeAdded,
    };

    return vlc_preparser_Push(preparser, pp->res_msg.res.item,
                              pp->req_msg.req.options, &cbs, pp);
}

static struct vlc_preparser_req *
preparser_req_PreparseThumbnail(vlc_preparser_t *preparser,
                               struct preparser *pp)
{
    assert(pp->req_msg.req_type == VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL);

    static const struct vlc_thumbnailer_cbs cbs = {
        .on_ended = thumbnailer_OnEnded,
    };

    return vlc_preparser_GenerateThumbnail(preparser, pp->res_msg.res.item,
                                           &pp->req_msg.req.arg,
                                           &cbs, pp);
}

static struct vlc_preparser_req *
preparser_req_PreparseThumbnailToFiles(vlc_preparser_t *preparser,
                                       struct preparser *pp)
{
    assert(pp->req_msg.req_type == VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL_TO_FILES);

    static const struct vlc_thumbnailer_to_files_cbs cbs = {
        .on_ended = thumbnailer_to_files_OnEnded,
    };

    return vlc_preparser_GenerateThumbnailToFiles(preparser,
                                                  pp->res_msg.res.item,
                                                  &pp->req_msg.req.arg,
                                                  pp->req_msg.req.outputs.data,
                                                  pp->req_msg.req.outputs.size,
                                                  &cbs, pp);
}

static int
preparser_req_Preparse(vlc_preparser_t *preparser, struct preparser *pp)
{
    assert(preparser != NULL);
    assert(pp != NULL);
    assert(pp->req_msg.type == VLC_PREPARSER_MSG_TYPE_REQ);
    assert(pp->req_msg.req.uri != NULL);

    int ret = VLC_EGENERIC;
    vlc_preparser_msg_Init(&pp->res_msg, VLC_PREPARSER_MSG_TYPE_RES,
                           pp->req_msg.req_type);
    pp->res_msg.res.item = input_item_New(pp->req_msg.req.uri, NULL);
    if (pp->res_msg.res.item == NULL) {
        goto end;
    }

    struct vlc_preparser_req *req = NULL;
    switch (pp->req_msg.req_type) {
        case VLC_PREPARSER_MSG_REQ_TYPE_PARSE:
            req = preparser_req_PreparsePush(preparser, pp);
            break;
        case VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL:
            req = preparser_req_PreparseThumbnail(preparser, pp);
            break;
        case VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL_TO_FILES:
            req = preparser_req_PreparseThumbnailToFiles(preparser, pp);
            break;
        default:
            goto end;
    }

    if (req == NULL) {
        if (pp->serdes != NULL) {
            ret = vlc_preparser_msg_serdes_Serialize(pp->serdes, &pp->res_msg,
                                                     NULL);
        }
    } else {
        ret = VLC_SUCCESS;
        vlc_preparser_req_Release(req);
        vlc_sem_wait(&pp->sem);
    }

end:
    vlc_preparser_msg_Clean(&pp->res_msg);
    return ret;
}

/****************************************************************************
 * Serdes Callbacks
 *****************************************************************************/

static ssize_t write_cbs(const void *data, size_t size, void *userdata)
{
    ssize_t ret = -1;
    if (userdata == NULL) {
        ret = write(STDOUT_FILENO, data, size);
        return ret;
    }
    struct vlc_tls *tls = userdata;
    ret = vlc_tls_Write(tls, data, size);
    return ret;
}

static ssize_t read_cbs(void *data, size_t size, void *userdata)
{
    ssize_t ret = -1;
    if (userdata == NULL) {
        ret = read(STDIN_FILENO, data, size);
        return ret;
    }
    struct vlc_tls *tls = userdata;
    ret = vlc_tls_Read(tls, data, size, false);
    return ret;
}

/****************************************************************************
 * Cmd line arg
 *****************************************************************************/

static int
preparser_args_Preparse(vlc_preparser_t *preparser,
                        struct preparser *pp, char *uri,
                        struct preparser_args *args)
{
    assert(preparser != NULL);
    assert(pp != NULL);
    assert(uri != NULL);
    assert(args != NULL);

    if (args->types & VLC_PREPARSER_TYPE_PARSE) {
        int t = args->types & ~(VLC_PREPARSER_TYPE_THUMBNAIL |
                                VLC_PREPARSER_TYPE_THUMBNAIL_TO_FILES);
        vlc_preparser_msg_Init(&pp->req_msg, VLC_PREPARSER_MSG_TYPE_REQ,
                               VLC_PREPARSER_MSG_REQ_TYPE_PARSE);
        pp->req_msg.req.options = t;
        pp->req_msg.req.uri = uri;
        preparser_req_Preparse(preparser, pp);
        pp->req_msg.req.uri = NULL;
        if (pp->status != VLC_SUCCESS) {
            fprintf(stderr, "Error while parsing '%s'\n", uri);
            vlc_preparser_msg_Clean(&pp->req_msg);
            return VLC_EGENERIC;
        }
        vlc_preparser_msg_Clean(&pp->req_msg);
    }
    if (args->types & VLC_PREPARSER_TYPE_THUMBNAIL) {
        vlc_preparser_msg_Init(&pp->req_msg, VLC_PREPARSER_MSG_TYPE_REQ,
                               VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL);
        pp->req_msg.req.arg.seek.type = args->seek.type;
        if (args->seek.type == VLC_THUMBNAILER_SEEK_TIME) {
            pp->req_msg.req.arg.seek.time = args->seek.time;
        } else if (args->seek.type == VLC_THUMBNAILER_SEEK_POS) {
            pp->req_msg.req.arg.seek.pos = args->seek.pos;
        }
        pp->req_msg.req.arg.seek.speed = args->seek.speed;
        pp->req_msg.req.arg.hw_dec = false;
        pp->req_msg.req.uri = strdup(uri);
        preparser_req_Preparse(preparser, pp);
        pp->req_msg.req.uri = NULL;
        if (pp->status != VLC_SUCCESS) {
            fprintf(stderr, "Error while parsing '%s'\n", uri);
            vlc_preparser_msg_Clean(&pp->req_msg);
            return VLC_EGENERIC;
        }
        vlc_preparser_msg_Clean(&pp->req_msg);
    }
    if (args->types & VLC_PREPARSER_TYPE_THUMBNAIL_TO_FILES) {
        vlc_preparser_msg_Init(&pp->req_msg, VLC_PREPARSER_MSG_TYPE_REQ,
                               VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL_TO_FILES);
        pp->req_msg.req.arg.seek.type = args->seek.type;
        if (args->seek.type == VLC_THUMBNAILER_SEEK_TIME) {
            pp->req_msg.req.arg.seek.time = args->seek.time;
        } else if (args->seek.type == VLC_THUMBNAILER_SEEK_POS) {
            pp->req_msg.req.arg.seek.pos = args->seek.pos;
        }
        pp->req_msg.req.arg.seek.speed = args->seek.speed;
        pp->req_msg.req.arg.hw_dec = false;
        struct vlc_thumbnailer_output out = {
            .width = args->output.width,
            .height = args->output.height,
            .file_path = args->output.file_path,
            .format = args->output.format,
            .creat_mode = 0766,
            .crop = args->output.crop,
        };
        vlc_vector_push(&pp->req_msg.req.outputs, out);
        pp->req_msg.req.uri = strdup(uri);
        preparser_req_Preparse(preparser, pp);
        pp->req_msg.req.uri = NULL;
        if (pp->status != VLC_SUCCESS) {
            fprintf(stderr, "Error while parsing '%s'\n", uri);
            vlc_preparser_msg_Clean(&pp->req_msg);
            return VLC_EGENERIC;
        }
        vlc_preparser_msg_Clean(&pp->req_msg);
    }

    return VLC_SUCCESS;
}


static int
preparser_args_Loop(vlc_object_t *obj, vlc_preparser_t *preparser,
                    char *const *argv, int argc, struct preparser_args *args)
{
    assert(preparser != NULL);
    assert(argv != NULL);
    assert(argc != 0);
    assert(args != 0);

    struct preparser pp;
    vlc_sem_init(&pp.sem, 0);
    pp.status = 0;
    pp.daemon = false;
    pp.tls_in = NULL;
    pp.tls_out = NULL;

    static const struct vlc_preparser_msg_serdes_cbs args_cbs = {
        .write = write_cbs,
        .read = NULL,
    };

    pp.serdes = vlc_preparser_msg_serdes_Create(obj, &args_cbs, false);
    if (pp.serdes == NULL) {
        return VLC_EGENERIC;
    }

    for (int i = 0; i < argc; i++) {
        char *uri = NULL;
        if (strstr(argv[i], "://" ) == NULL) {
            uri = vlc_path2uri(argv[i], NULL);
        } else {
            uri = strdup(argv[i]);
        }
        if (uri == NULL) {
            vlc_preparser_msg_serdes_Delete(pp.serdes);
            return VLC_ENOMEM;
        }
        preparser_args_Preparse(preparser, &pp, uri, args);
        free(uri);
    }
    vlc_preparser_msg_serdes_Delete(pp.serdes);
    return VLC_SUCCESS;
}

/****************************************************************************
 * Daemon
 *****************************************************************************/

static int
preparser_daemon_Loop(vlc_object_t *obj, vlc_preparser_t *preparser)
{
    assert(preparser != NULL);

    struct preparser pp;
    vlc_sem_init(&pp.sem, 0);
    pp.status = 0;
    pp.daemon = true;

    static const struct vlc_preparser_msg_serdes_cbs deamon_cbs = {
        .write = write_cbs,
        .read = read_cbs,
    };

    pp.serdes = vlc_preparser_msg_serdes_Create(obj, &deamon_cbs, true);
    if (pp.serdes == NULL) {
        return VLC_EGENERIC;
    }

#ifndef _WIN32
    pp.tls_in = vlc_tls_SocketOpen(STDIN_FILENO);
    if (pp.tls_in == NULL) {
        vlc_preparser_msg_serdes_Delete(pp.serdes);
        return VLC_EGENERIC;
    }
    pp.tls_out = vlc_tls_SocketOpen(STDOUT_FILENO);
    if (pp.tls_out == NULL) {
        pp.tls_in->ops->close(pp.tls_in);
        vlc_preparser_msg_serdes_Delete(pp.serdes);
        return VLC_EGENERIC;
    }
#else
    pp.tls_in = NULL;
    pp.tls_out = NULL;
#endif

    int status = VLC_SUCCESS;
    while (status == VLC_SUCCESS) {
        status = vlc_preparser_msg_serdes_Deserialize(pp.serdes, &pp.req_msg,
                                                      pp.tls_in);
        if (status != VLC_SUCCESS) {
            break;
        }
        if (pp.req_msg.req.uri != NULL) {
            status = preparser_req_Preparse(preparser, &pp);
        } else {
            status = VLC_EGENERIC;
        }
        vlc_preparser_msg_Clean(&pp.req_msg);
    }

    if (pp.tls_in != NULL) {
        pp.tls_in->ops->close(pp.tls_in);
    }
    if (pp.tls_out != NULL) {
        pp.tls_out->ops->close(pp.tls_out);
    }
    vlc_preparser_msg_serdes_Delete(pp.serdes);
    return status;
}

/****************************************************************************
 * Main
 *****************************************************************************/
const char vlc_module_name[] = "vlc-preparser";

/**
 * Wait for new request and call preparser_Preparse.
 */
int main(int argc, char **argv)
{
#ifdef TOP_BUILDDIR
    setenv("VLC_PLUGIN_PATH", TOP_BUILDDIR"/modules", 1);
    setenv("VLC_DATA_PATH", TOP_SRCDIR"/share", 1);
    setenv("VLC_LIB_PATH", TOP_BUILDDIR"/modules", 1);
#endif

#ifdef _WIN32
#include <fcntl.h>
    setmode(STDIN_FILENO, O_BINARY);
    setmode(STDOUT_FILENO, O_BINARY);
#endif

    vlc_thread_set_name("vlc-preparser");

    struct preparser_args args = {
        .timeout = VLC_TICK_INVALID,
        .types = 0,
        .daemon = false,
        .seek.type = VLC_THUMBNAILER_SEEK_NONE,
        .seek.speed = VLC_THUMBNAILER_SEEK_FAST,
        .output.file_path = NULL,
        .output.format = VLC_THUMBNAILER_FORMAT_PNG,
        .output.height = 0,
        .output.width = 0,
        .output.crop = false,
        .verbosity = "-1",
    };

    int ret = preparser_cmdline_Parse(argc, argv, &args);
    if (ret <= 0) {
        return ret == 0 ? 0 : 1;
    }

    const char *libvlc_args[] = {
        "--verbose", args.verbosity, "--vout=vdummy", "--aout=adummy",
        "--text-renderer=tdummy",
    };

    libvlc_instance_t *vlc = libvlc_new(ARRAY_SIZE(libvlc_args), libvlc_args);
    if (vlc == NULL) {
        return 1;
    }
    vlc_object_t *obj = VLC_OBJECT(vlc->p_libvlc_int);

    const struct vlc_preparser_cfg cfg = {
        .types = args.types,
        .max_parser_threads = 1,
        .max_thumbnailer_threads = 1,
        .timeout = args.timeout,
        .external_process = false,
    };

    vlc_preparser_t *preparser = vlc_preparser_New(obj, &cfg);
    if (preparser == NULL) {
        libvlc_release(vlc);
        return 1;
    }

    if (args.daemon) {
        ret = preparser_daemon_Loop(obj, preparser);
    } else {
        ret = preparser_args_Loop(obj, preparser, argv + args.arg_idx,
                                  argc - args.arg_idx, &args);
    }

    vlc_preparser_Delete(preparser);
    libvlc_release(vlc);
    return ret;
}
