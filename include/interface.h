/******************************************************************************
 * interface.h: interface access for other threads
 * (c)1999 VideoLAN
 ******************************************************************************
 * This library provides basic functions for threads to interact with user
 * interface, such as message output.
 ******************************************************************************
 * Required headers:
 *  <pthread.h>
 *  <sys/uio.h>
 *  <X11/Xlib.h>
 *  <X11/extensions/XShm.h>
 *  "config.h"
 *  "common.h"
 *  "mtime.h"
 *  "input.h"
 *  "video.h"
 *  "video_output.h"
 *  "audio_output.h"
 *  "xconsole.h"
 ******************************************************************************/

/******************************************************************************
 * intf_thread_t: describe an interface thread
 ******************************************************************************
 * This structe describes all interface-specific data of the main (interface)
 * thread.
 ******************************************************************************/
typedef struct
{
    boolean_t           b_die;                                  /* `die' flag */

    /* Threads control */
    input_thread_t *    pp_input[INPUT_MAX_THREADS];         /* input threads */
    vout_thread_t *     pp_vout[VOUT_MAX_THREADS];            /* vout threads */
    aout_thread_t *     p_aout;                                /* aout thread */

    int                 i_input;                      /* default input thread */
    int                 i_vout;                      /* default output thread */

    /* Specific interfaces */     
    xconsole_t          xconsole;                              /* X11 console */
} intf_thread_t;

/******************************************************************************
 * Prototypes
 ******************************************************************************/
int intf_Run( intf_thread_t * p_intf );
