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
 * video_synchro_t and video_synchro_tab_s : timers for the video synchro
 *****************************************************************************/
typedef struct video_synchro_tab_s
{
    double mean;
    double deviation;
    
} video_synchro_tab_t;

typedef struct video_synchro_s
{
    int modulo;

    /* P images since the last I */
    int current_p_count;
    double p_count_predict;
    /* B images since the last I */
    int current_b_count;
    double b_count_predict;

    /* 1 for linear count, 2 for binary count, 3 for ternary count */
    video_synchro_tab_t tab_p[6];
    video_synchro_tab_t tab_b[6];

} video_synchro_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
boolean_t vpar_SynchroChoose( struct vpar_thread_s * p_vpar, int i_coding_type, 
                         int i_structure );
void vpar_SynchroTrash( struct vpar_thread_s * p_vpar, int i_coding_type,
                        int i_structure );
void vpar_SynchroDecode( struct vpar_thread_s * p_vpar, int i_coding_type,
                            int i_structure );
mtime_t vpar_SynchroEnd( struct vpar_thread_s * p_vpar );
