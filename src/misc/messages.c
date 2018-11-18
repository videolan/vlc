/*****************************************************************************
 * messages.c: messages interface
 * This library provides an interface to the message queue to be used by other
 * modules, especially intf modules. See vlc_config.h for output configuration.
 *****************************************************************************
 * Copyright (C) 1998-2005 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <stdarg.h>                                       /* va_list for BSD */
#include <unistd.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_interface.h>
#include <vlc_charset.h>
#include <vlc_modules.h>
#include "../libvlc.h"

struct vlc_logger_t
{
    struct vlc_common_members obj;
    vlc_rwlock_t lock;
    const struct vlc_logger_operations *ops;
    void *sys;
};

static void vlc_vaLogCallback(libvlc_int_t *vlc, int type,
                              const vlc_log_t *item, const char *format,
                              va_list ap)
{
    vlc_logger_t *logger = libvlc_priv(vlc)->logger;
    int canc;

    assert(logger != NULL);
    canc = vlc_savecancel();
    vlc_rwlock_rdlock(&logger->lock);
    logger->ops->log(logger->sys, type, item, format, ap);
    vlc_rwlock_unlock(&logger->lock);
    vlc_restorecancel(canc);
}

static void vlc_LogCallback(libvlc_int_t *vlc, int type, const vlc_log_t *item,
                            const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vlc_vaLogCallback(vlc, type, item, format, ap);
    va_end(ap);
}

#ifdef _WIN32
static void Win32DebugOutputMsg (void *, int , const vlc_log_t *,
                                 const char *, va_list);
#endif

/**
 * Emit a log message. This function is the variable argument list equivalent
 * to vlc_Log().
 */
void vlc_vaLog (vlc_object_t *obj, int type, const char *module,
                const char *file, unsigned line, const char *func,
                const char *format, va_list args)
{
    if (obj != NULL && obj->obj.flags & OBJECT_FLAGS_QUIET)
        return;

    /* Get basename from the module filename */
    char *p = strrchr(module, '/');
    if (p != NULL)
        module = p + 1;
    p = strchr(module, '.');

    size_t modlen = (p != NULL) ? (p - module) : 0;
    char modulebuf[modlen + 1];
    if (p != NULL)
    {
        memcpy(modulebuf, module, modlen);
        modulebuf[modlen] = '\0';
        module = modulebuf;
    }

    /* Fill message information fields */
    vlc_log_t msg;

    msg.i_object_id = (uintptr_t)obj;
    msg.psz_object_type = (obj != NULL) ? obj->obj.object_type : "generic";
    msg.psz_module = module;
    msg.psz_header = NULL;
    msg.file = file;
    msg.line = line;
    msg.func = func;
    msg.tid = vlc_thread_id();

    for (vlc_object_t *o = obj; o != NULL; o = o->obj.parent)
        if (o->obj.header != NULL)
        {
            msg.psz_header = o->obj.header;
            break;
        }

#ifdef _WIN32
    va_list ap;

    va_copy (ap, args);
    Win32DebugOutputMsg (NULL, type, &msg, format, ap);
    va_end (ap);
#endif

    /* Pass message to the callback */
    if (obj != NULL)
        vlc_vaLogCallback(obj->obj.libvlc, type, &msg, format, args);
}

/**
 * Emit a log message.
 * \param obj VLC object emitting the message or NULL
 * \param type VLC_MSG_* message type (info, error, warning or debug)
 * \param module name of module from which the message come
 *               (normally vlc_module_name)
 * \param file source module file name (normally __FILE__) or NULL
 * \param line function call source line number (normally __LINE__) or 0
 * \param func calling function name (normally __func__) or NULL
 * \param format printf-like message format
 */
void vlc_Log(vlc_object_t *obj, int type, const char *module,
             const char *file, unsigned line, const char *func,
             const char *format, ... )
{
    va_list ap;

    va_start(ap, format);
    vlc_vaLog(obj, type, module, file, line, func, format, ap);
    va_end(ap);
}

