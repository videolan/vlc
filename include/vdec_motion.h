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
typedef void (*f_chroma_motion_t)( struct macroblock_s*, struct motion_arg_s* );

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

/* Empty function for intra macroblocks motion compensation */
void vdec_MotionDummy( struct macroblock_s * p_mb );

/* Motion compensation */
void vdec_MotionFieldField( struct macroblock_s * p_mb );
void vdec_MotionField16x8( struct macroblock_s * p_mb );
void vdec_MotionFieldDMV( struct macroblock_s * p_mb );
void vdec_MotionFrameFrame( struct macroblock_s * p_mb );
void vdec_MotionFrameField( struct macroblock_s * p_mb );
void vdec_MotionFrameDMV( struct macroblock_s * p_mb );

/* Motion compensation functions for the 3 chroma formats */
void vdec_Motion420( struct macroblock_s * p_mb, struct motion_arg_s * p_motion );
void vdec_Motion422( struct macroblock_s * p_mb, struct motion_arg_s * p_motion );
void vdec_Motion444( struct macroblock_s * p_mb, struct motion_arg_s * p_motion );
