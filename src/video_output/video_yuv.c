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

/* RGB/YUV inversion matrix (ISO/IEC 13818-2 section 6.3.6, table 6.9) */
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

#define SHIFT 20
#define U_GREEN_COEF ((int)(-0.391 * (1<<SHIFT) / 1.164))
#define U_BLUE_COEF ((int)(2.018 * (1<<SHIFT) / 1.164))
#define V_RED_COEF ((int)(1.596 * (1<<SHIFT) / 1.164))
#define V_GREEN_COEF ((int)(-0.813 * (1<<SHIFT) / 1.164))

/*******************************************************************************
 * Local prototypes
 *******************************************************************************/
static int      BinaryLog         ( u32 i );
static void     MaskToShift       ( int *pi_right, int *pi_left, u32 i_mask );
static void     SetGammaTable     ( int *pi_table, double f_gamma );
static void     SetYUV            ( vout_thread_t *p_vout );

static void     ConvertY4Gray16   ( p_vout_thread_t p_vout, u16 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                    int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                    int i_matrix_coefficients );
static void     ConvertY4Gray24   ( p_vout_thread_t p_vout, void *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                    int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                    int i_matrix_coefficients );
static void     ConvertY4Gray32   ( p_vout_thread_t p_vout, u32 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
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

/*******************************************************************************
 * CLIP_BYTE macro: boundary detection
 *******************************************************************************
 * Return parameter if between 0 and 255, else return nearest boundary (0 or 
 * 255). This macro is used to build translations tables.
 *******************************************************************************/
#define CLIP_BYTE( i_val ) ( (i_val < 0) ? 0 : ((i_val > 255) ? 255 : i_val) )

/*******************************************************************************
 * CONVERT_YUV_GRAY macro: grayscale YUV convertion
 *******************************************************************************
 * This macro does not perform any scaling, but crops the picture. It is
 * provided as a temporary way of implementing an YUV convertion function.
 *******************************************************************************/
#define CONVERT_YUV_GRAY                                                \
/* Change boundaries according to picture size */                       \
i_width =               MIN( i_width, i_pic_width );                    \
i_height =              MIN( i_height, i_pic_height );                  \
i_pic_line_width -=     i_width;                                        \
                                                                        \
/* Loop */                                                              \
for (i_y = 0; i_y < i_height ; i_y++)                                   \
{                                                                       \
    for (i_x = 0; i_x < i_width; )                                      \
    {                                                                   \
        /* Convert 16 pixels (width is always multiple of 16 */         \
        p_pic[i_x++] = p_gray[ p_y[i_x] ];                              \
        p_pic[i_x++] = p_gray[ p_y[i_x] ];                              \
        p_pic[i_x++] = p_gray[ p_y[i_x] ];                              \
        p_pic[i_x++] = p_gray[ p_y[i_x] ];                              \
        p_pic[i_x++] = p_gray[ p_y[i_x] ];                              \
        p_pic[i_x++] = p_gray[ p_y[i_x] ];                              \
        p_pic[i_x++] = p_gray[ p_y[i_x] ];                              \
        p_pic[i_x++] = p_gray[ p_y[i_x] ];                              \
        p_pic[i_x++] = p_gray[ p_y[i_x] ];                              \
        p_pic[i_x++] = p_gray[ p_y[i_x] ];                              \
        p_pic[i_x++] = p_gray[ p_y[i_x] ];                              \
        p_pic[i_x++] = p_gray[ p_y[i_x] ];                              \
        p_pic[i_x++] = p_gray[ p_y[i_x] ];                              \
        p_pic[i_x++] = p_gray[ p_y[i_x] ];                              \
        p_pic[i_x++] = p_gray[ p_y[i_x] ];                              \
        p_pic[i_x++] = p_gray[ p_y[i_x] ];                              \
    }                                                                   \
                                                                        \
    /* Skip until beginning of next line */                             \
    p_pic += i_pic_line_width;                                          \
}

/*******************************************************************************
 * CONVERT_YUV_RGB: color YUV convertion
 *******************************************************************************
 * This macro does not perform any scaling, but crops the picture. It is
 * provided as a temporary way of implementing an YUV convertion function.
 *******************************************************************************/
#define CONVERT_YUV_RGB( CHROMA, CRV, CGV, CBU, CGU )                   \
/* Change boundaries according to picture size */                       \
i_width =               MIN( i_width, i_pic_width );                    \
i_height =              MIN( i_height, i_pic_height );                  \
i_chroma_width =        (CHROMA == 444) ? i_width : i_width / 2;        \
i_pic_line_width -=     i_width;                                        \
                                                                        \
/* Loop */                                                              \
for (i_y = 0; i_y < i_height ; i_y++)                                   \
{                                                                       \
    for (i_x = 0; i_x < i_width; )                                      \
    {                                                                   \
        /* First sample (complete) */                                   \
        i_yval = 76309 * p_y[i_x] - 1188177;                            \
        i_uval = *p_u++ - 128;                                          \
        i_vval = *p_v++ - 128;                                          \
        p_pic[i_x++] =                                                  \
            p_red  [(i_yval+CRV*i_vval)              >>16] |            \
            p_green[(i_yval-CGU*i_uval-CGV*i_vval)   >>16] |            \
            p_blue [(i_yval+CBU*i_uval)              >>16];             \
        i_yval = 76309 * p_y[i_x] - 1188177;                            \
        /* Second sample (partial) */                                   \
        if( CHROMA == 444 )                                             \
        {                                                               \
            i_uval = *p_u++ - 128;                                      \
            i_vval = *p_v++ - 128;                                      \
        }                                                               \
        p_pic[i_x++] =                                                  \
            p_red  [(i_yval+CRV*i_vval)              >>16] |            \
            p_green[(i_yval-CGU*i_uval-CGV*i_vval)   >>16] |            \
            p_blue [(i_yval+CBU*i_uval)              >>16];             \
    }                                                                   \
                                                                        \
    /* Rewind in 4:2:0 */                                               \
    if( (CHROMA == 420) && !(i_y & 0x1) )                               \
    {                                                                   \
        p_u -= i_chroma_width;                                          \
        p_v -= i_chroma_width;                                          \
    }                                                                   \
                                                                        \
    /* Skip until beginning of next line */                             \
    p_pic += i_pic_line_width;                                          \
}

/*******************************************************************************
 * vout_InitYUV: allocate and initialize translations tables
 *******************************************************************************
 * This function will allocate memory to store translation tables, depending
 * of the screen depth.
 *******************************************************************************/
int vout_InitYUV( vout_thread_t *p_vout )
{
    size_t      tables_size;                          /* tables size, in bytes */
    
    /* Computes tables size */
    switch( p_vout->i_screen_depth )
    {
    case 15:
    case 16:
        tables_size = sizeof( u16 ) * (1024 * (p_vout->b_grayscale ? 1 : 3) + 1935);
        break;        
    case 24:        
    case 32:
#ifndef DEBUG
    default:        
#endif
        tables_size = sizeof( u32 ) * (1024 * (p_vout->b_grayscale ? 1 : 3) + 1935);        
        break;        
#ifdef DEBUG
    default:
        intf_DbgMsg("error: invalid screen depth %d\n", p_vout->i_screen_depth );
        tables_size = 0;
        break;        
#endif      
    }

    /* Add conversion buffer size. The conversions functions need one comple line
     * plus one pixel, so we give them two. */
    tables_size += p_vout->i_bytes_per_line * 2;    
    
    /* Allocate memory */
    p_vout->yuv.p_base = malloc( tables_size );
    if( p_vout->yuv.p_base == NULL )
    {
        intf_ErrMsg("error: %s\n", strerror(ENOMEM));
        return( 1 );                
    }

    /* Initialize tables */
    SetYUV( p_vout );    
    return( 0 );    
}

/*******************************************************************************
 * vout_ResetTables: re-initialize translations tables
 *******************************************************************************
 * This function will initialize the tables allocated by vout_CreateTables and
 * set functions pointers.
 *******************************************************************************/
int vout_ResetYUV( vout_thread_t *p_vout )
{
    vout_EndYUV( p_vout );    
    return( vout_InitYUV( p_vout ) );    
}

/*******************************************************************************
 * vout_EndYUV: destroy translations tables
 *******************************************************************************
 * Free memory allocated by vout_CreateTables.
 *******************************************************************************/
void vout_EndYUV( vout_thread_t *p_vout )
{
    free( p_vout->yuv.p_base );
}

/* following functions are local */

/*******************************************************************************
 * BinaryLog: computes the base 2 log of a binary value
 *******************************************************************************
 * This functions is used by MaskToShift during tables initialisation, to
 * get a bit index from a binary value.
 *******************************************************************************/
static int BinaryLog(u32 i)
{
    int i_log;

    i_log = 0;
    if (i & 0xffff0000) 
    {        
        i_log = 16;
    }    
    if (i & 0xff00ff00) 
    {        
        i_log += 8;
    }    
    if (i & 0xf0f0f0f0) 
    {        
        i_log += 4;
    }    
    if (i & 0xcccccccc) 
    {        
        i_log += 2;
    }    
    if (i & 0xaaaaaaaa) 
    {        
        i_log++;
    }    
    if (i != ((u32)1 << i_log))
    {        
	intf_ErrMsg("internal error: binary log overflow\n");        
    }    

    return( i_log );
}

/*******************************************************************************
 * MaskToShift: Transform a color mask into right and left shifts
 *******************************************************************************
 * This function is used during table initialisation. It can return a value
 *******************************************************************************/
static void MaskToShift (int *pi_right, int *pi_left, u32 i_mask)
{
    u32 i_low, i_high;                   /* lower hand higher bits of the mask */

    /* Get bits */
    i_low =  i_mask & (- i_mask);                     /* lower bit of the mask */
    i_high = i_mask + i_low;                         /* higher bit of the mask */

    /* Transform bits into an index */
    i_low =  BinaryLog (i_low);
    i_high = BinaryLog (i_high);

    /* Update pointers and return */
    *pi_left =   i_low;
    *pi_right = (8 - i_high + i_low);
}

/*******************************************************************************
 * SetGammaTable: return intensity table transformed by gamma curve.
 *******************************************************************************
 * pi_table is a table of 256 entries from 0 to 255.
 *******************************************************************************/
static void SetGammaTable( int *pi_table, double f_gamma )
{
    int         i_y;                                         /* base intensity */

    /* Use exp(gamma) instead of gamma */
    f_gamma = exp(f_gamma );

    /* Build gamma table */
    for( i_y = 0; i_y < 256; i_y++ )
    {
        pi_table[ i_y ] = pow( (double)i_y / 256, f_gamma ) * 256;
    }
 }

/*******************************************************************************
 * SetYUV: compute tables and set function pointers
+ *******************************************************************************/
static void SetYUV( vout_thread_t *p_vout )
{
    int         pi_gamma[256];                                  /* gamma table */    
    int         i_index;                                    /* index in tables */
    int         i_red_right, i_red_left;                         /* red shifts */
    int         i_green_right, i_green_left;                   /* green shifts */
    int         i_blue_right, i_blue_left;                      /* blue shifts */

    /* Build gamma table */    
    SetGammaTable( pi_gamma, p_vout->f_gamma );
    
    /*          
     * Set color masks and shifts
     */
    switch( p_vout->i_screen_depth )
    {
    case 15:
        MaskToShift( &i_red_right,   &i_red_left,   0x7c00 );
        MaskToShift( &i_green_right, &i_green_left, 0x03e0 );
        MaskToShift( &i_blue_right,  &i_blue_left,  0x001f );        
        break;        
    case 16:
        MaskToShift( &i_red_right,   &i_red_left,   0xf800 );
        MaskToShift( &i_green_right, &i_green_left, 0x07e0 );
        MaskToShift( &i_blue_right,  &i_blue_left,  0x001f );
        break;        
    case 24:
    case 32:        
        MaskToShift( &i_red_right,   &i_red_left,   0x00ff0000 );
        MaskToShift( &i_green_right, &i_green_left, 0x0000ff00 );
        MaskToShift( &i_blue_right,  &i_blue_left,  0x000000ff );
        break;
#ifdef DEBUG
    default:
        intf_DbgMsg("error: invalid screen depth %d\n", p_vout->i_screen_depth );
        break;        
#endif      
    }

    /*
     * Set pointers and build YUV tables
     */        
    if( p_vout->b_grayscale )
    {
        /* Grayscale: build gray table */
        switch( p_vout->i_screen_depth )
        {
        case 15:
        case 16:         
            p_vout->yuv.yuv.gray16.p_gray =  (u16 *)p_vout->yuv.p_base + 384;
            for( i_index = -384; i_index < 640; i_index++) 
            {
                p_vout->yuv.yuv.gray16.p_gray[ i_index ] = 
                    ((pi_gamma[CLIP_BYTE( i_index )] >> i_red_right)   << i_red_left)   |
                    ((pi_gamma[CLIP_BYTE( i_index )] >> i_green_right) << i_green_left) |
                    ((pi_gamma[CLIP_BYTE( i_index )] >> i_blue_right)  << i_blue_left);
            }
            break;        
        case 24:
        case 32:        
            p_vout->yuv.yuv.gray32.p_gray =  (u32 *)p_vout->yuv.p_base + 384;
            for( i_index = -384; i_index < 640; i_index++) 
            {
                p_vout->yuv.yuv.gray32.p_gray[ i_index ] = 
                    ((pi_gamma[CLIP_BYTE( i_index )] >> i_red_right)   << i_red_left)   |
                    ((pi_gamma[CLIP_BYTE( i_index )] >> i_green_right) << i_green_left) |
                    ((pi_gamma[CLIP_BYTE( i_index )] >> i_blue_right)  << i_blue_left);
            }        
            break;        
        }
    }
    else
    {
        /* Color: build red, green and blue tables */
        switch( p_vout->i_screen_depth )
        {
        case 15:
        case 16:            
            p_vout->yuv.yuv.rgb16.p_red =    (u16 *)p_vout->yuv.p_base +          384;
            p_vout->yuv.yuv.rgb16.p_green =  (u16 *)p_vout->yuv.p_base +   1024 + 384;
            p_vout->yuv.yuv.rgb16.p_blue =   (u16 *)p_vout->yuv.p_base + 2*1024 + 384;
            p_vout->yuv.yuv2.p_rgb16 =       (u16 *)p_vout->yuv.p_base + 3*1024;
            p_vout->yuv.p_buffer =           (u16 *)p_vout->yuv.p_base + 3*1024 + 1935;            
            for( i_index = -384; i_index < 640; i_index++) 
            {
                p_vout->yuv.yuv.rgb16.p_red[i_index] =   (pi_gamma[CLIP_BYTE(i_index)]>>i_red_right)<<i_red_left;
                p_vout->yuv.yuv.rgb16.p_green[i_index] = (pi_gamma[CLIP_BYTE(i_index)]>>i_green_right)<<i_green_left;
                p_vout->yuv.yuv.rgb16.p_blue[i_index] =  (pi_gamma[CLIP_BYTE(i_index)]>>i_blue_right)<<i_blue_left;
            }
            for( i_index = 0; i_index < 178; i_index++ )
            {
                p_vout->yuv.yuv2.p_rgb16[1501 - 178 + i_index] = (pi_gamma[0]>>i_red_right)<<i_red_left;
                p_vout->yuv.yuv2.p_rgb16[1501 + 256 + i_index] = (pi_gamma[255]>>i_red_right)<<i_red_left;                
            }
            for( i_index = 0; i_index < 135; i_index++ )
            {
                p_vout->yuv.yuv2.p_rgb16[135 - 135 + i_index] = (pi_gamma[0]>>i_green_right)<<i_green_left;
                p_vout->yuv.yuv2.p_rgb16[135 + 256 + i_index] = (pi_gamma[255]>>i_green_right)<<i_green_left;
            }
            for( i_index = 0; i_index < 224; i_index++ )
            {
                p_vout->yuv.yuv2.p_rgb16[818 - 224 + i_index] = (pi_gamma[0]>>i_blue_right)<<i_blue_left;
                p_vout->yuv.yuv2.p_rgb16[818 + 256 + i_index] = (pi_gamma[255]>>i_blue_right)<<i_blue_left;                
            }
            for( i_index = 0; i_index < 256; i_index++ )
            {
                p_vout->yuv.yuv2.p_rgb16[1501 + i_index] = (pi_gamma[i_index]>>i_red_right)<<i_red_left;
                p_vout->yuv.yuv2.p_rgb16[135 + i_index] = (pi_gamma[i_index]>>i_green_right)<<i_green_left;
                p_vout->yuv.yuv2.p_rgb16[818 + i_index] = (pi_gamma[i_index]>>i_blue_right)<<i_blue_left;
            }            
            break;        
        case 24:
        case 32:
            p_vout->yuv.yuv.rgb32.p_red =    (u32 *)p_vout->yuv.p_base +          384;
            p_vout->yuv.yuv.rgb32.p_green =  (u32 *)p_vout->yuv.p_base +   1024 + 384;
            p_vout->yuv.yuv.rgb32.p_blue =   (u32 *)p_vout->yuv.p_base + 2*1024 + 384;
            p_vout->yuv.yuv2.p_rgb32 =       (u32 *)p_vout->yuv.p_base + 3*1024;
            p_vout->yuv.p_buffer =           (u32 *)p_vout->yuv.p_base + 3*1024 + 1935;            
            for( i_index = -384; i_index < 640; i_index++) 
            {
                p_vout->yuv.yuv.rgb32.p_red[i_index] =   (pi_gamma[CLIP_BYTE(i_index)]>>i_red_right)<<i_red_left;
                p_vout->yuv.yuv.rgb32.p_green[i_index] = (pi_gamma[CLIP_BYTE(i_index)]>>i_green_right)<<i_green_left;
                p_vout->yuv.yuv.rgb32.p_blue[i_index] =  (pi_gamma[CLIP_BYTE(i_index)]>>i_blue_right)<<i_blue_left;
            }
            //?? walken's yuv
            break;        
        }
    }    

    /*
     * Set functions pointers
     */
    if( p_vout->b_grayscale )
    {
        /* Grayscale */
        switch( p_vout->i_screen_depth )
        {
        case 15:
        case 16:  
            p_vout->yuv.p_Convert420 = (vout_yuv_convert_t *) ConvertY4Gray16;        
            p_vout->yuv.p_Convert420 = (vout_yuv_convert_t *) ConvertY4Gray16;        
            p_vout->yuv.p_Convert420 = (vout_yuv_convert_t *) ConvertY4Gray16;        
            break;        
        case 24:
            p_vout->yuv.p_Convert420 = (vout_yuv_convert_t *) ConvertY4Gray24;        
            p_vout->yuv.p_Convert420 = (vout_yuv_convert_t *) ConvertY4Gray24;        
            p_vout->yuv.p_Convert420 = (vout_yuv_convert_t *) ConvertY4Gray24;        
            break;        
        case 32:        
            p_vout->yuv.p_Convert420 = (vout_yuv_convert_t *) ConvertY4Gray32;        
            p_vout->yuv.p_Convert420 = (vout_yuv_convert_t *) ConvertY4Gray32;        
            p_vout->yuv.p_Convert420 = (vout_yuv_convert_t *) ConvertY4Gray32;        
            break;        
        }        
    }
    else
    {
        /* Color */
        switch( p_vout->i_screen_depth )
        {
        case 15:
        case 16:  
            p_vout->yuv.p_Convert420 =   (vout_yuv_convert_t *) ConvertYUV420RGB16;        
            p_vout->yuv.p_Convert422 =   (vout_yuv_convert_t *) ConvertYUV422RGB16;        
            p_vout->yuv.p_Convert444 =   (vout_yuv_convert_t *) ConvertYUV444RGB16;        
            break;        
        case 24:
            p_vout->yuv.p_Convert420 =   (vout_yuv_convert_t *) ConvertYUV420RGB24;        
            p_vout->yuv.p_Convert422 =   (vout_yuv_convert_t *) ConvertYUV422RGB24;        
            p_vout->yuv.p_Convert444 =   (vout_yuv_convert_t *) ConvertYUV444RGB24;        
            break;        
        case 32:        
            p_vout->yuv.p_Convert420 =   (vout_yuv_convert_t *) ConvertYUV420RGB32;        
            p_vout->yuv.p_Convert422 =   (vout_yuv_convert_t *) ConvertYUV422RGB32;        
            p_vout->yuv.p_Convert444 =   (vout_yuv_convert_t *) ConvertYUV444RGB32;        
            break;        
        }
    }        
}

/*******************************************************************************
 * ConvertY4Gray16: grayscale YUV 4:x:x to RGB 15 or 16 bpp
 *******************************************************************************/
static void ConvertY4Gray16( p_vout_thread_t p_vout, u16 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                             int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                             int i_matrix_coefficients )
{
    u16 *       p_gray;                                          /* gray table */    
    int         i_x, i_y;                               /* picture coordinates */
    
    p_gray = p_vout->yuv.yuv.gray16.p_gray;
    CONVERT_YUV_GRAY
}

/*******************************************************************************
 * ConvertY4Gray24: grayscale YUV 4:x:x to RGB 24 bpp
 *******************************************************************************/
static void ConvertY4Gray24( p_vout_thread_t p_vout, void *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                             int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                             int i_matrix_coefficients )
{
    //??
}

/*******************************************************************************
 * ConvertY4Gray32: grayscale YUV 4:x:x to RGB 32 bpp
 *******************************************************************************/
static void ConvertY4Gray32( p_vout_thread_t p_vout, u32 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                             int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                             int i_matrix_coefficients )
{
    u32 *       p_gray;                                          /* gray table */    
    int         i_x, i_y;                               /* picture coordinates */
    
    p_gray = p_vout->yuv.yuv.gray32.p_gray;
    CONVERT_YUV_GRAY
}

/*******************************************************************************
 * ConvertYUV420RGB16: color YUV 4:2:0 to RGB 15 or 16 bpp
 *******************************************************************************/
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

#if 0
    u16 *       p_red;                                            /* red table */
    u16 *       p_green;                                        /* green table */
    u16 *       p_blue;                                          /* blue table */
    int         i_uval, i_yval, i_vval;                             /* samples */   
    int         i_x, i_y;                               /* picture coordinates */
    int         i_chroma_width, i_chroma_skip;     /* width and eol for chroma */
    int         i_crv, i_cbu, i_cgu, i_cgv;     /* transformation coefficients */

    p_red   = p_vout->yuv.yuv.rgb16.p_red;
    p_green = p_vout->yuv.yuv.rgb16.p_green;
    p_blue  = p_vout->yuv.yuv.rgb16.p_blue;
    i_crv = MATRIX_COEFFICIENTS_TABLE[i_matrix_coefficients][0];
    i_cbu = MATRIX_COEFFICIENTS_TABLE[i_matrix_coefficients][1];
    i_cgu = MATRIX_COEFFICIENTS_TABLE[i_matrix_coefficients][2];
    i_cgv = MATRIX_COEFFICIENTS_TABLE[i_matrix_coefficients][3];
    
    CONVERT_YUV_RGB( 420, i_crv, i_cgv, i_cbu, i_cgu );        
#else
    int         i_x, i_y;                   /* horizontal and vertical indexes */
    int         i_uval, i_vval;                             /* U and V samples */
    int         i_red, i_green, i_blue;            /* U and V modified samples */
    int         i_chroma_width;                                /* chroma width */
    int         i_height_count;                       /* height modulo counter */
    u16 *       p_yuv;                                /* base convertion table */
    u16 *       p_ybase;                       /* Y dependant convertion table */   
    u16 *       p_pic_start;    
    
    /* Initialize values */
    i_height_count =    i_pic_height;
    i_chroma_width =    i_width / 2;
    p_yuv =             p_vout->yuv.yuv2.p_rgb16;

    /*?? temporary kludge to protect from segfault at startup */
    i_height = MIN( i_height, i_pic_height );    

    /*
     * Perform convertion
     */
    for( i_y = 0; i_y < i_height; i_y++ )
    {
        /* Mark beginnning of line */
        p_pic_start = p_pic;        

        /* Convert line using 16 pixels blocks, since picture come from 16 pixels width
         * macroblocks */
        for( i_x = i_width / 16; i_x--; )
        {
            i_uval =    *p_u++;
            i_vval =    *p_v++;
            i_red =     (V_RED_COEF * i_vval) >> SHIFT;
            i_green =   (U_GREEN_COEF * i_uval + V_GREEN_COEF * i_vval) >> SHIFT;
            i_blue =    (U_BLUE_COEF * i_uval) >> SHIFT;

            p_ybase = p_yuv + *(p_y++);
            *(p_pic++) = p_ybase[1501 - ((V_RED_COEF*128)>>SHIFT) + i_red] |
                p_ybase[135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) + i_green ] |
                p_ybase[818 - ((U_BLUE_COEF*128)>>SHIFT) + i_blue];
            p_ybase = p_yuv + *(p_y++);
            *(p_pic++) = p_ybase[1501 - ((V_RED_COEF*128)>>SHIFT) + i_red] |
                p_ybase[135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) + i_green ] |
                p_ybase[818 - ((U_BLUE_COEF*128)>>SHIFT) + i_blue];

            i_uval =    *p_u++;
            i_vval =    *p_v++;
            i_red =     (V_RED_COEF * i_vval) >> SHIFT;
            i_green =   (U_GREEN_COEF * i_uval + V_GREEN_COEF * i_vval) >> SHIFT;
            i_blue =    (U_BLUE_COEF * i_uval) >> SHIFT;

            p_ybase = p_yuv + *(p_y++);
            *(p_pic++) = p_ybase[1501 - ((V_RED_COEF*128)>>SHIFT) + i_red] |
                p_ybase[135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) + i_green ] |
                p_ybase[818 - ((U_BLUE_COEF*128)>>SHIFT) + i_blue];
            p_ybase = p_yuv + *(p_y++);
            *(p_pic++) = p_ybase[1501 - ((V_RED_COEF*128)>>SHIFT) + i_red] |
                p_ybase[135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) + i_green ] |
                p_ybase[818 - ((U_BLUE_COEF*128)>>SHIFT) + i_blue];

            i_uval =    *p_u++;
            i_vval =    *p_v++;
            i_red =     (V_RED_COEF * i_vval) >> SHIFT;
            i_green =   (U_GREEN_COEF * i_uval + V_GREEN_COEF * i_vval) >> SHIFT;
            i_blue =    (U_BLUE_COEF * i_uval) >> SHIFT;

            p_ybase = p_yuv + *(p_y++);
            *(p_pic++) = p_ybase[1501 - ((V_RED_COEF*128)>>SHIFT) + i_red] |
                p_ybase[135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) + i_green ] |
                p_ybase[818 - ((U_BLUE_COEF*128)>>SHIFT) + i_blue];
            p_ybase = p_yuv + *(p_y++);
            *(p_pic++) = p_ybase[1501 - ((V_RED_COEF*128)>>SHIFT) + i_red] |
                p_ybase[135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) + i_green ] |
                p_ybase[818 - ((U_BLUE_COEF*128)>>SHIFT) + i_blue];

            i_uval =    *p_u++;
            i_vval =    *p_v++;
            i_red =     (V_RED_COEF * i_vval) >> SHIFT;
            i_green =   (U_GREEN_COEF * i_uval + V_GREEN_COEF * i_vval) >> SHIFT;
            i_blue =    (U_BLUE_COEF * i_uval) >> SHIFT;

            p_ybase = p_yuv + *(p_y++);
            *(p_pic++) = p_ybase[1501 - ((V_RED_COEF*128)>>SHIFT) + i_red] |
                p_ybase[135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) + i_green ] |
                p_ybase[818 - ((U_BLUE_COEF*128)>>SHIFT) + i_blue];
            p_ybase = p_yuv + *(p_y++);
            *(p_pic++) = p_ybase[1501 - ((V_RED_COEF*128)>>SHIFT) + i_red] |
                p_ybase[135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) + i_green ] |
                p_ybase[818 - ((U_BLUE_COEF*128)>>SHIFT) + i_blue];

            i_uval =    *p_u++;
            i_vval =    *p_v++;
            i_red =     (V_RED_COEF * i_vval) >> SHIFT;
            i_green =   (U_GREEN_COEF * i_uval + V_GREEN_COEF * i_vval) >> SHIFT;
            i_blue =    (U_BLUE_COEF * i_uval) >> SHIFT;

            p_ybase = p_yuv + *(p_y++);
            *(p_pic++) = p_ybase[1501 - ((V_RED_COEF*128)>>SHIFT) + i_red] |
                p_ybase[135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) + i_green ] |
                p_ybase[818 - ((U_BLUE_COEF*128)>>SHIFT) + i_blue];
            p_ybase = p_yuv + *(p_y++);
            *(p_pic++) = p_ybase[1501 - ((V_RED_COEF*128)>>SHIFT) + i_red] |
                p_ybase[135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) + i_green ] |
                p_ybase[818 - ((U_BLUE_COEF*128)>>SHIFT) + i_blue];

            i_uval =    *p_u++;
            i_vval =    *p_v++;
            i_red =     (V_RED_COEF * i_vval) >> SHIFT;
            i_green =   (U_GREEN_COEF * i_uval + V_GREEN_COEF * i_vval) >> SHIFT;
            i_blue =    (U_BLUE_COEF * i_uval) >> SHIFT;

            p_ybase = p_yuv + *(p_y++);
            *(p_pic++) = p_ybase[1501 - ((V_RED_COEF*128)>>SHIFT) + i_red] |
                p_ybase[135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) + i_green ] |
                p_ybase[818 - ((U_BLUE_COEF*128)>>SHIFT) + i_blue];
            p_ybase = p_yuv + *(p_y++);
            *(p_pic++) = p_ybase[1501 - ((V_RED_COEF*128)>>SHIFT) + i_red] |
                p_ybase[135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) + i_green ] |
                p_ybase[818 - ((U_BLUE_COEF*128)>>SHIFT) + i_blue];

            i_uval =    *p_u++;
            i_vval =    *p_v++;
            i_red =     (V_RED_COEF * i_vval) >> SHIFT;
            i_green =   (U_GREEN_COEF * i_uval + V_GREEN_COEF * i_vval) >> SHIFT;
            i_blue =    (U_BLUE_COEF * i_uval) >> SHIFT;

            p_ybase = p_yuv + *(p_y++);
            *(p_pic++) = p_ybase[1501 - ((V_RED_COEF*128)>>SHIFT) + i_red] |
                p_ybase[135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) + i_green ] |
                p_ybase[818 - ((U_BLUE_COEF*128)>>SHIFT) + i_blue];
            p_ybase = p_yuv + *(p_y++);
            *(p_pic++) = p_ybase[1501 - ((V_RED_COEF*128)>>SHIFT) + i_red] |
                p_ybase[135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) + i_green ] |
                p_ybase[818 - ((U_BLUE_COEF*128)>>SHIFT) + i_blue];

            i_uval =    *p_u++;
            i_vval =    *p_v++;
            i_red =     (V_RED_COEF * i_vval) >> SHIFT;
            i_green =   (U_GREEN_COEF * i_uval + V_GREEN_COEF * i_vval) >> SHIFT;
            i_blue =    (U_BLUE_COEF * i_uval) >> SHIFT;

            p_ybase = p_yuv + *(p_y++);
            *(p_pic++) = p_ybase[1501 - ((V_RED_COEF*128)>>SHIFT) + i_red] |
                p_ybase[135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) + i_green ] |
                p_ybase[818 - ((U_BLUE_COEF*128)>>SHIFT) + i_blue];
            p_ybase = p_yuv + *(p_y++);
            *(p_pic++) = p_ybase[1501 - ((V_RED_COEF*128)>>SHIFT) + i_red] |
                p_ybase[135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) + i_green ] |
                p_ybase[818 - ((U_BLUE_COEF*128)>>SHIFT) + i_blue];            
        }        

        /* If line is odd, rewind U and V samples */
        if( i_y & 0x1 )
        {
            p_u -= i_chroma_width;
            p_v -= i_chroma_width;
        }

        /* End of line: skip picture to reach beginning of next line */
        p_pic += i_pic_line_width - i_pic_width;
 
        /* Copy line if needed */
        while( (i_height_count -= i_height) >= 0 )
        {   
            for( i_x = i_pic_width / 16; i_x--; )
            {
                *(((u64 *) p_pic)++) = *(((u64 *) p_pic_start)++ );
                *(((u64 *) p_pic)++) = *(((u64 *) p_pic_start)++ );
                *(((u64 *) p_pic)++) = *(((u64 *) p_pic_start)++ );
                *(((u64 *) p_pic)++) = *(((u64 *) p_pic_start)++ );
            }
            p_pic +=            i_pic_line_width - i_pic_width;            
            p_pic_start +=      i_pic_line_width - i_pic_width;            
        }        
        i_height_count += i_pic_height;        
    }    
