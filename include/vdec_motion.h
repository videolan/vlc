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

/* Empty function for intra macroblocks motion compensation */
void vdec_MotionDummy( struct macroblock_s * p_mb );

/* Motion compensation */
void vdec_MotionFieldField420( struct macroblock_s * p_mb );
void vdec_MotionField16x8420( struct macroblock_s * p_mb );
void vdec_MotionFieldDMV( struct macroblock_s * p_mb );
void vdec_MotionFrameFrame420( struct macroblock_s * p_mb );
void vdec_MotionFrameField420( struct macroblock_s * p_mb );
void vdec_MotionFrameDMV( struct macroblock_s * p_mb );
void vdec_MotionFieldField422( struct macroblock_s * p_mb );
void vdec_MotionField16x8422( struct macroblock_s * p_mb );
void vdec_MotionFieldDMV( struct macroblock_s * p_mb );
void vdec_MotionFrameFrame422( struct macroblock_s * p_mb );
void vdec_MotionFrameField422( struct macroblock_s * p_mb );
void vdec_MotionFrameDMV( struct macroblock_s * p_mb );
void vdec_MotionFieldField444( struct macroblock_s * p_mb );
void vdec_MotionField16x8444( struct macroblock_s * p_mb );
void vdec_MotionFieldDMV( struct macroblock_s * p_mb );
void vdec_MotionFrameFrame444( struct macroblock_s * p_mb );
void vdec_MotionFrameField444( struct macroblock_s * p_mb );
void vdec_MotionFrameDMV( struct macroblock_s * p_mb );