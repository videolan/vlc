/*****************************************************************************
 * intf_msg.c: messages interface
 * This library provides basic functions for threads to interact with user
 * interface, such as message output. See config.h for output configuration.
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: intf_msg.c,v 1.49.2.1 2002/08/10 19:40:04 sam Exp $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
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

#include <videolan/vlc.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>                                      /* close(), write() */
#endif

#include "interface.h"


/*****************************************************************************
 * intf_msg_t
 *****************************************************************************
 * Store all data requiered by messages interfaces. It has a single reference
 * int p_main.
 *****************************************************************************/
typedef struct msg_bank_s
{
    /* Message queue lock */
    vlc_mutex_t             lock;

    /* Message queue */
    msg_item_t              msg[INTF_MSG_QSIZE];            /* message queue */
    int i_start;
    int i_stop;

    /* Subscribers */
    int i_sub;
    intf_subscription_t **pp_sub;

} msg_bank_t;

msg_bank_t msg_bank;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void QueueMsg ( int, int, char *, va_list );
static void FlushMsg ( void );

#if defined( WIN32 )
static char *ConvertPrintfFormatString ( char *psz_format );
#endif

/*****************************************************************************
 * intf_MsgCreate: initialize messages interface                         (ok ?)
 *****************************************************************************
 * This functions has to be called before any call to other intf_*Msg functions.
 * It set up the locks and the message queue if it is used.
 *****************************************************************************/
void intf_MsgCreate( void )
{
    /* Message queue initialization */
    vlc_mutex_init( &msg_bank.lock );
    msg_bank.i_start = 0;
    msg_bank.i_stop = 0;

    msg_bank.i_sub = 0;
    msg_bank.pp_sub = NULL;
}

/*****************************************************************************
 * intf_MsgDestroy: free resources allocated by intf_InitMsg            (ok ?)
 *****************************************************************************
 * This functions prints all messages remaining in queue, then free all the
 * resources allocated by intf_InitMsg.
 * No other messages interface functions should be called after this one.
 *****************************************************************************/
void intf_MsgDestroy( void )
{
    /* Destroy lock */
    vlc_mutex_destroy( &msg_bank.lock );

    if( msg_bank.i_sub )
    {
        fprintf( stderr, "intf error: stale interface subscribers\n" );
    }

    /* Free remaining messages */
    FlushMsg( );
}

/*****************************************************************************
 * intf_MsgSub: subscribe to the message queue.
 *****************************************************************************/
intf_subscription_t *intf_MsgSub( void )
{
    intf_subscription_t *p_sub = malloc( sizeof( intf_subscription_t ) );

    vlc_mutex_lock( &msg_bank.lock );

    /* Add subscription to the list */
    msg_bank.i_sub++;
    msg_bank.pp_sub = realloc( msg_bank.pp_sub,
        msg_bank.i_sub * sizeof( intf_subscription_t* ) );

    msg_bank.pp_sub[ msg_bank.i_sub - 1 ] = p_sub;

    p_sub->i_start = msg_bank.i_start;
    p_sub->pi_stop = &msg_bank.i_stop;

    p_sub->p_msg   = msg_bank.msg;
    p_sub->p_lock  = &msg_bank.lock;

    vlc_mutex_unlock( &msg_bank.lock );

    return p_sub;
}

/*****************************************************************************
 * intf_MsgUnsub: unsubscribe from the message queue.
 *****************************************************************************/