#endif
}

/*******************************************************************************
 * ConvertYUV422RGB16: color YUV 4:2:2 to RGB 15 or 16 bpp
 *******************************************************************************/
static void ConvertYUV422RGB16( p_vout_thread_t p_vout, u16 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                int i_matrix_coefficients )
{
    //??
}

/*******************************************************************************
 * ConvertYUV444RGB16: color YUV 4:4:4 to RGB 15 or 16 bpp
 *******************************************************************************/
static void ConvertYUV444RGB16( p_vout_thread_t p_vout, u16 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                int i_matrix_coefficients )
{
    //??
}

/*******************************************************************************
 * ConvertYUV420RGB24: color YUV 4:2:0 to RGB 24 bpp
 *******************************************************************************/
static void ConvertYUV420RGB24( p_vout_thread_t p_vout, void *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                int i_matrix_coefficients )
{
    //???
}

/*******************************************************************************
 * ConvertYUV422RGB24: color YUV 4:2:2 to RGB 24 bpp
 *******************************************************************************/
static void ConvertYUV422RGB24( p_vout_thread_t p_vout, void *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                int i_matrix_coefficients )
{
    //???
}

/*******************************************************************************
 * ConvertYUV444RGB24: color YUV 4:4:4 to RGB 24 bpp
 *******************************************************************************/
