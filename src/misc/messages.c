/*****************************************************************************
 * messages.c: messages interface
 * This library provides an interface to the message queue to be used by other
 * modules, especially intf modules. See config.h for output configuration.
 *****************************************************************************
 * Copyright (C) 1998-2002 VideoLAN
 * $Id: messages.c,v 1.9 2002/08/20 18:25:42 sam Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <errno.h>                                                  /* errno */
#include <fcntl.h>                     /* O_CREAT, O_TRUNC, O_WRONLY, O_SYNC */
#include <stdio.h>                                               /* required */
#include <stdarg.h>                                       /* va_list for BSD */
#include <stdlib.h>                                              /* malloc() */
#include <string.h>                                            /* strerror() */

#include <vlc/vlc.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>                                      /* close(), write() */
#endif

#include "interface.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void QueueMsg ( vlc_object_t *, int , const char *,
                       const char *, va_list );
static void FlushMsg ( msg_bank_t * );
static void PrintMsg ( vlc_object_t *, msg_item_t * );

#if defined( WIN32 )
static char *ConvertPrintfFormatString ( const char *psz_format );
#endif

/*****************************************************************************
 * msg_Create: initialize messages interface
 *****************************************************************************
 * This functions has to be called before any call to other msg_* functions.
 * It set up the locks and the message queue if it is used.
 *****************************************************************************/
void __msg_Create( vlc_object_t *p_this )
{
    /* Message queue initialization */
    vlc_mutex_init( p_this, &p_this->p_vlc->msg_bank.lock );

    p_this->p_vlc->msg_bank.b_configured = VLC_FALSE;
    p_this->p_vlc->msg_bank.b_overflow = VLC_FALSE;

    p_this->p_vlc->msg_bank.i_start = 0;
    p_this->p_vlc->msg_bank.i_stop = 0;

    p_this->p_vlc->msg_bank.i_sub = 0;
    p_this->p_vlc->msg_bank.pp_sub = NULL;
}

/*****************************************************************************
 * msg_Flush: flush the message queue
 *****************************************************************************/
void __msg_Flush( vlc_object_t *p_this )
{
    int i_index;

    vlc_mutex_lock( &p_this->p_vlc->msg_bank.lock );

    p_this->p_vlc->msg_bank.b_configured = VLC_TRUE;

    for( i_index = p_this->p_vlc->msg_bank.i_start;
         i_index != p_this->p_vlc->msg_bank.i_stop;
         i_index = (i_index+1) % VLC_MSG_QSIZE )
    {
        PrintMsg( p_this, &p_this->p_vlc->msg_bank.msg[i_index] );
    }

    FlushMsg( &p_this->p_vlc->msg_bank );

    vlc_mutex_unlock( &p_this->p_vlc->msg_bank.lock );
}

/*****************************************************************************
 * msg_Destroy: free resources allocated by msg_Create
 *****************************************************************************
 * This functions prints all messages remaining in queue, then free all the
 * resources allocated by msg_Create.
 * No other messages interface functions should be called after this one.
 *****************************************************************************/
void __msg_Destroy( vlc_object_t *p_this )
{
    if( p_this->p_vlc->msg_bank.i_sub )
    {
        msg_Err( p_this, "stale interface subscribers" );
    }

    /* Flush the queue */
    if( !p_this->p_vlc->msg_bank.b_configured )
    {
        msg_Flush( p_this );
    }
    else
    {
        FlushMsg( &p_this->p_vlc->msg_bank );
    }

    /* Destroy lock */
    vlc_mutex_destroy( &p_this->p_vlc->msg_bank.lock );
}

/*****************************************************************************
 * msg_Subscribe: subscribe to the message queue.
 *****************************************************************************/
