/*****************************************************************************
 * interface.h: interface access for other threads
 * (c)1999 VideoLAN
 *****************************************************************************
 * This library provides basic functions for threads to interact with user
 * interface, such as message output.
 *****************************************************************************
 * Required headers:
 *  <sys/uio.h>
 *  <X11/Xlib.h>
 *  <X11/extensions/XShm.h>
 *  "config.h"
 *  "common.h"
 *  "mtime.h"
 *  "vlc_thread.h"
 *  "input.h"
 *  "video.h"
 *  "video_output.h"
 *  "audio_output.h"
 *  "xconsole.h"
 *****************************************************************************/

/*****************************************************************************
 * intf_thread_t: describe an interface thread
 *****************************************************************************
 * This structe describes all interface-specific data of the main (interface)
 * thread.
 *****************************************************************************/
typedef int   ( intf_sys_create_t )   ( p_intf_thread_t p_intf );
typedef void  ( intf_sys_destroy_t )  ( p_intf_thread_t p_intf );
typedef void  ( intf_sys_manage_t )   ( p_intf_thread_t p_intf );

typedef struct intf_thread_s
{
    boolean_t           b_die;                                 /* `die' flag */

    /* Specific interfaces */
    p_intf_console_t    p_console;                                /* console */
    p_intf_sys_t        p_sys;                           /* system interface */

    /* method-specific functions */
    intf_sys_create_t *     p_sys_create;         /* create interface thread */
    intf_sys_manage_t *     p_sys_manage;                       /* main loop */
    intf_sys_destroy_t *    p_sys_destroy;              /* destroy interface */

    /* Channels array - NULL if not used */
    p_intf_channel_t    p_channel;                /* description of channels */

    /* Main threads - NULL if not active */
    p_vout_thread_t     p_vout;
    p_input_thread_t    p_input;

} intf_thread_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
intf_thread_t * intf_Create             ( void );
void            intf_Run                ( intf_thread_t * p_intf );
void            intf_Destroy            ( intf_thread_t * p_intf );

int             intf_SelectChannel      ( intf_thread_t * p_intf, int i_channel );
int             intf_ProcessKey         ( intf_thread_t * p_intf, int i_key );