static void ConvertYUV444RGB24( p_vout_thread_t p_vout, void *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                int i_matrix_coefficients )
{    
    //???
}

/*******************************************************************************
 * ConvertYUV420RGB32: color YUV 4:2:0 to RGB 32 bpp
 *******************************************************************************/
static void ConvertYUV420RGB32( p_vout_thread_t p_vout, u32 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                int i_matrix_coefficients )
{
    //??
}

/*******************************************************************************
 * ConvertYUV422RGB32: color YUV 4:2:2 to RGB 32 bpp
 *******************************************************************************/
static void ConvertYUV422RGB32( p_vout_thread_t p_vout, u32 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                int i_matrix_coefficients )
{
    //??
}

/*******************************************************************************
 * ConvertYUV444RGB32: color YUV 4:4:4 to RGB 32 bpp
 *******************************************************************************/
static void ConvertYUV444RGB32( p_vout_thread_t p_vout, u32 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                int i_width, int i_height, int i_pic_width, int i_pic_height, int i_pic_line_width,
                                int i_matrix_coefficients )
{
    //??
}

//-------------------- walken code follow ---------------------------------------

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

int rgbTable16 (short table [1935],
		       int redMask, int greenMask, int blueMask,
		       unsigned char gamma[256])
{
    int redRight;
    int redLeft;
    int greenRight;
    int greenLeft;
    int blueRight;
    int blueLeft;
    short * redTable;
    short * greenTable;
    short * blueTable;
    int i;
    int y;

    MaskToShift (&redRight, &redLeft, redMask);    
    MaskToShift (&greenRight, &greenLeft, greenMask);    
    MaskToShift (&blueRight, &blueLeft, blueMask);

    /*
     * green blue red +- 2 just to be sure
     * green = 0-525 [151-370]
     * blue = 594-1297 [834-1053] <834-29>
     * red = 1323-1934 [1517-1736] <493-712>
     */

    redTable = table + 1501;
    greenTable = table + 135;
    blueTable = table + 818;

    for (i = 0; i < 178; i++) {
	redTable[i-178] = 0;
	redTable[i+256] = redMask;
    }
    for (i = 0; i < 135; i++) {
	greenTable[i-135] = 0;
	greenTable[i+256] = greenMask;
    }
    for (i = 0; i < 224; i++) {
	blueTable[i-224] = 0;
	blueTable[i+256] = blueMask;
    }

    for (i = 0; i < 256; i++) {
	y = gamma[i];
	redTable[i] = ((y >> redRight) << redLeft);
	greenTable[i] = ((y >> greenRight) << greenLeft);
	blueTable[i] = ((y >> blueRight) << blueLeft);
    }

    return 0;
}

