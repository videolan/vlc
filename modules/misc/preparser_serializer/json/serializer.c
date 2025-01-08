// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * serializer.c: preparser serializer to json module
 *****************************************************************************
 * Copyright © 2025 Videolabs, VideoLAN and VLC authors
 *
 * Authors: Gabriel Lafond Thenaille <gabriel@videolabs.io>
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <limits.h>
#include <stdckdint.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_preparser_ipc.h>
#include <vlc_memstream.h>
#include <vlc_strings.h>
#include <vlc_vector.h>

#include "serializer.h"
#include "../../../demux/json/json.h"

/****************************************************************************
 * serdes buf
 *****************************************************************************/

int
serdes_buf_write(struct serdes_sys *sys, const void *_data, size_t size)
{
    assert(sys != NULL);
    assert(sys->parent != NULL);
    assert(sys->parent->owner.cbs != NULL);
    assert(sys->parent->owner.cbs->write != NULL);

    if (size == 0) {
        return VLC_SUCCESS;
    }
    assert(_data != NULL);
    const uint8_t *data = _data;

    while (size != 0) {
        size_t left = sys->cap - sys->size;
        if (size <= left) {
            memcpy(sys->buffer + sys->size, data, size);
            sys->size += size;
            return VLC_SUCCESS;
        }
        memcpy(sys->buffer + sys->size, data, left);
        sys->size += left;
        data += left;
        size -= left;
        int ret = sys->parent->owner.cbs->write(sys->buffer, sys->size,
                                                sys->userdata);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            serdes_set_error(-errno);
            return ret;
        } else if (ret == 0) {
            continue;
        } else if ((size_t)ret < sys->size) {
            sys->size = sys->size - ret;
            memmove(sys->buffer, sys->buffer + ret,
                    sys->size);
        } else {
            sys->size = 0;
        }
    }
    return VLC_SUCCESS;
}

int
serdes_buf_puts(struct serdes_sys *sys, const char *data)
{
    assert(sys != NULL);
    assert(data != NULL);
    return serdes_buf_write(sys, data, strlen(data));
}

int
serdes_buf_putc(struct serdes_sys *sys, char c)
{
    assert(sys != NULL);
    assert(sys->parent != NULL);
    assert(sys->parent->owner.cbs != NULL);
    assert(sys->parent->owner.cbs->write != NULL);

    while (sys->cap == sys->size) {
        ssize_t ret = sys->parent->owner.cbs->write(sys->buffer, sys->size,
                                                    sys->userdata);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            serdes_set_error(-errno);
            return VLC_EGENERIC;
        } else if (ret == 0) {
            continue;
        } else if ((size_t)ret < sys->size) {
            sys->size = sys->size - ret;
            memmove(sys->buffer, sys->buffer + ret, sys->size);
        } else {
            sys->size = 0;
        }
    }
    sys->buffer[sys->size++] = c;
    return VLC_SUCCESS;
}

int
serdes_buf_printf(struct serdes_sys *sys, const char *fmt, ...)
{
    assert(sys != NULL);
    assert(fmt != NULL);

    va_list ap;
    va_list cp;
    va_start(ap, fmt);
    va_copy(cp, ap);
    int ret = vsnprintf(NULL, 0, fmt, cp);
    va_end(cp);
    if (ret < 0) {
        serdes_set_error(VLC_EGENERIC);
        va_end(ap);
        return VLC_EGENERIC;
    }
    size_t left = sys->cap - sys->size;
    if ((size_t)ret < left) {
        ret = vsnprintf((void *)(sys->buffer + sys->size), ret + 1,
                        fmt, ap);
        if (ret < 0) {
            serdes_set_error(VLC_EGENERIC);
            va_end(ap);
            return VLC_EGENERIC;
        }
        sys->size += ret;
        ret = VLC_SUCCESS;
    } else {
        char *ptr = NULL;
        ret = vasprintf(&ptr, fmt, ap);
        if (ret < 0) {
            serdes_set_error(VLC_EGENERIC);
            va_end(ap);
            return VLC_EGENERIC;
        }
        ret = serdes_buf_write(sys, ptr, ret);
        free(ptr);
    }
    va_end(ap);
    return ret;
}