msg_subscription_t *__msg_Subscribe( vlc_object_t *p_this )
{
    msg_subscription_t *p_sub = malloc( sizeof( msg_subscription_t ) );

    vlc_mutex_lock( &p_this->p_vlc->msg_bank.lock );

    /* Add subscription to the list */
    p_this->p_vlc->msg_bank.i_sub++;
    p_this->p_vlc->msg_bank.pp_sub = realloc( p_this->p_vlc->msg_bank.pp_sub,
        p_this->p_vlc->msg_bank.i_sub * sizeof( msg_subscription_t* ) );

    p_this->p_vlc->msg_bank.pp_sub[ p_this->p_vlc->msg_bank.i_sub - 1 ] = p_sub;

    p_sub->i_start = p_this->p_vlc->msg_bank.i_start;
    p_sub->pi_stop = &p_this->p_vlc->msg_bank.i_stop;

    p_sub->p_msg   = p_this->p_vlc->msg_bank.msg;
    p_sub->p_lock  = &p_this->p_vlc->msg_bank.lock;

    vlc_mutex_unlock( &p_this->p_vlc->msg_bank.lock );

    return p_sub;
}

/*****************************************************************************
 * msg_Unsubscribe: unsubscribe from the message queue.
 *****************************************************************************/
void __msg_Unsubscribe( vlc_object_t *p_this, msg_subscription_t *p_sub )
{
    int i_index;

    vlc_mutex_lock( &p_this->p_vlc->msg_bank.lock );

    /* Sanity check */
    if( !p_this->p_vlc->msg_bank.i_sub )
    {
        msg_Err( p_this, "no subscriber in the list" );
        return;
    }

    /* Look for the appropriate subscription */
    for( i_index = 0; i_index < p_this->p_vlc->msg_bank.i_sub; i_index++ )
    {
        if( p_this->p_vlc->msg_bank.pp_sub[ i_index ] == p_sub )
        {
            break;
        }
    }

    if( p_this->p_vlc->msg_bank.pp_sub[ i_index ] != p_sub )
    {
        msg_Err( p_this, "subscriber not found" );
        vlc_mutex_unlock( &p_this->p_vlc->msg_bank.lock );
        return;
    }

    /* Remove this subscription */
    for( ; i_index < (p_this->p_vlc->msg_bank.i_sub - 1); i_index++ )
    {
        p_this->p_vlc->msg_bank.pp_sub[ i_index ] = p_this->p_vlc->msg_bank.pp_sub[ i_index+1 ];
    }

    p_this->p_vlc->msg_bank.i_sub--;
    if( p_this->p_vlc->msg_bank.i_sub )
    {
        p_this->p_vlc->msg_bank.pp_sub = realloc( p_this->p_vlc->msg_bank.pp_sub,
            p_this->p_vlc->msg_bank.i_sub * sizeof( msg_subscription_t* ) );
    }
    else
    {
        free( p_this->p_vlc->msg_bank.pp_sub );
        p_this->p_vlc->msg_bank.pp_sub = NULL;
    }


    vlc_mutex_unlock( &p_this->p_vlc->msg_bank.lock );
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

/* Generic functions used when variadic macros are not available. */
#define DECLARE_MSG_FN( FN_NAME, FN_TYPE ) \
    void FN_NAME( void *p_this, const char *psz_format, ... ) \
    { \
        va_list args; \
        va_start( args, psz_format ); \
        QueueMsg( (vlc_object_t *)p_this, FN_TYPE, "unknown", \
                  psz_format, args ); \
        va_end( args ); \
    } \
    struct _

DECLARE_MSG_FN( __msg_Info, VLC_MSG_INFO );
DECLARE_MSG_FN( __msg_Err,  VLC_MSG_ERR );
DECLARE_MSG_FN( __msg_Warn, VLC_MSG_WARN );
DECLARE_MSG_FN( __msg_Dbg,  VLC_MSG_DBG );

/*****************************************************************************
 * QueueMsg: add a message to a queue
 *****************************************************************************
 * This function provides basic functionnalities to other msg_* functions.
 * It adds a message to a queue (after having printed all stored messages if it
 * is full). If the message can't be converted to string in memory, it issues
 * a warning.
 *****************************************************************************/
static void QueueMsg( vlc_object_t *p_this, int i_type, const char *psz_module,
                      const char *psz_format, va_list args )
{
    msg_bank_t *        p_bank = &p_this->p_vlc->msg_bank;   /* message bank */
    char *              psz_str = NULL;          /* formatted message string */
    msg_item_t *        p_item = NULL;                 /* pointer to message */
    msg_item_t          item;             /* message in case of a full queue */
#ifdef WIN32
    char *              psz_temp;
#endif
#ifndef HAVE_VASPRINTF
    int                 i_size = strlen(psz_format) + INTF_MAX_MSG_SIZE;
#endif

    /*
     * Convert message to string
     */
#ifdef HAVE_VASPRINTF
    vasprintf( &psz_str, psz_format, args );
#else
    psz_str = (char*) malloc( i_size * sizeof(char) );
#endif

    if( psz_str == NULL )
    {
        fprintf( stderr, "main warning: can't store message (%s): ",
                 strerror(errno) );
        vfprintf( stderr, psz_format, args );
        fprintf( stderr, "\n" );
        return;
    }

#ifndef HAVE_VASPRINTF
#   ifdef WIN32
    psz_temp = ConvertPrintfFormatString( psz_format );
    if( !psz_temp )
    {
        fprintf( stderr, "main warning: couldn't print message\n" );
        return;
    }
    vsnprintf( psz_str, i_size, psz_temp, args );
    free( psz_temp );
#   else
    vsnprintf( psz_str, i_size, psz_format, args );
#   endif
    psz_str[ i_size - 1 ] = 0; /* Just in case */
#endif

    /* Put message in queue */
    vlc_mutex_lock( &p_bank->lock );

    /* Check there is room in the queue for our message */
    if( p_bank->b_overflow )
    {
        FlushMsg( &p_this->p_vlc->msg_bank );

        if( ((p_bank->i_stop - p_bank->i_start + 1) % VLC_MSG_QSIZE) == 0 )
        {
            /* Still in overflow mode, print from a dummy item */
            p_item = &item;
        }
        else
        {
            /* Pheeew, at last, there is room in the queue! */
            p_bank->b_overflow = VLC_FALSE;
        }
    }
    else if( ((p_bank->i_stop - p_bank->i_start + 2) % VLC_MSG_QSIZE) == 0 )
    {
        FlushMsg( &p_this->p_vlc->msg_bank );

        if( ((p_bank->i_stop - p_bank->i_start + 2) % VLC_MSG_QSIZE) == 0 )
        {
            p_bank->b_overflow = VLC_TRUE;

            /* Put the overflow message in the queue */
            p_item = p_bank->msg + p_bank->i_stop;
            p_bank->i_stop = (p_bank->i_stop + 1) % VLC_MSG_QSIZE;

            p_item->i_type =      VLC_MSG_ERR;
            p_item->i_object_id = p_this->i_object_id;
            p_item->psz_module =  strdup( "message" );
            p_item->psz_msg =     strdup( "message queue overflowed" );

            PrintMsg( p_this, p_item );

            /* We print from a dummy item */
            p_item = &item;
        }
    }

    if( !p_bank->b_overflow )
    {
        /* Put the message in the queue */
        p_item = p_bank->msg + p_bank->i_stop;
        p_bank->i_stop = (p_bank->i_stop + 1) % VLC_MSG_QSIZE;
    }

    /* Fill message information fields */
    p_item->i_type =      i_type;
    p_item->i_object_id = p_this->i_object_id;
    p_item->psz_module =  strdup( psz_module );
    p_item->psz_msg =     psz_str;

    PrintMsg( p_this, p_item );

    if( p_bank->b_overflow )
    {
        free( p_item->psz_module );
        free( p_item->psz_msg );
    }

    vlc_mutex_unlock( &p_bank->lock );
}

/* following functions are local */

/*****************************************************************************
 * FlushMsg
 *****************************************************************************
 * Print all messages remaining in queue. MESSAGE QUEUE MUST BE LOCKED, since
 * this function does not check the lock.
 *****************************************************************************/
static void FlushMsg ( msg_bank_t *p_bank )
{
    int i_index, i_start, i_stop;

    /* Only flush the queue if it has been properly configured */
    if( !p_bank->b_configured )
    {
        return;
    }

    /* Get the maximum message index that can be freed */
    i_stop = p_bank->i_stop;

    /* Check until which value we can free messages */
    for( i_index = 0; i_index < p_bank->i_sub; i_index++ )
    {
        i_start = p_bank->pp_sub[ i_index ]->i_start;

        /* If this subscriber is late, we don't free messages before
         * his i_start value, otherwise he'll miss messages */
        if(   ( i_start < i_stop
               && (p_bank->i_stop <= i_start || i_stop <= p_bank->i_stop) )
           || ( i_stop < i_start
               && (i_stop <= p_bank->i_stop && p_bank->i_stop <= i_start) ) )
        {
            i_stop = i_start;
        }
    }

    /* Free message data */
    for( i_index = p_bank->i_start;
         i_index != i_stop;
         i_index = (i_index+1) % VLC_MSG_QSIZE )
    {
        free( p_bank->msg[i_index].psz_msg );
        free( p_bank->msg[i_index].psz_module );
    }

    /* Update the new start value */
    p_bank->i_start = i_index;
}

/*****************************************************************************
 * PrintMsg: output a message item to stderr
 *****************************************************************************
 * Print a message to stderr, with colour formatting if needed.
 *****************************************************************************/
static void PrintMsg ( vlc_object_t * p_this, msg_item_t * p_item )
{
#   define COL(x)  "\033[" #x ";1m"
#   define RED     COL(31)
#   define GREEN   COL(32)
#   define YELLOW  COL(33)
#   define WHITE   COL(37)
#   define GRAY    "\033[0m"

    static const char * ppsz_type[4] = { "", " error", " warning", " debug" };
    static const char *ppsz_color[4] = { WHITE, RED, YELLOW, GRAY };
    int i_type = p_item->i_type;

    if( p_this->p_vlc->b_quiet || !p_this->p_vlc->msg_bank.b_configured )
    {
        return;
    }

    if( !p_this->p_vlc->b_verbose &&
         ( (i_type == VLC_MSG_WARN) || (i_type == VLC_MSG_DBG) ) )
    {
        return;
    }

    /* Send the message to stderr */
    if( p_this->p_vlc->b_color )
    {
        fprintf( stderr, "[" GREEN "%.2x" GRAY ":" GREEN "%.6x" GRAY "] "
                         "%s%s: %s%s" GRAY "\n", p_this->p_vlc->i_instance,
                         p_item->i_object_id, p_item->psz_module,
                         ppsz_type[i_type], ppsz_color[i_type],
                         p_item->psz_msg );
    }
    else
    {
        fprintf( stderr, "[%.2x:%.6x] %s%s: %s\n",
                         p_this->p_vlc->i_instance, p_item->i_object_id,
                         p_item->psz_module, ppsz_type[i_type],
                         p_item->psz_msg );
    }
}

/*****************************************************************************
 * ConvertPrintfFormatString: replace all occurrences of %ll with %I64 in the
 *                            printf format string.
 *****************************************************************************
 * Win32 doesn't recognize the "%ll" format in a printf string, so we have
 * to convert this string to something that win32 can handle.
 * This is a REALLY UGLY HACK which won't even work in every situation,
 * but hey I don't want to put an ifdef WIN32 each time I use printf with
 * a "long long" type!!!
 * By the way, if we don't do this we can sometimes end up with segfaults.
 *****************************************************************************/
#if defined( WIN32 )
static char *ConvertPrintfFormatString( const char *psz_format )
{
  int i, i_counter=0, i_pos=0;
  char *psz_dest;

  /* We first need to check how many occurences of %ll there are in the
   * psz_format string. Once we'll know that we'll be able to malloc the
   * destination string */

  if( strlen( psz_format ) <= 3 )
      return strdup( psz_format );

  for( i=0; i <= (strlen(psz_format) - 3); i++ )
  {
      if( !strncmp( (char *)(psz_format + i), "%ll", 3 ) )
      {
          i_counter++;
      }
  }

  /* malloc the destination string */
  psz_dest = malloc( strlen(psz_format) + i_counter + 1 );
  if( psz_dest == NULL )
  {
      fprintf( stderr, "main warning: ConvertPrintfFormatString failed\n" );
      return NULL;
  }

  /* Now build the modified string */
  i_counter = 0;
  for( i=0; i <= (strlen(psz_format) - 3); i++ )
  {
      if( !strncmp( (char *)(psz_format + i), "%ll", 3 ) )
      {
          memcpy( psz_dest+i_pos+i_counter, psz_format+i_pos, i-i_pos+1);
          *(psz_dest+i+i_counter+1)='I';
          *(psz_dest+i+i_counter+2)='6';
          *(psz_dest+i+i_counter+3)='4';
          i_pos = i+3;
          i_counter++;
      }
  }
  strcpy( psz_dest+i_pos+i_counter, psz_format+i_pos );

  return psz_dest;
}
#endif
