/*****************************************************************************
 * intf_msg.c: messages interface
 * This library provides basic functions for threads to interact with user
 * interface, such as message output. See config.h for output configuration.
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: intf_msg.c,v 1.39 2001/11/28 15:08:06 massiot Exp $
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
#include "defs.h"

#include <errno.h>                                                  /* errno */
#include <fcntl.h>                     /* O_CREAT, O_TRUNC, O_WRONLY, O_SYNC */
#include <stdio.h>                                               /* required */
#include <stdarg.h>                                       /* va_list for BSD */
#include <stdlib.h>                                              /* malloc() */
#include <string.h>                                            /* strerror() */

#ifdef HAVE_UNISTD_H
#include <unistd.h>                                      /* close(), write() */
#endif

#include "config.h"
#include "common.h"
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"

#include "interface.h"

#include "main.h"

/*****************************************************************************
 * intf_msg_item_t
 *****************************************************************************
 * Store a single message. Messages have a maximal size of INTF_MSG_MSGSIZE.
 * If TRACE is defined, messages have a date field and debug messages are
 * printed with a date to allow more precise profiling.
 *****************************************************************************/
typedef struct
{
    int     i_type;                               /* message type, see below */
    char *  psz_msg;                                   /* the message itself */

#ifdef TRACE
    /* Debugging informations - in TRACE mode, debug messages have calling
     * location information printed */
    mtime_t date;                                     /* date of the message */
    char *  psz_file;               /* file in which the function was called */
    char *  psz_function;     /* function from which the function was called */
    int     i_line;                 /* line at which the function was called */
#endif
} intf_msg_item_t;

/* Message types */
#define INTF_MSG_STD    0                                /* standard message */
#define INTF_MSG_ERR    1                                   /* error message */
#define INTF_MSG_DBG    3                                   /* debug message */
#define INTF_MSG_WARN   4                                 /* warning message */
#define INTF_MSG_STAT   5                               /* statistic message */


/*****************************************************************************
 * intf_msg_t
 *****************************************************************************
 * Store all data requiered by messages interfaces. It has a single reference
 * int p_main.
 *****************************************************************************/
typedef struct intf_msg_s
{
#ifdef INTF_MSG_QUEUE
    /* Message queue */
    vlc_mutex_t             lock;                      /* message queue lock */
    int                     i_count;            /* number of messages stored */
    intf_msg_item_t         msg[INTF_MSG_QSIZE];            /* message queue */
#endif

#ifdef TRACE_LOG
    /* Log file */
    FILE *                  p_log_file;                          /* log file */
#endif

#if !defined(INTF_MSG_QUEUE) && !defined(TRACE_LOG)
    /* If neither messages queue, neither log file is used, then the structure
     * is empty. However, empty structures are not allowed in C. Therefore, a
     * dummy integer is used to fill it. */
    int                     i_dummy;                        /* unused filler */
#endif
} intf_msg_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static void QueueMsg        ( intf_msg_t *p_msg, int i_type,
                              char *psz_format, va_list ap );
static void PrintMsg        ( intf_msg_item_t *p_msg );
#ifdef TRACE
static void QueueDbgMsg     ( intf_msg_t *p_msg, char *psz_file,
                              char *psz_function, int i_line,
                              char *psz_format, va_list ap );
#endif
#ifdef INTF_MSG_QUEUE
static void FlushLockedMsg  ( intf_msg_t *p_msg );
#endif

#if defined( WIN32 )
static char *ConvertPrintfFormatString ( char *psz_format );
#endif

/*****************************************************************************
 * intf_MsgCreate: initialize messages interface                         (ok ?)
 *****************************************************************************
 * This functions has to be called before any call to other intf_*Msg functions.
 * It set up the locks and the message queue if it is used.
 *****************************************************************************/
