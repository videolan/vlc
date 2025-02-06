// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * vlc_preparser_serializer.h: preparser serializer
 *****************************************************************************
 * Copyright © 2025 Videolabs, VideoLAN and VLC authors
 *
 * Authors: Gabriel Lafond Thenaille <gabriel@videolabs.io>
 *****************************************************************************/

#ifndef VLC_PREPARSER_IPC_H
#define VLC_PREPARSER_IPC_H

#include <vlc_common.h>
#include <vlc_vector.h>
#include <vlc_input.h>
#include <vlc_input_item.h>
#include <vlc_preparser.h>
#include <vlc_interrupt.h>

/**
 * @defgroup preparser_ipc Preparser IPC
 * @ingroup preparser
 * @{
 * @file
 * VLC Preparser IPC API
 *
 * @defgroup preparser_msg preparser message api
 * @ingroup preparser_ipc
 * @{
 */

/**
 * Request types
 */
enum vlc_preparser_msg_req_type {
    /** 
     * Type of the request emitted by a `vlc_preparser_Push` call.
     */
    VLC_PREPARSER_MSG_REQ_TYPE_PARSE,
    /**
     * Type of the request emitted by a `vlc_preparser_GenerateThumbnail`
     * call.
     */
    VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL,
    /**
     * Type of the request emitted by a
     * `vlc_preparser_GenerateThumbnailToFiles` call.
     */
    VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL_TO_FILES,
};

/**
 * Preparser request.
 */
struct vlc_preparser_msg_req {
    /**
     * Type of the request.
     */
    enum vlc_preparser_msg_req_type type;

    /**
     * Used only by request of type `VLC_PREPARSER_MSG_REQ_TYPE_PARSE`.
     */
    int options;

    /**
     * Used by both type `VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL` and
     * `VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL_TO_FILES`.
     */
    struct vlc_thumbnailer_arg arg;

    /**
     * Used only by request of type
     * `VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL_TO_FILES`.
     */
    /* all `output_path` will be freed so they must be heap allocated or set to
     * NULL before a `vlc_preparser_msg_Clean` call. */
    struct VLC_VECTOR(char *) outputs_path;
    struct VLC_VECTOR(struct vlc_thumbnailer_output) outputs;

    /* `uri` will be freed so it must be heap allocated or set to
     * NULL before a `vlc_preparser_msg_Clean` call. */
    char *uri;
};

/**
 * Preparser Response
 */
struct vlc_preparser_msg_res {
    /**
     * Type of the response (As the response answering a request they share the
     * same type).
     */
    enum vlc_preparser_msg_req_type type;

    /**
     * Used only by request of type `VLC_PREPARSER_MSG_REQ_TYPE_PARSE`.
     */
    struct VLC_VECTOR(input_attachment_t *) attachments;
    input_item_node_t *subtree;

    /**
     * Used only by request of type `VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL`.
     */
    picture_t *pic;

    /**
     * Used only by request of type
     * `VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL_TO_FILES`.
     */
    struct VLC_VECTOR(bool) result;

    /**
     * Used by all types of request.
     */
    int status;
    input_item_t *item;
};

/**
 * Preparser message.
 */
struct vlc_preparser_msg {
    /**
     * Type of the message can be a request or a response.
     */
    enum {
        VLC_PREPARSER_MSG_TYPE_REQ,
        VLC_PREPARSER_MSG_TYPE_RES,
    } type;

    /**
     * Type of the underling request or response.
     */
    enum vlc_preparser_msg_req_type req_type;
    union {
        struct vlc_preparser_msg_req req;
        struct vlc_preparser_msg_res res;
    };
};

/**
 * Initialize a preparser message.
 * 
 * @info    All data specific to each request/response have to be initialized 
 *          by hand.
 *
 * @param   msg         message to initialize.
 * @param   msg_type    message type (request or response).
 * @param   req_type    request/response type (see enum vlc_preparser_req_type
 *                      for more information).
 */
VLC_API void
vlc_preparser_msg_Init(struct vlc_preparser_msg *msg, int msg_type,
                       enum vlc_preparser_msg_req_type req_type);

/**
 * Clean all memory used by a message.
 * 
 * @info    This function don't free the `msg` pointer.
 *
 * @param   msg         message to release.
 */
VLC_API void
vlc_preparser_msg_Clean(struct vlc_preparser_msg *msg);

/**
 * @} preparser_msg
 *
 * @defgroup preparser_serdes preparser serializer api
 * @ingroup preparser_ipc
 *
 * @{
 */
#define VLC_PREPARSER_MSG_SERDES_TYPE_DATA              0x1
#define VLC_PREPARSER_MSG_SERDES_TYPE_ATTACHMENT        0x2
#define VLC_PREPARSER_MSG_SERDES_TYPE_END_DATA          0x4
#define VLC_PREPARSER_MSG_SERDES_TYPE_END_ATTACHMENT    0x8

