/*******************************************************************************
 * video_yuv.c: YUV transformation functions
 * (c)1999 VideoLAN
 *******************************************************************************
 * Provides functions to perform the YUV conversion. The functions provided here
 * are a complete and portable C implementation, and may be replaced in certain
 * case by optimized functions.
 *******************************************************************************/

/*******************************************************************************
 * Preamble
 *******************************************************************************/
#include <math.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "config.h"
#include "mtime.h"
#include "vlc_thread.h"
#include "video.h"
#include "video_output.h"
#include "video_yuv.h"
#include "intf_msg.h"

/*******************************************************************************
 * Constants
 *******************************************************************************/

/* Color masks for different color depths - 8bpp masks can be choosen, since 
 * colormaps instead of hardware-defined colors are used. */
//?? remove
#define RED_8BPP_MASK           0xe0
#define GREEN_8BPP_MASK         0x1c
#define BLUE_8BPP_MASK          0x03

#define RED_15BPP_MASK          0xf800
#define GREEN_15BPP_MASK        0x03e0
#define BLUE_15BPP_MASK         0x001f

#define RED_16BPP_MASK          0xf800
#define GREEN_16BPP_MASK        0x07e0
#define BLUE_16BPP_MASK         0x001f

#define RED_24BPP_MASK          0xff0000
#define GREEN_24BPP_MASK        0x00ff00
#define BLUE_24BPP_MASK         0x0000ff

/* RGB/YUV inversion matrix (ISO/IEC 13818-2 section 6.3.6, table 6.9) */
//?? no more used ?
const int MATRIX_COEFFICIENTS_TABLE[8][4] =
{
  {117504, 138453, 13954, 34903},       /* no sequence_display_extension */
  {117504, 138453, 13954, 34903},       /* ITU-R Rec. 709 (1990) */
  {104597, 132201, 25675, 53279},       /* unspecified */
  {104597, 132201, 25675, 53279},       /* reserved */
  {104448, 132798, 24759, 53109},       /* FCC */
  {104597, 132201, 25675, 53279},       /* ITU-R Rec. 624-4 System B, G */
  {104597, 132201, 25675, 53279},       /* SMPTE 170M */
  {117579, 136230, 16907, 35559}        /* SMPTE 240M (1987) */
};

/* Margins and offsets in conversion tables - Margins are used in case a RGB
 * RGB conversion would give a value outside the 0-255 range. Offsets have been
 * calculated to avoid using the same cache line for 2 tables. conversion tables
 * are 2*MARGIN + 256 long and stores pixels.*/
#define RED_MARGIN      178
#define GREEN_MARGIN    135
#define BLUE_MARGIN     224
#define RED_OFFSET      1501                                   /* 1323 to 1935 */
#define GREEN_OFFSET    135                                        /* 0 to 526 */
#define BLUE_OFFSET     818                                     /* 594 to 1298 */
#define RGB_TABLE_SIZE  1935                               /* total table size */

#define GRAY_MARGIN     384
#define GRAY_TABLE_SIZE 1024                               /* total table size */

//??
#define SHIFT 20
#define U_GREEN_COEF    ((int)(-0.391 * (1<<SHIFT) / 1.164))
#define U_BLUE_COEF     ((int)(2.018 * (1<<SHIFT) / 1.164))
#define V_RED_COEF      ((int)(1.596 * (1<<SHIFT) / 1.164))
#define V_GREEN_COEF    ((int)(-0.813 * (1<<SHIFT) / 1.164))

/*******************************************************************************
 * Local prototypes
 *******************************************************************************/
static void     SetGammaTable     ( int *pi_table, double f_gamma );
static void     SetYUV            ( vout_thread_t *p_vout );
static void     SetOffset         ( int i_width, int i_height, int i_pic_width, int i_pic_height, 
                                    boolean_t *pb_h_scaling, int *pi_v_scaling, int *p_offset );

static void     ConvertY4Gray8    ( p_vout_thread_t p_vout, u8 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                    int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                    int i_matrix_coefficients );
static void     ConvertY4Gray16   ( p_vout_thread_t p_vout, u16 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                    int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                    int i_matrix_coefficients );
static void     ConvertY4Gray24   ( p_vout_thread_t p_vout, void *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                    int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                    int i_matrix_coefficients );
static void     ConvertY4Gray32   ( p_vout_thread_t p_vout, u32 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                    int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                    int i_matrix_coefficients );
static void     ConvertYUV420RGB8 ( p_vout_thread_t p_vout, u8 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                    int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                    int i_matrix_coefficients );
static void     ConvertYUV422RGB8 ( p_vout_thread_t p_vout, u8 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                    int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                    int i_matrix_coefficients );
static void     ConvertYUV444RGB8 ( p_vout_thread_t p_vout, u8 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                    int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                    int i_matrix_coefficients );
static void     ConvertYUV420RGB16( p_vout_thread_t p_vout, u16 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                    int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                    int i_matrix_coefficients );
static void     ConvertYUV422RGB16( p_vout_thread_t p_vout, u16 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                    int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                    int i_matrix_coefficients );
static void     ConvertYUV444RGB16( p_vout_thread_t p_vout, u16 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                    int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                    int i_matrix_coefficients );
static void     ConvertYUV420RGB24( p_vout_thread_t p_vout, void *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                    int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                    int i_matrix_coefficients );
static void     ConvertYUV422RGB24( p_vout_thread_t p_vout, void *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                    int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                    int i_matrix_coefficients );
static void     ConvertYUV444RGB24( p_vout_thread_t p_vout, void *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                    int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                    int i_matrix_coefficients );
static void     ConvertYUV420RGB32( p_vout_thread_t p_vout, u32 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                    int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                    int i_matrix_coefficients );
static void     ConvertYUV422RGB32( p_vout_thread_t p_vout, u32 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                    int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                    int i_matrix_coefficients );
static void     ConvertYUV444RGB32( p_vout_thread_t p_vout, u32 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                    int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                    int i_matrix_coefficients );

/*****************************************************************************
 * CONVERT_YUV_PIXEL, CONVERT_Y_PIXEL: pixel conversion blocks
 *****************************************************************************
 * These conversion routines are used by YUV conversion functions.
 * conversion are made from p_y, p_u, p_v, which are modified, to p_buffer,
 * which is also modified.
 *****************************************************************************/
