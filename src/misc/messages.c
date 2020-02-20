/*****************************************************************************
 * messages.c: messages interface
 * This library provides an interface to the message queue to be used by other
 * modules, especially intf modules. See vlc_config.h for output configuration.
 *****************************************************************************
 * Copyright (C) 1998-2005 VLC authors and VideoLAN
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

static void vlc_LogSpam(vlc_object_t *obj)
{
    /* Announce who we are */
    msg_Dbg(obj, "VLC media player - %s", VERSION_MESSAGE);
    msg_Dbg(obj, "%s", COPYRIGHT_MESSAGE);
    msg_Dbg(obj, "revision %s", psz_vlc_changeset);
    msg_Dbg(obj, "configured with %s", CONFIGURE_LINE);
}

struct vlc_logger {
    const struct vlc_logger_operations *ops;
};

static void vlc_vaLogCallback(vlc_logger_t *logger, int type,
                              const vlc_log_t *item, const char *format,
                              va_list ap)
{
    if (logger != NULL) {
        int canc = vlc_savecancel();

        logger->ops->log(logger, type, item, format, ap);
        vlc_restorecancel(canc);
    }
}

static void vlc_LogCallback(vlc_logger_t *logger, int type,
                            const vlc_log_t *item, const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vlc_vaLogCallback(logger, type, item, format, ap);
    va_end(ap);
}

#ifdef _WIN32
static void Win32DebugOutputMsg (void *, int , const vlc_log_t *,
                                 const char *, va_list);
#endif

void vlc_vaLog(struct vlc_logger *const *loggerp, int type,
               const char *typename, const char *module,
               const char *file, unsigned line, const char *func,
               const char *format, va_list args)
{
    struct vlc_logger *logger = *loggerp;
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

    msg.i_object_id = (uintptr_t)(void *)loggerp;
    msg.psz_object_type = typename;
    msg.psz_module = module;
    msg.psz_header = NULL;
    msg.file = file;
    msg.line = line;
    msg.func = func;
    msg.tid = vlc_thread_id();

#ifdef _WIN32
    va_list ap;

    va_copy (ap, args);
    Win32DebugOutputMsg (NULL, type, &msg, format, ap);
    va_end (ap);
#endif

    /* Pass message to the callback */
    if (logger != NULL)
        vlc_vaLogCallback(logger, type, &msg, format, args);
}

void vlc_Log(struct vlc_logger *const *logger, int type,
             const char *typename, const char *module,
             const char *file, unsigned line, const char *func,
             const char *format, ... )
{
    va_list ap;

    va_start(ap, format);
    vlc_vaLog(logger, type, typename, module, file, line, func, format, ap);
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
            if (likely(wmsg != NULL))
            {
                OutputDebugStringW(wmsg);
                free(wmsg);
            }
            free(psz_msg);
        }
    }
    free(msg);
}
#endif

/**
 * Early (latched) message log.
 *
 * A message log that stores messages in memory until another log is available.
 */
typedef struct vlc_log_early_t
{
    struct vlc_log_early_t *next;
    int type;
    vlc_log_t meta;
    char *msg;
} vlc_log_early_t;

typedef struct vlc_logger_early {
    vlc_mutex_t lock;
    vlc_log_early_t *head;
    vlc_log_early_t **tailp;
    vlc_logger_t *sink;
    struct vlc_logger logger;
} vlc_logger_early_t;

static void vlc_vaLogEarly(void *d, int type, const vlc_log_t *item,
                           const char *format, va_list ap)
{
    struct vlc_logger *logger = d;
    struct vlc_logger_early *early =
        container_of(logger, struct vlc_logger_early, logger);

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

    vlc_mutex_lock(&early->lock);
    assert(early->tailp != NULL);
    assert(*(early->tailp) == NULL);
    *(early->tailp) = log;
    early->tailp = &log->next;
    vlc_mutex_unlock(&early->lock);
}

static void vlc_LogEarlyClose(void *d)
{
    struct vlc_logger *logger = d;
    struct vlc_logger_early *early =
        container_of(logger, struct vlc_logger_early, logger);
    vlc_logger_t *sink = early->sink;

    /* Drain early log messages */
    for (vlc_log_early_t *log = early->head, *next; log != NULL; log = next)
    {
        vlc_LogCallback(sink, log->type, &log->meta, "%s",
                        (log->msg != NULL) ? log->msg : "message lost");
        free(log->msg);
        next = log->next;
        free(log);
    }

    free(early);
}

static const struct vlc_logger_operations early_ops = {
    vlc_vaLogEarly,
    vlc_LogEarlyClose,
};

