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

/* Empty function for intra macroblocks motion compensation */
void vdec_DummyRecon    ( struct macroblock_s* );

/* Motion compensation for skipped macroblocks */
void vdec_MotionField   ( struct macroblock_s* );
void vdec_MotionFrame   ( struct macroblock_s* );

/* Motion compensation for non skipped macroblocks */
void vdec_FieldRecon    ( struct macroblock_s* );
void vdec_16x8Recon     ( struct macroblock_s* );
void vdec_FrameRecon    ( struct macroblock_s* );
void vdec_DMVRecon      ( struct macroblock_s* );

/* Motion compensation functions for the 3 chroma formats */
void vdec_Motion420();
void vdec_Motion422();
void vdec_Motion444();