#define CONVERT_Y_PIXEL( BPP )                                                \
    /* Only Y sample is present */                                            \
    p_ybase = p_yuv + *p_y++;                                                 \
    *p_buffer++ = p_ybase[RED_OFFSET-((V_RED_COEF*128)>>SHIFT) + i_red] |     \
        p_ybase[GREEN_OFFSET-(((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT)       \
        + i_green ] | p_ybase[BLUE_OFFSET-((U_BLUE_COEF*128)>>SHIFT) + i_blue];

#define CONVERT_YUV_PIXEL( BPP )                                              \
    /* Y, U and V samples are present */                                      \
    i_uval =    *p_u++;                                                       \
    i_vval =    *p_v++;                                                       \
    i_red =     (V_RED_COEF * i_vval) >> SHIFT;                               \
    i_green =   (U_GREEN_COEF * i_uval + V_GREEN_COEF * i_vval) >> SHIFT;     \
    i_blue =    (U_BLUE_COEF * i_uval) >> SHIFT;                              \
    CONVERT_Y_PIXEL( BPP )                                                    \

/*****************************************************************************
 * CONVERT_4YUV_PIXELS, CONVERT_4YUV_PIXELS_SCALE: dither 4 pixels in 8 bpp
 *****************************************************************************
 * These macros dither 4 pixels in 8 bpp, with or without horiz. scaling
 *****************************************************************************/
#define CONVERT_4YUV_PIXELS( CHROMA )                                         \
    *p_pic++ = p_vout->lookup[                                                \
        (((*p_y++ + dither10[i_real_y]) >> 4) << 7)                           \
      + ((*p_u + dither20[i_real_y]) >> 5) * 9                                \
      + ((*p_v + dither20[i_real_y]) >> 5) ];                                 \
    *p_pic++ = p_vout->lookup[                                                \
        (((*p_y++ + dither11[i_real_y]) >> 4) << 7)                           \
      + ((*p_u++ + dither21[i_real_y]) >> 5) * 9                              \
      + ((*p_v++ + dither21[i_real_y]) >> 5) ];                               \
    *p_pic++ = p_vout->lookup[                                                \
        (((*p_y++ + dither12[i_real_y]) >> 4) << 7)                           \
      + ((*p_u + dither22[i_real_y]) >> 5) * 9                                \
      + ((*p_v + dither22[i_real_y]) >> 5) ];                                 \
    *p_pic++ = p_vout->lookup[                                                \
        (((*p_y++ + dither13[i_real_y]) >> 4) << 7)                           \
      + ((*p_u++ + dither23[i_real_y]) >> 5) * 9                              \
      + ((*p_v++ + dither23[i_real_y]) >> 5) ];                               \
 
#define CONVERT_4YUV_PIXELS_SCALE( CHROMA )                                   \
    *p_pic++ = p_vout->lookup[                                                \
        (((*p_y + dither10[i_real_y]) >> 4) << 7)                             \
        + ((*p_u + dither20[i_real_y])   >> 5) * 9                            \
        + ((*p_v + dither20[i_real_y])   >> 5) ];                             \
    i_jump_uv = (i_jump_uv + *p_offset) & 0x1;                                \
    p_y += *p_offset;                                                         \
    p_u += *p_offset   & i_jump_uv;                                           \
    p_v += *p_offset++ & i_jump_uv;                                           \
    *p_pic++ = p_vout->lookup[                                                \
        (((*p_y + dither11[i_real_y]) >> 4) << 7)                             \
        + ((*p_u + dither21[i_real_y])   >> 5) * 9                            \
        + ((*p_v + dither21[i_real_y])   >> 5) ];                             \
    i_jump_uv = (i_jump_uv + *p_offset) & 0x1;                                \
    p_y += *p_offset;                                                         \
    p_u += *p_offset   & i_jump_uv;                                           \
    p_v += *p_offset++ & i_jump_uv;                                           \
    *p_pic++ = p_vout->lookup[                                                \
        (((*p_y + dither12[i_real_y]) >> 4) << 7)                             \
        + ((*p_u + dither22[i_real_y])   >> 5) * 9                            \
        + ((*p_v + dither22[i_real_y])   >> 5) ];                             \
    i_jump_uv = (i_jump_uv + *p_offset) & 0x1;                                \
    p_y += *p_offset;                                                         \
    p_u += *p_offset   & i_jump_uv;                                           \
    p_v += *p_offset++ & i_jump_uv;                                           \
    *p_pic++ = p_vout->lookup[                                                \
        (((*p_y + dither13[i_real_y]) >> 4) << 7)                             \
        + ((*p_u + dither23[i_real_y])   >> 5) * 9                            \
        + ((*p_v + dither23[i_real_y])   >> 5) ];                             \
    i_jump_uv = (i_jump_uv + *p_offset) & 0x1;                                \
    p_y += *p_offset;                                                         \
    p_u += *p_offset   & i_jump_uv;                                           \
    p_v += *p_offset++ & i_jump_uv;                                           \

/*****************************************************************************
 * SCALE_WIDTH: scale a line horizontally
 *****************************************************************************
 * This macro scales a line using rendering buffer and offset array. It works
 * for 1, 2 and 4 Bpp.
 *****************************************************************************/
#define SCALE_WIDTH                                                           \
    if( b_horizontal_scaling )                                                \
    {                                                                         \
        /* Horizontal scaling, conversion has been done to buffer.            \
         * Rewind buffer and offset, then copy and scale line */              \
        p_buffer = p_buffer_start;                                            \
        p_offset = p_offset_start;                                            \
        for( i_x = i_pic_width / 16; i_x--; )                                 \
        {                                                                     \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
            *p_pic++ = *p_buffer;   p_buffer += *p_offset++;                  \
        }                                                                     \
        p_pic += i_pic_line_width;                                            \
    }                                                                         \
    else                                                                      \
    {                                                                         \
        /* No scaling, conversion has been done directly in picture memory.   \
         * Increment of picture pointer to end of line is still needed */     \
        p_pic += i_pic_width + i_pic_line_width;                              \
    }                                                                         \


/*****************************************************************************
 * SCALE_WIDTH_DITHER: scale a line horizontally for dithered 8 bpp
 *****************************************************************************
 * This macro scales a line using an offset array.
 *****************************************************************************/
#define SCALE_WIDTH_DITHER( CHROMA )                                          \
    if( b_horizontal_scaling )                                                \
    {                                                                         \
        /* Horizontal scaling, but we can't use a buffer due to dither */     \
        p_offset = p_offset_start;                                            \
	i_jump_uv = 0;                                                        \
        for( i_x = i_pic_width / 16; i_x--; )                                 \
        {                                                                     \
            CONVERT_4YUV_PIXELS_SCALE( CHROMA )                               \
            CONVERT_4YUV_PIXELS_SCALE( CHROMA )                               \
            CONVERT_4YUV_PIXELS_SCALE( CHROMA )                               \
            CONVERT_4YUV_PIXELS_SCALE( CHROMA )                               \
        }                                                                     \
    }                                                                         \
    else                                                                      \
    {                                                                         \
        for( i_x = i_width / 16; i_x--;  )                                    \
        {                                                                     \
            CONVERT_4YUV_PIXELS( CHROMA )                                     \
            CONVERT_4YUV_PIXELS( CHROMA )                                     \
            CONVERT_4YUV_PIXELS( CHROMA )                                     \
            CONVERT_4YUV_PIXELS( CHROMA )                                     \
	}                                                                     \
    }                                                                         \
    /* Increment of picture pointer to end of line is still needed */         \
    p_pic += i_pic_line_width;                                                \
    i_real_y = (i_real_y + 1) & 0x3;                                          \

/*****************************************************************************
 * SCALE_HEIGHT: handle vertical scaling
 *****************************************************************************
 * This macro handle vertical scaling for a picture. CHROMA may be 420, 422 or
 * 444 for RGB conversion, or 400 for gray conversion. It works for 1, 2, 3
 * and 4 Bpp.
 *****************************************************************************/
#define SCALE_HEIGHT( CHROMA, BPP )                                           \
    /* If line is odd, rewind 4:2:0 U and V samples */                        \
    if( ((CHROMA == 420) || (CHROMA == 422)) && !(i_y & 0x1) )                \
    {                                                                         \
        p_u -= i_chroma_width;                                                \
        p_v -= i_chroma_width;                                                \
    }                                                                         \
                                                                              \
    /*                                                                        \
     * Handle vertical scaling. The current line can be copied or next one    \
     * can be ignored.                                                        \
     */                                                                       \
    switch( i_vertical_scaling )                                              \
    {                                                                         \
    case -1:                             /* vertical scaling factor is < 1 */ \
        while( (i_scale_count -= i_pic_height) >= 0 )                         \
        {                                                                     \
            /* Height reduction: skip next source line */                     \
            p_y += i_width;                                                   \
            i_y++;                                                            \
            if( (CHROMA == 420) || (CHROMA == 422) )                          \
            {                                                                 \
                if( i_y & 0x1 )                                               \
                {                                                             \
                    p_u += i_chroma_width;                                    \
                    p_v += i_chroma_width;                                    \
                }                                                             \
            }                                                                 \
            else if( CHROMA == 444 )                                          \
            {                                                                 \
                p_u += i_width;                                               \
                p_v += i_width;                                               \
            }                                                                 \
        }                                                                     \
        i_scale_count += i_height;                                            \
        break;                                                                \
    case 1:                              /* vertical scaling factor is > 1 */ \
        while( (i_scale_count -= i_height) > 0 )                              \
        {                                                                     \
            /* Height increment: copy previous picture line */                \
            for( i_x = i_pic_width / 16; i_x--; )                             \
            {                                                                 \
                *(((u64 *) p_pic)++) = *(((u64 *) p_pic_start)++ );           \
                *(((u64 *) p_pic)++) = *(((u64 *) p_pic_start)++ );           \
                if( BPP > 1 )                               /* 2, 3, 4 Bpp */ \
                {                                                             \
                    *(((u64 *) p_pic)++) = *(((u64 *) p_pic_start)++ );       \
                    *(((u64 *) p_pic)++) = *(((u64 *) p_pic_start)++ );       \
                }                                                             \
                if( BPP > 2 )                                  /* 3, 4 Bpp */ \
                {                                                             \
                    *(((u64 *) p_pic)++) = *(((u64 *) p_pic_start)++ );       \
                    *(((u64 *) p_pic)++) = *(((u64 *) p_pic_start)++ );       \
                }                                                             \
                if( BPP > 3 )                                     /* 4 Bpp */ \
                {                                                             \
                    *(((u64 *) p_pic)++) = *(((u64 *) p_pic_start)++ );       \
                    *(((u64 *) p_pic)++) = *(((u64 *) p_pic_start)++ );       \
                }                                                             \
            }                                                                 \
            p_pic +=        i_pic_line_width;                                 \
            p_pic_start +=  i_pic_line_width;                                 \
        }                                                                     \
        i_scale_count += i_pic_height;                                        \
        break;                                                                \
    }                                                                         \

/*****************************************************************************
 * SCALE_HEIGHT_DITHER: handle vertical scaling for dithered 8 bpp
 *****************************************************************************
 * This macro handles vertical scaling for a picture. CHROMA may be 420, 422 or
 * 444 for RGB conversion, or 400 for gray conversion.
 *****************************************************************************/
#define SCALE_HEIGHT_DITHER( CHROMA )                                         \
                                                                              \
    /* If line is odd, rewind 4:2:0 U and V samples */                        \
    if( ((CHROMA == 420) || (CHROMA == 422)) && !(i_y & 0x1) )                \
    {                                                                         \
        p_u -= i_chroma_width;                                                \
        p_v -= i_chroma_width;                                                \
    }                                                                         \
                                                                              \
    /*                                                                        \
     * Handle vertical scaling. The current line can be copied or next one    \
     * can be ignored.                                                        \
     */                                                                       \
                                                                              \
    switch( i_vertical_scaling )                                              \
    {                                                                         \
    case -1:                             /* vertical scaling factor is < 1 */ \
        while( (i_scale_count -= i_pic_height) >= 0 )                         \
        {                                                                     \
            /* Height reduction: skip next source line */                     \
            p_y += i_width;                                                   \
            i_y++;                                                            \
            if( (CHROMA == 420) || (CHROMA == 422) )                          \
            {                                                                 \
                if( i_y & 0x1 )                                               \
                {                                                             \
                    p_u += i_chroma_width;                                    \
                    p_v += i_chroma_width;                                    \
                }                                                             \
            }                                                                 \
            else if( CHROMA == 444 )                                          \
            {                                                                 \
                p_u += i_width;                                               \
                p_v += i_width;                                               \
            }                                                                 \
        }                                                                     \
        i_scale_count += i_height;                                            \
        break;                                                                \
    case 1:                              /* vertical scaling factor is > 1 */ \
        while( (i_scale_count -= i_height) > 0 )                              \
        {                                                                     \
            SCALE_WIDTH_DITHER( CHROMA );                                     \
            p_y -= i_width;                                                   \
            p_u -= i_chroma_width;                                            \
            p_v -= i_chroma_width;                                            \
            p_pic +=        i_pic_line_width;                                 \
        }                                                                     \
        i_scale_count += i_pic_height;                                        \
        break;                                                                \
    }                                                                         \

/*****************************************************************************
 * vout_InitYUV: allocate and initialize translations tables
 *****************************************************************************
 * This function will allocate memory to store translation tables, depending
 * of the screen depth.
 *****************************************************************************/
int vout_InitYUV( vout_thread_t *p_vout )
{
    size_t      tables_size;                        /* tables size, in bytes */
    
    /* Computes tables size - 3 Bpp use 32 bits pixel entries in tables */
    switch( p_vout->i_bytes_per_pixel )
    {
    case 1:
        /* nothing to allocate - will put the palette here afterwards */
        tables_size = 1;
        break;        
    case 2:
        tables_size = sizeof( u16 ) * (p_vout->b_grayscale ? GRAY_TABLE_SIZE : RGB_TABLE_SIZE);
        break;        
    case 3:        
    case 4:
    default:         
        tables_size = sizeof( u32 ) * (p_vout->b_grayscale ? GRAY_TABLE_SIZE : RGB_TABLE_SIZE);        
        break;        
    }
    
    /* Allocate memory */
    p_vout->yuv.p_base = malloc( tables_size );
    if( p_vout->yuv.p_base == NULL )
    {
        intf_ErrMsg("error: %s\n", strerror(ENOMEM));
        return( 1 );                
    }

    /* Allocate memory for conversion buffer and offset array */
    p_vout->yuv.p_buffer = malloc( VOUT_MAX_WIDTH * p_vout->i_bytes_per_pixel );
    if( p_vout->yuv.p_buffer == NULL )
    {
        intf_ErrMsg("error: %s\n", strerror(ENOMEM));
        free( p_vout->yuv.p_base );
        return( 1 );                
    }
    p_vout->yuv.p_offset = malloc( p_vout->i_width * sizeof( int ) );    
    if( p_vout->yuv.p_offset == NULL )
    {
        intf_ErrMsg("error: %s\n", strerror(ENOMEM));
        free( p_vout->yuv.p_base );
        free( p_vout->yuv.p_buffer );        
        return( 1 );                
    }

    /* Initialize tables */
    SetYUV( p_vout );
    return( 0 );    
}

/*****************************************************************************
 * vout_ResetTables: re-initialize translations tables
 *****************************************************************************
 * This function will initialize the tables allocated by vout_CreateTables and
 * set functions pointers.
 *****************************************************************************/
int vout_ResetYUV( vout_thread_t *p_vout )
{
    vout_EndYUV( p_vout );    
    return( vout_InitYUV( p_vout ) );    
}

/*****************************************************************************
 * vout_EndYUV: destroy translations tables
 *****************************************************************************
 * Free memory allocated by vout_CreateTables.
 *****************************************************************************/
void vout_EndYUV( vout_thread_t *p_vout )
{
    free( p_vout->yuv.p_base );
    free( p_vout->yuv.p_buffer );
    free( p_vout->yuv.p_offset );    
}

/* following functions are local */

/*****************************************************************************
 * SetGammaTable: return intensity table transformed by gamma curve.
 *****************************************************************************
 * pi_table is a table of 256 entries from 0 to 255.
 *****************************************************************************/
static void SetGammaTable( int *pi_table, double f_gamma )
{
    int         i_y;                                       /* base intensity */

    /* Use exp(gamma) instead of gamma */
    f_gamma = exp(f_gamma );

    /* Build gamma table */
    for( i_y = 0; i_y < 256; i_y++ )
    {
        pi_table[ i_y ] = pow( (double)i_y / 256, f_gamma ) * 256;
    }
 }

/*****************************************************************************
 * SetYUV: compute tables and set function pointers
+ *****************************************************************************/
static void SetYUV( vout_thread_t *p_vout )
{
    int         pi_gamma[256];                                /* gamma table */
    int         i_index;                                  /* index in tables */

    /* Build gamma table */    
    SetGammaTable( pi_gamma, p_vout->f_gamma );
    
    /*
     * Set pointers and build YUV tables
     */        
    if( p_vout->b_grayscale )
    {
        /* Grayscale: build gray table */
        switch( p_vout->i_bytes_per_pixel )
        {
        case 1:
            break;        
        case 2:
            p_vout->yuv.yuv.p_gray16 =  (u16 *)p_vout->yuv.p_base + GRAY_MARGIN;
            for( i_index = 0; i_index < GRAY_MARGIN; i_index++ )
            {
                p_vout->yuv.yuv.p_gray16[ -i_index ] =      RGB2PIXEL( p_vout, pi_gamma[0], pi_gamma[0], pi_gamma[0] );
                p_vout->yuv.yuv.p_gray16[ 256 + i_index ] = RGB2PIXEL( p_vout, pi_gamma[255], pi_gamma[255], pi_gamma[255] );
            }            
            for( i_index = 0; i_index < 256; i_index++) 
            {
                p_vout->yuv.yuv.p_gray16[ i_index ] = RGB2PIXEL( p_vout, pi_gamma[i_index], pi_gamma[i_index], pi_gamma[i_index] );
            }
            break;        
        case 3:
        case 4:        
            p_vout->yuv.yuv.p_gray32 =  (u32 *)p_vout->yuv.p_base + GRAY_MARGIN;
            for( i_index = 0; i_index < GRAY_MARGIN; i_index++ )
            {
                p_vout->yuv.yuv.p_gray32[ -i_index ] =      RGB2PIXEL( p_vout, pi_gamma[0], pi_gamma[0], pi_gamma[0] );
                p_vout->yuv.yuv.p_gray32[ 256 + i_index ] = RGB2PIXEL( p_vout, pi_gamma[255], pi_gamma[255], pi_gamma[255] );
            }            
            for( i_index = 0; i_index < 256; i_index++) 
            {
                p_vout->yuv.yuv.p_gray32[ i_index ] = RGB2PIXEL( p_vout, pi_gamma[i_index], pi_gamma[i_index], pi_gamma[i_index] );
            }
            break;        
         }
    }
    else
    {
        /* Color: build red, green and blue tables */
        switch( p_vout->i_bytes_per_pixel )
        {
        case 1:
            break;        
        case 2:
            p_vout->yuv.yuv.p_rgb16 = (u16 *)p_vout->yuv.p_base;
            for( i_index = 0; i_index < RED_MARGIN; i_index++ )
            {
                p_vout->yuv.yuv.p_rgb16[RED_OFFSET - RED_MARGIN + i_index] = RGB2PIXEL( p_vout, pi_gamma[0], 0, 0 );
                p_vout->yuv.yuv.p_rgb16[RED_OFFSET + 256 + i_index] =        RGB2PIXEL( p_vout, pi_gamma[255], 0, 0 );
            }
            for( i_index = 0; i_index < GREEN_MARGIN; i_index++ )
            {
                p_vout->yuv.yuv.p_rgb16[GREEN_OFFSET - GREEN_MARGIN + i_index] = RGB2PIXEL( p_vout, 0, pi_gamma[0], 0 );
                p_vout->yuv.yuv.p_rgb16[GREEN_OFFSET + 256 + i_index] =          RGB2PIXEL( p_vout, 0, pi_gamma[255], 0 );
            }
            for( i_index = 0; i_index < BLUE_MARGIN; i_index++ )
            {
                p_vout->yuv.yuv.p_rgb16[BLUE_OFFSET - BLUE_MARGIN + i_index] = RGB2PIXEL( p_vout, 0, 0, pi_gamma[0] );
                p_vout->yuv.yuv.p_rgb16[BLUE_OFFSET + BLUE_MARGIN + i_index] = RGB2PIXEL( p_vout, 0, 0, pi_gamma[255] );
            }
            for( i_index = 0; i_index < 256; i_index++ )
            {
                p_vout->yuv.yuv.p_rgb16[RED_OFFSET + i_index] =   RGB2PIXEL( p_vout, pi_gamma[ i_index ], 0, 0 );
                p_vout->yuv.yuv.p_rgb16[GREEN_OFFSET + i_index] = RGB2PIXEL( p_vout, 0, pi_gamma[ i_index ], 0 );
                p_vout->yuv.yuv.p_rgb16[BLUE_OFFSET + i_index] =  RGB2PIXEL( p_vout, 0, 0, pi_gamma[ i_index ] ); 
            }            
            break;        
        case 3:
        case 4:
            p_vout->yuv.yuv.p_rgb32 = (u32 *)p_vout->yuv.p_base;
            for( i_index = 0; i_index < RED_MARGIN; i_index++ )
            {
                p_vout->yuv.yuv.p_rgb32[RED_OFFSET - RED_MARGIN + i_index] = RGB2PIXEL( p_vout, pi_gamma[0], 0, 0 );
                p_vout->yuv.yuv.p_rgb32[RED_OFFSET + 256 + i_index] =        RGB2PIXEL( p_vout, pi_gamma[255], 0, 0 );
            }
            for( i_index = 0; i_index < GREEN_MARGIN; i_index++ )
            {
                p_vout->yuv.yuv.p_rgb32[GREEN_OFFSET - GREEN_MARGIN + i_index] = RGB2PIXEL( p_vout, 0, pi_gamma[0], 0 );
                p_vout->yuv.yuv.p_rgb32[GREEN_OFFSET + 256 + i_index] =          RGB2PIXEL( p_vout, 0, pi_gamma[255], 0 );
            }
            for( i_index = 0; i_index < BLUE_MARGIN; i_index++ )
            {
                p_vout->yuv.yuv.p_rgb32[BLUE_OFFSET - BLUE_MARGIN + i_index] = RGB2PIXEL( p_vout, 0, 0, pi_gamma[0] );
                p_vout->yuv.yuv.p_rgb32[BLUE_OFFSET + BLUE_MARGIN + i_index] = RGB2PIXEL( p_vout, 0, 0, pi_gamma[255] );
            }
            for( i_index = 0; i_index < 256; i_index++ )
            {
                p_vout->yuv.yuv.p_rgb32[RED_OFFSET + i_index] =   RGB2PIXEL( p_vout, pi_gamma[ i_index ], 0, 0 );
                p_vout->yuv.yuv.p_rgb32[GREEN_OFFSET + i_index] = RGB2PIXEL( p_vout, 0, pi_gamma[ i_index ], 0 );
                p_vout->yuv.yuv.p_rgb32[BLUE_OFFSET + i_index] =  RGB2PIXEL( p_vout, 0, 0, pi_gamma[ i_index ] ); 
            }            
            break;        
        }
    }    

    /*
     * Set functions pointers
     */
    if( p_vout->b_grayscale )
    {
        /* Grayscale */
        switch( p_vout->i_bytes_per_pixel )
        {
        case 1:
            p_vout->yuv.p_Convert420 = (vout_yuv_convert_t *) ConvertY4Gray8;        
            p_vout->yuv.p_Convert422 = (vout_yuv_convert_t *) ConvertY4Gray8;        
            p_vout->yuv.p_Convert444 = (vout_yuv_convert_t *) ConvertY4Gray8;        
            break;        
        case 2:
            p_vout->yuv.p_Convert420 = (vout_yuv_convert_t *) ConvertY4Gray16;        
            p_vout->yuv.p_Convert422 = (vout_yuv_convert_t *) ConvertY4Gray16;        
            p_vout->yuv.p_Convert444 = (vout_yuv_convert_t *) ConvertY4Gray16;        
            break;        
        case 3:
            p_vout->yuv.p_Convert420 = (vout_yuv_convert_t *) ConvertY4Gray24;        
            p_vout->yuv.p_Convert422 = (vout_yuv_convert_t *) ConvertY4Gray24;        
            p_vout->yuv.p_Convert444 = (vout_yuv_convert_t *) ConvertY4Gray24;        
            break;        
        case 4:        
            p_vout->yuv.p_Convert420 = (vout_yuv_convert_t *) ConvertY4Gray32;        
            p_vout->yuv.p_Convert422 = (vout_yuv_convert_t *) ConvertY4Gray32;        
            p_vout->yuv.p_Convert444 = (vout_yuv_convert_t *) ConvertY4Gray32;        
            break;        
        }        
    }
    else
    {
        /* Color */
        switch( p_vout->i_bytes_per_pixel )
        {
        case 1:
            p_vout->yuv.p_Convert420 = (vout_yuv_convert_t *) ConvertYUV420RGB8;
            p_vout->yuv.p_Convert422 = (vout_yuv_convert_t *) ConvertYUV422RGB8;
            p_vout->yuv.p_Convert444 = (vout_yuv_convert_t *) ConvertYUV444RGB8;
            break;        
        case 2:
            p_vout->yuv.p_Convert420 =   (vout_yuv_convert_t *) ConvertYUV420RGB16;        
            p_vout->yuv.p_Convert422 =   (vout_yuv_convert_t *) ConvertYUV422RGB16;        
            p_vout->yuv.p_Convert444 =   (vout_yuv_convert_t *) ConvertYUV444RGB16;        
            break;        
        case 3:
            p_vout->yuv.p_Convert420 =   (vout_yuv_convert_t *) ConvertYUV420RGB24;        
            p_vout->yuv.p_Convert422 =   (vout_yuv_convert_t *) ConvertYUV422RGB24;        
            p_vout->yuv.p_Convert444 =   (vout_yuv_convert_t *) ConvertYUV444RGB24;        
            break;        
        case 4:        
            p_vout->yuv.p_Convert420 =   (vout_yuv_convert_t *) ConvertYUV420RGB32;        
            p_vout->yuv.p_Convert422 =   (vout_yuv_convert_t *) ConvertYUV422RGB32;        
            p_vout->yuv.p_Convert444 =   (vout_yuv_convert_t *) ConvertYUV444RGB32;        
            break;        
        }
    }        
}

/*****************************************************************************
 * SetOffset: build offset array for conversion functions
 *****************************************************************************
 * This function will build an offset array used in later conversion functions.
 * It will also set horizontal and vertical scaling indicators.
 *****************************************************************************/
static void SetOffset( int i_width, int i_height, int i_pic_width, int i_pic_height, 
                       boolean_t *pb_h_scaling, int *pi_v_scaling, int *p_offset )
{    
    int i_x;                                    /* x position in destination */
    int i_scale_count;                                     /* modulo counter */

    /*
     * Prepare horizontal offset array
     */      
    if( i_pic_width - i_width > 0 )
    {
        /* Prepare scaling array for horizontal extension */
        *pb_h_scaling =  1;   
        i_scale_count =         i_pic_width;
        for( i_x = i_width; i_x--; )
        {
            while( (i_scale_count -= i_width) > 0 )
            {
                *p_offset++ = 0;                
            }
            *p_offset++ = 1;            
            i_scale_count += i_pic_width;            
        }        
    }
    else if( i_pic_width - i_width < 0 )
    {
        /* Prepare scaling array for horizontal reduction */
        *pb_h_scaling =  1;
        i_scale_count =         i_pic_width;
        for( i_x = i_pic_width; i_x--; )
        {
            *p_offset = 1;            
            while( (i_scale_count -= i_pic_width) >= 0 )
            {                
                *p_offset += 1;                
            }
            p_offset++;
            i_scale_count += i_width;
        }        
    }
    else
    {
        /* No horizontal scaling: YUV conversion is done directly to picture */          
        *pb_h_scaling = 0;        
    }

    /*
     * Set vertical scaling indicator
     */
    if( i_pic_height - i_height > 0 )
    {
        *pi_v_scaling = 1;        
    }
    else if( i_pic_height - i_height < 0 )
    {
        *pi_v_scaling = -1;        
    }
    else
    {
        *pi_v_scaling = 0;        
    }
}

/*****************************************************************************
 * ConvertY4Gray8: grayscale YUV 4:x:x to RGB 8 bpp
 *****************************************************************************/
static void ConvertY4Gray8( p_vout_thread_t p_vout, u8 *p_pic, yuv_data_t *p_y,
                            yuv_data_t *p_u, yuv_data_t *p_v, int i_width,
                            int i_height, int i_pic_width, int i_pic_height,
                            int i_pic_line_width, int i_matrix_coefficients )
{
    boolean_t   b_horizontal_scaling;             /* horizontal scaling type */
    int         i_vertical_scaling;                 /* vertical scaling type */
    int         i_x, i_y;                 /* horizontal and vertical indexes */
    int         i_scale_count;                       /* scale modulo counter */
    int         i_chroma_width;                    /* chroma width, not used */
    u8 *        p_gray;                             /* base conversion table */
    u8 *        p_pic_start;       /* beginning of the current line for copy */
    u8 *        p_buffer_start;                   /* conversion buffer start */
    u8 *        p_buffer;                       /* conversion buffer pointer */
    int *       p_offset_start;                        /* offset array start */
    int *       p_offset;                            /* offset array pointer */
    
    /* 
     * Initialize some values  - i_pic_line_width will store the line skip 
     */
    i_pic_line_width -= i_pic_width;                                            
    p_gray =            p_vout->yuv.yuv.p_gray8;    
    p_buffer_start =    p_vout->yuv.p_buffer;                                   
    p_offset_start =    p_vout->yuv.p_offset;                                   
    SetOffset( i_width, i_height, i_pic_width, i_pic_height, 
               &b_horizontal_scaling, &i_vertical_scaling, p_offset_start );

    /*
     * Perform conversion
     */
    i_scale_count = i_pic_height;
    for( i_y = 0; i_y < i_height; i_y++ )
    {
        /* Mark beginnning of line for possible later line copy, and initialize
         * buffer */
        p_pic_start =   p_pic;
        p_buffer =      b_horizontal_scaling ? p_buffer_start : p_pic;

        /* Do YUV conversion to buffer - YUV picture is always formed of 16
         * pixels wide blocks */
        for( i_x = i_width / 16; i_x--;  )
        {
            *p_buffer++ = *p_y++; *p_buffer++ = *p_y++;
            *p_buffer++ = *p_y++; *p_buffer++ = *p_y++;
            *p_buffer++ = *p_y++; *p_buffer++ = *p_y++;
            *p_buffer++ = *p_y++; *p_buffer++ = *p_y++;
            *p_buffer++ = *p_y++; *p_buffer++ = *p_y++;
            *p_buffer++ = *p_y++; *p_buffer++ = *p_y++;
            *p_buffer++ = *p_y++; *p_buffer++ = *p_y++;
            *p_buffer++ = *p_y++; *p_buffer++ = *p_y++;
        }             

        /* Do horizontal and vertical scaling */
        SCALE_WIDTH;
        SCALE_HEIGHT(400, 1);        
    }
}

/*****************************************************************************
 * ConvertY4Gray16: grayscale YUV 4:x:x to RGB 2 Bpp
 *****************************************************************************/
static void ConvertY4Gray16( p_vout_thread_t p_vout, u16 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                             int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                             int i_matrix_coefficients )
{
    boolean_t   b_horizontal_scaling;             /* horizontal scaling type */
    int         i_vertical_scaling;                 /* vertical scaling type */
    int         i_x, i_y;                 /* horizontal and vertical indexes */
    int         i_scale_count;                       /* scale modulo counter */
    int         i_chroma_width;                    /* chroma width, not used */
    u16 *       p_gray;                             /* base conversion table */
    u16 *       p_pic_start;       /* beginning of the current line for copy */
    u16 *       p_buffer_start;                   /* conversion buffer start */
    u16 *       p_buffer;                       /* conversion buffer pointer */
    int *       p_offset_start;                        /* offset array start */
    int *       p_offset;                            /* offset array pointer */
    
    /* 
     * Initialize some values  - i_pic_line_width will store the line skip 
     */
    i_pic_line_width -= i_pic_width;                                            
    p_gray =            p_vout->yuv.yuv.p_gray16;    
    p_buffer_start =    p_vout->yuv.p_buffer;                                   
    p_offset_start =    p_vout->yuv.p_offset;                                   
    SetOffset( i_width, i_height, i_pic_width, i_pic_height, 
               &b_horizontal_scaling, &i_vertical_scaling, p_offset_start );

    /*
     * Perform conversion
     */
    i_scale_count = i_pic_height;
    for( i_y = 0; i_y < i_height; i_y++ )
    {
        /* Mark beginnning of line for possible later line copy, and initialize
         * buffer */
        p_pic_start =   p_pic;
        p_buffer =      b_horizontal_scaling ? p_buffer_start : p_pic;        

        /* Do YUV conversion to buffer - YUV picture is always formed of 16
         * pixels wide blocks */
        for( i_x = i_width / 16; i_x--;  )
        {
            *p_buffer++ = p_gray[ *p_y++ ]; *p_buffer++ = p_gray[ *p_y++ ];
            *p_buffer++ = p_gray[ *p_y++ ]; *p_buffer++ = p_gray[ *p_y++ ];
            *p_buffer++ = p_gray[ *p_y++ ]; *p_buffer++ = p_gray[ *p_y++ ];
            *p_buffer++ = p_gray[ *p_y++ ]; *p_buffer++ = p_gray[ *p_y++ ];
            *p_buffer++ = p_gray[ *p_y++ ]; *p_buffer++ = p_gray[ *p_y++ ];
            *p_buffer++ = p_gray[ *p_y++ ]; *p_buffer++ = p_gray[ *p_y++ ];
            *p_buffer++ = p_gray[ *p_y++ ]; *p_buffer++ = p_gray[ *p_y++ ];
            *p_buffer++ = p_gray[ *p_y++ ]; *p_buffer++ = p_gray[ *p_y++ ];
        }             

        /* Do horizontal and vertical scaling */
        SCALE_WIDTH;
        SCALE_HEIGHT(400, 2);        
    }
}

/*****************************************************************************
 * ConvertY4Gray24: grayscale YUV 4:x:x to RGB 3 Bpp
 *****************************************************************************/
static void ConvertY4Gray24( p_vout_thread_t p_vout, void *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                             int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                             int i_matrix_coefficients )
{
    //??
}

/*****************************************************************************
 * ConvertY4Gray32: grayscale YUV 4:x:x to RGB 4 Bpp
 *****************************************************************************/
static void ConvertY4Gray32( p_vout_thread_t p_vout, u32 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                             int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                             int i_matrix_coefficients )
{
    boolean_t   b_horizontal_scaling;             /* horizontal scaling type */
    int         i_vertical_scaling;                 /* vertical scaling type */
    int         i_x, i_y;                 /* horizontal and vertical indexes */
    int         i_scale_count;                       /* scale modulo counter */
    int         i_chroma_width;                    /* chroma width, not used */
    u32 *       p_gray;                             /* base conversion table */
    u32 *       p_pic_start;       /* beginning of the current line for copy */
    u32 *       p_buffer_start;                   /* conversion buffer start */
    u32 *       p_buffer;                       /* conversion buffer pointer */
    int *       p_offset_start;                        /* offset array start */
    int *       p_offset;                            /* offset array pointer */
    
    /* 
     * Initialize some values  - i_pic_line_width will store the line skip 
     */
    i_pic_line_width -= i_pic_width;                                            
    p_gray =            p_vout->yuv.yuv.p_gray32;    
    p_buffer_start =    p_vout->yuv.p_buffer;                                   
    p_offset_start =    p_vout->yuv.p_offset;                                   
    SetOffset( i_width, i_height, i_pic_width, i_pic_height, 
               &b_horizontal_scaling, &i_vertical_scaling, p_offset_start );

    /*
     * Perform conversion
     */
    i_scale_count = i_pic_height;
    for( i_y = 0; i_y < i_height; i_y++ )
    {
        /* Mark beginnning of line for possible later line copy, and initialize
         * buffer */
        p_pic_start =   p_pic;
        p_buffer =      b_horizontal_scaling ? p_buffer_start : p_pic;        

        /* Do YUV conversion to buffer - YUV picture is always formed of 16
         * pixels wide blocks */
        for( i_x = i_width / 16; i_x--;  )
        {
            *p_buffer++ = p_gray[ *p_y++ ]; *p_buffer++ = p_gray[ *p_y++ ];
            *p_buffer++ = p_gray[ *p_y++ ]; *p_buffer++ = p_gray[ *p_y++ ];
            *p_buffer++ = p_gray[ *p_y++ ]; *p_buffer++ = p_gray[ *p_y++ ];
            *p_buffer++ = p_gray[ *p_y++ ]; *p_buffer++ = p_gray[ *p_y++ ];
            *p_buffer++ = p_gray[ *p_y++ ]; *p_buffer++ = p_gray[ *p_y++ ];
            *p_buffer++ = p_gray[ *p_y++ ]; *p_buffer++ = p_gray[ *p_y++ ];
            *p_buffer++ = p_gray[ *p_y++ ]; *p_buffer++ = p_gray[ *p_y++ ];
            *p_buffer++ = p_gray[ *p_y++ ]; *p_buffer++ = p_gray[ *p_y++ ];
        }             

        /* Do horizontal and vertical scaling */
        SCALE_WIDTH;
        SCALE_HEIGHT(400, 4);        
    }
}

/*****************************************************************************
 * ConvertYUV420RGB8: color YUV 4:2:0 to RGB 8 bpp
 *****************************************************************************/
static void ConvertYUV420RGB8( p_vout_thread_t p_vout, u8 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                int i_matrix_coefficients )
{
    boolean_t   b_horizontal_scaling;             /* horizontal scaling type */
    int         i_vertical_scaling;                 /* vertical scaling type */
    int         i_x, i_y;                 /* horizontal and vertical indexes */
    int         i_scale_count;                       /* scale modulo counter */
    int         i_jump_uv;
    int         i_real_y;
    int         i_chroma_width;                              /* chroma width */
    int *       p_offset_start;                        /* offset array start */
    int *       p_offset;                            /* offset array pointer */
    
    int dither10[4] = { 0x0, 0x8, 0x2, 0xa };
    int dither11[4] = { 0xc, 0x4, 0xe, 0x6 };
    int dither12[4] = { 0x3, 0xb, 0x1, 0x9 };
    int dither13[4] = { 0xf, 0x7, 0xd, 0x5 };
    int dither20[4] = { 0x00, 0x10, 0x04, 0x14 };
    int dither21[4] = { 0x18, 0x08, 0x1c, 0x0c };
    int dither22[4] = { 0x06, 0x16, 0x02, 0x12 };
    int dither23[4] = { 0x1e, 0x0e, 0x1a, 0x0a };

    //int dither[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 16 }, { 3, 11, 1, 9}, {15, 7, 13, 5} };
    //int dither[4][4] = { { 7, 8, 0, 15 }, { 0, 15, 8, 7 }, { 7, 0, 15, 8 }, { 15, 7, 8, 0 } };
    //int dither[4][4] = { { 0, 15, 0, 15 }, { 15, 0, 15, 0 }, { 0, 15, 0, 15 }, { 15, 0, 15, 0 } };
    //int dither[4][4] = { { 15, 15, 0, 0 }, { 15, 15, 0, 0 }, { 0, 0, 15, 15 }, { 0, 0, 15, 15 } };
    //int dither[4][4] = { { 8, 8, 8, 8 }, { 8, 8, 8, 8 }, { 8, 8, 8, 8 }, { 8, 8, 8, 8 } };
    //int dither[4][4] = { { 0, 1, 2, 3 }, { 4, 5, 6, 7 }, { 8, 9, 10, 11 }, { 12, 13, 14, 15 } };
    /* 
     * Initialize some values  - i_pic_line_width will store the line skip 
     */
    i_pic_line_width -= i_pic_width;
    i_chroma_width =    i_width / 2;
    p_offset_start =    p_vout->yuv.p_offset;                    
    SetOffset( i_width, i_height, i_pic_width, i_pic_height, 
               &b_horizontal_scaling, &i_vertical_scaling, p_offset_start );

    /*
     * Perform conversion
     */
    i_scale_count = i_pic_height;
    i_real_y = 0;
    for( i_y = 0; i_y < i_height; i_y++ )
    {
        /* Do horizontal and vertical scaling */
        SCALE_WIDTH_DITHER( 420 );
        SCALE_HEIGHT_DITHER( 420 );
    }
}

/*****************************************************************************
 * ConvertYUV422RGB8: color YUV 4:2:2 to RGB 8 bpp
 *****************************************************************************/
static void ConvertYUV422RGB8( p_vout_thread_t p_vout, u8 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                int i_matrix_coefficients )
{
    boolean_t   b_horizontal_scaling;             /* horizontal scaling type */
    int         i_vertical_scaling;                 /* vertical scaling type */
    int         i_x, i_y;                 /* horizontal and vertical indexes */
    int         i_scale_count;                       /* scale modulo counter */
    int         i_uval, i_vval;                           /* U and V samples */
    int         i_red, i_green, i_blue;          /* U and V modified samples */
    int         i_chroma_width;                              /* chroma width */
    u8 *        p_yuv;                              /* base conversion table */
    u8 *        p_ybase;                     /* Y dependant conversion table */
    u8 *        p_pic_start;       /* beginning of the current line for copy */
    u8 *        p_buffer_start;                   /* conversion buffer start */
    u8 *        p_buffer;                       /* conversion buffer pointer */
    int *       p_offset_start;                        /* offset array start */
    int *       p_offset;                            /* offset array pointer */
    
    /* 
     * Initialize some values  - i_pic_line_width will store the line skip 
     */
    i_pic_line_width -= i_pic_width;
    i_chroma_width =    i_width / 2;
    p_yuv =             p_vout->yuv.yuv.p_rgb8;
    p_buffer_start =    p_vout->yuv.p_buffer;        
    p_offset_start =    p_vout->yuv.p_offset;                    
    SetOffset( i_width, i_height, i_pic_width, i_pic_height, 
               &b_horizontal_scaling, &i_vertical_scaling, p_offset_start );

    /*
     * Perform conversion
     */
    i_scale_count = i_pic_height;
    for( i_y = 0; i_y < i_height; i_y++ )
    {
        /* Mark beginnning of line for possible later line copy, and initialize
         * buffer */
        p_pic_start =   p_pic;
        p_buffer =      b_horizontal_scaling ? p_buffer_start : p_pic;        

        /* Do YUV conversion to buffer - YUV picture is always formed of 16
         * pixels wide blocks */
        for( i_x = i_width / 16; i_x--;  )
        {
            CONVERT_YUV_PIXEL(1);  CONVERT_Y_PIXEL(1);
            CONVERT_YUV_PIXEL(1);  CONVERT_Y_PIXEL(1);
            CONVERT_YUV_PIXEL(1);  CONVERT_Y_PIXEL(1);
            CONVERT_YUV_PIXEL(1);  CONVERT_Y_PIXEL(1);
            CONVERT_YUV_PIXEL(1);  CONVERT_Y_PIXEL(1);
            CONVERT_YUV_PIXEL(1);  CONVERT_Y_PIXEL(1);
            CONVERT_YUV_PIXEL(1);  CONVERT_Y_PIXEL(1);
            CONVERT_YUV_PIXEL(1);  CONVERT_Y_PIXEL(1);
        }             

        /* Do horizontal and vertical scaling */
        SCALE_WIDTH;
        SCALE_HEIGHT(422, 1);        
    }
}

/*****************************************************************************
 * ConvertYUV444RGB8: color YUV 4:4:4 to RGB 8 bpp
 *****************************************************************************/
static void ConvertYUV444RGB8( p_vout_thread_t p_vout, u8 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                int i_matrix_coefficients )
{
    boolean_t   b_horizontal_scaling;             /* horizontal scaling type */
    int         i_vertical_scaling;                 /* vertical scaling type */
    int         i_x, i_y;                 /* horizontal and vertical indexes */
    int         i_scale_count;                       /* scale modulo counter */
    int         i_uval, i_vval;                           /* U and V samples */
    int         i_red, i_green, i_blue;          /* U and V modified samples */
    int         i_chroma_width;                    /* chroma width, not used */
    u8 *        p_yuv;                              /* base conversion table */
    u8 *        p_ybase;                     /* Y dependant conversion table */
    u8 *        p_pic_start;       /* beginning of the current line for copy */
    u8 *        p_buffer_start;                   /* conversion buffer start */
    u8 *        p_buffer;                       /* conversion buffer pointer */
    int *       p_offset_start;                        /* offset array start */
    int *       p_offset;                            /* offset array pointer */
    
    /* 
     * Initialize some values  - i_pic_line_width will store the line skip 
     */
    i_pic_line_width -= i_pic_width;
    p_yuv =             p_vout->yuv.yuv.p_rgb8;
    p_buffer_start =    p_vout->yuv.p_buffer;        
    p_offset_start =    p_vout->yuv.p_offset;                    
    SetOffset( i_width, i_height, i_pic_width, i_pic_height, 
               &b_horizontal_scaling, &i_vertical_scaling, p_offset_start );

    /*
     * Perform conversion
     */
    i_scale_count = i_pic_height;
    for( i_y = 0; i_y < i_height; i_y++ )
    {
        /* Mark beginnning of line for possible later line copy, and initialize
         * buffer */
        p_pic_start =   p_pic;
        p_buffer =      b_horizontal_scaling ? p_buffer_start : p_pic;        

        /* Do YUV conversion to buffer - YUV picture is always formed of 16
         * pixels wide blocks */
        for( i_x = i_width / 16; i_x--;  )
        {
            CONVERT_YUV_PIXEL(1);  CONVERT_YUV_PIXEL(1);
            CONVERT_YUV_PIXEL(1);  CONVERT_YUV_PIXEL(1);
            CONVERT_YUV_PIXEL(1);  CONVERT_YUV_PIXEL(1);
            CONVERT_YUV_PIXEL(1);  CONVERT_YUV_PIXEL(1);
            CONVERT_YUV_PIXEL(1);  CONVERT_YUV_PIXEL(1);
            CONVERT_YUV_PIXEL(1);  CONVERT_YUV_PIXEL(1);
            CONVERT_YUV_PIXEL(1);  CONVERT_YUV_PIXEL(1);
            CONVERT_YUV_PIXEL(1);  CONVERT_YUV_PIXEL(1);
        }             

        /* Do horizontal and vertical scaling */
        SCALE_WIDTH;
        SCALE_HEIGHT(444, 1);        
    }
}

/*****************************************************************************
 * ConvertYUV420RGB16: color YUV 4:2:0 to RGB 2 Bpp
 *****************************************************************************/
static void ConvertYUV420RGB16( p_vout_thread_t p_vout, u16 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                int i_matrix_coefficients )
{
/* MMX version */
  //  int                 i_chroma_width, i_chroma_skip;      /* width and eol for chroma */
/*
    i_chroma_width =    i_width / 2;
    i_chroma_skip =     i_skip / 2;
    ConvertYUV420RGB16MMX( p_y, p_u, p_v, i_width, i_height, 
                           (i_width + i_skip) * sizeof( yuv_data_t ), 
                           (i_chroma_width + i_chroma_skip) * sizeof( yuv_data_t),
                           i_scale, (u8 *)p_pic, 0, 0, (i_width + i_pic_eol) * sizeof( u16 ),
                           p_vout->i_screen_depth == 15 );    
*/
    boolean_t   b_horizontal_scaling;             /* horizontal scaling type */
    int         i_vertical_scaling;                 /* vertical scaling type */
    int         i_x, i_y;                 /* horizontal and vertical indexes */
    int         i_scale_count;                       /* scale modulo counter */
    int         i_uval, i_vval;                           /* U and V samples */
    int         i_red, i_green, i_blue;          /* U and V modified samples */
    int         i_chroma_width;                              /* chroma width */
    u16 *       p_yuv;                              /* base conversion table */
    u16 *       p_ybase;                     /* Y dependant conversion table */
    u16 *       p_pic_start;       /* beginning of the current line for copy */
    u16 *       p_buffer_start;                   /* conversion buffer start */
    u16 *       p_buffer;                       /* conversion buffer pointer */
    int *       p_offset_start;                        /* offset array start */
    int *       p_offset;                            /* offset array pointer */
    
    /* 
     * Initialize some values  - i_pic_line_width will store the line skip 
     */
    i_pic_line_width -= i_pic_width;
    i_chroma_width =    i_width / 2;
    p_yuv =             p_vout->yuv.yuv.p_rgb16;
    p_buffer_start =    p_vout->yuv.p_buffer;        
    p_offset_start =    p_vout->yuv.p_offset;                    
    SetOffset( i_width, i_height, i_pic_width, i_pic_height, 
               &b_horizontal_scaling, &i_vertical_scaling, p_offset_start );

    /*
     * Perform conversion
     */
    i_scale_count = i_pic_height;
    for( i_y = 0; i_y < i_height; i_y++ )
    {
        /* Mark beginnning of line for possible later line copy, and initialize
         * buffer */
        p_pic_start =   p_pic;
        p_buffer =      b_horizontal_scaling ? p_buffer_start : p_pic;        

        /* Do YUV conversion to buffer - YUV picture is always formed of 16
         * pixels wide blocks */
        for( i_x = i_width / 16; i_x--;  )
        {
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
        }             

        /* Do horizontal and vertical scaling */
        SCALE_WIDTH;
        SCALE_HEIGHT(420, 2);        
    }
}

/*****************************************************************************
 * ConvertYUV422RGB16: color YUV 4:2:2 to RGB 2 Bpp
 *****************************************************************************/
static void ConvertYUV422RGB16( p_vout_thread_t p_vout, u16 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                int i_matrix_coefficients )
{
    boolean_t   b_horizontal_scaling;             /* horizontal scaling type */
    int         i_vertical_scaling;                 /* vertical scaling type */
    int         i_x, i_y;                 /* horizontal and vertical indexes */
    int         i_scale_count;                       /* scale modulo counter */
    int         i_uval, i_vval;                           /* U and V samples */
    int         i_red, i_green, i_blue;          /* U and V modified samples */
    int         i_chroma_width;                              /* chroma width */
    u16 *       p_yuv;                              /* base conversion table */
    u16 *       p_ybase;                     /* Y dependant conversion table */
    u16 *       p_pic_start;       /* beginning of the current line for copy */
    u16 *       p_buffer_start;                   /* conversion buffer start */
    u16 *       p_buffer;                       /* conversion buffer pointer */
    int *       p_offset_start;                        /* offset array start */
    int *       p_offset;                            /* offset array pointer */
    
    /* 
     * Initialize some values  - i_pic_line_width will store the line skip 
     */
    i_pic_line_width -= i_pic_width;
    i_chroma_width =    i_width / 2;
    p_yuv =             p_vout->yuv.yuv.p_rgb16;
    p_buffer_start =    p_vout->yuv.p_buffer;        
    p_offset_start =    p_vout->yuv.p_offset;                    
    SetOffset( i_width, i_height, i_pic_width, i_pic_height, 
               &b_horizontal_scaling, &i_vertical_scaling, p_offset_start );

    /*
     * Perform conversion
     */
    i_scale_count = i_pic_height;
    for( i_y = 0; i_y < i_height; i_y++ )
    {
        /* Mark beginnning of line for possible later line copy, and initialize
         * buffer */
        p_pic_start =   p_pic;
        p_buffer =      b_horizontal_scaling ? p_buffer_start : p_pic;        

        /* Do YUV conversion to buffer - YUV picture is always formed of 16
         * pixels wide blocks */
        for( i_x = i_width / 16; i_x--;  )
        {
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_Y_PIXEL(2);
        }             

        /* Do horizontal and vertical scaling */
        SCALE_WIDTH;
        SCALE_HEIGHT(422, 2);        
    }
}

/*****************************************************************************
 * ConvertYUV444RGB16: color YUV 4:4:4 to RGB 2 Bpp
 *****************************************************************************/
static void ConvertYUV444RGB16( p_vout_thread_t p_vout, u16 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                int i_matrix_coefficients )
{
    boolean_t   b_horizontal_scaling;             /* horizontal scaling type */
    int         i_vertical_scaling;                 /* vertical scaling type */
    int         i_x, i_y;                 /* horizontal and vertical indexes */
    int         i_scale_count;                       /* scale modulo counter */
    int         i_uval, i_vval;                           /* U and V samples */
    int         i_red, i_green, i_blue;          /* U and V modified samples */
    int         i_chroma_width;                    /* chroma width, not used */
    u16 *       p_yuv;                              /* base conversion table */
    u16 *       p_ybase;                     /* Y dependant conversion table */
    u16 *       p_pic_start;       /* beginning of the current line for copy */
    u16 *       p_buffer_start;                   /* conversion buffer start */
    u16 *       p_buffer;                       /* conversion buffer pointer */
    int *       p_offset_start;                        /* offset array start */
    int *       p_offset;                            /* offset array pointer */
    
    /* 
     * Initialize some values  - i_pic_line_width will store the line skip 
     */
    i_pic_line_width -= i_pic_width;
    p_yuv =             p_vout->yuv.yuv.p_rgb16;
    p_buffer_start =    p_vout->yuv.p_buffer;        
    p_offset_start =    p_vout->yuv.p_offset;                    
    SetOffset( i_width, i_height, i_pic_width, i_pic_height, 
               &b_horizontal_scaling, &i_vertical_scaling, p_offset_start );

    /*
     * Perform conversion
     */
    i_scale_count = i_pic_height;
    for( i_y = 0; i_y < i_height; i_y++ )
    {
        /* Mark beginnning of line for possible later line copy, and initialize
         * buffer */
        p_pic_start =   p_pic;
        p_buffer =      b_horizontal_scaling ? p_buffer_start : p_pic;        

        /* Do YUV conversion to buffer - YUV picture is always formed of 16
         * pixels wide blocks */
        for( i_x = i_width / 16; i_x--;  )
        {
            CONVERT_YUV_PIXEL(2);  CONVERT_YUV_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_YUV_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_YUV_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_YUV_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_YUV_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_YUV_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_YUV_PIXEL(2);
            CONVERT_YUV_PIXEL(2);  CONVERT_YUV_PIXEL(2);
        }             

        /* Do horizontal and vertical scaling */
        SCALE_WIDTH;
        SCALE_HEIGHT(444, 2);        
    }
}

/*****************************************************************************
 * ConvertYUV420RGB24: color YUV 4:2:0 to RGB 3 Bpp
 *****************************************************************************/
static void ConvertYUV420RGB24( p_vout_thread_t p_vout, void *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                int i_matrix_coefficients )
{
    //???
}

/*****************************************************************************
 * ConvertYUV422RGB24: color YUV 4:2:2 to RGB 3 Bpp
 *****************************************************************************/
static void ConvertYUV422RGB24( p_vout_thread_t p_vout, void *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                int i_matrix_coefficients )
{
    //???
}

/*****************************************************************************
 * ConvertYUV444RGB24: color YUV 4:4:4 to RGB 3 Bpp
 *****************************************************************************/
static void ConvertYUV444RGB24( p_vout_thread_t p_vout, void *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                int i_matrix_coefficients )
{    
    //???
}

/*****************************************************************************
 * ConvertYUV420RGB32: color YUV 4:2:0 to RGB 4 Bpp
 *****************************************************************************/
static void ConvertYUV420RGB32( p_vout_thread_t p_vout, u32 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                int i_matrix_coefficients )
{
    boolean_t   b_horizontal_scaling;             /* horizontal scaling type */
    int         i_vertical_scaling;                 /* vertical scaling type */
    int         i_x, i_y;                 /* horizontal and vertical indexes */
    int         i_scale_count;                       /* scale modulo counter */
    int         i_uval, i_vval;                           /* U and V samples */
    int         i_red, i_green, i_blue;          /* U and V modified samples */
    int         i_chroma_width;                              /* chroma width */
    u32 *       p_yuv;                              /* base conversion table */
    u32 *       p_ybase;                     /* Y dependant conversion table */
    u32 *       p_pic_start;       /* beginning of the current line for copy */
    u32 *       p_buffer_start;                   /* conversion buffer start */
    u32 *       p_buffer;                       /* conversion buffer pointer */
    int *       p_offset_start;                        /* offset array start */
    int *       p_offset;                            /* offset array pointer */
    
    /* 
     * Initialize some values  - i_pic_line_width will store the line skip 
     */
    i_pic_line_width -= i_pic_width;
    i_chroma_width =    i_width / 2;
    p_yuv =             p_vout->yuv.yuv.p_rgb32;
    p_buffer_start =    p_vout->yuv.p_buffer;        
    p_offset_start =    p_vout->yuv.p_offset;                    
    SetOffset( i_width, i_height, i_pic_width, i_pic_height, 
               &b_horizontal_scaling, &i_vertical_scaling, p_offset_start );

    /*
     * Perform conversion
     */
    i_scale_count = i_pic_height;
    for( i_y = 0; i_y < i_height; i_y++ )
    {
        /* Mark beginnning of line for possible later line copy, and initialize
         * buffer */
        p_pic_start =   p_pic;
        p_buffer =      b_horizontal_scaling ? p_buffer_start : p_pic;        

        /* Do YUV conversion to buffer - YUV picture is always formed of 16
         * pixels wide blocks */
        for( i_x = i_width / 16; i_x--;  )
        {
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
        }             

        /* Do horizontal and vertical scaling */
        SCALE_WIDTH;
        SCALE_HEIGHT(420, 4);        
    }
}

/*****************************************************************************
 * ConvertYUV422RGB32: color YUV 4:2:2 to RGB 4 Bpp
 *****************************************************************************/
static void ConvertYUV422RGB32( p_vout_thread_t p_vout, u32 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                int i_matrix_coefficients )
{
    boolean_t   b_horizontal_scaling;             /* horizontal scaling type */
    int         i_vertical_scaling;                 /* vertical scaling type */
    int         i_x, i_y;                 /* horizontal and vertical indexes */
    int         i_scale_count;                       /* scale modulo counter */
    int         i_uval, i_vval;                           /* U and V samples */
    int         i_red, i_green, i_blue;          /* U and V modified samples */
    int         i_chroma_width;                              /* chroma width */
    u32 *       p_yuv;                              /* base conversion table */
    u32 *       p_ybase;                     /* Y dependant conversion table */
    u32 *       p_pic_start;       /* beginning of the current line for copy */
    u32 *       p_buffer_start;                   /* conversion buffer start */
    u32 *       p_buffer;                       /* conversion buffer pointer */
    int *       p_offset_start;                        /* offset array start */
    int *       p_offset;                            /* offset array pointer */
    
    /* 
     * Initialize some values  - i_pic_line_width will store the line skip 
     */
    i_pic_line_width -= i_pic_width;
    i_chroma_width =    i_width / 2;
    p_yuv =             p_vout->yuv.yuv.p_rgb32;
    p_buffer_start =    p_vout->yuv.p_buffer;        
    p_offset_start =    p_vout->yuv.p_offset;                    
    SetOffset( i_width, i_height, i_pic_width, i_pic_height, 
               &b_horizontal_scaling, &i_vertical_scaling, p_offset_start );

    /*
     * Perform conversion
     */
    i_scale_count = i_pic_height;
    for( i_y = 0; i_y < i_height; i_y++ )
    {
        /* Mark beginnning of line for possible later line copy, and initialize
         * buffer */
        p_pic_start =   p_pic;
        p_buffer =      b_horizontal_scaling ? p_buffer_start : p_pic;        

        /* Do YUV conversion to buffer - YUV picture is always formed of 16
         * pixels wide blocks */
        for( i_x = i_width / 16; i_x--;  )
        {
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_Y_PIXEL(4);
        }             

        /* Do horizontal and vertical scaling */
        SCALE_WIDTH;
        SCALE_HEIGHT(422, 4);        
    }
}

/*****************************************************************************
 * ConvertYUV444RGB32: color YUV 4:4:4 to RGB 4 Bpp
 *****************************************************************************/
static void ConvertYUV444RGB32( p_vout_thread_t p_vout, u32 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                int i_matrix_coefficients )
{
    boolean_t   b_horizontal_scaling;             /* horizontal scaling type */
    int         i_vertical_scaling;                 /* vertical scaling type */
    int         i_x, i_y;                 /* horizontal and vertical indexes */
    int         i_scale_count;                       /* scale modulo counter */
    int         i_uval, i_vval;                           /* U and V samples */
    int         i_red, i_green, i_blue;          /* U and V modified samples */
    int         i_chroma_width;                    /* chroma width, not used */
    u32 *       p_yuv;                              /* base conversion table */
    u32 *       p_ybase;                     /* Y dependant conversion table */
    u32 *       p_pic_start;       /* beginning of the current line for copy */
    u32 *       p_buffer_start;                   /* conversion buffer start */
    u32 *       p_buffer;                       /* conversion buffer pointer */
    int *       p_offset_start;                        /* offset array start */
    int *       p_offset;                            /* offset array pointer */
    
    /* 
     * Initialize some values  - i_pic_line_width will store the line skip 
     */
    i_pic_line_width -= i_pic_width;
    p_yuv =             p_vout->yuv.yuv.p_rgb32;
    p_buffer_start =    p_vout->yuv.p_buffer;        
    p_offset_start =    p_vout->yuv.p_offset;                    
    SetOffset( i_width, i_height, i_pic_width, i_pic_height, 
               &b_horizontal_scaling, &i_vertical_scaling, p_offset_start );

    /*
     * Perform conversion
     */
    i_scale_count = i_pic_height;
    for( i_y = 0; i_y < i_height; i_y++ )
    {
        /* Mark beginnning of line for possible later line copy, and initialize
         * buffer */
        p_pic_start =   p_pic;
        p_buffer =      b_horizontal_scaling ? p_buffer_start : p_pic;        

        /* Do YUV conversion to buffer - YUV picture is always formed of 16
         * pixels wide blocks */
        for( i_x = i_width / 16; i_x--;  )
        {
            CONVERT_YUV_PIXEL(4);  CONVERT_YUV_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_YUV_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_YUV_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_YUV_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_YUV_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_YUV_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_YUV_PIXEL(4);
            CONVERT_YUV_PIXEL(4);  CONVERT_YUV_PIXEL(4);
        }             

        /* Do horizontal and vertical scaling */
        SCALE_WIDTH;
        SCALE_HEIGHT(444, 4);        
    }
}

//-------------------- walken code follows ------------------------------------

/*
 * YUV to RGB routines.
 *
 * these routines calculate r, g and b values from each pixel's y, u and v.
 * these r, g an b values are then passed thru a table lookup to take the
 * gamma curve into account and find the corresponding pixel value.
 *
 * the table must store more than 3*256 values because of the possibility
 * of overflow in the yuv->rgb calculation. actually the calculated r,g,b
 * values are in the following intervals :
 * -176 to 255+176 for red
 * -133 to 255+133 for green
 * -222 to 255+222 for blue
 *
 * If the input y,u,v values are right, the r,g,b results are not expected
 * to move out of the 0 to 255 interval but who knows what will happen in
 * real use...
 *
 * the red, green and blue conversion tables are stored in a single 1935-entry
 * array. The respective positions of each component in the array have been
 * calculated to minimize the cache interactions of the 3 tables.
 */

#if 0
//??
static void yuvToRgb24 (unsigned char * Y,
			unsigned char * U, unsigned char * V,
			char * dest, int table[1935], int width)
{
    int i;
    int u;
    int v;
    int uvRed;
    int uvGreen;
    int uvBlue;
    int * tableY;
    int tmp24;

    i = width >> 3;
    while (i--) {
	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	tmp24 = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		 tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			uvGreen] |
		 tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
	*(dest++) = tmp24;
	*(dest++) = tmp24 >> 8;
	*(dest++) = tmp24 >> 16;

	tableY = table + *(Y++);
	tmp24 = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		 tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			uvGreen] |
		 tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
	*(dest++) = tmp24;
	*(dest++) = tmp24 >> 8;
	*(dest++) = tmp24 >> 16;

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	tmp24 = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		 tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			uvGreen] |
		 tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
	*(dest++) = tmp24;
	*(dest++) = tmp24 >> 8;
	*(dest++) = tmp24 >> 16;

	tableY = table + *(Y++);
	tmp24 = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		 tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			uvGreen] |
		 tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
	*(dest++) = tmp24;
	*(dest++) = tmp24 >> 8;
	*(dest++) = tmp24 >> 16;

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	tmp24 = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		 tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			uvGreen] |
		 tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
	*(dest++) = tmp24;
	*(dest++) = tmp24 >> 8;
	*(dest++) = tmp24 >> 16;

	tableY = table + *(Y++);
	tmp24 = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		 tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			uvGreen] |
		 tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
	*(dest++) = tmp24;
	*(dest++) = tmp24 >> 8;
	*(dest++) = tmp24 >> 16;

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	tmp24 = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		 tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			uvGreen] |
		 tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
	*(dest++) = tmp24;
	*(dest++) = tmp24 >> 8;
	*(dest++) = tmp24 >> 16;

	tableY = table + *(Y++);
	tmp24 = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		 tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			uvGreen] |
		 tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
	*(dest++) = tmp24;
	*(dest++) = tmp24 >> 8;
	*(dest++) = tmp24 >> 16;
    }

    i = (width & 7) >> 1;
    while (i--) {
	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	tmp24 = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		 tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			uvGreen] |
		 tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
	*(dest++) = tmp24;
	*(dest++) = tmp24 >> 8;
	*(dest++) = tmp24 >> 16;

	tableY = table + *(Y++);
	tmp24 = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		 tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			uvGreen] |
		 tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
	*(dest++) = tmp24;
	*(dest++) = tmp24 >> 8;
	*(dest++) = tmp24 >> 16;
    }

    if (width & 1) {
	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	tmp24 = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		 tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			uvGreen] |
		 tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
	*(dest++) = tmp24;
	*(dest++) = tmp24 >> 8;
	*(dest++) = tmp24 >> 16;
    }
}
#endif
