/*****************************************************************************
 * video.h: common video definitions
 * This header is required by all modules which have to handle pictures. It
 * includes all common video types and constants.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: video.h,v 1.42 2002/02/08 15:57:29 sam Exp $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * plane_t: description of a planar graphic field
 *****************************************************************************/
typedef struct plane_s
{
    u8 *p_pixels;                               /* Start of the plane's data */

    /* Variables used for fast memcpy operations */
    int i_lines;                                          /* Number of lines */
    int i_pitch;             /* Number of bytes in a line, including margins */

    /* Size of a macropixel, defaults to 1 */
    int i_pixel_bytes;

    /* Is there a margin ? defaults to no */
    boolean_t b_margin;

    /* Variables used for pictures with margins */
    int i_visible_bytes;                 /* How many real pixels are there ? */
    boolean_t b_hidden;           /* Are we allowed to write to the margin ? */

    /* Variables used for RGB planes */
    int i_red_mask;
    int i_green_mask;
    int i_blue_mask;

} plane_t;

/*****************************************************************************
 * picture_t: video picture
 *****************************************************************************
 * Any picture destined to be displayed by a video output thread should be
 * stored in this structure from it's creation to it's effective display.
 * Picture type and flags should only be modified by the output thread. Note
 * that an empty picture MUST have its flags set to 0.
 *****************************************************************************/
typedef struct picture_s
{
    /* Picture data - data can always be freely modified, but p_data may
     * NEVER be modified. A direct buffer can be handled as the plugin
     * wishes, it can even swap p_pixels buffers. */
    u8             *p_data;
    plane_t         p[ VOUT_MAX_PLANES ];       /* description of the planes */
    int             i_planes;                  /* number of allocated planes */

    /* Type and flags - should NOT be modified except by the vout thread */
    int             i_status;                               /* picture flags */
    int             i_type;                  /* is picture a direct buffer ? */
    int             i_matrix_coefficients;     /* in YUV type, encoding type */

    /* Picture management properties - these properties can be modified using
     * the video output thread API, but should never be written directly */
    int             i_refcount;                    /* link reference counter */
    mtime_t         date;                                    /* display date */

    /* Picture dynamic properties - those properties can be changed by the
     * decoder */
    boolean_t       b_progressive;            /* is it a progressive frame ? */
    boolean_t       b_repeat_first_field;                         /* RFF bit */
    boolean_t       b_top_field_first;               /* which field is first */

    /* Macroblock counter - the decoder uses it to verify if it has
     * decoded all the macroblocks of the picture */
    int             i_deccount;
    vlc_mutex_t     lock_deccount;

    /* Private data - the video output plugin might want to put stuff here to
     * keep track of the picture */
    struct picture_sys_s *p_sys;

} picture_t;

/*****************************************************************************
 * picture_heap_t: video picture heap, either render (to store pictures used
 * by the decoder) or output (to store pictures displayed by the vout plugin)
 *****************************************************************************/
typedef struct picture_heap_s
{
    int             i_pictures;                         /* current heap size */

    /* Picture static properties - those properties are fixed at initialization
     * and should NOT be modified */
    int             i_width;                                /* picture width */
    int             i_height;                              /* picture height */
    u32             i_chroma;                              /* picture chroma */
    int             i_aspect;                                /* aspect ratio */

    /* Real pictures */
    picture_t*      pp_picture[VOUT_MAX_PICTURES];               /* pictures */

} picture_heap_t;

/*****************************************************************************
 * Flags used to describe the status of a picture
 *****************************************************************************/

/* Picture type */
#define EMPTY_PICTURE           0                            /* empty buffer */
#define MEMORY_PICTURE          100                 /* heap-allocated buffer */
#define DIRECT_PICTURE          200                         /* direct buffer */

/* Picture status */
#define FREE_PICTURE            0                  /* free and not allocated */
#define RESERVED_PICTURE        1                  /* allocated and reserved */
#define RESERVED_DATED_PICTURE  2              /* waiting for DisplayPicture */
#define RESERVED_DISP_PICTURE   3               /* waiting for a DatePicture */
#define READY_PICTURE           4                       /* ready for display */
#define DISPLAYED_PICTURE       5            /* been displayed but is linked */
#define DESTROYED_PICTURE       6              /* allocated but no more used */

/*****************************************************************************
 * Flags used to describe picture format - see http://www.webartz.com/fourcc/
 *****************************************************************************/