p_intf_msg_t intf_MsgCreate( void )
{
    p_intf_msg_t p_msg;

    /* Allocate structure */
    p_msg = malloc( sizeof( intf_msg_t ) );
    if( p_msg == NULL )
    {
        errno = ENOMEM;
    }
    else
    {
#ifdef INTF_MSG_QUEUE
    /* Message queue initialization */
    vlc_mutex_init( &p_msg->lock );                        /* intialize lock */
    p_msg->i_count = 0;                                    /* queue is empty */
#endif

    
#ifdef TRACE_LOG
        /* Log file initialization - on failure, file pointer will be null,
         * and no log will be issued, but this is not considered as an
         * error */
        p_msg->p_log_file = fopen( TRACE_LOG, "w" );
#endif
    }
    return( p_msg );
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
    intf_FlushMsg();                         /* print all remaining messages */

#ifdef TRACE_LOG
    /* Close log file if any */
    if( p_main->p_msg->p_log_file != NULL )
    {
        fclose( p_main->p_msg->p_log_file );
    }
#endif

#ifdef INTF_MSG_QUEUE
    /* destroy lock */
    vlc_mutex_destroy( &p_main->p_msg->lock );
#endif
    
    /* Free structure */
    free( p_main->p_msg );
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
    QueueMsg( p_main->p_msg, INTF_MSG_STD, psz_format, ap );
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
    QueueMsg( p_main->p_msg, INTF_MSG_ERR, psz_format, ap );
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
    
    if( i_level <= p_main->i_warning_level )
    {
        va_start( ap, psz_format );
        QueueMsg( p_main->p_msg, INTF_MSG_WARN, psz_format, ap );
        va_end( ap );
    }
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
        QueueMsg( p_main->p_msg, INTF_MSG_STAT, psz_format, ap );
        va_end( ap );
    }
}

/*****************************************************************************
 * _intf_DbgMsg: print a debugging message                               (ok ?)
 *****************************************************************************
 * This function prints a debugging message. Compared to other intf_*Msg
 * functions, it is only defined if TRACE is defined and require a file name,
 * a function name and a line number as additionnal debugging informations. It
 * also prints a debugging header for each received line.
 *****************************************************************************/
#ifdef TRACE
void _intf_DbgMsg( char *psz_file, char *psz_function, int i_line,
                   char *psz_format, ...)
{
    va_list ap;

    va_start( ap, psz_format );
    QueueDbgMsg( p_main->p_msg, psz_file, psz_function, i_line,
                 psz_format, ap );
    va_end( ap );
}
#endif

/*****************************************************************************
 * intf_MsgImm: print a message                                          (ok ?)
 *****************************************************************************
 * This function prints a message immediately. If the queue is used, all
 * waiting messages are also printed.
 *****************************************************************************/
void intf_MsgImm( char *psz_format, ... )
{
    va_list ap;

    va_start( ap, psz_format );
    QueueMsg( p_main->p_msg, INTF_MSG_STD, psz_format, ap );
    va_end( ap );
    intf_FlushMsg();
}

/*****************************************************************************
 * intf_ErrMsgImm: print an error message immediately                    (ok ?)
 *****************************************************************************
 * This function is the same as intf_MsgImm, except that it prints its message
 * on stderr.
 *****************************************************************************/
void intf_ErrMsgImm(char *psz_format, ...)
{
    va_list ap;

    va_start( ap, psz_format );
    QueueMsg( p_main->p_msg, INTF_MSG_ERR, psz_format, ap );
    va_end( ap );
    intf_FlushMsg();
}

/*****************************************************************************
 * intf_WarnMsgImm : print a warning message
 *****************************************************************************
 * This function is the same as intf_MsgImm, except that it concerns warning
 * messages for testing purpose.
 *****************************************************************************/
void intf_WarnMsgImm( int i_level, char *psz_format, ... )
{
    va_list ap;

    if( i_level <= p_main->i_warning_level )
    {
        va_start( ap, psz_format );
        QueueMsg( p_main->p_msg, INTF_MSG_WARN, psz_format, ap );
        va_end( ap );
    }
    intf_FlushMsg();
}



/*****************************************************************************
 * _intf_DbgMsgImm: print a debugging message immediately                (ok ?)
 *****************************************************************************
 * This function is the same as intf_DbgMsgImm, except that it prints its
 * message immediately. It should only be called through the macro
 * intf_DbgMsgImm().
 *****************************************************************************/
