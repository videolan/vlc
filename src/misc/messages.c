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

#include <vlc/vlc.h>

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

#define QUEUE(i) p_this->p_libvlc->msg_bank.queues[i]
#define LOCK_BANK vlc_mutex_lock( &p_this->p_libvlc->msg_bank.lock );
#define UNLOCK_BANK vlc_mutex_unlock( &p_this->p_libvlc->msg_bank.lock );

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void QueueMsg ( vlc_object_t *, int, int , const char *,
                       const char *, va_list );
static void FlushMsg ( msg_queue_t * );
static void PrintMsg ( vlc_object_t *, msg_item_t * );

/**
 * Initialize messages queues
 * This function initializes all message queues
 */
void __msg_Create( vlc_object_t *p_this )
{
    int i;
    vlc_mutex_init( (vlc_object_t *)NULL,
                    &(p_this->p_libvlc->msg_bank.lock) );

    for( i = 0; i < 2; i++ )
    {
         vlc_mutex_init( (vlc_object_t *)NULL, &QUEUE(i).lock );
         QUEUE(i).b_overflow = VLC_FALSE;
         QUEUE(i).i_id = i;
         QUEUE(i).i_start = 0;
         QUEUE(i).i_stop = 0;
         QUEUE(i).i_sub = 0;
         QUEUE(i).pp_sub = 0;
    }

#ifdef UNDER_CE
    QUEUE(MSG_QUEUE_NORMAL).logfile =
        CreateFile( L"vlc-log.txt", GENERIC_WRITE,
                    FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
                    CREATE_ALWAYS, 0, NULL );
    SetFilePointer( QUEUE(MSG_QUEUE_NORMAL).logfile, 0, NULL, FILE_END );
#endif
}

/**
 * Flush all message queues
 */
void __msg_Flush( vlc_object_t *p_this )
{
    int i;
    for( i = 0 ; i < NB_QUEUES ; i++ )
    {
        vlc_mutex_lock( &QUEUE(i).lock );
        FlushMsg( &QUEUE(i) );
        vlc_mutex_unlock( &QUEUE(i).lock );
    }
}

/**
 * Destroy the message queues
 *
 * This functions prints all messages remaining in the queues,
 * then frees all the allocated ressources
 * No other messages interface functions should be called after this one.
 */
void __msg_Destroy( vlc_object_t *p_this )
{
    int i;
    for( i = NB_QUEUES -1 ; i >= 0;  i-- )
    {
        if( QUEUE(i).i_sub )
            msg_Err( p_this, "stale interface subscribers" );

        FlushMsg( &QUEUE(i) );

#ifdef UNDER_CE
        if( i == MSG_QUEUE_NORMAL )
            CloseHandle( QUEUE(MSG_QUEUE_NORMAL).logfile );
#endif
        /* Destroy lock */
        vlc_mutex_destroy( &QUEUE(i).lock );
    }
    vlc_mutex_destroy( &(p_this->p_libvlc->msg_bank.lock) );
}

/**
 * Subscribe to a message queue.
 */
msg_subscription_t *__msg_Subscribe( vlc_object_t *p_this, int i )
{
    msg_subscription_t *p_sub = malloc( sizeof( msg_subscription_t ) );

    assert( i < NB_QUEUES );

    LOCK_BANK;
    vlc_mutex_lock( &QUEUE(i).lock );

    TAB_APPEND( QUEUE(i).i_sub, QUEUE(i).pp_sub, p_sub );

    p_sub->i_start = QUEUE(i).i_start;
    p_sub->pi_stop = &QUEUE(i).i_stop;
    p_sub->p_msg   = QUEUE(i).msg;
    p_sub->p_lock  = &QUEUE(i).lock;

    vlc_mutex_unlock( &QUEUE(i).lock );
    UNLOCK_BANK;

    return p_sub;
}

/**
 * Unsubscribe from a message queue.
 */
void __msg_Unsubscribe( vlc_object_t *p_this, msg_subscription_t *p_sub )
{
    int i,j;

    LOCK_BANK;
    for( i = 0 ; i< NB_QUEUES ; i++ )
    {
        vlc_mutex_lock( &QUEUE(i).lock );
        for( j = 0 ; j< QUEUE(i).i_sub ; j++ )
        {
            if( QUEUE(i).pp_sub[j] == p_sub )
            {
                REMOVE_ELEM( QUEUE(i).pp_sub, QUEUE(i).i_sub, j );
                if( p_sub ) free( p_sub );
            }
        }
        vlc_mutex_unlock( & QUEUE(i).lock );
    }
    UNLOCK_BANK;
}