int
serdes_buf_flush(struct serdes_sys *sys)
{
    assert(sys != NULL);
    assert(sys->parent != NULL);
    assert(sys->parent->owner.cbs != NULL);
    assert(sys->parent->owner.cbs->write != NULL);

    uint8_t *ptr = sys->buffer;
    size_t size = sys->size;
    int ret = 0;
    while (size != 0) {
        ret = sys->parent->owner.cbs->write(ptr, size, sys->userdata);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            serdes_set_error(-errno);
            return ret;
        }
        ptr += ret;
        size -= ret;
    }
    sys->size = 0;
    return VLC_SUCCESS;
}

ssize_t
serdes_buf_read(struct serdes_sys *sys, void *_data, size_t size, bool eod)
{
    assert(sys != NULL);
    assert(sys->parent != NULL);
    assert(sys->parent->owner.cbs != NULL);
    assert(sys->parent->owner.cbs->read != NULL);
    assert(size <= SSIZE_MAX);

    uint8_t *data = _data;
    uint8_t *ptr = _data;
    int ret = 0;
    if (eod) {
        while (1) {
            uint8_t *zero = memchr(sys->buffer, '\0', sys->size);
            if (zero != NULL) {
                sys->current_type = VLC_PREPARSER_MSG_SERDES_TYPE_END_DATA;
                size_t used = zero - sys->buffer;
                memcpy(ptr, sys->buffer, used);
                ptr += used;
                size_t left = sys->size - (used + 1);
                memmove(sys->buffer, zero + 1, left);
                sys->size = left;
                return ptr - data;
            }
            if (sys->size > size) {
                memcpy(ptr, sys->buffer, size);
                memmove(sys->buffer, sys->buffer + size, sys->size - size);
                sys->size -= size;
                ptr += size;
                return ptr - data;
            } else if (sys->size != 0) {
                memcpy(ptr, sys->buffer, sys->size);
                ptr += sys->size;
                size -= sys->size;
                sys->size = 0;
            }
            ret = sys->parent->owner.cbs->read(sys->buffer, sys->cap,
                                               sys->userdata);
            if (ret < 0) {
                if (errno == EINTR) {
                    continue;
                }
                serdes_set_error(-errno);
                return ret;
            } else if (ret == 0) {
                return ptr - data;
            }
            sys->size = ret;
        }
    } else {
        if (sys->size != 0) {
            size_t max = sys->size < size ? sys->size : size;
            memcpy(ptr, sys->buffer, max);
            ptr += max;
            size -= max;
            sys->size -= max;
            if (sys->size != 0) {
                memmove(sys->buffer, sys->buffer + max, sys->size);
                return max;
            }
        }
        while (size != 0) {
            ret = sys->parent->owner.cbs->read(ptr, size, sys->userdata);
            if (ret < 0) {
                if (errno == EINTR) {
                    continue;
                }
                serdes_set_error(-errno);
                return ret;
            } else if (ret == 0) {
                break;
            }
            ptr += ret;
            size -= ret;
        }
        return ptr - data;
    }
}

/****************************************************************************
 * serdes Operations
 *****************************************************************************/

static int
serdes_Serialize(struct vlc_preparser_msg_serdes *serdes,
                 const struct vlc_preparser_msg *msg, void *userdata)
{
    assert(serdes != NULL);
    struct serdes_sys *sys = serdes->owner.sys;
    assert(serdes == sys->parent);

    if (msg == NULL) {
        return VLC_SUCCESS;
    }
    sys->userdata = userdata;
    sys->current_type = VLC_PREPARSER_MSG_SERDES_TYPE_DATA;
    sys->error = VLC_SUCCESS;
    sys->attach_data.size = 0;
    sys->size = 0;

    toJSON_vlc_preparser_msg(serdes->owner.sys, msg);
    if (sys->error != VLC_SUCCESS) {
        return sys->error;
    }

    int ret = 0;
    if (!sys->bin_data) {
        ret = serdes_buf_putc(sys, '\n');
        if (ret < 0) {
            return sys->error;
        }
    } else {
        ret = serdes_buf_putc(sys, '\0');
        if (ret < 0) {
            return sys->error;
        }
        ret = serdes_buf_write(sys, sys->attach_data.data,
                               sys->attach_data.size);
        if (ret < 0) {
            vlc_vector_clear(&sys->attach_data);
            return sys->error;
        }
    }
    vlc_vector_clear(&sys->attach_data);
    ret = serdes_buf_flush(sys);
    if (ret < 0) {
        return sys->error;
    }
    return VLC_SUCCESS;
}