#ifdef TRACE
void _intf_DbgMsgImm( char *psz_file, char *psz_function, int i_line,
                      char *psz_format, ...)
{
    va_list ap;

    va_start( ap, psz_format );
    QueueDbgMsg( p_main->p_msg, psz_file, psz_function, i_line,
                 psz_format, ap );
    va_end( ap );
    intf_FlushMsg();
}
#endif

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

/*****************************************************************************
 * intf_FlushMsg                                                        (ok ?)
 *****************************************************************************
 * Print all messages remaining in queue: get lock and call FlushLockedMsg.
 * This function does nothing if the message queue isn't used.
 * This function is only implemented if message queue is used. If not, it is
 * an empty macro.
 *****************************************************************************/
#ifdef INTF_MSG_QUEUE
void intf_FlushMsg( void )
{
    vlc_mutex_lock( &p_main->p_msg->lock );                      /* get lock */
    FlushLockedMsg( p_main->p_msg );                       /* flush messages */
    vlc_mutex_unlock( &p_main->p_msg->lock );              /* give lock back */
}
#endif

/* following functions are local */

/*****************************************************************************
 * QueueMsg: add a message to a queue
 *****************************************************************************
 * This function provide basic functionnalities to other intf_*Msg functions.
 * It add a message to a queue (after having printed all stored messages if it
 * is full. If the message can't be converted to string in memory, it exit the
 * program. If the queue is not used, it prints the message immediately.
 *****************************************************************************/
static void QueueMsg( intf_msg_t *p_msg, int i_type, char *psz_format, va_list ap )
{
    char *                  psz_str;             /* formatted message string */
    intf_msg_item_t *       p_msg_item;                /* pointer to message */
#ifdef WIN32
    char *                  psz_temp;
#endif

#ifndef INTF_MSG_QUEUE /*................................... instant mode ...*/
    intf_msg_item_t         msg_item;                             /* message */
    p_msg_item =           &msg_item;
#endif /*....................................................................*/

    /*
     * Convert message to string
     */

#ifdef HAVE_VASPRINTF
    vasprintf( &psz_str, psz_format, ap );
#else
    psz_str = (char*) malloc( strlen(psz_format) + INTF_MAX_MSG_SIZE );
#endif
    if( psz_str == NULL )
    {
        fprintf(stderr, "warning: can't store following message (%s): ",
                strerror(errno) );
        vfprintf(stderr, psz_format, ap );
        fprintf(stderr, "\n" );
        exit( errno );
    }
#ifndef HAVE_VASPRINTF
#ifdef WIN32
    psz_temp = ConvertPrintfFormatString(psz_format);
    vsprintf( psz_str, psz_temp, ap );
    free( psz_temp );
#else
    vsprintf( psz_str, psz_format, ap );
#endif /* WIN32 */
#endif /* HAVE_VASPRINTF */

#ifdef INTF_MSG_QUEUE /*...................................... queue mode ...*/
    vlc_mutex_lock( &p_msg->lock );                              /* get lock */
    if( p_msg->i_count == INTF_MSG_QSIZE )          /* flush queue if needed */
    {
#ifdef DEBUG               /* in debug mode, queue overflow causes a warning */
        fprintf(stderr, "warning: message queue overflow\n" );
#endif
        FlushLockedMsg( p_msg );
    }
    p_msg_item = p_msg->msg + p_msg->i_count++;            /* select message */
#endif /*.............................................. end of queue mode ...*/

    /*
     * Fill message information fields
     */
    p_msg_item->i_type =     i_type;
    p_msg_item->psz_msg =    psz_str;
#ifdef TRACE    
    p_msg_item->date =       mdate();
#endif

#ifdef INTF_MSG_QUEUE /*......................................... queue mode */
    vlc_mutex_unlock( &p_msg->lock );                      /* give lock back */
#else /*....................................................... instant mode */
    PrintMsg( p_msg_item );                                 /* print message */
    free( psz_str );                                    /* free message data */
#endif /*....................................................................*/
}

/*****************************************************************************
 * QueueDbgMsg: add a message to a queue with debugging informations
 *****************************************************************************
 * This function is the same as QueueMsg, except that it is only defined when
 * TRACE is define, and require additionnal debugging informations.
 *****************************************************************************/
