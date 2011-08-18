/*****************************************************************************
 * messages.c: messages interface
 * This library provides an interface to the message queue to be used by other
 * modules, especially intf modules. See vlc_config.h for output configuration.
 *****************************************************************************
 * Copyright (C) 1998-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include <stdarg.h>                                       /* va_list for BSD */
#ifdef __APPLE__
# include <xlocale.h>
#elif defined(HAVE_LOCALE_H)
# include <locale.h>
#endif
#include <errno.h>                                                  /* errno */

#ifdef WIN32
#   include <vlc_network.h>          /* 'net_strerror' and 'WSAGetLastError' */
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>                                   /* close(), write() */
#endif

#include <assert.h>

#include <vlc_charset.h>
#include "../libvlc.h"

/*****************************************************************************
 * Local macros
 *****************************************************************************/
#if defined(HAVE_VA_COPY)
#   define vlc_va_copy(dest,src) va_copy(dest,src)
#elif defined(HAVE___VA_COPY)
#   define vlc_va_copy(dest,src) __va_copy(dest,src)
#else
#   define vlc_va_copy(dest,src) (dest)=(src)
#endif

static inline msg_bank_t *libvlc_bank (libvlc_int_t *inst)
{
    return (libvlc_priv (inst))->msg_bank;
}

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void PrintMsg ( vlc_object_t *, const msg_item_t * );

/**
 * Store all data required by messages interfaces.
 */
struct msg_bank_t
{
    /** Message queue lock */
    vlc_rwlock_t lock;

    /* Subscribers */
    int i_sub;
    msg_subscription_t **pp_sub;

    vlc_dictionary_t enabled_objects; ///< Enabled objects
    bool all_objects_enabled; ///< Should we print all objects?
};

/**
 * Initialize messages queues
 * This function initializes all message queues
 */
msg_bank_t *msg_Create (void)
{
    msg_bank_t *bank = malloc (sizeof (*bank));

    vlc_rwlock_init (&bank->lock);
    vlc_dictionary_init (&bank->enabled_objects, 0);
    bank->all_objects_enabled = true;

    bank->i_sub = 0;
    bank->pp_sub = NULL;

    return bank;
}

/**
 * Object Printing selection
 */
static void const * kObjectPrintingEnabled = &kObjectPrintingEnabled;
static void const * kObjectPrintingDisabled = &kObjectPrintingDisabled;


#undef msg_EnableObjectPrinting
void msg_EnableObjectPrinting (vlc_object_t *obj, const char * psz_object)
{
    msg_bank_t *bank = libvlc_bank (obj->p_libvlc);

    vlc_rwlock_wrlock (&bank->lock);
    if( !strcmp(psz_object, "all") )
        bank->all_objects_enabled = true;
    else
        vlc_dictionary_insert (&bank->enabled_objects, psz_object,
                               (void *)kObjectPrintingEnabled);
    vlc_rwlock_unlock (&bank->lock);
}

#undef msg_DisableObjectPrinting
void msg_DisableObjectPrinting (vlc_object_t *obj, const char * psz_object)
{
    msg_bank_t *bank = libvlc_bank (obj->p_libvlc);

    vlc_rwlock_wrlock (&bank->lock);
    if( !strcmp(psz_object, "all") )
        bank->all_objects_enabled = false;
    else
        vlc_dictionary_insert (&bank->enabled_objects, psz_object,
                               (void *)kObjectPrintingDisabled);
    vlc_rwlock_unlock (&bank->lock);
}

/**
 * Destroy the message queues
 *
 * This functions prints all messages remaining in the queues,
 * then frees all the allocated resources
 * No other messages interface functions should be called after this one.
 */
void msg_Destroy (msg_bank_t *bank)
{
    if (unlikely(bank->i_sub != 0))
        fputs ("stale interface subscribers (LibVLC might crash)\n", stderr);

    vlc_dictionary_clear (&bank->enabled_objects, NULL, NULL);

    vlc_rwlock_destroy (&bank->lock);
    free (bank);
}

struct msg_subscription_t
{
    libvlc_int_t   *instance;
    msg_callback_t  func;
    msg_cb_data_t  *opaque;
    int             verbosity;
};

