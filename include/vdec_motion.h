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
struct macroblock_s;
struct vpar_thread_s;
struct motion_arg_s;

typedef void (*f_motion_t)( struct macroblock_s* );

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
void vdec_MotionFieldField420( struct macroblock_s * p_mb );
void vdec_MotionField16x8420( struct macroblock_s * p_mb );
void vdec_MotionFieldDMV420( struct macroblock_s * p_mb );
void vdec_MotionFrameFrame420( struct macroblock_s * p_mb );
void vdec_MotionFrameField420( struct macroblock_s * p_mb );
void vdec_MotionFrameDMV420( struct macroblock_s * p_mb );
void vdec_MotionFieldField422( struct macroblock_s * p_mb );
void vdec_MotionField16x8422( struct macroblock_s * p_mb );
void vdec_MotionFieldDMV422( struct macroblock_s * p_mb );
void vdec_MotionFrameFrame422( struct macroblock_s * p_mb );
void vdec_MotionFrameField422( struct macroblock_s * p_mb );
void vdec_MotionFrameDMV422( struct macroblock_s * p_mb );
void vdec_MotionFieldField444( struct macroblock_s * p_mb );
void vdec_MotionField16x8444( struct macroblock_s * p_mb );
void vdec_MotionFieldDMV444( struct macroblock_s * p_mb );
void vdec_MotionFrameFrame444( struct macroblock_s * p_mb );
void vdec_MotionFrameField444( struct macroblock_s * p_mb );
void vdec_MotionFrameDMV444( struct macroblock_s * p_mb );