/*****************************************************************************
 * vdec_idct.h : types for the inverse discrete cosine transform
 * (c)1999 VideoLAN
 *****************************************************************************
 *****************************************************************************
 * Requires:
 *  "config.h"
 *  "common.h"
 *  "vlc_thread.h"
 *  "video_parser.h"
 *****************************************************************************/

/*****************************************************************************
 * Common declarations
 *****************************************************************************/ 
#ifndef VDEC_DFT
typedef short elem_t;
#else
typedef int elem_t;
#endif

struct vdec_thread_s;

/*****************************************************************************
 * Function pointers
 *****************************************************************************/
typedef void (*f_idct_t)( struct vdec_thread_s *, elem_t*, int );

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
void vdec_DummyIDCT( struct vdec_thread_s *, elem_t*, int );
void vdec_SparseIDCT( struct vdec_thread_s *, elem_t*, int );
void vdec_IDCT( struct vdec_thread_s *, elem_t*, int );