const char *msg_GetObjectTypeName(int i_object_type )
{
    switch( i_object_type )
    {
        case VLC_OBJECT_GLOBAL: return "global";
        case VLC_OBJECT_LIBVLC: return "libvlc";
        case VLC_OBJECT_MODULE: return "module";
        case VLC_OBJECT_INTF: return "interface";
        case VLC_OBJECT_PLAYLIST: return "playlist";
        case VLC_OBJECT_ITEM: return "item";
        case VLC_OBJECT_INPUT: return "input";
        case VLC_OBJECT_DECODER: return "decoder";
        case VLC_OBJECT_PACKETIZER: return "packetizer";
        case VLC_OBJECT_ENCODER: return "encoder";
        case VLC_OBJECT_VOUT: return "video output";
        case VLC_OBJECT_AOUT: return "audio output";
        case VLC_OBJECT_SOUT: return "stream output";
        case VLC_OBJECT_HTTPD: return "http server";
        case VLC_OBJECT_HTTPD_HOST: return "http server";
        case VLC_OBJECT_DIALOGS: return "dialogs provider";
        case VLC_OBJECT_VLM: return "vlm";
        case VLC_OBJECT_ANNOUNCE: return "announce handler";
        case VLC_OBJECT_DEMUX: return "demuxer";
        case VLC_OBJECT_ACCESS: return "access";
        case VLC_OBJECT_META_ENGINE: return "meta engine";
        default: return "private";
    }
}

/*****************************************************************************
 * __msg_*: print a message
 *****************************************************************************
 * These functions queue a message for later printing.
 *****************************************************************************/
void __msg_Generic( vlc_object_t *p_this, int i_queue, int i_type,
                    const char *psz_module,
                    const char *psz_format, ... )
{
    va_list args;

    va_start( args, psz_format );
    QueueMsg( p_this, i_queue, i_type, psz_module, psz_format, args );
    va_end( args );
}

void __msg_GenericVa( vlc_object_t *p_this, int i_queue,
                      int i_type, const char *psz_module,
                      const char *psz_format, va_list args )
{
    QueueMsg( p_this, i_queue, i_type, psz_module, psz_format, args );
}

