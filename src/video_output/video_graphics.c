/*******************************************************************************
 * video_graphics.c: pictures manipulation functions
 * (c)1999 VideoLAN
 *******************************************************************************
 * Includes function to compose, convert and display pictures, and also basic
 * functions to copy/convert pictures data or descriptors and read pictures
 * from a file.
 *******************************************************************************/

/*******************************************************************************
 * Preamble
 *******************************************************************************/

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>

#include "common.h"
#include "config.h"
#include "mtime.h"

#include "video.h"
#include "video_graphics.h"

#include "intf_msg.h"

/*
 * Local prototypes
 */
static int  ReadPictureConfiguration    ( int i_file, video_cfg_t *p_cfg );

/*******************************************************************************
 * video_CreatePicture: create an empty picture
 *******************************************************************************
 * This function create an empty image. A null pointer is returned if the
 * function fails. 
 * Following configuration properties are used:
 *  VIDEO_CFG_WIDTH     picture width (required)
 *  VIDEO_CFG_HEIGHT    picture height (required)
 *  VIDEO_CFG_TYPE      picture type (required)
 *  VIDEO_CFG_FLAGS     flags
 *  VIDEO_CFG_BPP       padded bpp (required for pixel pictures)
 *  VIDEO_CFG_POSITION  position in output window
 *  VIDEO_CFG_ALIGN     base position in output window
 *  VIDEO_CFG_RATIO     display ratio
 *  VIDEO_CFG_LEVEL     overlay hierarchical level  
 *  VIDEO_CFG_REFCOUNT  links reference counter
 *  VIDEO_CFG_STREAM    video stream id
 *  VIDEO_CFG_DATE      display date
 *  VIDEO_CFG_DURATION  duration for overlay pictures
 *  VIDEO_CFG_PIXEL     pixel value for mask pictures
 *  VIDEO_CFG_DATA      picture data (required if not owner and non blank)
 *******************************************************************************/
picture_t *video_CreatePicture( video_cfg_t *p_cfg )
{
    picture_t *p_newpic;                             /* new picture descriptor */

#ifdef DEBUG
    /* Check base configuration */
    if( (p_cfg->i_properties & (VIDEO_CFG_WIDTH | VIDEO_CFG_HEIGHT | VIDEO_CFG_TYPE))
        != (VIDEO_CFG_WIDTH | VIDEO_CFG_HEIGHT | VIDEO_CFG_TYPE) )
    {
        intf_DbgMsg("invalid picture configuration\n");
        return( NULL );
    }
    /* Check flags validity */
    if( (p_cfg->i_properties & VIDEO_CFG_FLAGS) 
        && (p_cfg->i_flags & ( RESERVED_PICTURE | DISPLAYED_PICTURE 
                               | DISPLAY_PICTURE | DESTROY_PICTURE )) )
    {
        intf_DbgMsg("invalid picture flags\n");
        return( NULL );
    }
#endif

    /* Allocate descriptor */
    p_newpic = malloc( sizeof(picture_t) );
    if( !p_newpic )                                  /* error: malloc() failed */
    {
        return( NULL );
    }
    
    /* Create picture */
    if( video_CreatePictureBody( p_newpic, p_cfg ) )                  /* error */
    {
        free( p_newpic );
        return( NULL );
    }

    /* Log and return */
#ifdef VOUT_DEBUG /* ?? -> video_debug ? */
    video_PrintPicture( p_newpic, "video: created picture " );
#else
    intf_DbgMsg("created picture %dx%d at %p\n", p_newpic->i_width, p_newpic->i_height, p_newpic);
#endif
    return( p_newpic );
}

/*******************************************************************************
 * video_CopyPicture: create a copy of a picture
 *******************************************************************************
 * This function creates a copy of a picture. It returns NULL on error.
 *******************************************************************************
 * Messages type: video, major code: 102
 *******************************************************************************/
