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

/*******************************************************************************
 * External prototypes
 *******************************************************************************/
#ifdef HAVE_MMX

/* YUV transformations for MMX - in video_yuv_mmx.S 
 *      p_y, p_u, p_v:          Y U and V planes
 *      i_width, i_height:      frames dimensions (pixels)
 *      i_ypitch, i_vpitch:     Y and V lines sizes (bytes)
 *      i_aspect:               vertical aspect factor
 *      p_pic:                  RGB frame
 *      i_dci_offset:           ?? x offset for left image border
 *      i_offset_to_line_0:     ?? x offset for left image border
 *      i_pitch:                RGB line size (bytes)
 *      i_colortype:            0 for 565, 1 for 555 */
void ConvertYUV420RGB16MMX( u8* p_y, u8* p_u, u8 *p_v, 
                            unsigned int i_width, unsigned int i_height,
                            unsigned int i_ypitch, unsigned int i_vpitch,
                            unsigned int i_aspect, u8 *p_pic, 
                            u32 i_dci_offset, u32 i_offset_to_line_0,
                            int CCOPitch, int i_colortype );
#endif
