/*****************************************************************************
 * vpar_synchro.h : video parser blocks management
 * (c)1999 VideoLAN
 *****************************************************************************
 *****************************************************************************
 * Requires:
 *  "config.h"
 *  "common.h"
 *  "mtime.h"
 *  "vlc_thread.h"
 *  "input.h"
 *  "video.h"
 *  "video_output.h"
 *  "decoder_fifo.h"
 *  "video_fifo.h"
 *****************************************************************************/

/*****************************************************************************
 * video_synchro_t : timers for the video synchro
 *****************************************************************************/
typedef struct video_synchro_s
{

} video_synchro_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
boolean_t vpar_SynchroChoose( struct vpar_thread_s * p_vpar, int i_coding_type, 
                         int i_structure );
void vpar_SynchroTrash( struct vpar_thread_s * p_vpar, int i_coding_type,
                        int i_structure );
mtime_t vpar_SynchroDecode( struct vpar_thread_s * p_vpar, int i_coding_type,
                            int i_structure );
void vpar_SynchroEnd( struct vpar_thread_s * p_vpar );