picture_t * video_CopyPicture( picture_t *p_pic )
{
    picture_t * p_newpic;                            /* new picture descriptor */

    p_newpic = malloc( sizeof(picture_t) );
    if( p_newpic != NULL )
    {        
        video_CopyPictureDescriptor( p_newpic, p_pic );
        /* If source picture owns its data, make a copy */
        if( p_pic->i_flags & OWNER_PICTURE )
        {              
            p_newpic->p_data = malloc( p_pic->i_height * p_pic->i_bytes_per_line );
            if( p_newpic->p_data == NULL )                            /* error */
            {
                free( p_newpic );
                return( NULL );
            }
            memcpy( p_newpic->p_data, p_pic->p_data,
                    p_pic->i_height * p_pic->i_bytes_per_line );
        }
        /* If source picture does not owns its data, copy the reference */
        else              
        {
            p_newpic->p_data = p_pic->p_data;
        }
    }
    return( p_newpic );
}

/*******************************************************************************
 * video_ReplicatePicture: creates a replica of a picture
 *******************************************************************************
 * This function creates a replica of the original picture. It returns NULL on
 * error.
 *******************************************************************************
 * Messages type: video, major code: 103
 *******************************************************************************/
picture_t * video_ReplicatePicture( picture_t *p_pic )
{
    picture_t * p_newpic;                            /* new picture descrpitor */

    p_newpic = malloc( sizeof(picture_t) );
    if( p_newpic != NULL )
    {        
        video_CopyPictureDescriptor( p_newpic, p_pic );
        p_newpic->i_flags &= ~OWNER_PICTURE;
        p_newpic->p_data = p_pic->p_data;
    }
    return( p_newpic );
}

/*******************************************************************************
 * video_DestroyPicture: destroys a picture
 *******************************************************************************
 * This function destroy a picture created with any of the video_*Picture 
 * functions.
 *******************************************************************************
 * Messages type: video, major code: 104
 *******************************************************************************/
void video_DestroyPicture( picture_t *p_pic )
{    
    intf_DbgMsg("video debug 104-1: destroying picture %p\n", p_pic );
    
    /* Destroy data if picture owns it */
    if( p_pic->i_flags & OWNER_PICTURE )
    {
        free( p_pic->p_data );                                  
    }
    /* Destroy descriptor */
    free( p_pic );
}

/*******************************************************************************
 * video_ClearPicture: clear a picture
 *******************************************************************************
 * Set all pixel to 0 (black).
 *******************************************************************************
 * Messages type: video, major code: 105
 *******************************************************************************/
void video_ClearPicture( picture_t *p_pic )
{
    switch( p_pic->i_type )
    {
    case RGB_BLANK_PICTURE:
    case PIXEL_BLANK_PICTURE:
        /* Blank pictures will have their pixel value set to 0 */
        p_pic->pixel = 0;        
        break;
    default:
        /* All pictures types except blank ones have a meaningfull i_bytes_per_line
         * field, and blank ones don't need to be cleared. */
        memset( p_pic->p_data, 0, p_pic->i_height * p_pic->i_bytes_per_line );
        break;        
    }    
}

/*******************************************************************************
 * video_DrawPixel: set a pixel in a picture
 *******************************************************************************
 * This function set a pixel's value in an picture. It is an easy, but slow way
 * to render an image, and should be avoided. Exact meaning of value depends of
 * the picture type.
 *******************************************************************************
 * Messages type: video, major code: 106
 *******************************************************************************/