#ifdef TRACE
static void QueueDbgMsg(intf_msg_t *p_msg, char *psz_file, char *psz_function,
                        int i_line, char *psz_format, va_list ap)
{
    char *                  psz_str;             /* formatted message string */
    intf_msg_item_t *       p_msg_item;                /* pointer to message */
#ifdef WIN32
    char *                  psz_temp;
#endif

#ifndef INTF_MSG_QUEUE /*................................... instant mode ...*/
    intf_msg_item_t         msg_item;                             /* message */
    p_msg_item =           &msg_item;
#endif /*....................................................................*/

    /*
     * Convert message to string
     */
#ifdef HAVE_VASPRINTF
    vasprintf( &psz_str, psz_format, ap );
#else
    psz_str = (char*) malloc( INTF_MAX_MSG_SIZE );
#endif
    if( psz_str == NULL )
    {
        fprintf(stderr, "warning: can't store following message (%s): ",
                strerror(errno) );
        fprintf(stderr, INTF_MSG_DBG_FORMAT, psz_file, psz_function, i_line );
        vfprintf(stderr, psz_format, ap );
        fprintf(stderr, "\n" );
        exit( errno );
    }
#ifndef HAVE_VASPRINTF
#ifdef WIN32
    psz_temp = ConvertPrintfFormatString(psz_format);
    vsprintf( psz_str, psz_temp, ap );
    free( psz_temp );
#else
    vsprintf( psz_str, psz_format, ap );
#endif /* WIN32 */
#endif /* HAVE_VASPRINTF */

#ifdef INTF_MSG_QUEUE /*...................................... queue mode ...*/
    vlc_mutex_lock( &p_msg->lock );                              /* get lock */
    if( p_msg->i_count == INTF_MSG_QSIZE )          /* flush queue if needed */
    {
        fprintf(stderr, "warning: message queue overflow\n" );
        FlushLockedMsg( p_msg );
    }
    p_msg_item = p_msg->msg + p_msg->i_count++;            /* select message */
#endif /*.............................................. end of queue mode ...*/

    /*
     * Fill message information fields
     */
    p_msg_item->i_type =       INTF_MSG_DBG;
    p_msg_item->psz_msg =      psz_str;
    p_msg_item->psz_file =     psz_file;
    p_msg_item->psz_function = psz_function;
    p_msg_item->i_line =       i_line;
    p_msg_item->date =         mdate();

#ifdef INTF_MSG_QUEUE /*......................................... queue mode */
    vlc_mutex_unlock( &p_msg->lock );                      /* give lock back */
#else /*....................................................... instant mode */
    PrintMsg( p_msg_item );                                 /* print message */
    free( psz_str );                                    /* free message data */
#endif /*....................................................................*/
}
#endif

/*****************************************************************************
 * FlushLockedMsg                                                       (ok ?)
 *****************************************************************************
 * Print all messages remaining in queue. MESSAGE QUEUE MUST BE LOCKED, since
 * this function does not check the lock. This function is only defined if
 * INTF_MSG_QUEUE is defined.
 *****************************************************************************/
#ifdef INTF_MSG_QUEUE
static void FlushLockedMsg ( intf_msg_t *p_msg )
{
    int i_index;

    for( i_index = 0; i_index < p_msg->i_count; i_index++ )
    {
        /* Print message and free message data */
        PrintMsg( &p_msg->msg[i_index] );
        free( p_msg->msg[i_index].psz_msg );
    }

    p_msg->i_count = 0;
}
#endif

/*****************************************************************************
 * PrintMsg: print a message                                             (ok ?)
 *****************************************************************************
 * Print a single message. The message data is not freed. This function exists
 * in two version. The TRACE version prints a date with each message, and is
 * able to log messages (if TRACE_LOG is defined).
 * The normal one just prints messages to the screen.
 *****************************************************************************/
#ifdef TRACE