/**
 * Subscribe to the message queue.
 * Whenever a message is emitted, a callback will be called.
 * Callback invocation are serialized within a subscription.
 *
 * @param instance LibVLC instance to get messages from
 * @param cb callback function
 * @param opaque data for the callback function
 * @return a subscription pointer, or NULL in case of failure
 */
msg_subscription_t *msg_Subscribe (libvlc_int_t *instance, msg_callback_t cb,
                                   msg_cb_data_t *opaque)
{
    msg_subscription_t *sub = malloc (sizeof (*sub));
    if (sub == NULL)
        return NULL;

    sub->instance = instance;
    sub->func = cb;
    sub->opaque = opaque;
    sub->verbosity = 2; /* by default, give all the messages */

    msg_bank_t *bank = libvlc_bank (instance);
    vlc_rwlock_wrlock (&bank->lock);
    TAB_APPEND (bank->i_sub, bank->pp_sub, sub);
    vlc_rwlock_unlock (&bank->lock);

    return sub;
}

/**
 * Unsubscribe from the message queue.
 * This function waits for the message callback to return if needed.
 */
void msg_Unsubscribe (msg_subscription_t *sub)
{
    msg_bank_t *bank = libvlc_bank (sub->instance);

    vlc_rwlock_wrlock (&bank->lock);
    TAB_REMOVE (bank->i_sub, bank->pp_sub, sub);
    vlc_rwlock_unlock (&bank->lock);
    free (sub);
}

void msg_SubscriptionSetVerbosity( msg_subscription_t *sub, const int i_verbosity )
{
    if( i_verbosity < 0 || i_verbosity > 2 ) return;

    msg_bank_t *bank = libvlc_bank ( sub->instance );

    vlc_rwlock_wrlock (&bank->lock);

    sub->verbosity = i_verbosity;

    vlc_rwlock_unlock (&bank->lock);
}
/*****************************************************************************
 * msg_*: print a message
 *****************************************************************************
 * These functions queue a message for later printing.
 *****************************************************************************/
void msg_Generic( vlc_object_t *p_this, int i_type, const char *psz_module,
                    const char *psz_format, ... )
{
    va_list args;

    va_start( args, psz_format );
    msg_GenericVa (p_this, i_type, psz_module, psz_format, args);
    va_end( args );
}

#undef msg_GenericVa
/**
 * Add a message to a queue
 *
 * This function provides basic functionnalities to other msg_* functions.
 * It adds a message to a queue (after having printed all stored messages if it
 * is full). If the message can't be converted to string in memory, it issues
 * a warning.
 */
