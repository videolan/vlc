/*******************************************************************************
 * video_yuv.h: YUV transformation functions
 * (c)1999 VideoLAN
 *******************************************************************************
 * Provides functions prototypes to perform the YUV conversion. The functions
 * may be implemented in one of the video_yuv_* files.
 *******************************************************************************/

/*******************************************************************************
 * Prototypes
 *******************************************************************************/
int             vout_InitTables      ( vout_thread_t *p_vout );
int             vout_ResetTables     ( vout_thread_t *p_vout );
void            vout_EndTables       ( vout_thread_t *p_vout );