void intf_MsgUnsub( intf_subscription_t *p_sub )
{
    int i_index;

    vlc_mutex_lock( &msg_bank.lock );

    /* Sanity check */
    if( !msg_bank.i_sub )
    {
        intf_ErrMsg( "intf error: no subscriber in the list" );
        return;
    }

    /* Look for the appropriate subscription */
    for( i_index = 0; i_index < msg_bank.i_sub; i_index++ )
    {
        if( msg_bank.pp_sub[ i_index ] == p_sub )
        {
            break;
        }
    }

    if( msg_bank.pp_sub[ i_index ] != p_sub )
    {
        intf_ErrMsg( "intf error: subscriber not found" );
        vlc_mutex_unlock( &msg_bank.lock );
        return;
    }

    /* Remove this subscription */
    for( ; i_index < (msg_bank.i_sub - 1); i_index++ )
    {
        msg_bank.pp_sub[ i_index ] = msg_bank.pp_sub[ i_index+1 ];
    }

    msg_bank.i_sub--;
    if( msg_bank.i_sub )
    {
        msg_bank.pp_sub = realloc( msg_bank.pp_sub,
            msg_bank.i_sub * sizeof( intf_subscription_t* ) );
    }
    else
    {
        free( msg_bank.pp_sub );
        msg_bank.pp_sub = NULL;
    }

    vlc_mutex_unlock( &msg_bank.lock );
}

/*****************************************************************************
 * intf_Msg: print a message                                             (ok ?)
 *****************************************************************************
 * This function queue a message for later printing, or print it immediately
 * if the queue isn't used.
 *****************************************************************************/
void intf_Msg( char *psz_format, ... )
{
    va_list ap;

    va_start( ap, psz_format );
    QueueMsg( INTF_MSG_STD, 0, psz_format, ap );
    va_end( ap );
}

/*****************************************************************************
 * intf_ErrMsg : print an error message                                  (ok ?)
 *****************************************************************************
 * This function is the same as intf_Msg, except that it prints its messages
 * on stderr.
 *****************************************************************************/
void intf_ErrMsg( char *psz_format, ... )
{
    va_list ap;

    va_start( ap, psz_format );
    QueueMsg( INTF_MSG_ERR, 0, psz_format, ap );
    va_end( ap );
}

/*****************************************************************************
 * intf_WarnMsg : print a warning message
 *****************************************************************************
 * This function is the same as intf_Msg, except that it concerns warning
 * messages for testing purpose.
 *****************************************************************************/
void intf_WarnMsg( int i_level, char *psz_format, ... )
{
    va_list ap;
    
    va_start( ap, psz_format );
    QueueMsg( INTF_MSG_WARN, i_level, psz_format, ap );
    va_end( ap );
}

/*****************************************************************************
 * intf_StatMsg : print a statistic message
 *****************************************************************************
 * This function is the same as intf_Msg, except that it concerns statistic
 * messages for testing purpose.
 *****************************************************************************/
void intf_StatMsg( char *psz_format, ... )
{
    va_list ap;
    
    if( p_main->b_stats )
    {
        va_start( ap, psz_format );
        QueueMsg( INTF_MSG_STAT, 0, psz_format, ap );
        va_end( ap );
    }
}

/*****************************************************************************
 * intf_WarnHexDump : print a hexadecimal dump of a memory area
 *****************************************************************************
 * This is convenient for debugging purposes.
 *****************************************************************************/
void intf_WarnHexDump( int i_level, void *p_data, int i_size )
{
    int   i_index = 0;
    int   i_subindex;
    char  p_string[75];
    u8   *p_area = (u8 *)p_data;

    intf_WarnMsg( i_level, "hexdump: dumping %i bytes at address %p",
                           i_size, p_data );

    while( i_index < i_size )
    {
        i_subindex = 0;

        while( ( i_subindex < 24 ) && ( i_index + i_subindex < i_size ) )
        {
            sprintf( p_string + 3 * i_subindex, "%.2x ",
                     p_area[ i_index + i_subindex ] );

            i_subindex++;
        }

        /* -1 here is safe because we know we printed at least one */
        p_string[ 3 * i_subindex - 1 ] = '\0';
        intf_WarnMsg( i_level, "0x%.4x: %s", i_index, p_string );

        i_index += 24;
    }

    intf_WarnMsg( i_level, "hexdump: %i bytes dumped", i_size );
}

/* following functions are local */

/*****************************************************************************
 * QueueMsg: add a message to a queue
 *****************************************************************************
 * This function provides basic functionnalities to other intf_*Msg functions.
 * It adds a message to a queue (after having printed all stored messages if it
 * is full). If the message can't be converted to string in memory, it issues
 * a warning.
 *****************************************************************************/