void video_DrawPixel( picture_t *p_pic, int i_x, int i_y, pixel_t value )
{
    switch( p_pic->i_type )
    {
    case RGB_PICTURE:                          /* 24 bits encoded RGB pictures */
        p_pic->p_data[i_y * p_pic->i_bytes_per_line + i_x * 3 ] =    RGBVALUE2RED( value );
        p_pic->p_data[i_y * p_pic->i_bytes_per_line + i_x * 3 + 1] = RGBVALUE2GREEN( value );
        p_pic->p_data[i_y * p_pic->i_bytes_per_line + i_x * 3 + 2] = RGBVALUE2BLUE( value );
        break;

    case PIXEL_PICTURE:                              /* pixel encoded pictures */
        /* ?? */
        break;

    case RGB_MASK_PICTURE:                          /* 1 bpp rgb mask pictures */
    case PIXEL_MASK_PICTURE:                      /* 1 bpp pixel mask pictures */
        /* ?? */
        break;
#ifdef DEBUG
    default:
        intf_DbgMsg("video debug 106-1: invalid picture %p type %d\n", 
                    p_pic, p_pic->i_type);
        break;
#endif
    }
}



/*******************************************************************************
 * video_DrawHLine: draw an horizontal line in a picture
 *******************************************************************************
 *
 *******************************************************************************
 * Messages type: video, major code: 107
 *******************************************************************************/
void video_DrawHLine( picture_t *p_pic, int i_x, int i_y, int i_width, pixel_t value )
{
    /* ?? */
}

/*******************************************************************************
 * video_DrawVLine: draw a vertical line in a picture
 *******************************************************************************
 *
 *******************************************************************************
 * Messages type: video, major code: 108
 *******************************************************************************/
void video_DrawVLine( picture_t *p_pic, int i_x, int i_y, int i_width, pixel_t value )
{
    /* ?? */
}

/*******************************************************************************
 * video_DrawLine: draw a line in a picture
 *******************************************************************************
 * video_DrawHLine() and video_DrawVLine() functions should be prefered if
 * possible.
 *******************************************************************************
 * Messages type: video, major code: 109
 *******************************************************************************/
void video_DrawLine( picture_t *p_pic, int i_x1, int i_y1, int i_x2, int i_y2, pixel_t value )
{
    /* ?? */
}

/*******************************************************************************
 * video_DrawBar: draw a bar in a picture
 *******************************************************************************
 * Draw a bar (filled rectangle) in a picture/
 *******************************************************************************
 * Messages type: video, major code: 110
 *******************************************************************************/
void video_DrawBar( picture_t *p_pic, int i_x, int i_y, int i_width, int i_height, 
                   pixel_t value )
{
    /* ?? */
}

/*******************************************************************************
 * video_DrawRectangle: draw a rectangle in a picture
 *******************************************************************************
 * Draw a rectangle (empty) in an image
 *******************************************************************************
 * Messages type: video, major code: 111
 *******************************************************************************/
void video_DrawRectangle( picture_t *p_pic, int i_x, int i_y, 
                         int i_width, int i_height, pixel_t value )
{
    /* ?? */
}

/*******************************************************************************
 * video_DrawImage: insert a picture in another one
 *******************************************************************************
 *  ??
 *******************************************************************************
 * Messages type: video, major code: 112
 *******************************************************************************/
void video_DrawPicture( picture_t *p_pic, picture_t *p_insert, int i_x, int i_y )
{
    /* ?? */
}

/*******************************************************************************
 * video_DrawText: insert text in a picture
 *******************************************************************************
 * This function prints a simple text in an image. It does not support 
 * different fonts, or complex text settings, but is designed to provide a
 * simple way to display messages, counters and indications.
 *******************************************************************************
 * Messages type: video, major code: 113
 *******************************************************************************/
void video_DrawText( picture_t *p_picture, int i_x, int i_y, char *psz_text, 
                    int i_size, pixel_t value )
{
    /* ?? */
}

/*******************************************************************************
 * video_CopyPictureDescriptor: copy a picture descriptor
 *******************************************************************************
 * This function is used when a picture is added to the video heap. It does not
 * copy the p_data field. Some post-treatment must obviously be done, especially
 * concerning the OWNER_PICTURE flag, the i_refcount and p_data fields.
 * Although it is exported, since it is used in several of the vout_* modules,
 * it should not be used directly outside the video output module.
 *******************************************************************************
 * Messages type: video, major code 114
 *******************************************************************************/