static int rgbTable32 (int table [1935],
		       int redMask, int greenMask, int blueMask,
		       unsigned char gamma[256])
{
    int redRight;
    int redLeft;
    int greenRight;
    int greenLeft;
    int blueRight;
    int blueLeft;
    int * redTable;
    int * greenTable;
    int * blueTable;
    int i;
    int y;

    MaskToShift (&redRight, &redLeft, redMask);
    MaskToShift (&greenRight, &greenLeft, greenMask);
    MaskToShift (&blueRight, &blueLeft, blueMask);
    

    /*
     * green blue red +- 2 just to be sure
     * green = 0-525 [151-370]
     * blue = 594-1297 [834-1053] <834-29>
     * red = 1323-1934 [1517-1736] <493-712>
     */

    redTable = table + 1501;
    greenTable = table + 135;
    blueTable = table + 818;

    for (i = 0; i < 178; i++) {
	redTable[i-178] = 0;
	redTable[i+256] = redMask;
    }
    for (i = 0; i < 135; i++) {
	greenTable[i-135] = 0;
	greenTable[i+256] = greenMask;
    }
    for (i = 0; i < 224; i++) {
	blueTable[i-224] = 0;
	blueTable[i+256] = blueMask;
    }

    for (i = 0; i < 256; i++) {
	y = gamma[i];
	redTable[i] = ((y >> redRight) << redLeft);
	greenTable[i] = ((y >> greenRight) << greenLeft);
	blueTable[i] = ((y >> blueRight) << blueLeft);
    }

    return 0;
}


 void yuvToRgb16 (unsigned char * Y,
			unsigned char * U, unsigned char * V,
			short * dest, short table[1935], int width)
{
    int i;
    int u;
    int v;
    int uvRed;
    int uvGreen;
    int uvBlue;
    short * tableY;

    i = width >> 4;
    while (i--) {
	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
    }

    i = (width & 15) >> 1;
    while (i--) {
	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
    }

    if (width & 1) {
	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
    }
}

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

