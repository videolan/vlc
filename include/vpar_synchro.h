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

#define POLUX_SYNCHRO

/*****************************************************************************
 * video_synchro_t and video_synchro_tab_s : timers for the video synchro
 *****************************************************************************/
#ifdef SAM_SYNCHRO
typedef struct video_synchro_tab_s
{
    double mean;
    double deviation;

} video_synchro_tab_t;

typedef struct video_synchro_fifo_s
{
    /* type of image to be decoded, and decoding date */
    int i_image_type;
    mtime_t i_decode_date;
    mtime_t i_pts;

} video_synchro_fifo_t;

typedef struct video_synchro_s
{
    /* fifo containing decoding dates */
    video_synchro_fifo_t fifo[16];
    unsigned int i_fifo_start;
    unsigned int i_fifo_stop;

    /* mean decoding time */
    mtime_t i_mean_decode_time;
    /* dates */
    mtime_t i_last_display_pts;           /* pts of the last displayed image */
    mtime_t i_last_decode_pts;              /* pts of the last decoded image */
    mtime_t i_last_i_pts;                         /* pts of the last I image */
    mtime_t i_last_nondropped_i_pts;      /* pts of last non-dropped I image */
    unsigned int i_images_since_pts;

    /* il manquait un compteur */
    unsigned int modulo;

    /* P images since the last I */
    unsigned int current_p_count;
    unsigned int nondropped_p_count;
    double p_count_predict;
    /* B images since the last I */
    unsigned int current_b_count;
    unsigned int nondropped_b_count;
    double b_count_predict;

    /* can we display pictures ? */
    unsigned int    can_display_i;
    unsigned int    can_display_p;
    double          displayable_p;
    unsigned int    can_display_b;
    double          displayable_b;

    /* 1 for linear count, 2 for binary count, 3 for ternary count */
    video_synchro_tab_t tab_p[6];
    video_synchro_tab_t tab_b[6];

    double theorical_fps;
    double actual_fps;

} video_synchro_t;
#endif

#ifdef MEUUH_SYNCHRO
typedef struct video_synchro_s
{
    int         kludge_level, kludge_p, kludge_b, kludge_nbp, kludge_nbb;
    int         kludge_nbframes;
    mtime_t     kludge_date, kludge_prevdate;
    int         i_coding_type;
} video_synchro_t;

#define SYNC_TOLERATE   10000 /* 10 ms */
#define SYNC_DELAY      500000
#endif

#ifdef POLUX_SYNCHRO

typedef struct video_synchro_s
{
    /* Date Section */
    
    /* Dates needed to compute the date of the current frame 
     * We also use the stream frame rate (sequence.r_frame_rate) */
    mtime_t     i_current_frame_date;
    mtime_t     i_backward_frame_date;

    /* Frame Trashing Section */
    
    int         i_b_nb, i_p_nb;   /* number of decoded P and B between two I */
    int         i_b_count, i_p_count, i_i_count;
    int         i_b_trasher;                /* used for brensenham algorithm */
    
} video_synchro_t;

#endif

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
boolean_t vpar_SynchroChoose( struct vpar_thread_s * p_vpar, int i_coding_type,
                         int i_structure );
void vpar_SynchroTrash( struct vpar_thread_s * p_vpar, int i_coding_type,
                        int i_structure );
void vpar_SynchroDecode( struct vpar_thread_s * p_vpar, int i_coding_type,
                            int i_structure );
void vpar_SynchroEnd( struct vpar_thread_s * p_vpar );
mtime_t vpar_SynchroDate( struct vpar_thread_s * p_vpar );

#ifndef SAM_SYNCHRO
void vpar_SynchroKludge( struct vpar_thread_s *, mtime_t );
#endif