void video_CopyPictureDescriptor( picture_t *p_dest, picture_t *p_src )
{
    p_dest->i_type =            p_src->i_type;
    p_dest->i_flags =           p_src->i_flags;

    p_dest->i_width =           p_src->i_width;
    p_dest->i_height =          p_src->i_height;
    p_dest->i_bpp =             p_src->i_bpp;
    p_dest->i_bytes_per_line =  p_src->i_bytes_per_line;

    p_dest->i_x =               p_src->i_x;
    p_dest->i_y =               p_src->i_y;
    p_dest->i_h_align =         p_src->i_h_align;
    p_dest->i_v_align =         p_src->i_v_align;
    p_dest->i_h_ratio =         p_src->i_h_ratio;
    p_dest->i_v_ratio =         p_src->i_v_ratio;
    p_dest->i_level =           p_src->i_level;

    p_dest->i_refcount =        p_src->i_refcount;

    p_dest->i_stream =          p_src->i_stream;
    p_dest->date =              p_src->date;
    p_dest->duration =          p_src->duration;

    p_dest->pixel =             p_src->pixel;
}

/*******************************************************************************
 * video_CreatePictureBody: create a picture in an descriptor
 *******************************************************************************
 * This function is used vy vout_CreateReservedPicture and vout_CreatePicture.
 * It should not be called directly outside the video output module.
 * It returns non 0 if the creation failed.
 *******************************************************************************
 * Messages type: video, major code 115
 *******************************************************************************/
