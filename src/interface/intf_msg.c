/*******************************************************************************
 * intf_msg.c: messages interface
 * (c)1998 VideoLAN
 *******************************************************************************
 * This library provides basic functions for threads to interact with user
 * interface, such as message output. If INTF_MSG_QUEUE is defined (which is the
 * defaul), messages are not printed directly by threads, to bypass console 
 * limitations and slow printf() calls, but sent to a queue and printed later by
 * interface thread. 
 * If INTF_MSG_QUEUE is not defined, output is directly performed on stderr.
 * Exported symbols are declared in intf_msg.h.
 *******************************************************************************/

/*******************************************************************************
 * Preamble
 *******************************************************************************/
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/soundcard.h>
#include <sys/uio.h>
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

#include "config.h"
#include "common.h"
#include "mtime.h"
#include "vlc_thread.h"
#include "debug.h"

#include "input.h"
#include "input_vlan.h"

#include "audio_output.h"

#include "video.h"
#include "video_output.h"

#include "xconsole.h"
#include "interface.h"
#include "intf_msg.h"

#include "pgm_data.h"

/*
 * Local prototypes
 */

static void QueueMsg        ( interface_msg_t *p_intf_msg, int i_type,
                              char *psz_format, va_list ap );
static void PrintMsg        ( interface_msg_message_t *p_msg );
#ifdef DEBUG
static void QueueDbgMsg     ( interface_msg_t *p_intf_msg, char *psz_file, 
                              char *psz_function, int i_line, 
                              char *psz_format, va_list ap );
#endif
#ifdef INTF_MSG_QUEUE
static void FlushLockedMsg  ( interface_msg_t *p_intf_msg );
#endif

/*******************************************************************************
 * intf_InitMsg: initialize messages interface                            (ok ?)
 *******************************************************************************
 * This functions has to be called before any call to other intf_*Msg functions.
 * It set up the locks and the message queue if it is used. On error, 
 * it returns errno (without printing its own error messages) and free all 
 * alocated resources.
 *******************************************************************************/
int intf_InitMsg( interface_msg_t *p_intf_msg )
{
#ifdef INTF_MSG_QUEUE
    /* Message queue initialization */
    vlc_mutex_init( &p_intf_msg->lock );                     /* intialize lock */
    p_intf_msg->i_count = 0;                                 /* queue is empty */
#endif

#ifdef DEBUG_LOG
    /* Log file initialization */
    p_intf_msg->p_log_file = fopen( DEBUG_LOG, "w+" );
    if ( !p_intf_msg->p_log_file )
    {        
        return( errno );
    }    
#endif

    return( 0 );
}

/*******************************************************************************
 * intf_TerminateMsg: free resources allocated by intf_InitMsg            (ok ?)
 *******************************************************************************
 * This functions prints all messages remaining in queue, then free all the 
 * resources allocated by intf_InitMsg.
 * No other messages interface functions should be called after this one.
 *******************************************************************************/
void intf_TerminateMsg( interface_msg_t *p_intf_msg )
{
    intf_FlushMsg();                           /* print all remaining messages */

#ifdef DEBUG_LOG
    /* Close log file */
    fclose( p_intf_msg->p_log_file );
#endif
}

/*******************************************************************************
 * intf_Msg: print a message                                              (ok ?)
 *******************************************************************************
 * This function queue a message for later printing, or print it immediately
 * if the queue isn't used.
 *******************************************************************************/
void intf_Msg( char *psz_format, ... )
{
    va_list ap;

    va_start( ap, psz_format );
    QueueMsg( &p_program_data->intf_msg, INTF_MSG_STD, psz_format, ap );
    va_end( ap );
}
 
/*******************************************************************************
 * intf_ErrMsg : print an error message                                   (ok ?)
 *******************************************************************************
 * This function is the same as intf_Msg, except that it prints its messages
 * on stderr.
 *******************************************************************************/ 
void intf_ErrMsg(char *psz_format, ...)
{
    va_list ap;

    va_start( ap, psz_format );
    QueueMsg( &p_program_data->intf_msg, INTF_MSG_ERR, psz_format, ap );
    va_end( ap );
}