struct vlc_preparser_msg_serdes_cbs {
    /**
     * Write callback.
     *
     * @param [in]  data        buffer to write.
     * @param [in]  size        number of bytes to write.
     * @param [in]  userdata    callback userdata.
     *
     * @return      the number of bytes writen or an error code on failure.
     */
    ssize_t (*write)(const void *data, size_t size, void *userdata);

    /**
     * Read callback.
     *
     * @param [out] data        buffer to read into.
     * @param [in]  size        number of bytes to read.
     * @param [in]  userdata    callback userdata.
     *
     * @return      the number of bytes read or an error code on failure.
     */
    ssize_t (*read)(void *data, size_t size, void *userdata);
};

struct vlc_preparser_msg_serdes;

struct vlc_preparser_msg_serdes_operations {
    /**
     * Serialize `msg` and call the write callback with serialized data.
     *
     * @param [in]  serdes      serializer internal structure.
     * @param [in]  msg         message to serialize.
     * @param [in]  userdata    context for the write callbacks
     *
     * @return      VLC_SUCCESS or an error code on failure.
     */
    int (*serialize)(struct vlc_preparser_msg_serdes *serdes,
                     const struct vlc_preparser_msg *msg,
                     void *userdata);

    /**
     * Deserialize `msg` and call the read callback to get data to deserialize.
     *
     * @param [in]  serdes      serializer internal structure.
     * @param [out] msg         message to deserialize.
     * @param [in]  userdata    context for the read callbacks
     *
     * @return      VLC_SUCCESS or an error code on failure.
     */
    int (*deserialize)(struct vlc_preparser_msg_serdes *serdes,
                       struct vlc_preparser_msg *msg,
                       void *userdata);

    /**
     * Close the serializer/deserialier and release all used memory.
     *
     * @param [in]  serdes  preparser msg serdes internal struture.
     */
    void (*close)(struct vlc_preparser_msg_serdes *serdes);
};

/**
 * Internal structure used by serializer.
 */
struct vlc_preparser_msg_serdes {
    /** Operations */
    const struct vlc_preparser_msg_serdes_operations *ops;

    struct {
        /** Callbacks */
        const struct vlc_preparser_msg_serdes_cbs *cbs;
        /** Used by the serializer module. */
        void *sys;
    } owner;
};

typedef int (*vlc_preparser_msg_serdes_module)
                (struct vlc_preparser_msg_serdes *, bool);

#define set_callback_preparser_msg_serdes(activate, priority) \
    {\
        vlc_preparser_msg_serdes_module open__ = activate;\
        (void)open__;\
        set_callback(activate)\
    }\
    set_capability("preparser msg serdes", priority)

/**
 * Call the serialize operation.
 *
 * @param [in]  s
 * @param [out] buf
 * @param [in]  msg
 *
 * @return      size of the allocated buffer.
 */
static inline int
vlc_preparser_msg_serdes_Serialize(struct vlc_preparser_msg_serdes *serdes,
                                   const struct vlc_preparser_msg *msg,
                                   void *userdata)
{
    assert(serdes != NULL);

    if (serdes->ops != NULL && serdes->ops->serialize != NULL) {
        return serdes->ops->serialize(serdes, msg, userdata);
    }
    return VLC_EGENERIC;
}

/**
 * Call the deserialize operation.
 *
 * @param [in]  s
 * @param [in]  buf
 * @param [in]  size
 *
 * @return      size of the allocated buffer.
 */
static inline int
vlc_preparser_msg_serdes_Deserialize(struct vlc_preparser_msg_serdes *serdes,
                                     struct vlc_preparser_msg *msg,
                                     void *userdata)
{
    assert(serdes!= NULL);

    if (serdes->ops != NULL && serdes->ops->deserialize != NULL) {
        return serdes->ops->deserialize(serdes, msg, userdata);
    }
    return VLC_EGENERIC;
}

/**
 * Free the msg_serdes struct.
 *
 * @param [in]  msg_serdes
 */
static inline void
vlc_preparser_msg_serdes_Delete(struct vlc_preparser_msg_serdes *serdes)
{
    assert(serdes != NULL);

    if (serdes->ops != NULL && serdes->ops->close != NULL) {
        serdes->ops->close(serdes);
    }
    free(serdes);
}

/**
 * Create a vlc_preparser_msg_serdes object and load a preparser msg_serdes
 * module.
 *
 * @param [in]  obj         vlc object
 * @param [in]  c           serializer's callbacks
 * @param [in]  bin_data    describe if the serializer and deserializer use 
 *                          binary data (intput_attachment_t or plane_t)
 *
 * @return      a vlc_preparser_msg_serdes object or NULL on failure.
 */
VLC_API struct vlc_preparser_msg_serdes *
vlc_preparser_msg_serdes_Create(vlc_object_t *obj,
                                const struct vlc_preparser_msg_serdes_cbs *c,
                                bool bin_data);

/**
 * @} preparser_serdes
 * @} preparser_ipc
 */

#endif /* VLC_PREPARSER_IPC */