#ifdef _WIN32
static const char msg_type[4][9] = { "", " error", " warning", " debug" };

static void Win32DebugOutputMsg (void* d, int type, const vlc_log_t *p_item,
                                 const char *format, va_list dol)
{
    VLC_UNUSED(p_item);

    const signed char *pverbose = d;
    if (pverbose && (*pverbose < 0 || *pverbose < (type - VLC_MSG_ERR)))
        return;

    va_list dol2;
    va_copy (dol2, dol);
    int msg_len = vsnprintf(NULL, 0, format, dol2);
    va_end (dol2);

    if (msg_len <= 0)
        return;

    char *msg = malloc(msg_len + 1 + 1);
    if (!msg)
        return;

    msg_len = vsnprintf(msg, msg_len+1, format, dol);
    if (msg_len > 0){
        if (msg[msg_len-1] != '\n') {
            msg[msg_len] = '\n';
            msg[msg_len + 1] = '\0';
        }
        char* psz_msg = NULL;
        if (asprintf(&psz_msg, "%s %s%s: %s", p_item->psz_module,
                    p_item->psz_object_type, msg_type[type], msg) > 0) {
            wchar_t* wmsg = ToWide(psz_msg);
            OutputDebugStringW(wmsg);
            free(wmsg);
            free(psz_msg);
        }
    }
    free(msg);
}
#endif

typedef struct vlc_log_early_t
{
    struct vlc_log_early_t *next;
    int type;
    vlc_log_t meta;
    char *msg;
} vlc_log_early_t;

typedef struct
{
    vlc_mutex_t lock;
    vlc_log_early_t *head;
    vlc_log_early_t **tailp;
    vlc_logger_t *sink;
} vlc_logger_early_t;

static void vlc_vaLogEarly(void *d, int type, const vlc_log_t *item,
                           const char *format, va_list ap)
{
    vlc_logger_early_t *sys = d;

    vlc_log_early_t *log = malloc(sizeof (*log));
    if (unlikely(log == NULL))
        return;

    log->next = NULL;
    log->type = type;
    log->meta.i_object_id = item->i_object_id;
    /* NOTE: Object types MUST be static constant - no need to copy them. */
    log->meta.psz_object_type = item->psz_object_type;
    log->meta.psz_module = item->psz_module; /* Ditto. */
    log->meta.psz_header = item->psz_header ? strdup(item->psz_header) : NULL;
    log->meta.file = item->file;
    log->meta.line = item->line;
    log->meta.func = item->func;

    if (vasprintf(&log->msg, format, ap) == -1)
        log->msg = NULL;

    vlc_mutex_lock(&sys->lock);
    assert(sys->tailp != NULL);
    assert(*(sys->tailp) == NULL);
    *(sys->tailp) = log;
    sys->tailp = &log->next;
    vlc_mutex_unlock(&sys->lock);
}

static void vlc_LogEarlyClose(void *d)
{
    vlc_logger_early_t *sys = d;
    vlc_logger_t *logger = sys->sink;
    libvlc_int_t *vlc = logger->obj.libvlc;

    /* Drain early log messages */
    for (vlc_log_early_t *log = sys->head, *next; log != NULL; log = next)
    {
        vlc_LogCallback(vlc, log->type, &log->meta, "%s",
                        (log->msg != NULL) ? log->msg : "message lost");
        free(log->msg);
        next = log->next;
        free(log);
    }

    vlc_mutex_destroy(&sys->lock);
    free(sys);
}

static const struct vlc_logger_operations early_ops = {
    vlc_vaLogEarly,
    vlc_LogEarlyClose,
};

static
const struct vlc_logger_operations *vlc_LogEarlyOpen(vlc_logger_t *logger,
                                                     void **restrict sysp)
{
    vlc_logger_early_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return NULL;

    vlc_mutex_init(&sys->lock);
    sys->head = NULL;
    sys->tailp = &sys->head;
    sys->sink = logger;
    *sysp = sys;
    return &early_ops;
}