void msg_GenericVa (vlc_object_t *p_this, int i_type,
                           const char *psz_module,
                           const char *psz_format, va_list _args)
{
    va_list      args;

    assert (p_this);

    if( p_this->i_flags & OBJECT_FLAGS_QUIET )
        return;

    msg_bank_t *bank = libvlc_bank (p_this->p_libvlc);

    /* C locale to get error messages in English in the logs */
    locale_t c = newlocale (LC_MESSAGES_MASK, "C", (locale_t)0);
    locale_t locale = uselocale (c);

#ifndef __GLIBC__
    /* Expand %m to strerror(errno) - only once */
    char buf[strlen( psz_format ) + 2001], *ptr;
    strcpy( buf, psz_format );
    ptr = (char*)buf;
    psz_format = (const char*) buf;

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

    /* Convert message to string  */
    static const char nomemstr[] = "<not enough memory to format message>";
    char *str;

    vlc_va_copy( args, _args );
    if (unlikely(vasprintf (&str, psz_format, args) == -1))
        str = (char *)nomemstr;
    va_end( args );

    uselocale (locale);
    freelocale (c);

    /* Fill message information fields */
    msg_item_t msg;

    msg.i_type = i_type;
    msg.i_object_id = (uintptr_t)p_this;
    msg.psz_object_type = p_this->psz_object_type;
    msg.psz_module = psz_module;
    msg.psz_msg = str;
    msg.psz_header = NULL;

    for (vlc_object_t *o = p_this; o != NULL; o = o->p_parent)
        if (o->psz_header != NULL)
        {
            msg.psz_header = o->psz_header;
            break;
        }

    PrintMsg( p_this, &msg );

    vlc_rwlock_rdlock (&bank->lock);
    for (int i = 0; i < bank->i_sub; i++)
    {
        msg_subscription_t *sub = bank->pp_sub[i];
        libvlc_priv_t *priv = libvlc_priv( sub->instance );
        msg_bank_t *bank = priv->msg_bank;
        void *val = vlc_dictionary_value_for_key( &bank->enabled_objects,
                                                  msg.psz_module );
        if( val == kObjectPrintingDisabled ) continue;
        if( val != kObjectPrintingEnabled  ) /*if not allowed */
        {
            val = vlc_dictionary_value_for_key( &bank->enabled_objects,
                                                msg.psz_object_type );
            if( val == kObjectPrintingDisabled ) continue;
            if( val == kObjectPrintingEnabled  ); /* Allowed */
            else if( !bank->all_objects_enabled ) continue;
        }
        switch( msg.i_type )
        {
            case VLC_MSG_INFO:
            case VLC_MSG_ERR:
                if( sub->verbosity < 0 ) continue;
                break;
            case VLC_MSG_WARN:
                if( sub->verbosity < 1 ) continue;
                break;
            case VLC_MSG_DBG:
                if( sub->verbosity < 2 ) continue;
                break;
        }

        sub->func (sub->opaque, &msg);
    }
    vlc_rwlock_unlock (&bank->lock);

    if (likely(str != (char *)nomemstr))
        free (str);
}

/*****************************************************************************
 * PrintMsg: output a standard message item to stderr
 *****************************************************************************
 * Print a message to stderr, with colour formatting if needed.
 *****************************************************************************/
static void PrintMsg ( vlc_object_t *p_this, const msg_item_t *p_item )
{
#   define COL(x,y)  "\033[" #x ";" #y "m"
#   define RED     COL(31,1)
#   define GREEN   COL(32,1)
#   define YELLOW  COL(0,33)
#   define WHITE   COL(0,1)
#   define GRAY    "\033[0m"
    static const char msgtype[4][9] = { "", " error", " warning", " debug" };
    static const char msgcolor[4][8] = { WHITE, RED, YELLOW, GRAY };

    libvlc_priv_t *priv = libvlc_priv (p_this->p_libvlc);
    int type = p_item->i_type;

    if (priv->i_verbose < 0 || priv->i_verbose < (type - VLC_MSG_ERR))
        return;

    const char *objtype = p_item->psz_object_type;
    msg_bank_t *bank = priv->msg_bank;
    void * val = vlc_dictionary_value_for_key (&bank->enabled_objects,
                                               p_item->psz_module);
    if( val == kObjectPrintingDisabled )
        return;
    if( val == kObjectPrintingEnabled )
        /* Allowed */;
    else
    {
        val = vlc_dictionary_value_for_key (&bank->enabled_objects,
                                            objtype);
        if( val == kObjectPrintingDisabled )
            return;
        if( val == kObjectPrintingEnabled )
            /* Allowed */;
        else if( !bank->all_objects_enabled )
            return;
    }

    /* Send the message to stderr */
    FILE *stream = stderr;
    int canc = vlc_savecancel ();

    flockfile (stream);
    fprintf (stream, priv->b_color ? "["GREEN"%p"GRAY"] " : "[%p] ",
            (void *)p_item->i_object_id);
    if (p_item->psz_header != NULL)
        utf8_fprintf (stream, "[%s] ", p_item->psz_header);
    utf8_fprintf (stream, "%s %s%s: ", p_item->psz_module, objtype,
                  msgtype[type]);
    if (priv->b_color)
        fputs (msgcolor[type], stream);
    fputs (p_item->psz_msg, stream);
    if (priv->b_color)
        fputs (GRAY, stream);
    putc_unlocked ('\n', stream);
#if defined (WIN32) || defined (__OS2__)
    fflush (stream);
#endif
    funlockfile (stream);
    vlc_restorecancel (canc);
}