/*******************************************************************************
 * intf_IntfMsg : print an interface message                              (ok ?)
 *******************************************************************************
 * In opposition to all other intf_*Msg function, this function does not print
 * it's message on default terminal (stdout or stderr), but send it to 
 * interface (in fact to the X11 console). This means that the interface MUST
 * be initialized and a X11 console openned before this function is used, and
 * that once the console is closed, this call is vorbidden.
 * Practically, only the interface thread itself should call this function, and 
 * flush all messages before intf_CloseX11Console() is called.
 *******************************************************************************/
void intf_IntfMsg(char *psz_format, ...)
{
    va_list ap;

    va_start( ap, psz_format );
    QueueMsg( &p_program_data->intf_msg, INTF_MSG_INTF, psz_format, ap );
    va_end( ap );
}

/*******************************************************************************
 * _intf_DbgMsg: print a debugging message                                (ok ?)
 *******************************************************************************
 * This function prints a debugging message. Compared to other intf_*Msg 
 * functions, it is only defined if DEBUG is defined and require a file name,
 * a function name and a line number as additionnal debugging informations. It
 * also prints a debugging header for each received line.
 *******************************************************************************/ 
#ifdef DEBUG
void _intf_DbgMsg( char *psz_file, char *psz_function, int i_line, 
                   char *psz_format, ...)
{
    va_list ap;

    va_start( ap, psz_format );
    QueueDbgMsg( &p_program_data->intf_msg, psz_file, psz_function, i_line, 
                 psz_format, ap );
    va_end( ap );
}
#endif

/*******************************************************************************
 * intf_ErrMsgImm: print a message                                        (ok ?)
 *******************************************************************************
 * This function prints a message immediately. If the queue is used, all 
 * waiting messages are also printed.
 *******************************************************************************/
void intf_MsgImm( char *psz_format, ... )
{
    va_list ap;

    va_start( ap, psz_format );
    QueueMsg( &p_program_data->intf_msg, INTF_MSG_STD, psz_format, ap );
    va_end( ap );
    intf_FlushMsg();
}
 
/*******************************************************************************
 * intf_ErrMsgImm: print an error message immediately                     (ok ?)
 *******************************************************************************
 * This function is the same as intf_MsgImm, except that it prints its message
 * on stderr.
 *******************************************************************************/
void intf_ErrMsgImm(char *psz_format, ...)
{
    va_list ap;

    va_start( ap, psz_format );
    QueueMsg( &p_program_data->intf_msg, INTF_MSG_ERR, psz_format, ap );
    va_end( ap );
    intf_FlushMsg();
}

/*******************************************************************************
 * _intf_DbgMsgImm: print a debugging message immediately                 (ok ?)
 *******************************************************************************
 * This function is the same as intf_DbgMsgImm, except that it prints its
 * message immediately. It should only be called through the macro 
 * intf_DbgMsgImm().
 *******************************************************************************/
#ifdef DEBUG
void _intf_DbgMsgImm( char *psz_file, char *psz_function, int i_line, 
                      char *psz_format, ...)
{
    va_list ap;

    va_start( ap, psz_format );
    QueueDbgMsg( &p_program_data->intf_msg, psz_file, psz_function, i_line, 
                 psz_format, ap );
    va_end( ap );
    intf_FlushMsg();
}
#endif

/*******************************************************************************
 * intf_FlushMsg                                                          (ok ?)
 *******************************************************************************
 * Print all messages remaining in queue: get lock and call FlushLockedMsg.
 * This function does nothing if the message queue isn't used.
 * This function is only implemented if message queue is used. If not, it is an
 * empty macro.
 *******************************************************************************/
#ifdef INTF_MSG_QUEUE
void intf_FlushMsg( void )
{
    vlc_mutex_lock( &p_program_data->intf_msg.lock );              /* get lock */
    FlushLockedMsg( &p_program_data->intf_msg );             /* flush messages */
    vlc_mutex_unlock( &p_program_data->intf_msg.lock );      /* give lock back */
}
#endif

/* following functions are local */

/*******************************************************************************
 * QueueMsg: add a message to a queue                                    (ok ?)
 *******************************************************************************
 * This function provide basic functionnalities to other intf_*Msg functions.
 * It add a message to a queue (after having printed all stored messages if it
 * is full. If the message can't be converted to string in memory, it exit the  
 * program. If the queue is not used, it prints the message immediately.
 *******************************************************************************/