static void yuvToRgb32 (unsigned char * Y,
			unsigned char * U, unsigned char * V,
			int * dest, int table[1935], int width)
{
    int i;
    int u;
    int v;
    int uvRed;
    int uvGreen;
    int uvBlue;
    int * tableY;

    i = width >> 4;
    while (i--) {
	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
    }

    i = (width & 15) >> 1;
    while (i--) {
	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
    }

    if (width & 1) {
	u = *(U++);
	v = *(V++);
	uvRed = (V_RED_COEF*v) >> SHIFT;
	uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	tableY = table + *(Y++);
	*(dest++) = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		     tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
			    uvGreen] |
		     tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
    }
}

/* yuv routines with scaling */
/* 4:2:2 i, 16 bpp*/

void yuv422ToRgb16_scaled (unsigned char * Y,
			unsigned char * U, unsigned char * V,
			short * dest, short table[1935], int width , int dest_width,
            int height, int dest_height, int skip, int dest_skip,short * buffer)
{
    int i, i_hcount, i_vcount, j, k;
    int u;
    int v;
    int uvRed;
    int uvGreen;
    int uvBlue;
    short * tableY;
    short pix;

    if ( ( width < dest_width ) && ( height < dest_height ) )
    {
        i_vcount = dest_height;
        k = height;
        while ( k-- )
        {
            j = 0;
            i = width >> 1;
            i_hcount = dest_width;
            while ( i-- ) 
            {
	            u = *(U++);
	            v = *(V++);
	            uvRed = (V_RED_COEF*v) >> SHIFT;
	            uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	            uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	            tableY = table + *(Y++);
	            pix = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		               tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)
                       >>SHIFT) + uvGreen] |
		               tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
                while ( ( i_hcount -= width ) >= 0 )
                {
                    buffer[j++] = pix;
                }
                i_hcount += dest_width;
            
	            tableY = table + *(Y++);
	            pix = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		               tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)
                       >>SHIFT) + uvGreen] |
		               tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
                while ( ( i_hcount -= width ) >= 0 )
                {
                    buffer[j++] = pix;
                }
                i_hcount += dest_width;
            }
            while ( ( i_vcount -= height ) >= 0 )
            {
            for (j=0; j<dest_width; j+=16)
                {
                    dest[j]=buffer[j];
                    dest[j+1]=buffer[j+1];
                    dest[j+2]=buffer[j+2];
                    dest[j+3]=buffer[j+3];
                    dest[j+4]=buffer[j+4];
                    dest[j+6]=buffer[j+7];
                    dest[j+8]=buffer[j+9];
                    dest[j+10]=buffer[j+10];
                    dest[j+11]=buffer[j+11];
                    dest[j+12]=buffer[j+12];
                    dest[j+13]=buffer[j+13];
                    dest[j+14]=buffer[j+14];
                    dest[j+15]=buffer[j+15];
                }
                dest += dest_skip;
            }
        i_vcount += dest_height;
        }
    }
    else if ( ( width > dest_width ) && ( height < dest_height ) )
    {
        i_vcount = dest_height;
        k = height;
        while ( k-- )
        {
            j = 0;
            i_hcount = 0;
            i = width >> 1;
            while ( i-- ) 
            {
                u = *(U++);
                v = *(V++);
                uvRed = (V_RED_COEF*v) >> SHIFT;
                uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
                uvBlue = (U_BLUE_COEF*u) >> SHIFT;
                                                                        
	            if ( ( i_hcount -= dest_width ) >= 0 )
                    Y++;
                else
                {
                    tableY = table + *(Y++);
                    buffer[j++] = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + 
                                   uvRed] | 
                                   tableY [135 - 
                                   (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
                                   uvGreen] |
                                   tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + 
                                   uvBlue]);
                    i_hcount += width;
                }
                if ( ( i_hcount -= dest_width ) >= 0 )
                    Y++;
                else
                {     
                    tableY = table + *(Y++);
                    buffer[j++] = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + 
                                   uvRed] |
                                   tableY [135 - 
                                   (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
                                   uvGreen] |
                                   tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + 
                                   uvBlue]);
                                   i_hcount += width;
                }
            }
            while ( ( i_vcount -= height ) >= 0 )
            {
                for (j=0; j<dest_width; j+=16)
                 {
                    dest[j]=buffer[j];
                    dest[j+1]=buffer[j+1];
                    dest[j+2]=buffer[j+2];
                    dest[j+3]=buffer[j+3];
                    dest[j+4]=buffer[j+4];
                    dest[j+6]=buffer[j+7];
                    dest[j+8]=buffer[j+9];
                    dest[j+10]=buffer[j+10];
                    dest[j+11]=buffer[j+11];
                    dest[j+12]=buffer[j+12];
                    dest[j+13]=buffer[j+13];
                    dest[j+14]=buffer[j+14];
                    dest[j+15]=buffer[j+15];
                }
                dest += dest_skip;
            }
            i_vcount += dest_height;
        }
    }
    else if ( ( width < dest_width ) && ( height > dest_height ) )
    {
        i_vcount = 0;
        k = height;
        while ( k-- )
        {
            j = 0;
            i = width >> 1;
            i_hcount = dest_width;
            while ( i-- ) 
            {
	            u = *(U++);
	            v = *(V++);
	            uvRed = (V_RED_COEF*v) >> SHIFT;
	            uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	            uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	            tableY = table + *(Y++);
	            pix = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		               tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)
                       >>SHIFT) + uvGreen] |
		               tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
                while ( ( i_hcount -= width ) >= 0 )
                {
                    dest[j++] = pix;
                }
                i_hcount += dest_width;
            
	            tableY = table + *(Y++);
	            pix = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		               tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)
                       >>SHIFT) + uvGreen] |
		               tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
                while ( ( i_hcount -= width ) >= 0 )
                {
                    dest[j++] = pix;
                }
                i_hcount += dest_width;
            }
            while ( ( i_vcount -= height ) >= 0 )
            {
                Y += skip;
                U += skip >> 1;
                V += skip >> 1;
            }
            i_vcount += dest_height;
        }
    }
    else if ( ( width > dest_width ) && ( height > dest_height ) )
    {
        i_vcount = dest_height;
        k = height;
        while ( k-- )
        {
            j = 0;
            i_hcount = 0;
            i = width >> 1;
            while ( i-- ) 
            {
                u = *(U++);
                v = *(V++);
                uvRed = (V_RED_COEF*v) >> SHIFT;
                uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
                uvBlue = (U_BLUE_COEF*u) >> SHIFT;
                                                                        
	            if ( ( i_hcount -= dest_width ) >= 0 )
                    Y++;
                else
                {
                    tableY = table + *(Y++);
                    dest[j++] = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + 
                                 uvRed] | 
                                 tableY [135 - 
                                 (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
                                 uvGreen] |
                                 tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + 
                                 uvBlue]);
                    i_hcount += width;
                }
                if ( ( i_hcount -= dest_width ) >= 0 )
                    Y++;
                else
                {     
                    tableY = table + *(Y++);
                    dest[j++] = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + 
                                 uvRed] |
                                 tableY [135 - 
                                 (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
                                 uvGreen] |
                                 tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + 
                                 uvBlue]);
                                 i_hcount += width;
                }
            }
            while ( ( i_vcount -= height ) >= 0 )
            {
                Y += skip;
                U += skip >> 1;
                V += skip >> 1;
            }
            i_vcount += dest_height;
        }
    }
}

