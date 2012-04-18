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

#include <stdarg.h>                                       /* va_list for BSD */
#ifdef __APPLE__
# include <xlocale.h>
#elif defined(HAVE_LOCALE_H)
# include <locale.h>
#endif
#include <errno.h>                                                  /* errno */
#include <assert.h>

#include <vlc_common.h>
#include <vlc_interface.h>
#ifdef WIN32
#   include <vlc_network.h>          /* 'net_strerror' and 'WSAGetLastError' */
#endif
#include <vlc_charset.h>
#include "../libvlc.h"

/**
 * Store all data required by messages interfaces.
 */
vlc_rwlock_t msg_lock = VLC_STATIC_RWLOCK;
msg_subscription_t *msg_head;

/**
 * Subscribe to the message queue.
 * Whenever a message is emitted, a callback will be called.
 * Callback invocation are serialized within a subscription.
 *
 * @param cb callback function
 * @param opaque data for the callback function
 */
void vlc_Subscribe (msg_subscription_t *sub, msg_callback_t cb, void *opaque)
{
    sub->prev = NULL;
    sub->func = cb;
    sub->opaque = opaque;

    vlc_rwlock_wrlock (&msg_lock);
    sub->next = msg_head;
    msg_head = sub;
    vlc_rwlock_unlock (&msg_lock);
}

/**
 * Unsubscribe from the message queue.
 * This function waits for the message callback to return if needed.
 */
void vlc_Unsubscribe (msg_subscription_t *sub)
{
    vlc_rwlock_wrlock (&msg_lock);
    if (sub->next != NULL)
        sub->next->prev = sub->prev;
    if (sub->prev != NULL)
        sub->prev->next = sub->next;
    else
    {
        assert (msg_head == sub);
        msg_head = sub->next;
    }
    vlc_rwlock_unlock (&msg_lock);
}

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

static void PrintColorMsg (void *, int, const msg_item_t *,
                           const char *, va_list);
static void PrintMsg (void *, int, const msg_item_t *, const char *, va_list);
#ifdef WIN32
static void Win32DebugOutputMsg (void *, int , const msg_item_t *,
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

    /* C locale to get error messages in English in the logs */
    locale_t c = newlocale (LC_MESSAGES_MASK, "C", (locale_t)0);
    locale_t locale = uselocale (c);

#ifndef __GLIBC__
    /* Expand %m to strerror(errno) - only once */
    char buf[strlen(format) + 2001], *ptr;
    strcpy (buf, format);
    ptr = (char*)buf;
    format = (const char*) buf;

    for( ;; )
    {
        ptr = strchr( ptr, '%' );
        if( ptr == NULL )
            break;

        if( ptr[1] == 'm' )
        {
            char errbuf[2001];
            size_t errlen;

#ifndef WIN32
            strerror_r( errno, errbuf, 1001 );
#else
            int sockerr = WSAGetLastError( );
            if( sockerr )
            {
                strncpy( errbuf, net_strerror( sockerr ), 1001 );
                WSASetLastError( sockerr );
            }
            if ((sockerr == 0)
             || (strcmp ("Unknown network stack error", errbuf) == 0))
                strncpy( errbuf, strerror( errno ), 1001 );
#endif
            errbuf[1000] = 0;

            /* Escape '%' from the error string */
            for( char *percent = strchr( errbuf, '%' );
                 percent != NULL;
                 percent = strchr( percent + 2, '%' ) )
            {
                memmove( percent + 1, percent, strlen( percent ) + 1 );
            }

            errlen = strlen( errbuf );
            memmove( ptr + errlen, ptr + 2, strlen( ptr + 2 ) + 1 );
            memcpy( ptr, errbuf, errlen );
            break; /* Only once, so we don't overflow */
        }

        /* Looks for conversion specifier... */
        do
            ptr++;
        while( *ptr && ( strchr( "diouxXeEfFgGaAcspn%", *ptr ) == NULL ) );
        if( *ptr )
            ptr++; /* ...and skip it */
    }
#endif

    /* Fill message information fields */
    msg_item_t msg;

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

    /* Pass message to subscribers */
    libvlc_priv_t *priv = libvlc_priv (obj->p_libvlc);

    va_list ap;

    va_copy (ap, args);
    if (priv->b_color)
        PrintColorMsg (&priv->i_verbose, type, &msg, format, ap);
    else
        PrintMsg (&priv->i_verbose, type, &msg, format, ap);
    va_end (ap);

#ifdef WIN32
    va_copy (ap, args);
    Win32DebugOutputMsg (&priv->i_verbose, type, &msg, format, ap);
    va_end (ap);
#endif

    vlc_rwlock_rdlock (&msg_lock);
    for (msg_subscription_t *sub = msg_head; sub != NULL; sub = sub->next)
    {
        va_copy (ap, args);
        sub->func (sub->opaque, type, &msg, format, ap);
        va_end (ap);
    }
    vlc_rwlock_unlock (&msg_lock);

    uselocale (locale);
    freelocale (c);
}

static const char msg_type[4][9] = { "", " error", " warning", " debug" };
#define COL(x,y)  "\033[" #x ";" #y "m"
#define RED     COL(31,1)
#define GREEN   COL(32,1)
#define YELLOW  COL(0,33)
#define WHITE   COL(0,1)
#define GRAY    "\033[0m"
static const char msg_color[4][8] = { WHITE, RED, YELLOW, GRAY };

static void PrintColorMsg (void *d, int type, const msg_item_t *p_item,
                           const char *format, va_list ap)
{
    const signed char *pverbose = d;
    FILE *stream = stderr;

    if (*pverbose < 0 || *pverbose < (type - VLC_MSG_ERR))
        return;

    int canc = vlc_savecancel ();

    flockfile (stream);
    fprintf (stream, "["GREEN"%p"GRAY"] ", (void *)p_item->i_object_id);
    if (p_item->psz_header != NULL)
        utf8_fprintf (stream, "[%s] ", p_item->psz_header);
    utf8_fprintf (stream, "%s %s%s: %s", p_item->psz_module,
                  p_item->psz_object_type, msg_type[type], msg_color[type]);
    utf8_vfprintf (stream, format, ap);
    fputs (GRAY"\n", stream);
#if defined (WIN32) || defined (__OS2__)
    fflush (stream);
#endif
    funlockfile (stream);
    vlc_restorecancel (canc);
}

static void PrintMsg (void *d, int type, const msg_item_t *p_item,
                      const char *format, va_list ap)
{
    const signed char *pverbose = d;
    FILE *stream = stderr;

    if (*pverbose < 0 || *pverbose < (type - VLC_MSG_ERR))
        return;

    int canc = vlc_savecancel ();

    flockfile (stream);
    fprintf (stream, "[%p] ", (void *)p_item->i_object_id);
    if (p_item->psz_header != NULL)
        utf8_fprintf (stream, "[%s] ", p_item->psz_header);
    utf8_fprintf (stream, "%s %s%s: ", p_item->psz_module,
                  p_item->psz_object_type, msg_type[type]);
    utf8_vfprintf (stream, format, ap);
    putc_unlocked ('\n', stream);
#if defined (WIN32) || defined (__OS2__)
    fflush (stream);
#endif
    funlockfile (stream);
    vlc_restorecancel (canc);
}

#ifdef WIN32
static void Win32DebugOutputMsg (void* d, int type, const msg_item_t *p_item,
                                 const char *format, va_list dol)
{
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
        OutputDebugString(msg);
    }
    free(msg);
}
#endif
