/*******************************************************************************
 * intf_msg.h: messages interface
 * (c)1999 VideoLAN
 *******************************************************************************
 * This library provides basic functions for threads to interact with user
 * interface, such as message output. If INTF_MSG_QUEUE is defined (which is the
 * defaul), messages are not printed directly by threads, to bypass console 
 * limitations and slow printf() calls, but sent to a queue and printed later by
 * interface thread. 
 * If INTF_MSG_QUEUE is not defined, output is directly performed on stderr.
 *******************************************************************************
 * required headers:
 *  <pthread.h>
 *  config.h
 *  mtime.h
 *******************************************************************************/

/*******************************************************************************
 * interface_message_t                                             
 *******************************************************************************
 * Store a single message. Messages have a maximal size of INTF_MSG_MSGSIZE.
 * If DEBUG is defined, messages have a date field and debug messages are
 * printed with a date to allow more precise profiling.
 *******************************************************************************/
typedef struct
{
    int     i_type;                                 /* message type, see below */
    char *  psz_msg;                                     /* the message itself */

#ifdef DEBUG
    /* Debugging informations - in DEBUG mode, all messages are dated and debug
     * messages have calling location informations printed */
    mtime_t date;                        /* date of the message (all messages) */
    char *  psz_file;                 /* file in which the function was called */
    char *  psz_function;       /* function from which the function was called */
    int     i_line;                   /* line at which the function was called */
#endif
} interface_msg_message_t;

/* Message types */
#define INTF_MSG_STD    0                                  /* standard message */
#define INTF_MSG_ERR    1                                     /* error message */
#define INTF_MSG_INTF   2                                 /* interface message */
#define INTF_MSG_DBG    3                                     /* debug message */

/*******************************************************************************
 * interface_msg_t                                                    
 *******************************************************************************
 * Store all data requiered by messages interfaces. It has a singe instance in
 * program_data.
 *******************************************************************************/
typedef struct
{
#ifdef INTF_MSG_QUEUE
    /* Message queue */
    pthread_mutex_t         lock;                        /* message queue lock */
    int                     i_count;              /* number of messages stored */
    interface_msg_message_t msg[INTF_MSG_QSIZE];              /* message queue */
#endif

#ifdef DEBUG_LOG
    /* Log file */
    FILE *                  p_log_file;                            /* log file */
#endif

#ifndef INTF_MSG_QUEUE
#ifndef DEBUG_LOG
    /* If neither messages queue, neither log file is used, then the structure
     * is empty. However, empty structures are not allowed in C. Therefore, a
     * dummy integer is used to fill it. */
    int                     i_dummy;                          /* unused filler */
#endif
#endif
} interface_msg_t;

/*******************************************************************************
 * intf_DbgMsg macros and functions
 *******************************************************************************
 * The intf_DbgMsg* functions are defined as macro to be able to use the 
 * compiler extensions and print the file, the function and the line number
 * from which they have been called. They call _intf_DbgMsg*() functions after
 * having added debugging informations.
 * Outside DEBUG mode, intf_DbgMsg* functions do nothing.
 *******************************************************************************/
#ifdef DEBUG

/* DEBUG mode */
void    _intf_DbgMsg        ( char *psz_file, char *psz_function, int i_line, 
                              char *psz_format, ... );
void    _intf_DbgMsgImm     ( char *psz_file, char *psz_function, int i_line,
                              char *psz_format, ... );

#define intf_DbgMsg( format, args... ) \
    _intf_DbgMsg( __FILE__, __FUNCTION__, __LINE__, format, ## args )
#define intf_DbgMsgImm( format, args... ) \
    _intf_DbgMsg( __FILE__, __FUNCTION__, __LINE__, format, ## args )

#else

/* Non-DEBUG mode */
#define intf_DbgMsg( format, args... )      
#define intf_DbgMsgImm( format, args...)    

#endif

/*******************************************************************************
 * intf_FlushMsg macro and function
 *******************************************************************************
 * intf_FlushMsg is a function which flushs message queue and print all messages
 * remaining. It is only usefull if INTF_MSG_QUEUE is defined. In this case, it
 * is really a function. In the other case, it is a macro doing nothing.
 *******************************************************************************/
#ifdef INTF_MSG_QUEUE

/* Message queue mode */
void    intf_FlushMsg       ( void );

#else

/* Direct mode */
#define intf_FlushMsg()     ;

#endif

/*******************************************************************************
 * Prototypes                                                      
 *******************************************************************************/
int     intf_InitMsg        ( interface_msg_t *p_intf_msg );
void    intf_TerminateMsg   ( interface_msg_t *p_intf_msg );

void    intf_Msg            ( char *psz_format, ... );
void    intf_ErrMsg         ( char *psz_format, ... );
void    intf_IntfMsg        ( char *psz_format, ... );

void    intf_MsgImm         ( char *psz_format, ... );
void    intf_ErrMsgImm      ( char *psz_format, ... );


