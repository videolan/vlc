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

#include <vlc_common.h>
#include <vlc_interface.h>
#include <vlc_charset.h>
#include "../libvlc.h"

/**
 * Emit a log message.
 * \param obj VLC object emitting the message or NULL
 * \param type VLC_MSG_* message type (info, error, warning or debug)
 * \param module name of module from which the message come
 *               (normally MODULE_STRING)
 * \param format printf-like message format
 */
void vlc_Log (vlc_object_t *obj, int type, const char *module,
              const char *format, ... )
{
    va_list args;

    va_start (args, format);
    vlc_vaLog (obj, type, module, format, args);
    va_end (args);
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

    size_t modlen = (p != NULL) ? (p - module) : 1;
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

    /* Pass message to the callback */
    libvlc_priv_t *priv = obj ? libvlc_priv (obj->p_libvlc) : NULL;

#ifdef _WIN32
    va_list ap;

    va_copy (ap, args);
    Win32DebugOutputMsg (priv ? &priv->log.verbose : NULL, type, &msg, format, ap);
    va_end (ap);
#endif

    if (priv) {
        vlc_rwlock_rdlock (&priv->log.lock);
        priv->log.cb (priv->log.opaque, type, &msg, format, args);
        vlc_rwlock_unlock (&priv->log.lock);
    }
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

/**
 * Sets the message logging callback.
 * \param cb message callback, or NULL to reset
 * \param data data pointer for the message callback
 */
void vlc_LogSet (libvlc_int_t *vlc, vlc_log_cb cb, void *opaque)
{
    libvlc_priv_t *priv = libvlc_priv (vlc);

    if (cb == NULL)
    {
#if defined (HAVE_ISATTY) && !defined (_WIN32)
        if (isatty (STDERR_FILENO) && var_InheritBool (vlc, "color"))
            cb = PrintColorMsg;
        else
#endif
            cb = PrintMsg;
        opaque = (void *)(intptr_t)priv->log.verbose;
    }

    vlc_rwlock_wrlock (&priv->log.lock);
    priv->log.cb = cb;
    priv->log.opaque = opaque;
    vlc_rwlock_unlock (&priv->log.lock);

    /* Announce who we are */
    msg_Dbg (vlc, "VLC media player - %s", VERSION_MESSAGE);
    msg_Dbg (vlc, "%s", COPYRIGHT_MESSAGE);
    msg_Dbg (vlc, "revision %s", psz_vlc_changeset);
    msg_Dbg (vlc, "configured with %s", CONFIGURE_LINE);
}

void vlc_LogInit (libvlc_int_t *vlc)
{
    libvlc_priv_t *priv = libvlc_priv (vlc);
    const char *str;

    if (var_InheritBool (vlc, "quiet"))
        priv->log.verbose = -1;
    else
    if ((str = getenv ("VLC_VERBOSE")) != NULL)
        priv->log.verbose = atoi (str);
    else
        priv->log.verbose = var_InheritInteger (vlc, "verbose");

    vlc_rwlock_init (&priv->log.lock);
    vlc_LogSet (vlc, NULL, NULL);
}

void vlc_LogDeinit (libvlc_int_t *vlc)
{
    libvlc_priv_t *priv = libvlc_priv (vlc);

    vlc_rwlock_destroy (&priv->log.lock);
}