/* Packed RGB formats */
#define FOURCC_BI_RGB        0x00000000                      /* RGB for 8bpp */
#define FOURCC_RGB           0x32424752                  /* alias for BI_RGB */
#define FOURCC_BI_BITFIELDS  0x00000003            /* RGB, for 16, 24, 32bpp */
#define FOURCC_RV15          0x35315652    /* RGB 15bpp, 0x1f, 0x7e0, 0xf800 */
#define FOURCC_RV16          0x36315652    /* RGB 16bpp, 0x1f, 0x3e0, 0x7c00 */
#define FOURCC_RV32          0x32335652 /* RGB 32bpp, 0xff, 0xff00, 0xff0000 */

/* Planar YUV formats */
#define FOURCC_I420          0x30323449               /* Planar 4:2:0, Y:U:V */
#define FOURCC_IYUV          0x56555949                    /* alias for I420 */
#define FOURCC_YV12          0x32315659               /* Planar 4:2:0, Y:V:U */

/* Packed YUV formats */
#define FOURCC_IUYV          0x56595549 /* Packed 4:2:2, U:Y:V:Y, interlaced */
#define FOURCC_UYVY          0x59565955             /* Packed 4:2:2, U:Y:V:Y */
#define FOURCC_UYNV          0x564e5955                    /* alias for UYVY */
#define FOURCC_Y422          0x32323459                    /* alias for UYVY */
#define FOURCC_cyuv          0x76757963   /* Packed 4:2:2, U:Y:V:Y, reverted */
#define FOURCC_YUY2          0x32595559             /* Packed 4:2:2, Y:U:Y:V */
#define FOURCC_YUNV          0x564e5559                    /* alias for YUY2 */
#define FOURCC_YVYU          0x55585659             /* Packed 4:2:2, Y:V:Y:U */
#define FOURCC_Y211          0x31313259             /* Packed 2:1:1, Y:U:Y:V */

/* Custom formats which we use but which don't exist in the fourcc database */
#define FOURCC_YMGA          0x41474d59  /* Planar Y, packed UV, from Matrox */
#define FOURCC_I422          0x32323449               /* Planar 4:2:2, Y:U:V */
#define FOURCC_I444          0x34343449               /* Planar 4:4:4, Y:U:V */

/* Plane indices */
#define Y_PLANE      0
#define U_PLANE      1
#define V_PLANE      2

/* Shortcuts */
#define Y_PIXELS     p[Y_PLANE].p_pixels
#define U_PIXELS     p[U_PLANE].p_pixels
#define V_PIXELS     p[V_PLANE].p_pixels

static __inline__ int vout_ChromaCmp( u32 i_chroma, u32 i_amorhc )
{
    /* If they are the same, they are the same ! */
    if( i_chroma == i_amorhc )
    {
        return 1;
    }

    /* Check for equivalence classes */
    switch( i_chroma )
    {
        case FOURCC_I420:
        case FOURCC_IYUV:
        case FOURCC_YV12:
            switch( i_amorhc )
            {
                case FOURCC_I420:
                case FOURCC_IYUV:
                case FOURCC_YV12:
                    return 1;

                default:
                    return 0;
            }

        case FOURCC_UYVY:
        case FOURCC_UYNV:
        case FOURCC_Y422:
            switch( i_amorhc )
            {
                case FOURCC_UYVY:
                case FOURCC_UYNV:
                case FOURCC_Y422:
                    return 1;

                default:
                    return 0;
            }

        case FOURCC_YUY2:
        case FOURCC_YUNV:
            switch( i_amorhc )
            {
                case FOURCC_YUY2:
                case FOURCC_YUNV:
                    return 1;

                default:
                    return 0;
            }

        default:
            return 0;
    }
}

/*****************************************************************************
 * vout_CopyPicture: copy a picture to another one
 *****************************************************************************
 * This function takes advantage of the image format, and reduces the
 * number of calls to memcpy() to the minimum. Source and destination
 * images must have same width, height, and chroma.
 *****************************************************************************/