static void PrintMsg( intf_msg_item_t *p_msg )
{
    char    psz_date[MSTRTIME_MAX_SIZE];            /* formatted time buffer */
    int     i_msg_len = MSTRTIME_MAX_SIZE + strlen(p_msg->psz_msg) + 200;
    char   *psz_msg;                                       /* message buffer */

    psz_msg = malloc( sizeof( char ) * i_msg_len );

    /* Check if allocation succeeded */
    if( psz_msg == NULL )
    {
        fprintf( stderr, "error: not enough memory for message %s\n",
                 p_msg->psz_msg );
        return;
    }

    /* Format message - the message is formatted here because in case the log
     * file is used, it avoids another format string parsing */
    switch( p_msg->i_type )
    {
    case INTF_MSG_STD:                                   /* regular messages */
    case INTF_MSG_STAT:
    case INTF_MSG_ERR:
        snprintf( psz_msg, i_msg_len, "%s", p_msg->psz_msg );
        break;

    case INTF_MSG_WARN:                                   /* Warning message */
        mstrtime( psz_date, p_msg->date );
        snprintf( psz_msg, i_msg_len, "(%s) %s",
                  psz_date, p_msg->psz_msg );

        break;
        
    case INTF_MSG_DBG:                                     /* debug messages */
        mstrtime( psz_date, p_msg->date );
        snprintf( psz_msg, i_msg_len, "(%s) " INTF_MSG_DBG_FORMAT "%s",
                  psz_date, p_msg->psz_file, p_msg->psz_function, p_msg->i_line,
                  p_msg->psz_msg );
        break;
    }

    /*
     * Print messages
     */
    switch( p_msg->i_type )
    {
    case INTF_MSG_STD:                                  /* standard messages */
    case INTF_MSG_STAT:
        fprintf( stdout, "%s\n", psz_msg );
        break;
    case INTF_MSG_ERR:                                     /* error messages */
    case INTF_MSG_WARN:
#ifndef TRACE_LOG_ONLY
    case INTF_MSG_DBG:                                 /* debugging messages */
#endif
        fprintf( stderr, "%s\n", psz_msg );
        break;
    }

#ifdef TRACE_LOG
    /* Append all messages to log file */
    if( p_main->p_msg->p_log_file != NULL )
    {
        fwrite( psz_msg, strlen( psz_msg ), 1, p_main->p_msg->p_log_file );
        fwrite( "\n", 1, 1, p_main->p_msg->p_log_file );
    }
#endif

    /* Free the message */
    free( psz_msg );
}

#else

static void PrintMsg( intf_msg_item_t *p_msg )
{
    /*
     * Print messages on screen
     */
    switch( p_msg->i_type )
    {
    case INTF_MSG_STD:                                  /* standard messages */
    case INTF_MSG_STAT:
    case INTF_MSG_DBG:                                     /* debug messages */
        fprintf( stdout, "%s\n", p_msg->psz_msg );
        break;
    case INTF_MSG_ERR:                                     /* error messages */
    case INTF_MSG_WARN:
        fprintf( stderr, "%s\n", p_msg->psz_msg );        /* warning message */
        break;
    }
}

#endif


#if defined( WIN32 )
/*****************************************************************************
 * ConvertPrintfFormatString: replace all occurrences of %ll with %I64 in the
 *                            printf format string.
 *****************************************************************************
 * Win32 doesn't recognize the "%lld" format in a printf string, so we have
 * to convert this string to something that win32 can handle.
 * This is a REALLY UGLY HACK which won't even work in every situation,
 * but hey I don't want to put an ifdef WIN32 each time I use printf with
 * a "long long" type!!!
 * By the way, if we don't do this we can sometimes end up with segfaults.
 *****************************************************************************/
static char *ConvertPrintfFormatString( char *psz_format )
{
  int i, i_counter=0, i_pos=0;
  char *psz_dest;

  /* We first need to check how many occurences of %ll there are in the
   * psz_format string. Once we'll know that we'll be able to malloc the
   * destination string */

  for( i=0; i <= (strlen(psz_format) - 4); i++ )
  {
      if( !strncmp( (char *)(psz_format + i), "%ll", 3 ) )
	  i_counter++;
  }

  /* malloc the destination string */
  psz_dest = malloc( strlen(psz_format) + i_counter + 1 );
  if( psz_dest == NULL )
  {
      fprintf(stderr, "warning: malloc failed in ConvertPrintfFormatString\n");
      exit (errno);
  }

  /* Now build the modified string */
  i_counter = 0;
  for( i=0; i <= (strlen(psz_format) - 4); i++ )
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
