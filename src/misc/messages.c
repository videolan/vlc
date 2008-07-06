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

#ifdef HAVE_FCNTL_H
#   include <fcntl.h>                  /* O_CREAT, O_TRUNC, O_WRONLY, O_SYNC */
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

#define QUEUE priv->msg_bank.queue
#define LOCK_BANK vlc_mutex_lock( &priv->msg_bank.lock );
#define UNLOCK_BANK vlc_mutex_unlock( &priv->msg_bank.lock );

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void QueueMsg ( vlc_object_t *, int, const char *,
                       const char *, va_list );
static void FlushMsg ( msg_queue_t * );
static void PrintMsg ( vlc_object_t *, msg_item_t * );

/**
 * Initialize messages queues
 * This function initializes all message queues
 */
void msg_Create (libvlc_int_t *p_libvlc)
{
    libvlc_priv_t *priv = libvlc_priv (p_libvlc);
    vlc_mutex_init( &priv->msg_bank.lock );
    vlc_mutex_init( &QUEUE.lock );
    QUEUE.b_overflow = false;
    QUEUE.i_start = 0;
    QUEUE.i_stop = 0;
    QUEUE.i_sub = 0;
    QUEUE.pp_sub = 0;

#ifdef UNDER_CE
    QUEUE.logfile =
        CreateFile( L"vlc-log.txt", GENERIC_WRITE,
                    FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
                    CREATE_ALWAYS, 0, NULL );
    SetFilePointer( QUEUE.logfile, 0, NULL, FILE_END );
#endif
}

/**
 * Flush all message queues
 */
void msg_Flush (libvlc_int_t *p_libvlc)
{
    libvlc_priv_t *priv = libvlc_priv (p_libvlc);
    vlc_mutex_lock( &QUEUE.lock );
    FlushMsg( &QUEUE );
    vlc_mutex_unlock( &QUEUE.lock );
}

/**
 * Destroy the message queues
 *
 * This functions prints all messages remaining in the queues,
 * then frees all the allocated ressources
 * No other messages interface functions should be called after this one.
 */
void msg_Destroy (libvlc_int_t *p_libvlc)
{
    libvlc_priv_t *priv = libvlc_priv (p_libvlc);

    if( QUEUE.i_sub )
        msg_Err( p_libvlc, "stale interface subscribers" );

    FlushMsg( &QUEUE );

#ifdef UNDER_CE
    CloseHandle( QUEUE.logfile );
#endif
    /* Destroy lock */
    vlc_mutex_destroy( &QUEUE.lock );
    vlc_mutex_destroy( &priv->msg_bank.lock);
}

/**
 * Subscribe to a message queue.
 */
msg_subscription_t *__msg_Subscribe( vlc_object_t *p_this )
{
    libvlc_priv_t *priv = libvlc_priv (p_this->p_libvlc);
    msg_subscription_t *p_sub = malloc( sizeof( msg_subscription_t ) );

    if (p_sub == NULL)
        return NULL;

    LOCK_BANK;
    vlc_mutex_lock( &QUEUE.lock );

    TAB_APPEND( QUEUE.i_sub, QUEUE.pp_sub, p_sub );

    p_sub->i_start = QUEUE.i_start;
    p_sub->pi_stop = &QUEUE.i_stop;
    p_sub->p_msg   = QUEUE.msg;
    p_sub->p_lock  = &QUEUE.lock;

    vlc_mutex_unlock( &QUEUE.lock );
    UNLOCK_BANK;

    return p_sub;
}

/**
 * Unsubscribe from a message queue.
 */
void __msg_Unsubscribe( vlc_object_t *p_this, msg_subscription_t *p_sub )
{
    libvlc_priv_t *priv = libvlc_priv (p_this->p_libvlc);

    LOCK_BANK;
    vlc_mutex_lock( &QUEUE.lock );
    for( int j = 0 ; j< QUEUE.i_sub ; j++ )
    {
        if( QUEUE.pp_sub[j] == p_sub )
        {
            REMOVE_ELEM( QUEUE.pp_sub, QUEUE.i_sub, j );
            free( p_sub );
        }
    }
    vlc_mutex_unlock( &QUEUE.lock );
    UNLOCK_BANK;
}

/*****************************************************************************
 * __msg_*: print a message
 *****************************************************************************
 * These functions queue a message for later printing.
 *****************************************************************************/
void __msg_Generic( vlc_object_t *p_this, int i_type, const char *psz_module,
                    const char *psz_format, ... )
{
    va_list args;

    va_start( args, psz_format );
    QueueMsg( p_this, i_type, psz_module, psz_format, args );
    va_end( args );
}