static void QueueMsg(interface_msg_t *p_intf_msg, int i_type, char *psz_format, va_list ap)
{
    char *                          psz_str;       /* formatted message string */
#ifndef INTF_MSG_QUEUE
    interface_msg_message_t         msg;                            /* message */
#endif

    /* Convert message to string */   
    vasprintf( &psz_str, psz_format, ap );
    if( psz_str == NULL )
    {
        fprintf(stderr, "intf error: *** can not store message (%s) ***\n", 
                strerror(errno) );
        vfprintf(stderr, psz_format, ap );
        exit( errno );
    }

#ifdef INTF_MSG_QUEUE

    /* 
     * Queue mode: the queue is flushed if it is full, then the message is
     * queued. A lock is required on queue to avoid indexes corruption 
     */
    vlc_mutex_lock( &p_intf_msg->lock );                            /* get lock */
    
    if( p_intf_msg->i_count == INTF_MSG_QSIZE )        /* flush queue if needed */
    {  
#ifdef DEBUG                   /* in debug mode, queue overflow causes a waring */
        fprintf(stderr, "intf warning: *** message queue overflow ***\n" );
#endif
        FlushLockedMsg( p_intf_msg );
    }
    
    /* Queue message - if DEBUG if defined, the message is dated */
    p_intf_msg->msg[ p_intf_msg->i_count ].i_type =     i_type;
    p_intf_msg->msg[ p_intf_msg->i_count++ ].psz_msg =  psz_str;    
#ifdef DEBUG
    p_intf_msg->msg[ p_intf_msg->i_count ].date =       mdate();
#endif

    vlc_mutex_unlock( &p_intf_msg->lock );                  /* give lock back */

#else

    /* 
     * Instant mode: the message is converted and printed immediately 
     */
    msg.i_type = i_type; 
    msg.psz_msg = psz_str;
#ifdef DEBUG
    msg.date = mdate();
#endif
    PrintMsg( &msg );                                         /* print message */
    free( psz_str );                                      /* free message data */    

#endif
}

/*******************************************************************************
 * QueueDbgMsg: add a message to a queue with debugging informations
 *******************************************************************************
 * This function is the same as QueueMsg, except that it is only defined when
 * DEBUG is define, and require additionnal debugging informations.
 *******************************************************************************/
#ifdef DEBUG
static void QueueDbgMsg(interface_msg_t *p_intf_msg, char *psz_file, char *psz_function,
                     int i_line, char *psz_format, va_list ap)
{
    char *                          psz_str;       /* formatted message string */
#ifndef INTF_MSG_QUEUE
    interface_msg_message_t         msg;                            /* message */
#endif

    /* Convert message to string */   
    vasprintf( &psz_str, psz_format, ap );    
    if( psz_str == NULL )
    {                    /* critical error: not enough memory to store message */
        fprintf(stderr, "intf error: *** can not store message (%s) ***\n", strerror(errno) );
        fprintf(stderr, INTF_MSG_DBG_FORMAT, psz_file, psz_function, i_line );
        vfprintf(stderr, psz_format, ap );
        exit( errno );
    }

#ifdef INTF_MSG_QUEUE

    /* 
     * Queue mode: the queue is flushed if it is full, then the message is
     * queued. A lock is required on queue to avoid indexes corruption 
     */
    vlc_mutex_lock( &p_intf_msg->lock );                           /* get lock */
    
    if( p_intf_msg->i_count == INTF_MSG_QSIZE )       /* flush queue if needed */
    {  
        fprintf(stderr, "intf warning: *** message queue overflow ***\n" );
        FlushLockedMsg( p_intf_msg );
    }
    
    /* Queue message */
    p_intf_msg->msg[ p_intf_msg->i_count ].i_type =         INTF_MSG_DBG;    
    p_intf_msg->msg[ p_intf_msg->i_count ].date =           mdate();
    p_intf_msg->msg[ p_intf_msg->i_count ].psz_file =       psz_file;
    p_intf_msg->msg[ p_intf_msg->i_count ].psz_function =   psz_function;
    p_intf_msg->msg[ p_intf_msg->i_count ].i_line =         i_line;
    p_intf_msg->msg[ p_intf_msg->i_count++ ].psz_msg =      psz_str;    

    vlc_mutex_unlock( &p_intf_msg->lock );                  /* give lock back */

#else

    /* 
     * Instant mode: the message is converted and printed immediately 
     */
    msg.i_type =        INTF_MSG_DBG; 
    msg.psz_file =      psz_file;
    msg.psz_function =  psz_function;
    msg.i_line =        i_line;
#ifdef DEBUG
//    msg.date =          mdate();
#endif
    msg.psz_msg =       psz_str;
    PrintMsg( &msg );                                         /* print message */
    free( psz_str );                                      /* free message data */    

#endif
}
#endif

