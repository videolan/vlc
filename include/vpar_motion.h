/*****************************************************************************
 * vpar_motion.h : video parser motion compensation management
 * (c)1999 VideoLAN
 *****************************************************************************/

/*
 * Prototypes
 */

void vpar_MPEG1MotionVector ( vpar_thread_t * p_vpar, macroblock_t * p_mb, int i_s );
void vpar_MPEG2MotionVector ( vpar_thread_t * p_vpar, macroblock_t * p_mb, int i_s );
