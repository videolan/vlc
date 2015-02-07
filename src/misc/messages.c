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
#include "../libvlc.h"

#ifdef __ANDROID__
#include <android/log.h>
#endif

struct vlc_logger_t
{
    VLC_COMMON_MEMBERS
    vlc_rwlock_t lock;
    vlc_log_cb log;
    void *sys;
};

static void vlc_vaLogCallback(libvlc_int_t *vlc, int type,
                              const vlc_log_t *item, const char *format,
                              va_list ap)
{
    vlc_logger_t *logger = libvlc_priv(vlc)->logger;

    assert(logger != NULL);
    vlc_rwlock_rdlock(&logger->lock);
    logger->log(logger->sys, type, item, format, ap);
    vlc_rwlock_unlock(&logger->lock);
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
                const char *format, va_list args)
{
    if (obj != NULL && obj->i_flags & OBJECT_FLAGS_QUIET)
        return;

    /* Get basename from the module filename */
    char *p = strrchr(module, '/');
    if (p != NULL)
        module = p;
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
    msg.psz_object_type = (obj != NULL) ? obj->psz_object_type : "generic";
    msg.psz_module = module;
    msg.psz_header = NULL;

    for (vlc_object_t *o = obj; o != NULL; o = o->p_parent)
        if (o->psz_header != NULL)
        {
            msg.psz_header = o->psz_header;
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
        vlc_vaLogCallback(obj->p_libvlc, type, &msg, format, args);
}

/**
 * Emit a log message.
 * \param obj VLC object emitting the message or NULL
 * \param type VLC_MSG_* message type (info, error, warning or debug)
 * \param module name of module from which the message come
 *               (normally MODULE_STRING)
 * \param format printf-like message format
 */
void vlc_Log(vlc_object_t *obj, int type, const char *module,
             const char *format, ... )
{
    va_list ap;

    va_start(ap, format);
    vlc_vaLog(obj, type, module, format, ap);
    va_end(ap);
}

static const char msg_type[4][9] = { "", " error", " warning", " debug" };
#define COL(x,y)  "\033[" #x ";" #y "m"
#define RED     COL(31,1)
#define GREEN   COL(32,1)
#define YELLOW  COL(0,33)
#define WHITE   COL(0,1)
#define GRAY    "\033[0m"
static const char msg_color[4][8] = { WHITE, RED, YELLOW, GRAY };

/* Display size of a pointer */
static const int ptr_width = 2 * /* hex digits */ sizeof(uintptr_t);

static void PrintColorMsg (void *d, int type, const vlc_log_t *p_item,
                           const char *format, va_list ap)
{
    FILE *stream = stderr;
    int verbose = (intptr_t)d;

    if (verbose < 0 || verbose < (type - VLC_MSG_ERR))
        return;

    int canc = vlc_savecancel ();

    flockfile (stream);
    utf8_fprintf (stream, "["GREEN"%0*"PRIxPTR""GRAY"] ", ptr_width, p_item->i_object_id);
    if (p_item->psz_header != NULL)
        utf8_fprintf (stream, "[%s] ", p_item->psz_header);
    utf8_fprintf (stream, "%s %s%s: %s", p_item->psz_module,
                  p_item->psz_object_type, msg_type[type], msg_color[type]);
    utf8_vfprintf (stream, format, ap);
    fputs (GRAY"\n", stream);
#if defined (_WIN32) || defined (__OS2__)
    fflush (stream);
#endif
    funlockfile (stream);
    vlc_restorecancel (canc);
}

static void PrintMsg (void *d, int type, const vlc_log_t *p_item,
                      const char *format, va_list ap)
{
    FILE *stream = stderr;
    int verbose = (intptr_t)d;

    if (verbose < 0 || verbose < (type - VLC_MSG_ERR))
        return;

    int canc = vlc_savecancel ();

    flockfile (stream);
    utf8_fprintf (stream, "[%0*"PRIxPTR"] ", ptr_width, p_item->i_object_id);
    if (p_item->psz_header != NULL)
        utf8_fprintf (stream, "[%s] ", p_item->psz_header);
    utf8_fprintf (stream, "%s %s%s: ", p_item->psz_module,
                  p_item->psz_object_type, msg_type[type]);
    utf8_vfprintf (stream, format, ap);
    putc_unlocked ('\n', stream);
#if defined (_WIN32) || defined (__OS2__)
    fflush (stream);
#endif
    funlockfile (stream);
    vlc_restorecancel (canc);
}

#ifdef _WIN32
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

    if(msg_len <= 0)
        return;

    char *msg = malloc(msg_len + 1 + 1);
    if (!msg)
        return;

    msg_len = vsnprintf(msg, msg_len+1, format, dol);
    if (msg_len > 0){
        if(msg[msg_len-1] != '\n'){
            msg[msg_len] = '\n';
            msg[msg_len + 1] = '\0';
        }
        char* psz_msg = NULL;
        if(asprintf(&psz_msg, "%s %s%s: %s", p_item->psz_module,
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

#ifdef __ANDROID__
static void AndroidPrintMsg (void *d, int type, const vlc_log_t *p_item,
                             const char *format, va_list ap)
{
    int verbose = (intptr_t)d;
    int prio;

    if (verbose < 0 || verbose < (type - VLC_MSG_ERR))
        return;

    int canc = vlc_savecancel ();

    char *format2;
    if (asprintf (&format2, "[%0*"PRIxPTR"] %s %s: %s",
                  ptr_width, p_item->i_object_id, p_item->psz_module,
                  p_item->psz_object_type, format) < 0)
        return;
    switch (type) {
        case VLC_MSG_INFO:
            prio = ANDROID_LOG_INFO;
            break;
        case VLC_MSG_ERR:
            prio = ANDROID_LOG_ERROR;
            break;
        case VLC_MSG_WARN:
            prio = ANDROID_LOG_WARN;
            break;
        default:
        case VLC_MSG_DBG:
            prio = ANDROID_LOG_DEBUG;
    }
    __android_log_vprint (prio, "VLC", format2, ap);
    free (format2);
    vlc_restorecancel (canc);
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

    int canc = vlc_savecancel(); /* XXX: needed for vasprintf() ? */
    if (vasprintf(&log->msg, format, ap) == -1)
        log->msg = NULL;
    vlc_restorecancel(canc);

    vlc_mutex_lock(&sys->lock);
    assert(sys->tailp != NULL);
    assert(*(sys->tailp) == NULL);
    *(sys->tailp) = log;
    sys->tailp = &log->next;
    vlc_mutex_unlock(&sys->lock);
}

static int vlc_LogEarlyOpen(vlc_logger_t *logger)
{
    vlc_logger_early_t *sys = malloc(sizeof (*sys));

    if (unlikely(sys == NULL))
        return -1;

    vlc_mutex_init(&sys->lock);
    sys->head = NULL;
    sys->tailp = &sys->head;

    logger->log = vlc_vaLogEarly;
    logger->sys = sys;
    return 0;
}

static void vlc_LogEarlyClose(libvlc_int_t *vlc, void *d)
{
    vlc_logger_early_t *sys = d;

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

static void vlc_vaLogDiscard(void *d, int type, const vlc_log_t *item,
                             const char *format, va_list ap)
{
    (void) d; (void) type; (void) item; (void) format; (void) ap;
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

    libvlc_priv(vlc)->logger = logger;

    if (unlikely(logger == NULL))
        return -1;

    vlc_rwlock_init(&logger->lock);

    if (vlc_LogEarlyOpen(logger))
    {
        logger->log = vlc_vaLogDiscard;
        return -1;
    }

    /* Announce who we are */
    msg_Dbg(vlc, "VLC media player - %s", VERSION_MESSAGE);
    msg_Dbg(vlc, "%s", COPYRIGHT_MESSAGE);
    msg_Dbg(vlc, "revision %s", psz_vlc_changeset);
    msg_Dbg(vlc, "configured with %s", CONFIGURE_LINE);
    return 0;
}

/**
 * Initializes the messages logging subsystem and drain the early messages to
 * the configured log.
 *
 * \return 0 on success, -1 on error.
 */
int vlc_LogInit(libvlc_int_t *vlc)
{
    vlc_logger_t *logger = libvlc_priv(vlc)->logger;
    void *early_sys = NULL;
    vlc_log_cb cb = PrintMsg;
    signed char verbosity;

    if (unlikely(logger == NULL))
        return -1;

#ifdef __ANDROID__
    cb = AndroidPrintMsg;
#elif defined (HAVE_ISATTY) && !defined (_WIN32)
    if (isatty(STDERR_FILENO) && var_InheritBool(vlc, "color"))
        cb = PrintColorMsg;
#endif

    if (var_InheritBool(vlc, "quiet"))
        verbosity = -1;
    else
    {
        const char *str = getenv("VLC_VERBOSE");

        if (str == NULL || sscanf(str, "%hhd", &verbosity) < 1)
            verbosity = var_InheritInteger(vlc, "verbose");
    }

    vlc_rwlock_wrlock(&logger->lock);

    if (logger->log == vlc_vaLogEarly)
        early_sys = logger->sys;

    logger->log = cb;
    logger->sys = (void *)(intptr_t)verbosity;
    vlc_rwlock_unlock(&logger->lock);

    if (early_sys != NULL)
        vlc_LogEarlyClose(vlc, early_sys);

    return 0;
}

/**
 * Sets the message logging callback.
 * \param cb message callback, or NULL to clear
 * \param data data pointer for the message callback
 */
void vlc_LogSet(libvlc_int_t *vlc, vlc_log_cb cb, void *opaque)
{
    vlc_logger_t *logger = libvlc_priv(vlc)->logger;

    if (unlikely(logger == NULL))
        return;

    if (cb == NULL)
        cb = vlc_vaLogDiscard;

    vlc_rwlock_wrlock(&logger->lock);
    logger->log = cb;
    logger->sys = opaque;
    vlc_rwlock_unlock(&logger->lock);

    /* Announce who we are */
    msg_Dbg (vlc, "VLC media player - %s", VERSION_MESSAGE);
    msg_Dbg (vlc, "%s", COPYRIGHT_MESSAGE);
    msg_Dbg (vlc, "revision %s", psz_vlc_changeset);
    msg_Dbg (vlc, "configured with %s", CONFIGURE_LINE);
}

void vlc_LogDeinit(libvlc_int_t *vlc)
{
    vlc_logger_t *logger = libvlc_priv(vlc)->logger;

    if (unlikely(logger == NULL))
        return;

    /* Flush early log messages (corner case: no call to vlc_LogInit()) */
    if (logger->log == vlc_vaLogEarly)
    {
        logger->log = vlc_vaLogDiscard;
        vlc_LogEarlyClose(vlc, logger->sys);
    }

    vlc_rwlock_destroy(&logger->lock);
    vlc_object_release(logger);
}
