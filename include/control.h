/*****************************************************************************
 * control.h: user control functions
 * (c)1999 VideoLAN
 *****************************************************************************
 * Library of functions common to all interfaces, allowing access to various
 * structures and settings. Interfaces should only use those functions
 * to read or write informations from other threads.
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
 *  "xconsole.h"
 *  "interface.h"
 *****************************************************************************/

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
int     intf_SelectAudioStream  ( intf_thread_t *p_intf, int i_input, int i_id );
void    intf_DeselectAudioStream( intf_thread_t *p_intf, int i_input, int i_id );
int     intf_SelectVideoStream  ( intf_thread_t *p_intf, int i_input,
                                  int i_vout, int i_id );
void    intf_DeselectVideoStream( intf_thread_t *p_intf, int i_input, int i_id );