void __msg_GenericVa( vlc_object_t *p_this, int i_type, const char *psz_module,
                      const char *psz_format, va_list args )
{
    QueueMsg( p_this, i_type, psz_module, psz_format, args );
}

/* Generic functions used when variadic macros are not available. */
#define DECLARE_MSG_FN( FN_NAME, FN_TYPE ) \
    void FN_NAME( vlc_object_t *p_this, const char *psz_format, ... ) \
    { \
        va_list args; \
        va_start( args, psz_format ); \
        QueueMsg( p_this, FN_TYPE, "unknown", psz_format, args ); \
        va_end( args ); \
    } \
    struct _
/**
 * Output an informational message.
 * \note Do not use this for debug messages
 * \see input_AddInfo
 */
DECLARE_MSG_FN( __msg_Info, VLC_MSG_INFO );
/**
 * Output an error message.
 */
DECLARE_MSG_FN( __msg_Err,  VLC_MSG_ERR );
/**
 * Output a waring message
 */
DECLARE_MSG_FN( __msg_Warn, VLC_MSG_WARN );
/**
 * Output a debug message
 */
DECLARE_MSG_FN( __msg_Dbg,  VLC_MSG_DBG );

/**
 * Add a message to a queue
 *
 * This function provides basic functionnalities to other msg_* functions.
 * It adds a message to a queue (after having printed all stored messages if it
 * is full). If the message can't be converted to string in memory, it issues
 * a warning.
 */
static void QueueMsg( vlc_object_t *p_this, int i_type, const char *psz_module,
                      const char *psz_format, va_list _args )
{
    assert (p_this);
    libvlc_priv_t *priv = libvlc_priv (p_this->p_libvlc);
    int         i_header_size;             /* Size of the additionnal header */
    vlc_object_t *p_obj;
    char *       psz_str = NULL;                 /* formatted message string */
    char *       psz_header = NULL;
    va_list      args;
    msg_item_t * p_item = NULL;                        /* pointer to message */
    msg_item_t   item;                    /* message in case of a full queue */
    msg_queue_t *p_queue;

#if !defined(HAVE_VASPRINTF) || defined(__APPLE__) || defined(SYS_BEOS)
    int          i_size = strlen(psz_format) + INTF_MAX_MSG_SIZE;
#endif

    if( p_this->i_flags & OBJECT_FLAGS_QUIET ||
        (p_this->i_flags & OBJECT_FLAGS_NODBG && i_type == VLC_MSG_DBG) )
        return;

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
#if defined(HAVE_VASPRINTF) && !defined(__APPLE__) && !defined( SYS_BEOS )
    vlc_va_copy( args, _args );
    if( vasprintf( &psz_str, psz_format, args ) == -1 )
        psz_str = NULL;
    va_end( args );
#else
    psz_str = (char*) malloc( i_size );
#endif

    if( psz_str == NULL )
    {
#ifdef __GLIBC__
        fprintf( stderr, "main warning: can't store message (%m): " );
#else
        char psz_err[1001];
#ifndef WIN32
        /* we're not using GLIBC, so we are sure that the error description
         * will be stored in the buffer we provide to strerror_r() */
        strerror_r( errno, psz_err, 1001 );
#else
        strncpy( psz_err, strerror( errno ), 1001 );
#endif
        psz_err[1000] = '\0';
        fprintf( stderr, "main warning: can't store message (%s): ", psz_err );
#endif
        vlc_va_copy( args, _args );
        /* We should use utf8_vfprintf - but it calls malloc()... */
        vfprintf( stderr, psz_format, args );
        va_end( args );
        fputs( "\n", stderr );
        return;
    }

    i_header_size = 0;
    p_obj = p_this;
    while( p_obj != NULL )
    {
        char *psz_old = NULL;
        if( p_obj->psz_header )
        {
            i_header_size += strlen( p_obj->psz_header ) + 4;
            if( psz_header )
            {
                psz_old = strdup( psz_header );
                psz_header = (char*)realloc( psz_header, i_header_size );
                snprintf( psz_header, i_header_size , "[%s] %s",
                          p_obj->psz_header, psz_old );
            }
            else
            {
                psz_header = (char *)malloc( i_header_size );
                snprintf( psz_header, i_header_size, "[%s]",
                          p_obj->psz_header );
            }
        }
        free( psz_old );
        p_obj = p_obj->p_parent;
    }

#if !defined(HAVE_VASPRINTF) || defined(__APPLE__) || defined(SYS_BEOS)
    vlc_va_copy( args, _args );
    vsnprintf( psz_str, i_size, psz_format, args );
    va_end( args );
    psz_str[ i_size - 1 ] = 0; /* Just in case */
#endif

    LOCK_BANK;
    p_queue = &QUEUE;
    vlc_mutex_lock( &p_queue->lock );

    /* Check there is room in the queue for our message */
    if( p_queue->b_overflow )
    {
        FlushMsg( p_queue );

        if( ((p_queue->i_stop - p_queue->i_start + 1) % VLC_MSG_QSIZE) == 0 )
        {
            /* Still in overflow mode, print from a dummy item */
            p_item = &item;
        }
        else
        {
            /* Pheeew, at last, there is room in the queue! */
            p_queue->b_overflow = false;
        }
    }
    else if( ((p_queue->i_stop - p_queue->i_start + 2) % VLC_MSG_QSIZE) == 0 )
    {
        FlushMsg( p_queue );

        if( ((p_queue->i_stop - p_queue->i_start + 2) % VLC_MSG_QSIZE) == 0 )
        {
            p_queue->b_overflow = true;

            /* Put the overflow message in the queue */
            p_item = p_queue->msg + p_queue->i_stop;
            p_queue->i_stop = (p_queue->i_stop + 1) % VLC_MSG_QSIZE;

            p_item->i_type =        VLC_MSG_WARN;
            p_item->i_object_id =   p_this->i_object_id;
            p_item->psz_object_type = p_this->psz_object_type;
            p_item->psz_module =    strdup( "message" );
            p_item->psz_msg =       strdup( "message queue overflowed" );
            p_item->psz_header =    NULL;

            PrintMsg( p_this, p_item );
            /* We print from a dummy item */
            p_item = &item;
        }
    }

    if( !p_queue->b_overflow )
    {
        /* Put the message in the queue */
        p_item = p_queue->msg + p_queue->i_stop;
        p_queue->i_stop = (p_queue->i_stop + 1) % VLC_MSG_QSIZE;
    }

    /* Fill message information fields */
    p_item->i_type =        i_type;
    p_item->i_object_id =   p_this->i_object_id;
    p_item->psz_object_type = p_this->psz_object_type;
    p_item->psz_module =    strdup( psz_module );
    p_item->psz_msg =       psz_str;
    p_item->psz_header =    psz_header;

    PrintMsg( p_this, p_item );

    if( p_queue->b_overflow )
    {
        free( p_item->psz_module );
        free( p_item->psz_msg );
        free( p_item->psz_header );
    }

    vlc_mutex_unlock ( &p_queue->lock );
    UNLOCK_BANK;
}