static struct vlc_logger *vlc_LogEarlyOpen(struct vlc_logger *logger)
{
    vlc_logger_early_t *early = malloc(sizeof (*early));
    if (unlikely(early == NULL))
        return NULL;

    early->logger.ops = &early_ops;
    vlc_mutex_init(&early->lock);
    early->head = NULL;
    early->tailp = &early->head;
    early->sink = logger;
    return &early->logger;
}

static void vlc_vaLogDiscard(void *d, int type, const vlc_log_t *item,
                             const char *format, va_list ap)
{
    (void) d; (void) type; (void) item; (void) format; (void) ap;
}

static void vlc_LogDiscardClose(void *d)
{
    (void) d;
}

static const struct vlc_logger_operations discard_ops = {
    vlc_vaLogDiscard,
    vlc_LogDiscardClose,
};

static struct vlc_logger discard_log = { &discard_ops };

/**
 * Switchable message log.
 *
 * A message log that can be redirected live.
 */
struct vlc_logger_switch {
    struct vlc_logger *backend;
    struct vlc_logger frontend;
    vlc_rwlock_t lock;
};

static void vlc_vaLogSwitch(void *d, int type, const vlc_log_t *item,
                            const char *format, va_list ap)
{
    struct vlc_logger *logger = d;
    struct vlc_logger_switch *logswitch =
        container_of(logger, struct vlc_logger_switch, frontend);
    struct vlc_logger *backend;

    vlc_rwlock_rdlock(&logswitch->lock);
    backend = logswitch->backend;
    backend->ops->log(backend, type, item, format, ap);
    vlc_rwlock_unlock(&logswitch->lock);
}

static void vlc_LogSwitchClose(void *d)
{
    struct vlc_logger *logger = d;
    struct vlc_logger_switch *logswitch =
        container_of(logger, struct vlc_logger_switch, frontend);
    struct vlc_logger *backend = logswitch->backend;

    logswitch->backend = &discard_log;
    backend->ops->destroy(backend);

    vlc_rwlock_destroy(&logswitch->lock);
    free(logswitch);
}

static const struct vlc_logger_operations switch_ops = {
    vlc_vaLogSwitch,
    vlc_LogSwitchClose,
};

static void vlc_LogSwitch(vlc_logger_t *logger, vlc_logger_t *new_logger)
{
    struct vlc_logger_switch *logswitch =
        container_of(logger, struct vlc_logger_switch, frontend);
    struct vlc_logger *old_logger;

    assert(logger->ops == &switch_ops);

    if (new_logger == NULL)
        new_logger = &discard_log;

    vlc_rwlock_wrlock(&logswitch->lock);
    old_logger = logswitch->backend;
    logswitch->backend = new_logger;
    vlc_rwlock_unlock(&logswitch->lock);

    old_logger->ops->destroy(old_logger);
}

static struct vlc_logger *vlc_LogSwitchCreate(void)
{
    struct vlc_logger_switch *logswitch = malloc(sizeof (*logswitch));
    if (unlikely(logswitch == NULL))
        return NULL;

    logswitch->frontend.ops = &switch_ops;
    logswitch->backend = &discard_log;
    vlc_rwlock_init(&logswitch->lock);
    return &logswitch->frontend;
}

/**
 * Module-based message log.
 */
struct vlc_logger_module {
    struct vlc_object_t obj;
    struct vlc_logger frontend;
    const struct vlc_logger_operations *ops;
    void *opaque;
};

static int vlc_logger_load(void *func, bool forced, va_list ap)
{
    const struct vlc_logger_operations *(*activate)(vlc_object_t *,
                                                    void **) = func;
    struct vlc_logger_module *module = va_arg(ap, struct vlc_logger_module *);

    (void) forced;
    module->ops = activate(VLC_OBJECT(module), &module->opaque);
    return (module->ops != NULL) ? VLC_SUCCESS : VLC_EGENERIC;
}

static void vlc_vaLogModule(void *d, int type, const vlc_log_t *item,
                            const char *format, va_list ap)
{
    struct vlc_logger *logger = d;
    struct vlc_logger_module *module =
        container_of(logger, struct vlc_logger_module, frontend);

    module->ops->log(module->opaque, type, item, format, ap);
}

static void vlc_LogModuleClose(void *d)
{
    struct vlc_logger *logger = d;
    struct vlc_logger_module *module =
        container_of(logger, struct vlc_logger_module, frontend);

    if (module->ops->destroy != NULL)
        module->ops->destroy(module->opaque);

    vlc_object_delete(VLC_OBJECT(module));
}