static void vlc_vaLogDiscard(void *d, int type, const vlc_log_t *item,
                             const char *format, va_list ap)
{
    (void) d; (void) type; (void) item; (void) format; (void) ap;
}

static const struct vlc_logger_operations discard_ops =
{
    vlc_vaLogDiscard,
    NULL,
};

static int vlc_logger_load(void *func, va_list ap)
{
    const struct vlc_logger_operations *(*activate)(vlc_object_t *,
                                                    void **) = func;
    vlc_logger_t *logger = va_arg(ap, vlc_logger_t *);
    const struct vlc_logger_operations **ops = va_arg(ap,
                                        const struct vlc_logger_operations **);
    void **sys = va_arg(ap, void **);

    *ops = activate(VLC_OBJECT(logger), sys);
    return (*ops != NULL) ? VLC_SUCCESS : VLC_EGENERIC;
}

static void vlc_LogSwitch(libvlc_int_t *vlc,
                          const struct vlc_logger_operations *ops,
                          void *opaque)
{
    vlc_logger_t *logger = libvlc_priv(vlc)->logger;
    const struct vlc_logger_operations *old_ops;
    void *old_opaque;

    assert(logger != NULL);

    if (ops == NULL)
        ops = &discard_ops;

    vlc_rwlock_wrlock(&logger->lock);
    old_ops = logger->ops;
    old_opaque = logger->sys;
    logger->ops = ops;
    logger->sys = opaque;
    vlc_rwlock_unlock(&logger->lock);

    if (old_ops->destroy != NULL)
        old_ops->destroy(old_opaque);
}

/**
 * Performs preinitialization of the messages logging subsystem.
 *
 * Early log messages will be stored in memory until the subsystem is fully
 * initialized with vlc_LogInit(). This enables logging before the
 * configuration and modules bank are ready.
 *
 * \return 0 on success, -1 on error.
 */
int vlc_LogPreinit(libvlc_int_t *vlc)
{
    vlc_logger_t *logger = vlc_custom_create(vlc, sizeof (*logger), "logger");
    if (unlikely(logger == NULL))
        return -1;

    libvlc_priv(vlc)->logger = logger;
    vlc_rwlock_init(&logger->lock);
    logger->ops = &discard_ops;

    const struct vlc_logger_operations *ops;
    void *opaque;

    ops = vlc_LogEarlyOpen(logger, &opaque);
    vlc_LogSet(vlc, ops, opaque);
    return 0;
}

/**
 * Initializes the messages logging subsystem and drain the early messages to
 * the configured log.
 */
void vlc_LogInit(libvlc_int_t *vlc)
{
    vlc_logger_t *logger = libvlc_priv(vlc)->logger;
    const struct vlc_logger_operations *ops;
    void *opaque;

    /* TODO: module configuration item */
    if (vlc_module_load(logger, "logger", NULL, false,
                        vlc_logger_load, logger, &ops, &opaque) == NULL)
        ops = NULL;

    vlc_LogSwitch(vlc, ops, opaque);
}

/**
 * Sets the message logging callback.
 * \param ops message callback, or NULL to clear
 * \param data data pointer for the message callback
 */
void vlc_LogSet(libvlc_int_t *vlc, const struct vlc_logger_operations *ops,
                void *opaque)
{
    vlc_LogSwitch(vlc, ops, opaque);

    /* Announce who we are */
    msg_Dbg (vlc, "VLC media player - %s", VERSION_MESSAGE);
    msg_Dbg (vlc, "%s", COPYRIGHT_MESSAGE);
    msg_Dbg (vlc, "revision %s", psz_vlc_changeset);
    msg_Dbg (vlc, "configured with %s", CONFIGURE_LINE);
}

void vlc_LogDeinit(libvlc_int_t *vlc)
{
    vlc_logger_t *logger = libvlc_priv(vlc)->logger;

    vlc_LogSwitch(vlc, NULL, NULL);
    vlc_rwlock_destroy(&logger->lock);
    vlc_object_release(logger);
}
