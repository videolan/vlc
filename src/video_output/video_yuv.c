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
#include <errno.h>
#include <math.h>
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

/*******************************************************************************
 * Local prototypes
 *******************************************************************************/
static int      BinaryLog         ( u32 i );
static void     MaskToShift       ( int *pi_right, int *pi_left, u32 i_mask );
static void     SetTables         ( vout_thread_t *p_vout );

static void     ConvertY4Gray16   ( p_vout_thread_t p_vout, u16 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                    int i_width, int i_height, int i_eol, int i_pic_eol, int i_scale, int i_matrix_coefficients );
static void     ConvertY4Gray24   ( p_vout_thread_t p_vout, void *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                    int i_width, int i_height, int i_eol, int i_pic_eol, int i_scale, int i_matrix_coefficients );
static void     ConvertY4Gray32   ( p_vout_thread_t p_vout, u32 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                    int i_width, int i_height, int i_eol, int i_pic_eol, int i_scale, int i_matrix_coefficients );
static void     ConvertYUV420RGB16( p_vout_thread_t p_vout, u16 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                    int i_width, int i_height, int i_eol, int i_pic_eol, int i_scale, int i_matrix_coefficients );
static void     ConvertYUV422RGB16( p_vout_thread_t p_vout, u16 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                    int i_width, int i_height, int i_eol, int i_pic_eol, int i_scale, int i_matrix_coefficients );
static void     ConvertYUV444RGB16( p_vout_thread_t p_vout, u16 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                    int i_width, int i_height, int i_eol, int i_pic_eol, int i_scale, int i_matrix_coefficients );
static void     ConvertYUV420RGB24( p_vout_thread_t p_vout, void *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                    int i_width, int i_height, int i_eol, int i_pic_eol, int i_scale, int i_matrix_coefficients );
static void     ConvertYUV422RGB24( p_vout_thread_t p_vout, void *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                    int i_width, int i_height, int i_eol, int i_pic_eol, int i_scale, int i_matrix_coefficients );
static void     ConvertYUV444RGB24( p_vout_thread_t p_vout, void *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                    int i_width, int i_height, int i_eol, int i_pic_eol, int i_scale, int i_matrix_coefficients );
static void     ConvertYUV420RGB32( p_vout_thread_t p_vout, u32 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                    int i_width, int i_height, int i_eol, int i_pic_eol, int i_scale, int i_matrix_coefficients );
static void     ConvertYUV422RGB32( p_vout_thread_t p_vout, u32 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                    int i_width, int i_height, int i_eol, int i_pic_eol, int i_scale, int i_matrix_coefficients );
static void     ConvertYUV444RGB32( p_vout_thread_t p_vout, u32 *p_pic, yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                    int i_width, int i_height, int i_eol, int i_pic_eol, int i_scale, int i_matrix_coefficients );

/*******************************************************************************
 * CLIP_BYTE macro: boundary detection
 *******************************************************************************
 * Return parameter if between 0 and 255, else return nearest boundary (0 or 
 * 255). This macro is used to build translations tables.
 *******************************************************************************/
#define CLIP_BYTE( i_val ) ( (i_val < 0) ? 0 : ((i_val > 255) ? 255 : i_val) )

/*******************************************************************************
 * LINE_COPY macro: memcopy using 16 pixels blocks
 *******************************************************************************
 * Variables:
 *      p_pic                   destination pointer
 *      p_pic_src               source pointer
 *      i_width                 width
 *      i_x                     index
 *******************************************************************************/
#define LINE_COPY                                                       \
for( i_x = 0; i_x < i_width; i_x+=16 )                                  \
{                                                                       \
    *p_pic++ = *p_pic_src++;                                            \
    *p_pic++ = *p_pic_src++;                                            \
    *p_pic++ = *p_pic_src++;                                            \
    *p_pic++ = *p_pic_src++;                                            \
    *p_pic++ = *p_pic_src++;                                            \
    *p_pic++ = *p_pic_src++;                                            \
    *p_pic++ = *p_pic_src++;                                            \
    *p_pic++ = *p_pic_src++;                                            \
    *p_pic++ = *p_pic_src++;                                            \
    *p_pic++ = *p_pic_src++;                                            \
    *p_pic++ = *p_pic_src++;                                            \
    *p_pic++ = *p_pic_src++;                                            \
    *p_pic++ = *p_pic_src++;                                            \
    *p_pic++ = *p_pic_src++;                                            \
    *p_pic++ = *p_pic_src++;                                            \
    *p_pic++ = *p_pic_src++;                                            \
}

