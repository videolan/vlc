/*****************************************************************************
 * video_text.h : text manipulation functions
 * (c)1999 VideoLAN
 *****************************************************************************/

/* Text styles - these are primary text styles, used by the vout_Print function.
 * They may be ignored or interpreted by higher level functions */
#define WIDE_TEXT                       1         /* interspacing is doubled */
#define ITALIC_TEXT                     2                          /* italic */
#define OPAQUE_TEXT                     4            /* text with background */
#define OUTLINED_TEXT                   8           /* border around letters */
#define VOID_TEXT                      16                   /* no foreground */


/*****************************************************************************
 * Prototypes
 *****************************************************************************/
p_vout_font_t   vout_LoadFont   ( const char *psz_name );
void            vout_UnloadFont ( p_vout_font_t p_font );
void            vout_TextSize   ( p_vout_font_t p_font, int i_style,
                                  const char *psz_text,
                                  int *pi_width, int *pi_height );
void            vout_Print      ( p_vout_font_t p_font, byte_t *p_pic,
                                  int i_bytes_per_pixel, int i_bytes_per_line,
                                  u32 i_char_color, u32 i_border_color, u32 i_bg_color,
                                  int i_style, const char *psz_text );