static void QueueMsg( int i_type, int i_level, char *psz_format, va_list ap )
{
    char *                  psz_str;             /* formatted message string */
    msg_item_t *            p_item;                /* pointer to message */
#ifdef WIN32
    char *                  psz_temp;
#endif
#ifndef HAVE_VASPRINTF
    int                     i_size = strlen(psz_format) + INTF_MAX_MSG_SIZE;
#endif

    /*
     * Convert message to string
     */
#ifdef HAVE_VASPRINTF
    vasprintf( &psz_str, psz_format, ap );
#else
    psz_str = (char*) malloc( i_size * sizeof(char) );
#endif

    if( psz_str == NULL )
    {
        fprintf(stderr, "intf warning: can't store following message (%s): ",
                strerror(errno) );
        vfprintf(stderr, psz_format, ap );
        fprintf(stderr, "\n" );
        return;
    }

#ifndef HAVE_VASPRINTF
#   ifdef WIN32
    psz_temp = ConvertPrintfFormatString(psz_format);
    if( !psz_temp )
    {
        fprintf(stderr, "intf warning: couldn't print message");
        return;
    }
    vsnprintf( psz_str, i_size, psz_temp, ap );
    free( psz_temp );
#   else
    vsnprintf( psz_str, i_size, psz_format, ap );
#   endif
    psz_str[ i_size - 1 ] = 0; /* Just in case */
#endif

    /* Put message in queue */
    vlc_mutex_lock( &msg_bank.lock );

    /* Send the message to stderr */
    if( i_level <= p_main->i_warning_level )
    {
        fprintf( stderr, "%s\n", psz_str );
    }

    /* Put the message in the queue if there is room for it */
    if( ((msg_bank.i_stop - msg_bank.i_start + 1) % INTF_MSG_QSIZE) == 0 )
    {
        FlushMsg( );

        if( ((msg_bank.i_stop - msg_bank.i_start + 1) % INTF_MSG_QSIZE) == 0 )
        {
            fprintf( stderr, "intf warning: message queue overflow\n" );
            vlc_mutex_unlock( &msg_bank.lock );
            return;
        }
    }

    p_item = msg_bank.msg + msg_bank.i_stop;
    msg_bank.i_stop = (msg_bank.i_stop + 1) % INTF_MSG_QSIZE;

    /* Fill message information fields */
    p_item->i_type =     i_type;
    p_item->psz_msg =    psz_str;

    vlc_mutex_unlock( &msg_bank.lock );
}

/*****************************************************************************
 * FlushMsg
 *****************************************************************************
 * Print all messages remaining in queue. MESSAGE QUEUE MUST BE LOCKED, since
 * this function does not check the lock.
 *****************************************************************************/
static void FlushMsg ( void )
{
    int i_index, i_start, i_stop;

    /* Get the maximum message index that can be freed */
    i_stop = msg_bank.i_stop;

    /* Check until which value we can free messages */
    for( i_index = 0; i_index < msg_bank.i_sub; i_index++ )
    {
        i_start = msg_bank.pp_sub[ i_index ]->i_start;

        /* If this subscriber is late, we don't free messages before
         * his i_start value, otherwise he'll miss messages */
        if(   ( i_start < i_stop
               && (msg_bank.i_stop <= i_start || i_stop <= msg_bank.i_stop) )
           || ( i_stop < i_start
               && (i_stop <= msg_bank.i_stop && msg_bank.i_stop <= i_start) ) )
        {
            i_stop = i_start;
        }
    }

    /* Free message data */
    for( i_index = msg_bank.i_start;
         i_index != i_stop;
         i_index = (i_index+1) % INTF_MSG_QSIZE )
    {
        free( msg_bank.msg[i_index].psz_msg );
    }

    /* Update the new start value */
    msg_bank.i_start = i_index;
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
static char *ConvertPrintfFormatString( char *psz_format )
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
      fprintf( stderr, "intf warning: ConvertPrintfFormatString failed\n");
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