/* following functions are local */

/*****************************************************************************
 * FlushMsg
 *****************************************************************************
 * Print all messages remaining in queue. MESSAGE QUEUE MUST BE LOCKED, since
 * this function does not check the lock.
 *****************************************************************************/
static void FlushMsg ( msg_queue_t *p_queue )
{
    int i_index, i_start, i_stop;

    /* Get the maximum message index that can be freed */
    i_stop = p_queue->i_stop;

    /* Check until which value we can free messages */
    for( i_index = 0; i_index < p_queue->i_sub; i_index++ )
    {
        i_start = p_queue->pp_sub[ i_index ]->i_start;

        /* If this subscriber is late, we don't free messages before
         * his i_start value, otherwise he'll miss messages */
        if(   ( i_start < i_stop
               && (p_queue->i_stop <= i_start || i_stop <= p_queue->i_stop) )
           || ( i_stop < i_start
               && (i_stop <= p_queue->i_stop && p_queue->i_stop <= i_start) ) )
        {
            i_stop = i_start;
        }
    }

    /* Free message data */
    for( i_index = p_queue->i_start;
         i_index != i_stop;
         i_index = (i_index+1) % VLC_MSG_QSIZE )
    {
        free( p_queue->msg[i_index].psz_msg );
        free( p_queue->msg[i_index].psz_module );
        free( p_queue->msg[i_index].psz_header );
    }

    /* Update the new start value */
    p_queue->i_start = i_index;
}

/*****************************************************************************
 * PrintMsg: output a standard message item to stderr
 *****************************************************************************
 * Print a message to stderr, with colour formatting if needed.
 *****************************************************************************/
