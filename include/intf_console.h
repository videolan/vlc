/*****************************************************************************
 * intf_console.h: generic console methods for interface
 * (c)1998 VideoLAN
 *****************************************************************************/

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
p_intf_console_t  intf_ConsoleCreate    ( void );
void              intf_ConsoleDestroy   ( p_intf_console_t p_console );

void              intf_ConsoleClear     ( p_intf_console_t p_console );
void              intf_ConsolePrint     ( p_intf_console_t p_console, char *psz_str );
void              intf_ConsoleExec      ( p_intf_console_t p_console, char *psz_str );


