/*****************************************************************************
 * vdec_motion.h : types for the motion compensation algorithm
 * (c)1999 VideoLAN
 *****************************************************************************
 *****************************************************************************
 * Requires:
 *  "config.h"
 *  "common.h"
 *  "vlc_thread.h"
 *  "video_parser.h"
 *  "undec_picture.h"
 *****************************************************************************/
 
/*****************************************************************************
 * Function pointers
 *****************************************************************************/
typedef void (*f_motion_mb_t)( coeff_t *, pel_lookup_table_t *,
                               int, coeff_t *, int, int, int, int, int );
typedef void (*f_motion_t)( vdec_thread_t *, undec_picture_t *, int,
                            f_motion_mb_t );

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
void vdec_MotionMacroblock420( coeff_t * p_src, pel_lookup_table_t * p_lookup,
                               int i_width_line,
                               coeff_t * p_dest, int i_dest_x, i_dest_y,
                               int i_stride_line,
                               i_mv1_x, i_mv1_y, i_mv2_x, i_mv2_y );