/**
 * JSON read callback
 */
size_t json_read(void *opaque, void *buf, size_t max)
{
    assert(opaque != NULL);
    assert(buf != NULL);

    struct serdes_sys *sys = opaque;

    if (sys->current_type == VLC_PREPARSER_MSG_SERDES_TYPE_END_DATA) {
        return 0;
    }

    ssize_t ret = serdes_buf_read(sys, buf, max, true);
    if (ret < 0) {
        return 0;
    }
    return ret;
}

void json_parse_error(void *opaque, const char *msg)
{
    assert(opaque != NULL);
    assert(msg != NULL);
    struct serdes_sys *sys = opaque;
    serdes_set_error(VLC_EGENERIC);
}

static int
serdes_Deserialize(struct vlc_preparser_msg_serdes *serdes,
                   struct vlc_preparser_msg *msg, void *userdata)
{
    assert(serdes != NULL);
    assert(msg != NULL);

    assert(serdes->owner.sys != NULL);
    struct serdes_sys *sys = serdes->owner.sys;
    sys->userdata = userdata;
    sys->current_type = VLC_PREPARSER_MSG_SERDES_TYPE_DATA;
    sys->error = VLC_SUCCESS;
    sys->attach_data.size = 0;
    sys->size = 0;
    sys->parent = serdes;

    struct json_object obj;
    if (json_parse(sys, &obj) != 0) {
        return sys->error;
    }

    bool err = fromJSON_vlc_preparser_msg(sys, msg, &obj);
    json_free(&obj);
    if (err) {
        if (sys->error == VLC_SUCCESS) {
            serdes_set_error(VLC_EGENERIC);
        }
        return sys->error;
    }

    return VLC_SUCCESS;
}

static void
serdes_Close(struct vlc_preparser_msg_serdes *serdes)
{
    assert(serdes != NULL);
    assert(serdes->owner.sys != NULL);

    struct serdes_sys *sys = serdes->owner.sys;
    serdes->owner.sys = NULL;

    vlc_vector_clear(&sys->attach_data);
    free(sys);
}

#define SERDES_BUFFER_SIZE (1 << 18)

/**
 * Create a new de/serializer.
 */
static int
serdes_Open(struct vlc_preparser_msg_serdes *serdes, bool bin_data)
{
    assert(serdes != NULL);
    
    struct serdes_sys *sys = malloc(sizeof(*sys) + SERDES_BUFFER_SIZE);
    if (sys == NULL) {
        return VLC_ENOMEM;
    }
    sys->error = VLC_SUCCESS;
    sys->current_type = VLC_PREPARSER_MSG_SERDES_TYPE_DATA;
    sys->size = 0;
    sys->cap = SERDES_BUFFER_SIZE;
    sys->bin_data = bin_data;
    memset(sys->buffer, 0, sys->cap);
    vlc_vector_init(&sys->attach_data);
    sys->parent = serdes;

    static const struct vlc_preparser_msg_serdes_operations ops = {
        .serialize = serdes_Serialize,
        .deserialize = serdes_Deserialize,
        .close = serdes_Close,
    };
    serdes->ops = &ops;
    serdes->owner.sys = sys;

    return VLC_SUCCESS;
}

#undef SERDES_BUFFER_SIZE

vlc_module_begin()
    set_description(N_("Preparser Message Serializer/Deserializer"))
    set_callback_preparser_msg_serdes(serdes_Open, 50)
vlc_module_end()