int video_CreatePictureBody( picture_t *p_pic, video_cfg_t *p_cfg )
{
#ifdef DEBUG
    /* Check base configuration */
    if( (p_cfg->i_properties & (VIDEO_CFG_WIDTH | VIDEO_CFG_HEIGHT | VIDEO_CFG_TYPE))
        != (VIDEO_CFG_WIDTH | VIDEO_CFG_HEIGHT | VIDEO_CFG_TYPE) )
    {
        intf_DbgMsg("video debug 115-1: invalid picture configuration\n");
        return( 1 );
    }
    /* Check flags validity */
    if( (p_cfg->i_properties & VIDEO_CFG_FLAGS) 
        && (p_cfg->i_flags & ( RESERVED_PICTURE | DISPLAYED_PICTURE 
                               | DISPLAY_PICTURE | DESTROY_PICTURE )) )
    {
        intf_DbgMsg("video debug 115-2: invalid picture flags\n");
        return( 1 );
    }
#endif

    /* Initialize types, flags and dimensions */
    p_pic->i_type =     p_cfg->i_type;
    p_pic->i_flags =    ( p_cfg->i_properties & VIDEO_CFG_FLAGS ) ? p_cfg->i_flags : 0;
    p_pic->i_width =    p_cfg->i_width;
    p_pic->i_height =   p_cfg->i_height;

    /* Initialize other pictures properties */
    if( p_cfg->i_properties & VIDEO_CFG_POSITION )
    {
        p_pic->i_x = p_cfg->i_x;
        p_pic->i_y = p_cfg->i_y;
    }
    else
    {
        p_pic->i_x = 0;
        p_pic->i_y = 0;
    }
    if( p_cfg->i_properties & VIDEO_CFG_ALIGN )
    {
        p_pic->i_h_align = p_cfg->i_h_align;
        p_pic->i_v_align = p_cfg->i_v_align;
    }
    else
    {
        p_pic->i_h_align = ALIGN_H_DEFAULT;
        p_pic->i_v_align = ALIGN_V_DEFAULT;
    }
    if( p_cfg->i_properties & VIDEO_CFG_RATIO )
    {
        p_pic->i_h_ratio = p_cfg->i_h_ratio;
        p_pic->i_v_ratio = p_cfg->i_v_ratio;
    }
    else
    {
        p_pic->i_h_ratio = DISPLAY_RATIO_NORMAL;
        p_pic->i_v_ratio = DISPLAY_RATIO_NORMAL;
    }
    p_pic->i_level =    ( p_cfg->i_properties & VIDEO_CFG_LEVEL ) 
        ? p_cfg->i_level : ( (p_pic->i_flags & OVERLAY_PICTURE) ? 1 : 0 );
    p_pic->i_refcount = ( p_cfg->i_properties & VIDEO_CFG_REFCOUNT ) ? p_cfg->i_refcount : 0;
    p_pic->i_stream =   ( p_cfg->i_properties & VIDEO_CFG_STREAM ) ? p_cfg->i_stream : 0;
    p_pic->date =       ( p_cfg->i_properties & VIDEO_CFG_DATE ) ? p_cfg->date : 0;
    p_pic->duration =   ( p_cfg->i_properties & VIDEO_CFG_DURATION ) ? p_cfg->duration : 0;

    /* Initialize type-dependant properties */
    switch( p_pic->i_type )
    {
    case RGB_BLANK_PICTURE:                               /* rgb blank picture */
    case PIXEL_BLANK_PICTURE:                           /* pixel blank picture */
#ifdef DEBUG
        /* Check validity: blank pictures can't be transparent or owner */
        if( p_pic->i_flags & ( OWNER_PICTURE | TRANSPARENT_PICTURE ) )
        {            
            intf_DbgMsg("video debug 115-3: invalid blank picture flags\n");   
            return( 1 );
        }
        /* Set to 0 unused fields */
        p_pic->i_bpp = 0;
        p_pic->i_bytes_per_line = 0;
        p_pic->p_data = NULL;
#endif
        /* Set color */
        p_pic->pixel = ( p_cfg->i_properties & VIDEO_CFG_PIXEL ) ? p_cfg->pixel : 0;
        break;

    case RGB_PICTURE:                                            /* 24 bpp RGB */
#ifdef DEBUG
        /* Set to 0 unused fields */
        p_pic->pixel = 0;
#endif
        /* Set fields */
        p_pic->i_bpp = 24;
        p_pic->i_bytes_per_line = PAD( p_pic->i_width * 3, sizeof(int) );
        break;

    case PIXEL_PICTURE:                               /* pixel encoded picture */
#ifdef DEBUG
        /* Check validity: pixel pictures must have the bpp property defined, 
         * and it must be a multiple of 8 */
        if( ! (p_cfg->i_properties & VIDEO_CFG_BPP) )
        {            
            intf_DbgMsg("video debug 115-4: missing bpp for pixel picture\n");   
            return( 1 );
        }
        else if( p_cfg->i_bpp % 8 )
        {
            intf_DbgMsg("video debug 115-5: invalid bpp for pixel picture\n");   
            return( 1 );       
        }

        /* Set to 0 unused fields */
        p_pic->pixel = 0;
#endif
        /* Set fields */
        p_pic->i_bpp = p_cfg->i_bpp;
        p_pic->i_bytes_per_line = PAD( p_pic->i_width * (p_cfg->i_bpp / 8), sizeof(int) );
        break;

    case RGB_MASK_PICTURE:                                   /* 1 bpp rgb mask */
    case PIXEL_MASK_PICTURE:                               /* 1 bpp pixel mask */        
        /* Set fields */
        p_pic->i_bpp = 1;
        p_pic->i_bytes_per_line = PAD( p_pic->i_width / 8, sizeof(int) );
        p_pic->pixel = ( p_cfg->i_properties & VIDEO_CFG_PIXEL ) ? p_cfg->pixel : 0;
        break;

#ifdef DEBUG
    default:                                    /* error: unknown picture type */
        intf_DbgMsg("video debug 115-6: unknown picture type\n");
        return( 1 );
        break;
#endif
    }

    /* If picture is not blank, the p_data has some meaning */
    if( (p_pic->i_type != RGB_BLANK_PICTURE) 
        && (p_pic->i_type != PIXEL_BLANK_PICTURE) )
    {
        /* If picture owns its data, allocate p_data */
        if( p_pic->i_flags & OWNER_PICTURE )
        {
            p_pic->p_data = malloc( p_pic->i_height * p_pic->i_bytes_per_line );
            if( p_pic->p_data == NULL )                               /* error */
            {
                return( 1 );
            }
        }
        /* If picture is a reference, copy pointer */
        else
        {
#ifdef DEBUG
            /* Check configuration */
            if( !(p_cfg->i_properties & VIDEO_CFG_DATA) )
            {
                intf_DbgMsg("video debug 115-7: missing picture data\n");
                return( 1 );
            }
#endif
            p_pic->p_data = p_cfg->p_data;
        }
    }
   
    return( 0 );
}