/*******************************************************************************
 * CONVERT_YUV_GRAY macro: grayscale YUV convertion
 *******************************************************************************
 * Variables:
 *      ...see vout_convert_t
 *      i_x, i_y                coordinates
 *      i_pic_copy              same type as p_pic
 *      p_gray                  gray translation table
 *******************************************************************************/
#define CONVERT_YUV_GRAY                                                \
for (i_y = 0; i_y < i_height ; i_y++)                                   \
{                                                                       \
    for (i_x = 0; i_x < i_width; i_x += 16)                             \
    {                                                                   \
        /* Convert 16 pixels (width is always multiple of 16 */         \
        *p_pic++ = p_gray[ *p_y++ ];                                    \
        *p_pic++ = p_gray[ *p_y++ ];                                    \
        *p_pic++ = p_gray[ *p_y++ ];                                    \
        *p_pic++ = p_gray[ *p_y++ ];                                    \
        *p_pic++ = p_gray[ *p_y++ ];                                    \
        *p_pic++ = p_gray[ *p_y++ ];                                    \
        *p_pic++ = p_gray[ *p_y++ ];                                    \
        *p_pic++ = p_gray[ *p_y++ ];                                    \
        *p_pic++ = p_gray[ *p_y++ ];                                    \
        *p_pic++ = p_gray[ *p_y++ ];                                    \
        *p_pic++ = p_gray[ *p_y++ ];                                    \
        *p_pic++ = p_gray[ *p_y++ ];                                    \
        *p_pic++ = p_gray[ *p_y++ ];                                    \
        *p_pic++ = p_gray[ *p_y++ ];                                    \
        *p_pic++ = p_gray[ *p_y++ ];                                    \
        *p_pic++ = p_gray[ *p_y++ ];                                    \
    }                                                                   \
                                                                        \
    /* Handle scale factor */                                           \
    if( i_scale && ! (i_y % i_scale) )                                  \
    {                                                                   \
        if( i_scale < 0 )                                               \
        {                                                               \
            /* Copy previous line */                                    \
            p_pic_src = p_pic - i_width;                                \
            p_pic += i_pic_eol;                                         \
            LINE_COPY                                                   \
        }                                                               \
        else                                                            \
        {                                                               \
            /* Ignore next line */                                      \
            p_y += i_eol + i_width;                                     \
            i_y++;                                                      \
        }                                                               \
    }                                                                   \
                                                                        \
    /* Skip until beginning of next line */                             \
    p_pic += i_pic_eol;                                                 \
    p_y   += i_eol;                                                     \
}

/*******************************************************************************
 * CONVERT_YUV_RGB: color YUV convertion
 *******************************************************************************
 * Parameters
 *      CHROMA                          420, 422 or 444
 * Variables:
 *      ...see vout_convert_t
 *      i_x, i_y                        coordinates
 *      i_uval, i_yval, i_vval          samples
 *      p_pic_src                       same type as p_pic
 *      i_chroma_width                  chroma width
 *      i_chroma_eol                    chroma eol
 *      p_red                           red translation table
 *      p_green                         green translation table
 *      p_blue                          blue translation table
 *      i_crv, i_cgu, i_cgv, i_cbu      matrix coefficients
 *******************************************************************************/