/* Generic functions used when variadic macros are not available. */
#define DECLARE_MSG_FN( FN_NAME, FN_TYPE ) \
    void FN_NAME( vlc_object_t *p_this, const char *psz_format, ... ) \
    { \
        va_list args; \
        va_start( args, psz_format ); \
        QueueMsg( p_this,MSG_QUEUE_NORMAL, FN_TYPE, "unknown", \
                  psz_format, args ); \
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
static void QueueMsg( vlc_object_t *p_this, int i_queue, int i_type,
                      const char *psz_module,
                      const char *psz_format, va_list _args )
{
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

    if( p_this == NULL )
    {
#ifndef NDEBUG
        if( i_type == VLC_MSG_DBG )
            return;
#endif
        utf8_vfprintf( stderr, psz_format, _args );
        fputc ('\n', stderr);
        return;
    }

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
        if( psz_old ) free( psz_old );
        p_obj = p_obj->p_parent;
    }

#if !defined(HAVE_VASPRINTF) || defined(__APPLE__) || defined(SYS_BEOS)
    vlc_va_copy( args, _args );
    vsnprintf( psz_str, i_size, psz_format, args );
    va_end( args );
    psz_str[ i_size - 1 ] = 0; /* Just in case */
#endif

    assert( i_queue < NB_QUEUES );
    LOCK_BANK;
    p_queue = &QUEUE(i_queue) ;
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
            p_queue->b_overflow = VLC_FALSE;
        }
    }
    else if( ((p_queue->i_stop - p_queue->i_start + 2) % VLC_MSG_QSIZE) == 0 )
    {
        FlushMsg( p_queue );

        if( ((p_queue->i_stop - p_queue->i_start + 2) % VLC_MSG_QSIZE) == 0 )
        {
            p_queue->b_overflow = VLC_TRUE;

            if( p_queue->i_id == MSG_QUEUE_NORMAL )
            {
               /* Put the overflow message in the queue */
                p_item = p_queue->msg + p_queue->i_stop;
                p_queue->i_stop = (p_queue->i_stop + 1) % VLC_MSG_QSIZE;

                p_item->i_type =        VLC_MSG_WARN;
                p_item->i_object_id =   p_this->i_object_id;
                p_item->i_object_type = p_this->i_object_type;
                p_item->psz_module =    strdup( "message" );
                p_item->psz_msg =       strdup( "message queue overflowed" );
                p_item->psz_header =    NULL;

               PrintMsg( p_this, p_item );
               /* We print from a dummy item */
               p_item = &item;
            }
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
    p_item->i_object_type = p_this->i_object_type;
    p_item->psz_module =    strdup( psz_module );
    p_item->psz_msg =       psz_str;
    p_item->psz_header =    psz_header;

    if( p_queue->i_id == MSG_QUEUE_NORMAL )
        PrintMsg( p_this, p_item );

    if( p_queue->b_overflow )
    {
        if( p_item->psz_module )
            free( p_item->psz_module );
        if( p_item->psz_msg )
            free( p_item->psz_msg );
        if( p_item->psz_header )
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
        if( p_queue->msg[i_index].psz_msg )
            free( p_queue->msg[i_index].psz_msg );
        if( p_queue->msg[i_index].psz_module )
            free( p_queue->msg[i_index].psz_module );
        if( p_queue->msg[i_index].psz_header )
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
    static const char * ppsz_type[4] = { "", " error", " warning", " debug" };
    static const char *ppsz_color[4] = { WHITE, RED, YELLOW, GRAY };
    const char *psz_object;
    int i_type = p_item->i_type;

    switch( i_type )
    {
        case VLC_MSG_ERR:
            if( p_this->p_libvlc->i_verbose < 0 ) return;
            break;
        case VLC_MSG_INFO:
            if( p_this->p_libvlc->i_verbose < 0 ) return;
            break;
        case VLC_MSG_WARN:
            if( p_this->p_libvlc->i_verbose < 1 ) return;
            break;
        case VLC_MSG_DBG:
            if( p_this->p_libvlc->i_verbose < 2 ) return;
            break;
    }

    psz_object = msg_GetObjectTypeName(p_item->i_object_type);

#ifdef UNDER_CE
#   define CE_WRITE(str) WriteFile( QUEUE(MSG_QUEUE_NORMAL).logfile, \
                                    str, strlen(str), &i_dummy, NULL );
    CE_WRITE( p_item->psz_module );
    CE_WRITE( " " );
    CE_WRITE( psz_object );
    CE_WRITE( ppsz_type[i_type] );
    CE_WRITE( ": " );
    CE_WRITE( p_item->psz_msg );
    CE_WRITE( "\r\n" );
    FlushFileBuffers( QUEUE(MSG_QUEUE_NORMAL).logfile );

#else
    /* Send the message to stderr */
    if( p_this->p_libvlc->b_color )
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

void msg_StackSet( int i_code, const char *psz_message, ... )
{
    va_list ap;
    msg_context_t *p_ctx = GetContext();
    assert( p_ctx );

    va_start( ap, psz_message );
    free( p_ctx->psz_message );

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
    assert( p_ctx );

    va_start( ap, psz_message );
    if( vasprintf( &psz_tmp, psz_message, ap ) == -1 )
        psz_tmp = NULL;
    va_end( ap );

    if( !p_ctx->psz_message )
        p_ctx->psz_message = psz_tmp;
    else
    {
        char *psz_old = malloc( strlen( p_ctx->psz_message ) + 1 );
        memcpy( psz_old, p_ctx->psz_message, strlen( p_ctx->psz_message ) + 1 );
        p_ctx->psz_message = realloc( p_ctx->psz_message,
                                      strlen( p_ctx->psz_message ) +
                                      /* ':', ' ', '0' */
                                      strlen( psz_tmp ) + 3 );
        sprintf( p_ctx->psz_message, "%s: %s", psz_tmp, psz_old );
        free( psz_tmp ); free( psz_old );
    }
}

const char* msg_StackMsg( void )
{
    msg_context_t *p_ctx = GetContext();
    assert( p_ctx );
    return p_ctx->psz_message;
}
