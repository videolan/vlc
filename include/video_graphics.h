/*******************************************************************************
 * video_graphics.h: pictures manipulation primitives
 * (c)1999 VideoLAN
 *******************************************************************************
 * Includes function to compose, convert and display pictures, and also basic
 * functions to copy pictures data or descriptors. 
 *******************************************************************************
 * Requires:
 *  "config.h"
 *  "common.h"
 *  "mtime.h"
 *  "video.h"
 *******************************************************************************/

/*******************************************************************************
 * Prototypes
 *******************************************************************************/

/* Pictures management functions */
picture_t * video_CreatePicture         ( video_cfg_t *p_cfg );
picture_t * video_CopyPicture           ( picture_t *p_pic );
picture_t * video_ReplicatePicture      ( picture_t *p_pic );
void        video_DestroyPicture        ( picture_t *p_pic );

/* Files functions */
picture_t * video_ReadPicture           ( int i_file );

/* Drawing functions */
void        video_ClearPicture          ( picture_t *p_pic );
void        video_DrawPixel             ( picture_t *p_pic, int i_x, int i_y, pixel_t value );
void        video_DrawHLine             ( picture_t *p_pic, int i_x, int i_y, int i_width, pixel_t value );
void        video_DrawVLine             ( picture_t *p_pic, int i_x, int i_y, int i_height, pixel_t value );
void        video_DrawLine              ( picture_t *p_pic, int i_x1, int i_y1, 
                                          int i_x2, int i_y2, pixel_t value );
void        video_DrawBar               ( picture_t *p_pic, int i_x, int i_y, int i_width, 
                                          int i_height, pixel_t value );
void        video_DrawRectangle         ( picture_t *p_pic, int i_x, int i_y, 
                                          int i_width, int i_height, pixel_t color );
void        video_DrawPicture           ( picture_t *p_pic, picture_t *p_insert, int i_x, int i_y );
void        video_DrawText              ( picture_t *p_pic, int i_x, int i_y, char *psz_text, 
                                          int i_size, pixel_t color );

/* Convertion functions */
/* ?? rgb->pixel, pixel->rgb */

/* Low-level shared functions */
void        video_CopyPictureDescriptor ( picture_t *p_dest, picture_t *p_src );
int         video_CreatePictureBody     ( picture_t *p_pic, video_cfg_t *p_cfg );

#ifdef DEBUG
/* Debugging functions */
void        video_PrintPicture          ( picture_t *p_pic, char *psz_str );
#endif