static const struct vlc_logger_operations module_ops = {
    vlc_vaLogModule,
    vlc_LogModuleClose,
};

static struct vlc_logger *vlc_LogModuleCreate(vlc_object_t *parent)
{
    struct vlc_logger_module *module;

    module = vlc_custom_create(parent, sizeof (*module), "logger");
    if (unlikely(module == NULL))
        return NULL;

    /* TODO: module configuration item */
    if (vlc_module_load(VLC_OBJECT(module), "logger", NULL, false,
                        vlc_logger_load, module) == NULL) {
        vlc_object_delete(VLC_OBJECT(module));
        return NULL;
    }

    module->frontend.ops = &module_ops;
    return &module->frontend;
}

/**
 * Initializes the messages logging subsystem and drain the early messages to
 * the configured log.
 */
void vlc_LogInit(libvlc_int_t *vlc)
{
    struct vlc_logger *logger = vlc_LogModuleCreate(VLC_OBJECT(vlc));
    if (logger == NULL)
        logger = &discard_log;

    vlc_LogSwitch(vlc->obj.logger, logger);
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
    vlc_logger_t *logger = vlc_LogSwitchCreate();
    if (unlikely(logger == NULL))
        return -1;
    vlc->obj.logger = logger;

    struct vlc_logger *early = vlc_LogEarlyOpen(logger);
    if (early != NULL) {
        vlc_LogSwitch(logger, early);
        vlc_LogSpam(VLC_OBJECT(vlc));
    }
    return 0;
}

/**
 * Message log with "header".
 */
struct vlc_logger_header {
    struct vlc_logger logger;
    struct vlc_logger *parent;
    char header[];
};

static void vlc_vaLogHeader(void *d, int type, const vlc_log_t *item,
                            const char *format, va_list ap)
{
    struct vlc_logger *logger = d;
    struct vlc_logger_header *header =
        container_of(logger, struct vlc_logger_header, logger);
    struct vlc_logger *parent = header->parent;
    vlc_log_t hitem = *item;

    hitem.psz_header = header->header;
    parent->ops->log(parent, type, &hitem, format, ap);
}

static const struct vlc_logger_operations header_ops = {
    vlc_vaLogHeader,
    free,
};

struct vlc_logger *vlc_LogHeaderCreate(struct vlc_logger *parent,
                                       const char *str)
{
    size_t len = strlen(str) + 1;
    struct vlc_logger_header *header = malloc(sizeof (*header) + len);
    if (unlikely(header == NULL))
        return NULL;

    header->logger.ops = &header_ops;
    header->parent = parent;
    memcpy(header->header, str, len);
    return &header->logger;
}

/**
 * External custom log callback
 */
struct vlc_logger_external {
    struct vlc_logger logger;
    const struct vlc_logger_operations *ops;
    void *opaque;
};

static void vlc_vaLogExternal(void *d, int type, const vlc_log_t *item,
                              const char *format, va_list ap)
{
    struct vlc_logger_external *ext = d;

    ext->ops->log(ext->opaque, type, item, format, ap);
}

static void vlc_LogExternalClose(void *d)
{
    struct vlc_logger_external *ext = d;

    if (ext->ops->destroy != NULL)
        ext->ops->destroy(ext->opaque);
    free(ext);
}

static const struct vlc_logger_operations external_ops = {
    vlc_vaLogExternal,
    vlc_LogExternalClose,
};

static struct vlc_logger *
vlc_LogExternalCreate(const struct vlc_logger_operations *ops, void *opaque)
{
    struct vlc_logger_external *ext = malloc(sizeof (*ext));
    if (unlikely(ext == NULL))
        return NULL;

    ext->logger.ops = &external_ops;
    ext->ops = ops;
    ext->opaque = opaque;
    return &ext->logger;
}

/**
 * Sets the message logging callback.
 * \param ops message callback, or NULL to clear
 * \param data data pointer for the message callback
 */
void vlc_LogSet(libvlc_int_t *vlc, const struct vlc_logger_operations *ops,
                void *opaque)
{
    struct vlc_logger *logger;

    if (ops != NULL)
        logger = vlc_LogExternalCreate(ops, opaque);
    else
        logger = NULL;

    if (logger == NULL)
        logger = &discard_log;

    vlc_LogSwitch(vlc->obj.logger, logger);
    vlc_LogSpam(VLC_OBJECT(vlc));
}

void vlc_LogDestroy(vlc_logger_t *logger)
{
    logger->ops->destroy(logger);
}