static __inline__ void vout_CopyPicture( picture_t *p_src, picture_t *p_dest )
{
    int i;

    for( i = 0; i < p_src->i_planes ; i++ )
    {
        if( p_src->p[i].i_pitch == p_dest->p[i].i_pitch )
        {
            if( p_src->p[i].b_margin )
            {
                /* If p_src->b_margin is set, p_dest->b_margin must be set */
                if( p_dest->p[i].b_hidden )
                {
                    /* There are margins, but they are hidden : perfect ! */
                    FAST_MEMCPY( p_dest->p[i].p_pixels, p_src->p[i].p_pixels,
                                 p_src->p[i].i_pitch * p_src->p[i].i_lines );
                    continue;
                }
                else
                {
                    /* We can't directly copy the margin. Too bad. */
                }
            }
            else
            {
                /* Same pitch, no margins : perfect ! */
                FAST_MEMCPY( p_dest->p[i].p_pixels, p_src->p[i].p_pixels,
                             p_src->p[i].i_pitch * p_src->p[i].i_lines );
                continue;
            }
        }
        else
        {
            /* Pitch values are different */
        }

        /* We need to proceed line by line */
        {
            u8 *p_in = p_src->p[i].p_pixels, *p_out = p_dest->p[i].p_pixels;
            int i_line;

            for( i_line = p_src->p[i].i_lines; i_line--; )
            {
                FAST_MEMCPY( p_out, p_in, p_src->p[i].i_visible_bytes );
                p_in += p_src->p[i].i_pitch;
                p_out += p_dest->p[i].i_pitch;
            }
        }
    }
}

/*****************************************************************************
 * subpicture_t: video subtitle
 *****************************************************************************
 * Any subtitle destined to be displayed by a video output thread should
 * be stored in this structure from it's creation to it's effective display.
 * Subtitle type and flags should only be modified by the output thread. Note
 * that an empty subtitle MUST have its flags set to 0.
 *****************************************************************************/
typedef struct subpicture_s
{
    /* Type and flags - should NOT be modified except by the vout thread */
    int             i_type;                                          /* type */
    int             i_status;                                       /* flags */
    int             i_size;                                     /* data size */
    struct subpicture_s *   p_next;         /* next subtitle to be displayed */

    /* Date properties */
    mtime_t         i_start;                    /* beginning of display date */
    mtime_t         i_stop;                           /* end of display date */
    boolean_t       b_ephemer;             /* does the subtitle have a TTL ? */

    /* Display properties - these properties are only indicative and may be
     * changed by the video output thread, or simply ignored depending of the
     * subtitle type. */
    int             i_x;                   /* offset from alignment position */
    int             i_y;                   /* offset from alignment position */
    int             i_width;                                /* picture width */
    int             i_height;                              /* picture height */
    int             i_horizontal_align;              /* horizontal alignment */
    int             i_vertical_align;                  /* vertical alignment */

    /* Additionnal properties depending of the subpicture type */
    union
    {
        /* Text subpictures properties - text is stored in data area, in ASCIIZ
         * format */
        struct
        {
            p_vout_font_t       p_font;            /* font, NULL for default */
            int                 i_style;                       /* text style */
            u32                 i_char_color;             /* character color */
            u32                 i_border_color;              /* border color */
            u32                 i_bg_color;              /* background color */
        } text;
        /* DVD subpicture units properties */
        struct
        {
            int                 i_offset[2];         /* byte offsets to data */
        } spu;
    } type;

    /* Subpicture data, format depends of type - data can always be freely
     * modified. p_data itself (the pointer) should NEVER be modified. */
    void *          p_data;                               /* subpicture data */
} subpicture_t;

/* Subpicture type */
#define EMPTY_SUBPICTURE       0     /* subtitle slot is empty and available */
#define DVD_SUBPICTURE         100                    /* DVD subpicture unit */
#define TEXT_SUBPICTURE        200                       /* single line text */

/* Subpicture status */
#define FREE_SUBPICTURE        0                   /* free and not allocated */
#define RESERVED_SUBPICTURE    1                   /* allocated and reserved */
#define READY_SUBPICTURE       2                        /* ready for display */
#define DESTROYED_SUBPICTURE   3           /* allocated but not used anymore */

/* Alignment types */
#define RIGHT_ALIGN            10                 /* x is absolute for right */
#define LEFT_ALIGN             11                  /* x is absolute for left */
#define RIGHT_RALIGN           12      /* x is relative for right from right */
#define LEFT_RALIGN            13        /* x is relative for left from left */

#define CENTER_ALIGN           20            /* x, y are absolute for center */
#define CENTER_RALIGN          21 /* x,y are relative for center from center */

#define BOTTOM_ALIGN           30                /* y is absolute for bottom */
#define TOP_ALIGN              31                   /* y is absolute for top */
#define BOTTOM_RALIGN          32    /* y is relative for bottom from bottom */
#define TOP_RALIGN             33          /* y is relative for top from top */
#define SUBTITLE_RALIGN        34  /* y is relative for center from subtitle */