#define CONVERT_YUV_RGB( CHROMA )                                       \
for (i_y = 0; i_y < i_height ; i_y++)                                   \
{                                                                       \
    for (i_x = 0; i_x < i_width; i_x += 2 )                             \
    {                                                                   \
        /* First sample (complete) */                                   \
        i_yval = 76309 * *p_y++ - 1188177;                              \
        i_uval = *p_u++ - 128;                                          \
        i_vval = *p_v++ - 128;                                          \
        *p_pic++ =                                                      \
            p_red  [(i_yval+i_crv*i_vval)                >>16] |        \
            p_green[(i_yval-i_cgu*i_uval-i_cgv*i_vval)   >>16] |        \
            p_blue [(i_yval+i_cbu*i_uval)                >>16];         \
        i_yval = 76309 * *p_y++ - 1188177;                              \
        /* Second sample (partial) */                                   \
        if( CHROMA == 444 )                                             \
        {                                                               \
            i_uval = *p_u++ - 128;                                      \
            i_vval = *p_v++ - 128;                                      \
        }                                                               \
        *p_pic++ =                                                      \
            p_red  [(i_yval+i_crv*i_vval)                >>16] |        \
            p_green[(i_yval-i_cgu*i_uval-i_cgv*i_vval)   >>16] |        \
            p_blue [(i_yval+i_cbu*i_uval)                >>16];         \
    }                                                                   \
                                                                        \
    /* Handle scale factor and rewind in 4:2:0 */                       \
    if( i_scale && ! (i_y % i_scale) )                                  \
    {                                                                   \
        if( i_scale < 0 )                                               \
        {                                                               \
            /* Copy previous line, rewind if required */                \
            p_pic_src = p_pic - i_width;                                \
            p_pic += i_pic_eol;                                         \
            LINE_COPY                                                   \
            if( (CHROMA == 420) && !(i_y & 0x1) )                       \
            {                                                           \
                p_u -= i_chroma_width;                                  \
                p_v -= i_chroma_width;                                  \
            }                                                           \
            else                                                        \
            {                                                           \
                p_u += i_chroma_eol;                                    \
                p_v += i_chroma_eol;                                    \
            }                                                           \
        }                                                               \
        else                                                            \
        {                                                               \
            /* Ignore next line */                                      \
            p_y += i_eol + i_width;                                     \
            p_u += i_chroma_eol;                                        \
            p_v += i_chroma_eol;                                        \
            i_y++;                                                      \
        }                                                               \
    }                                                                   \
    else if( (CHROMA == 420) && !(i_y & 0x1) )                          \
    {                                                                   \
        p_u -= i_chroma_width;                                          \
        p_v -= i_chroma_width;                                          \
    }                                                                   \
    else                                                                \
    {                                                                   \
        p_u += i_chroma_eol;                                            \
        p_v += i_chroma_eol;                                            \
    }                                                                   \
                                                                        \
    /* Skip until beginning of next line */                             \
    p_pic += i_pic_eol;                                                 \
    p_y   += i_eol;                                                     \
}


/*******************************************************************************
 * vout_InitTables: allocate and initialize translations tables
 *******************************************************************************
 * This function will allocate memory to store translation tables, depending
 * of the screen depth.
 *******************************************************************************/
int vout_InitTables( vout_thread_t *p_vout )
{
    size_t      tables_size;                          /* tables size, in bytes */
    
    /* Computes tables size */
    switch( p_vout->i_screen_depth )
    {
    case 15:
    case 16:
        tables_size = sizeof( u16 ) * 1024 * (p_vout->b_grayscale ? 1 : 3);
        break;        
    case 24:        
    case 32:
#ifndef DEBUG
    default:        
#endif
        tables_size = sizeof( u32 ) * 1024 * (p_vout->b_grayscale ? 1 : 3);        
        break;        
#ifdef DEBUG
    default:
        intf_DbgMsg("error: invalid screen depth %d\n", p_vout->i_screen_depth );
        tables_size = 0;
        break;        
#endif      
    }
    
    /* Allocate memory */
    p_vout->tables.p_base = malloc( tables_size );
    if( p_vout->tables.p_base == NULL )
    {
        intf_ErrMsg("error: %s\n", strerror(ENOMEM));
        return( 1 );                
    }

    /* Initialize tables */
    SetTables( p_vout );    
    return( 0 );    
}

/*******************************************************************************
 * vout_ResetTables: re-initialize translations tables
 *******************************************************************************
 * This function will initialize the tables allocated by vout_CreateTables and
 * set functions pointers.
 *******************************************************************************/