static void PrintMsg ( vlc_object_t * p_this, msg_item_t * p_item )
{
#   define COL(x)  "\033[" #x ";1m"
#   define RED     COL(31)
#   define GREEN   COL(32)
#   define YELLOW  COL(33)
#   define WHITE   COL(0)
#   define GRAY    "\033[0m"

#ifdef UNDER_CE
    int i_dummy;
#endif
    static const char ppsz_type[4][9] = { "", " error", " warning", " debug" };
    static const char ppsz_color[4][8] = { WHITE, RED, YELLOW, GRAY };
    const char *psz_object;
    libvlc_priv_t *priv = libvlc_priv (p_this->p_libvlc);
    int i_type = p_item->i_type;

    switch( i_type )
    {
        case VLC_MSG_ERR:
            if( priv->i_verbose < 0 ) return;
            break;
        case VLC_MSG_INFO:
            if( priv->i_verbose < 0 ) return;
            break;
        case VLC_MSG_WARN:
            if( priv->i_verbose < 1 ) return;
            break;
        case VLC_MSG_DBG:
            if( priv->i_verbose < 2 ) return;
            break;
    }

    psz_object = p_item->psz_object_type;

#ifdef UNDER_CE
#   define CE_WRITE(str) WriteFile( QUEUE.logfile, \
                                    str, strlen(str), &i_dummy, NULL );
    CE_WRITE( p_item->psz_module );
    CE_WRITE( " " );
    CE_WRITE( psz_object );
    CE_WRITE( ppsz_type[i_type] );
    CE_WRITE( ": " );
    CE_WRITE( p_item->psz_msg );
    CE_WRITE( "\r\n" );
    FlushFileBuffers( QUEUE.logfile );

#else
    /* Send the message to stderr */
    if( priv->b_color )
    {
        if( p_item->psz_header )
        {
            utf8_fprintf( stderr, "[" GREEN "%.8i" GRAY "] %s %s %s%s: %s%s" GRAY
                              "\n",
                         p_item->i_object_id, p_item->psz_header,
                         p_item->psz_module, psz_object,
                         ppsz_type[i_type], ppsz_color[i_type],
                         p_item->psz_msg );
        }
        else
        {
             utf8_fprintf( stderr, "[" GREEN "%.8i" GRAY "] %s %s%s: %s%s" GRAY "\n",
                         p_item->i_object_id, p_item->psz_module, psz_object,
                         ppsz_type[i_type], ppsz_color[i_type],
                         p_item->psz_msg );
        }
    }
    else
    {
        if( p_item->psz_header )
        {
            utf8_fprintf( stderr, "[%.8i] %s %s %s%s: %s\n", p_item->i_object_id,
                         p_item->psz_header, p_item->psz_module,
                         psz_object, ppsz_type[i_type], p_item->psz_msg );
        }
        else
        {
            utf8_fprintf( stderr, "[%.8i] %s %s%s: %s\n", p_item->i_object_id,
                         p_item->psz_module, psz_object, ppsz_type[i_type],
                         p_item->psz_msg );
        }
    }

#   if defined(WIN32)
    fflush( stderr );
#   endif
#endif
}

static msg_context_t* GetContext(void)
{
    msg_context_t *p_ctx = vlc_threadvar_get( &msg_context_global_key );
    if( p_ctx == NULL )
    {
        MALLOC_NULL( p_ctx, msg_context_t );
        p_ctx->psz_message = NULL;
        vlc_threadvar_set( &msg_context_global_key, p_ctx );
    }
    return p_ctx;
}

void msg_StackDestroy (void *data)
{
    msg_context_t *p_ctx = data;

    free (p_ctx->psz_message);
    free (p_ctx);
}

void msg_StackSet( int i_code, const char *psz_message, ... )
{
    va_list ap;
    msg_context_t *p_ctx = GetContext();

    if( p_ctx == NULL )
        return;
    free( p_ctx->psz_message );

    va_start( ap, psz_message );
    if( vasprintf( &p_ctx->psz_message, psz_message, ap ) == -1 )
        p_ctx->psz_message = NULL;
    va_end( ap );

    p_ctx->i_code = i_code;
}

void msg_StackAdd( const char *psz_message, ... )
{
    char *psz_tmp;
    va_list ap;
    msg_context_t *p_ctx = GetContext();

    if( p_ctx == NULL )
        return;

    va_start( ap, psz_message );
    if( vasprintf( &psz_tmp, psz_message, ap ) == -1 )
        psz_tmp = NULL;
    va_end( ap );

    if( !p_ctx->psz_message )
        p_ctx->psz_message = psz_tmp;
    else
    {
        char *psz_new;
        if( asprintf( &psz_new, "%s: %s", psz_tmp, p_ctx->psz_message ) == -1 )
            psz_new = NULL;

        free( p_ctx->psz_message );
        p_ctx->psz_message = psz_new;
        free( psz_tmp );
    }
}

const char* msg_StackMsg( void )
{
    msg_context_t *p_ctx = GetContext();
    assert( p_ctx );
    return p_ctx->psz_message;
}