/*******************************************************************************
 * video_ReadPicture: read a picture from a file
 *******************************************************************************
 * This function reads a picture from an openned file. The picture must be
 * stored in native format, ie: header, according to ReadPictureConfiguration
 * function specifications, then raw data.
 *******************************************************************************
 * Messages type: video, major code 116
 *******************************************************************************/
picture_t * video_ReadPicture( int i_file )
{
    video_cfg_t     p_cfg;                        /* new picture configuration */    
    picture_t *     p_newpic;                        /* new picture descriptor */
    
    /* Read picture configuration */
    if( ReadPictureConfiguration( i_file, &p_cfg ) )
    {
        return( NULL );
    }
    
    /* Create picture and allocate data */
    p_newpic = video_CreatePicture( &p_cfg );    
    if( p_newpic == NULL ) 
    {
        return( NULL );        
    }
    
    /* Read data if required */
    if( (p_newpic->i_type != RGB_BLANK_PICTURE) && (p_newpic->i_type != RGB_BLANK_PICTURE) )
    {
        if( read( i_file, p_newpic->p_data, p_newpic->i_height * p_newpic->i_bytes_per_line ) 
            != p_newpic->i_height * p_newpic->i_bytes_per_line )
        {
            intf_ErrMsg("video error 116-2: %s\n", strerror(errno));
            video_DestroyPicture( p_newpic );
            return( NULL );            
        }
        
    }

    /* Log and return */
#ifdef VOUT_DEBUG /* ?? -> video_debug ? */
    video_PrintPicture( p_newpic, "video debug 116-1: read picture " );
#endif
    return( p_newpic );
}

/* following functions are debugging functions */

/*******************************************************************************
 * video_PrintPicture: display picture state (debugging function)
 *******************************************************************************
 * This function, which is only defined if DEBUG is defined, can be used for
 * debugging purposes. It prints on debug stream the main characteristics of
 * a picture. The second parameter is printed in front of data. Note that no
 * header is printed by default.
 *******************************************************************************
 * Messages type: video, major code 141
 *******************************************************************************/