/*******************************************************************************
 * FlushLockedMsg                                                        (ok ?)
 *******************************************************************************
 * Print all messages remaining in queue. MESSAGE QUEUE MUST BE LOCKED, since
 * this function does not check the lock. This function is only defined if
 * INTF_MSG_QUEUE is defined.
 *******************************************************************************/
#ifdef INTF_MSG_QUEUE
static void FlushLockedMsg ( interface_msg_t *p_intf_msg )
{
    int i_index;

    for( i_index = 0; i_index < p_intf_msg->i_count; i_index++ )
    {
        /* Print message and free message data */
        PrintMsg( &p_intf_msg->msg[i_index] );      
        free( p_intf_msg->msg[i_index].psz_msg );
    }
    
    p_intf_msg->i_count = 0;
}
#endif

/*******************************************************************************
 * PrintMsg: print a message                                              (ok ?)
 *******************************************************************************
 * Print a single message. The message data is not freed. This function exists
 * in two version. The DEBUG version prints a date with each message, and is
 * able to log messages (if DEBUG_LOG is defined).
 * The normal one just prints messages to the screen.
 *******************************************************************************/
#ifdef DEBUG

static void PrintMsg( interface_msg_message_t *p_msg )
{
    char    psz_date[MSTRTIME_MAX_SIZE];              /* formatted time buffer */
    char *  psz_msg;                                         /* message buffer */
    

    /* Computes date */
    mstrtime( psz_date, p_msg->date );    

    /* Format message - the message is formatted here because in case the log
     * file is used, it avoids another format string parsing */
    switch( p_msg->i_type )
    {
    case INTF_MSG_STD:                                     /* regular messages */
    case INTF_MSG_ERR:
        asprintf( &psz_msg, "(%s) %s", psz_date, p_msg->psz_msg );
        break;

    case INTF_MSG_INTF:                                  /* interface messages */
    case INTF_MSG_DBG:
        asprintf( &psz_msg, p_msg->psz_msg );
        break;
#if 0        
    case INTF_MSG_DBG:                                       /* debug messages */
        asprintf( &psz_msg, "(%s) " INTF_MSG_DBG_FORMAT "%s", 
                  psz_date, p_msg->psz_file, p_msg->psz_function, p_msg->i_line, 
                  p_msg->psz_msg );            
        break;                
#endif
    }
 
    /* Check if formatting function suceeded */
    if( psz_msg == NULL )
    {
        fprintf( stderr, "intf error: *** can not format message (%s): %s ***\n", 
                 strerror( errno ), p_msg->psz_msg );        
        return;        
    }

    /*
     * Print messages
     */
    switch( p_msg->i_type )
    {
    case INTF_MSG_STD:                                    /* standard messages */
        fprintf( stdout, psz_msg );
        break;
    case INTF_MSG_ERR:                                       /* error messages */
#ifndef DEBUG_LOG_ONLY
    case INTF_MSG_DBG:                                   /* debugging messages */
#endif
        fprintf( stderr, psz_msg );
        break;
    case INTF_MSG_INTF:                                  /* interface messages */
        intf_PrintXConsole( &p_program_data->intf_thread.xconsole, psz_msg );
        break;
    }
    
#ifdef DEBUG_LOG
    /* Append all messages to log file */
    fprintf( p_program_data->intf_msg.p_log_file, psz_msg );
#endif

    /* Free formatted message */
    free( psz_msg );    
}

#else

static void PrintMsg( interface_msg_message_t *p_msg )
{
    /*
     * Print messages on screen 
     */
    switch( p_msg->i_type )
    {
    case INTF_MSG_STD:                                    /* standard messages */
    case INTF_MSG_DBG:                                       /* debug messages */
        fprintf( stdout, p_msg->psz_msg );
        break;
    case INTF_MSG_ERR:                                       /* error messages */
        fprintf( stderr, p_msg->psz_msg );
        break;
    case INTF_MSG_INTF:                                  /* interface messages */
        intf_PrintXConsole( &p_program_data->intf_thread.xconsole, 
                            p_msg->psz_msg );
        break;
    } 
}

#endif
