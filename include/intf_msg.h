/*****************************************************************************
 * intf_msg.h: messages interface
 * (c)1999 VideoLAN
 *****************************************************************************
 * This library provides basic functions for threads to interact with user
 * interface, such as message output. See config.h for output configuration.
 *****************************************************************************/

/*****************************************************************************
 * intf_DbgMsg macros and functions
 *****************************************************************************
 * The intf_DbgMsg* functions are defined as macro to be able to use the
 * compiler extensions and print the file, the function and the line number
 * from which they have been called. They call _intf_DbgMsg*() functions after
 * having added debugging informations.
 * Outside DEBUG mode, intf_DbgMsg* functions do nothing.
 *****************************************************************************/
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

/*****************************************************************************
 * intf_FlushMsg macro and function
 *****************************************************************************
 * intf_FlushMsg is a function which flushs message queue and print all messages
 * remaining. It is only usefull if INTF_MSG_QUEUE is defined. In this case, it
 * is really a function. In the other case, it is a macro doing nothing.
 *****************************************************************************/
#ifdef INTF_MSG_QUEUE

/* Message queue mode */
void    intf_FlushMsg       ( void );

#else

/* Direct mode */
#define intf_FlushMsg()     ;

#endif

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
p_intf_msg_t intf_MsgCreate      ( void );
void         intf_MsgDestroy     ( void );

void         intf_Msg            ( char *psz_format, ... );
void         intf_ErrMsg         ( char *psz_format, ... );
void         intf_IntfMsg        ( char *psz_format, ... );

void         intf_MsgImm         ( char *psz_format, ... );
void         intf_ErrMsgImm      ( char *psz_format, ... );
