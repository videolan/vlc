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

typedef void (*f_motion_t)( struct macroblock_s* );
typedef void (*f_chroma_motion_t)( struct macroblock_s* );

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
void vdec_MotionField( struct macroblock_s* );
void vdec_MotionFrame( struct macroblock_s* );