/* yuv routines with scaling */
/* 4:2:0 i, 16 bpp*/

void yuv420ToRgb16_scaled (unsigned char * Y,
			unsigned char * U, unsigned char * V,
			short * dest, short table[1935], int width , int dest_width,
            int height, int dest_height, int skip, int dest_skip,short * buffer)
{
    int i, i_hcount, i_vcount, j, k;
    int u;
    int v;
    int uvRed;
    int uvGreen;
    int uvBlue;
    short * tableY;
    short pix;

    if ( ( width < dest_width ) && ( height < dest_height ) )
    {
        i_vcount = dest_height;
        k = height >> 1;
        while ( k-- )
        {
            j = 0;
            i = width >> 1;
            i_hcount = dest_width;
            while ( i-- ) 
            {
	            u = *(U++);
	            v = *(V++);
	            uvRed = (V_RED_COEF*v) >> SHIFT;
	            uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	            uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	            tableY = table + *(Y++);
	            pix = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		               tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)
                       >>SHIFT) + uvGreen] |
		               tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
                while ( ( i_hcount -= width ) >= 0 )
                {
                    buffer[j++] = pix;
                }
                i_hcount += dest_width;
            
	            tableY = table + *(Y++);
	            pix = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		               tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)
                       >>SHIFT) + uvGreen] |
		               tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
                while ( ( i_hcount -= width ) >= 0 )
                {
                    buffer[j++] = pix;
                }
                i_hcount += dest_width;
            }
            while ( ( i_vcount -= height ) >= 0 )
            {
                for (j=0; j<dest_width; j+=16)
                    {
                        dest[j]=buffer[j];
                        dest[j+1]=buffer[j+1];
                        dest[j+2]=buffer[j+2];
                        dest[j+3]=buffer[j+3];
                        dest[j+4]=buffer[j+4];
                        dest[j+6]=buffer[j+7];
                        dest[j+8]=buffer[j+9];
                        dest[j+10]=buffer[j+10];
                        dest[j+11]=buffer[j+11];
                        dest[j+12]=buffer[j+12];
                        dest[j+13]=buffer[j+13];
                        dest[j+14]=buffer[j+14];
                        dest[j+15]=buffer[j+15];
                    }
                dest += dest_skip;
            }
            i_vcount += dest_height;
            U -= skip >> 1;
            V -= skip >> 1;
            j = 0;
            i = width >> 1;
            i_hcount = dest_width;
            while ( i-- ) 
            {
	            u = *(U++);
	            v = *(V++);
	            uvRed = (V_RED_COEF*v) >> SHIFT;
	            uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	            uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	            tableY = table + *(Y++);
	            pix = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		               tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)
                       >>SHIFT) + uvGreen] |
		               tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
                while ( ( i_hcount -= width ) >= 0 )
                {
                    buffer[j++] = pix;
                }
                i_hcount += dest_width;
            
	            tableY = table + *(Y++);
	            pix = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		               tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)
                       >>SHIFT) + uvGreen] |
		               tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
                while ( ( i_hcount -= width ) >= 0 )
                {
                    buffer[j++] = pix;
                }
                i_hcount += dest_width;
            }
            while ( ( i_vcount -= height ) >= 0 )
            {
                for (j=0; j<dest_width; j+=16)
                    {
                        dest[j]=buffer[j];
                        dest[j+1]=buffer[j+1];
                        dest[j+2]=buffer[j+2];
                        dest[j+3]=buffer[j+3];
                        dest[j+4]=buffer[j+4];
                        dest[j+6]=buffer[j+7];
                        dest[j+8]=buffer[j+9];
                        dest[j+10]=buffer[j+10];
                        dest[j+11]=buffer[j+11];
                        dest[j+12]=buffer[j+12];
                        dest[j+13]=buffer[j+13];
                        dest[j+14]=buffer[j+14];
                        dest[j+15]=buffer[j+15];
                    }
                dest += dest_skip;
            }
            i_vcount += dest_height;
        }
    }
    else if ( ( width > dest_width ) && ( height < dest_height ) )
    {
        i_vcount = dest_height;
        k = height;
        while ( k-- )
        {
            j = 0;
            i_hcount = 0;
            i = width >> 1;
            while ( i-- ) 
            {
                u = *(U++);
                v = *(V++);
                uvRed = (V_RED_COEF*v) >> SHIFT;
                uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
                uvBlue = (U_BLUE_COEF*u) >> SHIFT;
                                                                        
	            if ( ( i_hcount -= dest_width ) >= 0 )
                    Y++;
                else
                {
                    tableY = table + *(Y++);
                    buffer[j++] = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + 
                                   uvRed] | 
                                   tableY [135 - 
                                   (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
                                   uvGreen] |
                                   tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + 
                                   uvBlue]);
                    i_hcount += width;
                }
                if ( ( i_hcount -= dest_width ) >= 0 )
                    Y++;
                else
                {     
                    tableY = table + *(Y++);
                    buffer[j++] = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + 
                                   uvRed] |
                                   tableY [135 - 
                                   (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
                                   uvGreen] |
                                   tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + 
                                   uvBlue]);
                                   i_hcount += width;
                }
            }
            while ( ( i_vcount -= height ) >= 0 )
            {
                for (j=0; j<dest_width; j+=16)
                 {
                    dest[j]=buffer[j];
                    dest[j+1]=buffer[j+1];
                    dest[j+2]=buffer[j+2];
                    dest[j+3]=buffer[j+3];
                    dest[j+4]=buffer[j+4];
                    dest[j+6]=buffer[j+7];
                    dest[j+8]=buffer[j+9];
                    dest[j+10]=buffer[j+10];
                    dest[j+11]=buffer[j+11];
                    dest[j+12]=buffer[j+12];
                    dest[j+13]=buffer[j+13];
                    dest[j+14]=buffer[j+14];
                    dest[j+15]=buffer[j+15];
                }
                dest += dest_skip;
            }
            i_vcount += dest_height;
            U -= skip >> 1;
            V -= skip >> 1;
            j = 0;
            i_hcount = 0;
            i = width >> 1;
            while ( i-- ) 
            {
                u = *(U++);
                v = *(V++);
                uvRed = (V_RED_COEF*v) >> SHIFT;
                uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
                uvBlue = (U_BLUE_COEF*u) >> SHIFT;
                                                                        
	            if ( ( i_hcount -= dest_width ) >= 0 )
                    Y++;
                else
                {
                    tableY = table + *(Y++);
                    buffer[j++] = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + 
                                   uvRed] | 
                                   tableY [135 - 
                                   (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
                                   uvGreen] |
                                   tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + 
                                   uvBlue]);
                    i_hcount += width;
                }
                if ( ( i_hcount -= dest_width ) >= 0 )
                    Y++;
                else
                {     
                    tableY = table + *(Y++);
                    buffer[j++] = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + 
                                   uvRed] |
                                   tableY [135 - 
                                   (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
                                   uvGreen] |
                                   tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + 
                                   uvBlue]);
                                   i_hcount += width;
                }
            }
            while ( ( i_vcount -= height ) >= 0 )
            {
                for (j=0; j<dest_width; j+=16)
                 {
                    dest[j]=buffer[j];
                    dest[j+1]=buffer[j+1];
                    dest[j+2]=buffer[j+2];
                    dest[j+3]=buffer[j+3];
                    dest[j+4]=buffer[j+4];
                    dest[j+6]=buffer[j+7];
                    dest[j+8]=buffer[j+9];
                    dest[j+10]=buffer[j+10];
                    dest[j+11]=buffer[j+11];
                    dest[j+12]=buffer[j+12];
                    dest[j+13]=buffer[j+13];
                    dest[j+14]=buffer[j+14];
                    dest[j+15]=buffer[j+15];
                }
                dest += dest_skip;
            }
            i_vcount += dest_height;
        }
    }
    else if ( ( width < dest_width ) && ( height > dest_height ) )
    {
        i_vcount = 0;
        k = height;
        while ( k-- )
        {
            j = 0;
            i = width >> 1;
            i_hcount = dest_width;
            while ( i-- ) 
            {
	            u = *(U++);
	            v = *(V++);
	            uvRed = (V_RED_COEF*v) >> SHIFT;
	            uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
	            uvBlue = (U_BLUE_COEF*u) >> SHIFT;

	            tableY = table + *(Y++);
	            pix = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		               tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)
                       >>SHIFT) + uvGreen] |
		               tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
                while ( ( i_hcount -= width ) >= 0 )
                {
                    dest[j++] = pix;
                }
                i_hcount += dest_width;
            
	            tableY = table + *(Y++);
	            pix = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + uvRed] |
		               tableY [135 - (((U_GREEN_COEF+V_GREEN_COEF)*128)
                       >>SHIFT) + uvGreen] |
		               tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + uvBlue]);
                while ( ( i_hcount -= width ) >= 0 )
                {
                    dest[j++] = pix;
                }
                i_hcount += dest_width;
            }
            j = 0;
            while ( ( i_vcount -= height ) >= 0 )
            {
                Y += skip;
                j++;
            }
            U += skip * ( j >> 1 );
            V += skip * ( j >> 1 );
            i_vcount += dest_height;
        }
    }
    else if ( ( width > dest_width ) && ( height > dest_height ) )
    {
        i_vcount = dest_height;
        k = height;
        while ( k-- )
        {
            j = 0;
            i_hcount = 0;
            i = width >> 1;
            while ( i-- ) 
            {
                u = *(U++);
                v = *(V++);
                uvRed = (V_RED_COEF*v) >> SHIFT;
                uvGreen = (U_GREEN_COEF*u + V_GREEN_COEF*v) >> SHIFT;
                uvBlue = (U_BLUE_COEF*u) >> SHIFT;
                                                                        
	            if ( ( i_hcount -= dest_width ) >= 0 )
                    Y++;
                else
                {
                    tableY = table + *(Y++);
                    dest[j++] = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + 
                                 uvRed] | 
                                 tableY [135 - 
                                 (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
                                 uvGreen] |
                                 tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + 
                                 uvBlue]);
                    i_hcount += width;
                }
                if ( ( i_hcount -= dest_width ) >= 0 )
                    Y++;
                else
                {     
                    tableY = table + *(Y++);
                    dest[j++] = (tableY [1501 - ((V_RED_COEF*128)>>SHIFT) + 
                                 uvRed] |
                                 tableY [135 - 
                                 (((U_GREEN_COEF+V_GREEN_COEF)*128)>>SHIFT) +
                                 uvGreen] |
                                 tableY [818 - ((U_BLUE_COEF*128)>>SHIFT) + 
                                 uvBlue]);
                                 i_hcount += width;
                }
            }
            j = 0;
            while ( ( i_vcount -= height ) >= 0 )
            {
                Y += skip;
                j++;
            }
            U += skip * ( j >> 1 );
            V += skip * ( j >> 1 );
            i_vcount += dest_height;
        }
    }
}