#ifdef DEBUG
void video_PrintPicture( picture_t *p_pic, char *psz_str )
{
    char *      psz_type;                                       /* type string */
    char        sz_flags[9];                                   /* flags string */
    char        sz_date[MSTRTIME_MAX_SIZE];                     /* date buffer */
    char        sz_duration[MSTRTIME_MAX_SIZE];             /* duration buffer */

    /* Non empty picture */
    if( p_pic->i_type != EMPTY_PICTURE )
    {
        /* Build picture type information string */
        switch( p_pic->i_type )
        {
        case RGB_BLANK_PICTURE:
            psz_type = "rgb-blank";
            break;
        case PIXEL_BLANK_PICTURE:
            psz_type = "pixel-blank";
            break;
        case RGB_PICTURE:
            psz_type = "rgb";
            break;
        case PIXEL_PICTURE:
            psz_type = "pixel";
            break;
        case RGB_MASK_PICTURE:
            psz_type = "rgb mask";
            break;
        case PIXEL_MASK_PICTURE:
            psz_type = "pixel mask";
            break;            
        default:
            psz_type = "?";
            break;                
        }

        /* Build flags information strings */
        sz_flags[0] = ( p_pic->i_flags & RESERVED_PICTURE ) ?       'r' : '-';
        sz_flags[1] = ( p_pic->i_flags & PERMANENT_PICTURE ) ?      'p' : '-';
        sz_flags[2] = ( p_pic->i_flags & DISPLAYED_PICTURE ) ?      'D' : '-';
        sz_flags[3] = ( p_pic->i_flags & OWNER_PICTURE ) ?          'O' : '-';
        sz_flags[4] = ( p_pic->i_flags & DISPLAY_PICTURE ) ?        'd' : '-';
        sz_flags[5] = ( p_pic->i_flags & DESTROY_PICTURE ) ?        'x' : '-';
        sz_flags[6] = ( p_pic->i_flags & TRANSPARENT_PICTURE ) ?    't' : '-';
        sz_flags[7] = ( p_pic->i_flags & OVERLAY_PICTURE ) ?        'o' : '-';
        sz_flags[8] = '\0';

        /* Print information strings */
        intf_DbgMsg("%s%p: %s (%s) %dx%d-%d.%d s=%d rc=%d d=%s t=%s\n",
                    psz_str, p_pic,
                    psz_type, sz_flags, 
                    p_pic->i_width, p_pic->i_height, p_pic->i_bpp, p_pic->i_bytes_per_line, 
                    p_pic->i_stream,
                    p_pic->i_refcount, 
                    mstrtime( sz_date, p_pic->date ), mstrtime( sz_duration, p_pic->duration) );
    }
    /* Empty picture */
    else
    {
        intf_DbgMsg("%sempty\n", psz_str);
    }
}   
#endif

/* following functions are local */

/*******************************************************************************
 * ReadPictureConfiguration: read a vout_cfg_t structure from a file
 *******************************************************************************
 * This function reads a picture configuration and properties from a file. Note
 * that some fields of the vout_cft_t, which do not concern pictures, are
 * ignored. Note that the file is in MSB (little-endian).
 *******************************************************************************
 * Messages type: video, major code 151
 *******************************************************************************/
static int ReadPictureConfiguration( int i_file, video_cfg_t *p_cfg )
{
    byte_t  p_buffer[42];         /* buffer used to store the packed structure */

    /* Read buffer */
    if( read( i_file, p_buffer, 42 ) != 42 )
    {
        intf_ErrMsg("video error 151-1: %s\n", strerror(errno) );        
        return( errno );
    }

    /* Parse buffer */
    p_cfg->i_properties =   ntoh32( *(u32 *)( p_buffer ) );
    p_cfg->i_type =         *(u8 *)( p_buffer + 4 );
    p_cfg->i_bpp=           *(u8 *)( p_buffer + 5 );
    p_cfg->i_width =        ntoh16( *(u16 *)( p_buffer + 6 ) );
    p_cfg->i_height =       ntoh16( *(u16 *)( p_buffer + 8 ) );
    p_cfg->i_flags =        ntoh16( *(u16 *)( p_buffer + 10 ) );
    p_cfg->i_x =            ntoh16( *(s16 *)( p_buffer + 12 ) );
    p_cfg->i_y =            ntoh16( *(s16 *)( p_buffer + 14 ) );
    p_cfg->i_h_align =      *(s8 *)( p_buffer + 16 );
    p_cfg->i_v_align =      *(s8 *)( p_buffer + 17 );
    p_cfg->i_h_ratio =      *(s8 *)( p_buffer + 19 );
    p_cfg->i_v_ratio =      *(s8 *)( p_buffer + 19 );
    p_cfg->i_level  =       *(s8 *)( p_buffer + 20 );
    p_cfg->i_stream =       *(u8 *)( p_buffer + 21 );
    p_cfg->date =           ntoh64( *(u64 *)( p_buffer + 22 ) );
    p_cfg->duration =       ntoh64( *(u64 *)( p_buffer + 30 ) );
    p_cfg->pixel =          ntoh32( *(u32 *)( p_buffer + 38 ) );
   
    return( 0 );
}