int vout_ResetTables( vout_thread_t *p_vout )
{
    vout_EndTables( p_vout );    
    return( vout_InitTables( p_vout ) );    
}

/*******************************************************************************
 * vout_EndTables: destroy translations tables
 *******************************************************************************
 * Free memory allocated by vout_CreateTables.
 *******************************************************************************/
void vout_EndTables( vout_thread_t *p_vout )
{
    free( p_vout->tables.p_base );
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
 * SetTables: compute tables and set function pointers
 *******************************************************************************/
static void SetTables( vout_thread_t *p_vout )
{
    u8          i_gamma[256];                                   /* gamma table */    
    int         i_index;                                    /* index in tables */
    int         i_red_right, i_red_left;                         /* red shifts */
    int         i_green_right, i_green_left;                   /* green shifts */
    int         i_blue_right, i_blue_left;                      /* blue shifts */

    /*
     * Build gamma table 
     */     
    for( i_index = 0; i_index < 256; i_index++ )
    {
        i_gamma[i_index] = 255. * pow( (double)i_index / 255., exp(p_vout->f_gamma) );        
    }

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
            p_vout->tables.yuv.gray16.p_gray =  (u16 *)p_vout->tables.p_base + 384;
            for( i_index = -384; i_index < 640; i_index++) 
            {
                p_vout->tables.yuv.gray16.p_gray[ i_index ] = 
                    ((i_gamma[CLIP_BYTE( i_index )] >> i_red_right)   << i_red_left)   |
                    ((i_gamma[CLIP_BYTE( i_index )] >> i_green_right) << i_green_left) |
                    ((i_gamma[CLIP_BYTE( i_index )] >> i_blue_right)  << i_blue_left);                
            }
            break;        
        case 24:
        case 32:        
            p_vout->tables.yuv.gray32.p_gray =  (u32 *)p_vout->tables.p_base + 384;
            for( i_index = -384; i_index < 640; i_index++) 
            {
                p_vout->tables.yuv.gray32.p_gray[ i_index ] = 
                    ((i_gamma[CLIP_BYTE( i_index )] >> i_red_right)   << i_red_left)   |
                    ((i_gamma[CLIP_BYTE( i_index )] >> i_green_right) << i_green_left) |
                    ((i_gamma[CLIP_BYTE( i_index )] >> i_blue_right)  << i_blue_left);                
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
            p_vout->tables.yuv.rgb16.p_red =    (u16 *)p_vout->tables.p_base +          384;
            p_vout->tables.yuv.rgb16.p_green =  (u16 *)p_vout->tables.p_base +   1024 + 384;
            p_vout->tables.yuv.rgb16.p_blue =   (u16 *)p_vout->tables.p_base + 2*1024 + 384;
            for( i_index = -384; i_index < 640; i_index++) 
            {
                p_vout->tables.yuv.rgb16.p_red[i_index] =   (i_gamma[CLIP_BYTE(i_index)]>>i_red_right)<<i_red_left;
                p_vout->tables.yuv.rgb16.p_green[i_index] = (i_gamma[CLIP_BYTE(i_index)]>>i_green_right)<<i_green_left;
                p_vout->tables.yuv.rgb16.p_blue[i_index] =  (i_gamma[CLIP_BYTE(i_index)]>>i_blue_right)<<i_blue_left;
            }
            break;        
        case 24:
        case 32:
            p_vout->tables.yuv.rgb32.p_red =    (u32 *)p_vout->tables.p_base +          384;
            p_vout->tables.yuv.rgb32.p_green =  (u32 *)p_vout->tables.p_base +   1024 + 384;
            p_vout->tables.yuv.rgb32.p_blue =   (u32 *)p_vout->tables.p_base + 2*1024 + 384;
            for( i_index = -384; i_index < 640; i_index++) 
            {
                p_vout->tables.yuv.rgb32.p_red[i_index] =   (i_gamma[CLIP_BYTE(i_index)]>>i_red_right)<<i_red_left;
                p_vout->tables.yuv.rgb32.p_green[i_index] = (i_gamma[CLIP_BYTE(i_index)]>>i_green_right)<<i_green_left;
                p_vout->tables.yuv.rgb32.p_blue[i_index] =  (i_gamma[CLIP_BYTE(i_index)]>>i_blue_right)<<i_blue_left;
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
        switch( p_vout->i_screen_depth )
        {
        case 15:
        case 16:  
            p_vout->p_ConvertYUV420 = (vout_convert_t *) ConvertY4Gray16;        
            p_vout->p_ConvertYUV420 = (vout_convert_t *) ConvertY4Gray16;        
            p_vout->p_ConvertYUV420 = (vout_convert_t *) ConvertY4Gray16;        
            break;        
        case 24:
            p_vout->p_ConvertYUV420 = (vout_convert_t *) ConvertY4Gray24;        
            p_vout->p_ConvertYUV420 = (vout_convert_t *) ConvertY4Gray24;        
            p_vout->p_ConvertYUV420 = (vout_convert_t *) ConvertY4Gray24;        
            break;        
        case 32:        
            p_vout->p_ConvertYUV420 = (vout_convert_t *) ConvertY4Gray32;        
            p_vout->p_ConvertYUV420 = (vout_convert_t *) ConvertY4Gray32;        
            p_vout->p_ConvertYUV420 = (vout_convert_t *) ConvertY4Gray32;        
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
            p_vout->p_ConvertYUV420 =   (vout_convert_t *) ConvertYUV420RGB16;        
            p_vout->p_ConvertYUV422 =   (vout_convert_t *) ConvertYUV422RGB16;        
            p_vout->p_ConvertYUV444 =   (vout_convert_t *) ConvertYUV444RGB16;        
            break;        
        case 24:
            p_vout->p_ConvertYUV420 =   (vout_convert_t *) ConvertYUV420RGB24;        
            p_vout->p_ConvertYUV422 =   (vout_convert_t *) ConvertYUV422RGB24;        
            p_vout->p_ConvertYUV444 =   (vout_convert_t *) ConvertYUV444RGB24;        
            break;        
        case 32:        
            p_vout->p_ConvertYUV420 =   (vout_convert_t *) ConvertYUV420RGB32;        
            p_vout->p_ConvertYUV422 =   (vout_convert_t *) ConvertYUV422RGB32;        
            p_vout->p_ConvertYUV444 =   (vout_convert_t *) ConvertYUV444RGB32;        
            break;        
        }
    }        
}

/*******************************************************************************
 * ConvertY4Gray16: grayscale YUV 4:x:x to RGB 15 or 16 bpp
 *******************************************************************************/
static void ConvertY4Gray16( p_vout_thread_t p_vout, u16 *p_pic,
                             yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                             int i_width, int i_height, int i_eol, int i_pic_eol,
                             int i_scale, int i_matrix_coefficients )
{
    u16 *       p_pic_src;                   /* source pointer in case of copy */
    u16 *       p_gray;                                          /* gray table */    
    int         i_x, i_y;                               /* picture coordinates */
    
    p_gray = p_vout->tables.yuv.gray16.p_gray;
    CONVERT_YUV_GRAY
}

/*******************************************************************************
 * ConvertY4Gray24: grayscale YUV 4:x:x to RGB 24 bpp
 *******************************************************************************/
static void ConvertY4Gray24( p_vout_thread_t p_vout, void *p_pic,
                             yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                             int i_width, int i_height, int i_eol, int i_pic_eol,
                             int i_scale, int i_matrix_coefficients )
{
    //??
}

/*******************************************************************************
 * ConvertY4Gray32: grayscale YUV 4:x:x to RGB 32 bpp
 *******************************************************************************/
static void ConvertY4Gray32( p_vout_thread_t p_vout, u32 *p_pic,
                             yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                             int i_width, int i_height, int i_eol, int i_pic_eol,
                             int i_scale, int i_matrix_coefficients )
{
    u32 *       p_pic_src;                   /* source pointer in case of copy */
    u32 *       p_gray;                                          /* gray table */    
    int         i_x, i_y;                               /* picture coordinates */
    
    p_gray = p_vout->tables.yuv.gray32.p_gray;
    CONVERT_YUV_GRAY
}

/*******************************************************************************
 * ConvertYUV420RGB16: color YUV 4:2:0 to RGB 15 or 16 bpp
 *******************************************************************************/
static void ConvertYUV420RGB16( p_vout_thread_t p_vout, u16 *p_pic,
                                yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                int i_width, int i_height, int i_eol, int i_pic_eol,
                                int i_scale, int i_matrix_coefficients )
{
#ifdef HAVE_MMX
    int         i_chroma_width, i_chroma_eol;      /* width and eol for chroma */

    i_chroma_width =    i_width / 2;
    i_chroma_eol =      i_eol / 2;
    ConvertYUV420RGB16MMX( p_y, p_u, p_v, i_width, i_height, 
                           (i_width + i_eol) * sizeof( yuv_data_t ), 
                           (i_chroma_width + i_chroma_eol) * sizeof( yuv_data_t),
                           i_scale, (u8 *)p_pic, 0, 0, (i_width + i_pic_eol) * sizeof( u16 ),
                           p_vout->i_screen_depth == 15 );    
#else
    u16 *       p_pic_src;                   /* source pointer in case of copy */
    u16 *       p_red;                                            /* red table */
    u16 *       p_green;                                        /* green table */
    u16 *       p_blue;                                          /* blue table */
    int         i_uval, i_yval, i_vval;                             /* samples */   
    int         i_x, i_y;                               /* picture coordinates */
    int         i_chroma_width, i_chroma_eol;      /* width and eol for chroma */
    int         i_crv, i_cbu, i_cgu, i_cgv;     /* transformation coefficients */

    i_crv = MATRIX_COEFFICIENTS_TABLE[i_matrix_coefficients][0];
    i_cbu = MATRIX_COEFFICIENTS_TABLE[i_matrix_coefficients][1];
    i_cgu = MATRIX_COEFFICIENTS_TABLE[i_matrix_coefficients][2];
    i_cgv = MATRIX_COEFFICIENTS_TABLE[i_matrix_coefficients][3];
    p_red   = p_vout->tables.yuv.rgb16.p_red;
    p_green = p_vout->tables.yuv.rgb16.p_green;
    p_blue  = p_vout->tables.yuv.rgb16.p_blue;
    i_chroma_width =    i_width / 2;
    i_chroma_eol =      i_eol / 2;
    CONVERT_YUV_RGB( 420 )
#endif
}

/*******************************************************************************
 * ConvertYUV422RGB16: color YUV 4:2:2 to RGB 15 or 16 bpp
 *******************************************************************************/
static void ConvertYUV422RGB16( p_vout_thread_t p_vout, u16 *p_pic,
                                yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                int i_width, int i_height, int i_eol, int i_pic_eol,
                                int i_scale, int i_matrix_coefficients )
{
    u16 *       p_pic_src;                   /* source pointer in case of copy */
    u16 *       p_red;                                            /* red table */
    u16 *       p_green;                                        /* green table */
    u16 *       p_blue;                                          /* blue table */
    int         i_uval, i_yval, i_vval;                             /* samples */   
    int         i_x, i_y;                               /* picture coordinates */
    int         i_chroma_width, i_chroma_eol;      /* width and eol for chroma */
    int         i_crv, i_cbu, i_cgu, i_cgv;     /* transformation coefficients */

    i_crv = MATRIX_COEFFICIENTS_TABLE[i_matrix_coefficients][0];
    i_cbu = MATRIX_COEFFICIENTS_TABLE[i_matrix_coefficients][1];
    i_cgu = MATRIX_COEFFICIENTS_TABLE[i_matrix_coefficients][2];
    i_cgv = MATRIX_COEFFICIENTS_TABLE[i_matrix_coefficients][3];
    p_red   = p_vout->tables.yuv.rgb16.p_red;
    p_green = p_vout->tables.yuv.rgb16.p_green;
    p_blue  = p_vout->tables.yuv.rgb16.p_blue;
    i_chroma_width =    i_width / 2;
    i_chroma_eol =      i_eol / 2;
    CONVERT_YUV_RGB( 422 )
}

/*******************************************************************************
 * ConvertYUV444RGB16: color YUV 4:4:4 to RGB 15 or 16 bpp
 *******************************************************************************/
static void ConvertYUV444RGB16( p_vout_thread_t p_vout, u16 *p_pic,
                                yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                int i_width, int i_height, int i_eol, int i_pic_eol,
                                int i_scale, int i_matrix_coefficients )
{
    u16 *       p_pic_src;                   /* source pointer in case of copy */
    u16 *       p_red;                                            /* red table */
    u16 *       p_green;                                        /* green table */
    u16 *       p_blue;                                          /* blue table */
    int         i_uval, i_yval, i_vval;                             /* samples */   
    int         i_x, i_y;                               /* picture coordinates */
    int         i_chroma_width, i_chroma_eol;      /* width and eol for chroma */
    int         i_crv, i_cbu, i_cgu, i_cgv;     /* transformation coefficients */

    i_crv = MATRIX_COEFFICIENTS_TABLE[i_matrix_coefficients][0];
    i_cbu = MATRIX_COEFFICIENTS_TABLE[i_matrix_coefficients][1];
    i_cgu = MATRIX_COEFFICIENTS_TABLE[i_matrix_coefficients][2];
    i_cgv = MATRIX_COEFFICIENTS_TABLE[i_matrix_coefficients][3];
    p_red   = p_vout->tables.yuv.rgb16.p_red;
    p_green = p_vout->tables.yuv.rgb16.p_green;
    p_blue  = p_vout->tables.yuv.rgb16.p_blue;
    i_chroma_width =    i_width;
    i_chroma_eol =      i_eol;
    CONVERT_YUV_RGB( 444 )
}

/*******************************************************************************
 * ConvertYUV420RGB24: color YUV 4:2:0 to RGB 24 bpp
 *******************************************************************************/
static void ConvertYUV420RGB24( p_vout_thread_t p_vout, void *p_pic,
                                yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                int i_width, int i_height, int i_eol, int i_pic_eol,
                                int i_scale, int i_matrix_coefficients )
{
    //???
}

/*******************************************************************************
 * ConvertYUV422RGB24: color YUV 4:2:2 to RGB 24 bpp
 *******************************************************************************/
static void ConvertYUV422RGB24( p_vout_thread_t p_vout, void *p_pic,
                                yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                int i_width, int i_height, int i_eol, int i_pic_eol,
                                int i_scale, int i_matrix_coefficients )
{
    //???
}

/*******************************************************************************
 * ConvertYUV444RGB24: color YUV 4:4:4 to RGB 24 bpp
 *******************************************************************************/
static void ConvertYUV444RGB24( p_vout_thread_t p_vout, void *p_pic,
                                yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                int i_width, int i_height, int i_eol, int i_pic_eol,
                                int i_scale, int i_matrix_coefficients )
{    
    //???
}

/*******************************************************************************
 * ConvertYUV420RGB32: color YUV 4:2:0 to RGB 32 bpp
 *******************************************************************************/
static void ConvertYUV420RGB32( p_vout_thread_t p_vout, u32 *p_pic,
                                yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                int i_width, int i_height, int i_eol, int i_pic_eol,
                                int i_scale, int i_matrix_coefficients )
{
    u32 *       p_pic_src;                   /* source pointer in case of copy */
    u32 *       p_red;                                            /* red table */
    u32 *       p_green;                                        /* green table */
    u32 *       p_blue;                                          /* blue table */
    int         i_uval, i_yval, i_vval;                             /* samples */   
    int         i_x, i_y;                               /* picture coordinates */
    int         i_chroma_width, i_chroma_eol;      /* width and eol for chroma */
    int         i_crv, i_cbu, i_cgu, i_cgv;     /* transformation coefficients */

    i_crv = MATRIX_COEFFICIENTS_TABLE[i_matrix_coefficients][0];
    i_cbu = MATRIX_COEFFICIENTS_TABLE[i_matrix_coefficients][1];
    i_cgu = MATRIX_COEFFICIENTS_TABLE[i_matrix_coefficients][2];
    i_cgv = MATRIX_COEFFICIENTS_TABLE[i_matrix_coefficients][3];
    p_red   = p_vout->tables.yuv.rgb32.p_red;
    p_green = p_vout->tables.yuv.rgb32.p_green;
    p_blue  = p_vout->tables.yuv.rgb32.p_blue;
    i_chroma_width =    i_width / 2;
    i_chroma_eol =      i_eol / 2;
    CONVERT_YUV_RGB( 420 )
}

/*******************************************************************************
 * ConvertYUV422RGB32: color YUV 4:2:2 to RGB 32 bpp
 *******************************************************************************/
static void ConvertYUV422RGB32( p_vout_thread_t p_vout, u32 *p_pic,
                                yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                int i_width, int i_height, int i_eol, int i_pic_eol,
                                int i_scale, int i_matrix_coefficients )
{
    u32 *       p_pic_src;                   /* source pointer in case of copy */
    u32 *       p_red;                                            /* red table */
    u32 *       p_green;                                        /* green table */
    u32 *       p_blue;                                          /* blue table */
    int         i_uval, i_yval, i_vval;                             /* samples */   
    int         i_x, i_y;                               /* picture coordinates */
    int         i_chroma_width, i_chroma_eol;      /* width and eol for chroma */
    int         i_crv, i_cbu, i_cgu, i_cgv;     /* transformation coefficients */

    i_crv = MATRIX_COEFFICIENTS_TABLE[i_matrix_coefficients][0];
    i_cbu = MATRIX_COEFFICIENTS_TABLE[i_matrix_coefficients][1];
    i_cgu = MATRIX_COEFFICIENTS_TABLE[i_matrix_coefficients][2];
    i_cgv = MATRIX_COEFFICIENTS_TABLE[i_matrix_coefficients][3];
    p_red   = p_vout->tables.yuv.rgb32.p_red;
    p_green = p_vout->tables.yuv.rgb32.p_green;
    p_blue  = p_vout->tables.yuv.rgb32.p_blue;
    i_chroma_width =    i_width / 2;
    i_chroma_eol =      i_eol / 2;
    CONVERT_YUV_RGB( 422 )
}

/*******************************************************************************
 * ConvertYUV444RGB32: color YUV 4:4:4 to RGB 32 bpp
 *******************************************************************************/
static void ConvertYUV444RGB32( p_vout_thread_t p_vout, u32 *p_pic,
                                yuv_data_t *p_y, yuv_data_t *p_u, yuv_data_t *p_v,
                                int i_width, int i_height, int i_eol, int i_pic_eol,
                                int i_scale, int i_matrix_coefficients )
{
    u32 *       p_pic_src;                   /* source pointer in case of copy */
    u32 *       p_red;                                            /* red table */
    u32 *       p_green;                                        /* green table */
    u32 *       p_blue;                                          /* blue table */
    int         i_uval, i_yval, i_vval;                             /* samples */   
    int         i_x, i_y;                               /* picture coordinates */
    int         i_chroma_width, i_chroma_eol;      /* width and eol for chroma */
    int         i_crv, i_cbu, i_cgu, i_cgv;     /* transformation coefficients */

    i_crv = MATRIX_COEFFICIENTS_TABLE[i_matrix_coefficients][0];
    i_cbu = MATRIX_COEFFICIENTS_TABLE[i_matrix_coefficients][1];
    i_cgu = MATRIX_COEFFICIENTS_TABLE[i_matrix_coefficients][2];
    i_cgv = MATRIX_COEFFICIENTS_TABLE[i_matrix_coefficients][3];
    p_red   = p_vout->tables.yuv.rgb32.p_red;
    p_green = p_vout->tables.yuv.rgb32.p_green;
    p_blue  = p_vout->tables.yuv.rgb32.p_blue;
    i_chroma_width =    i_width;
    i_chroma_eol =      i_eol;
    CONVERT_YUV_RGB( 444 )
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

#define SHIFT 20
#define U_GREEN_COEF ((int)(-0.391 * (1<<SHIFT) / 1.164))
#define U_BLUE_COEF ((int)(2.018 * (1<<SHIFT) / 1.164))
#define V_RED_COEF ((int)(1.596 * (1<<SHIFT) / 1.164))
#define V_GREEN_COEF ((int)(-0.813 * (1<<SHIFT) / 1.164))

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

