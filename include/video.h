/*****************************************************************************
 * video.h: common video definitions
 * This header is required by all modules which have to handle pictures. It
 * includes all common video types and constants.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: video.h,v 1.37 2001/12/31 04:53:33 sam Exp $
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
typedef u8 pixel_data_t;

typedef struct plane_s
{
    pixel_data_t *p_data;                       /* Start of the plane's data */

    /* Variables used for fast memcpy operations */
    int i_bytes;                       /* Total number of bytes in the plane */
    int i_line_bytes;                     /* Total number of bytes in a line */

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
    /* Picture data - data can always be freely modified, but no pointer
     * may EVER be modified. A direct buffer can be handled as the plugin
     * wishes, but for internal video output pictures the allocated pointer
     * MUST be planes[0].p_data */
    plane_t         planes[ VOUT_MAX_PLANES ];  /* description of the planes */
    int             i_planes;                  /* number of allocated planes */

    /* Type and flags - should NOT be modified except by the vout thread */
    int             i_status;                               /* picture flags */
    int             i_type;                  /* is picture a direct buffer ? */
    int             i_matrix_coefficients;     /* in YUV type, encoding type */

    /* Picture management properties - these properties can be modified using
     * the video output thread API, but should never be written directly */
    int             i_refcount;                    /* link reference counter */
    mtime_t         date;                                    /* display date */

    /* Picture margins - needed because of possible padding issues */
    int             i_left_margin;
    int             i_right_margin;
    int             i_top_margin;
    int             i_bottom_margin;

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
    int             i_chroma;                              /* picture chroma */
    int             i_aspect;                                /* aspect ratio */

    /* Real pictures */
    picture_t*      pp_picture[VOUT_MAX_PICTURES];               /* pictures */

} picture_heap_t;

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

/* Picture chroma */
#define EMPTY_PICTURE           0     /* picture slot is empty and available */
#define YUV_420_PICTURE         100                     /* 4:2:0 YUV picture */
#define YUV_422_PICTURE         101                     /* 4:2:2 YUV picture */
#define YUV_444_PICTURE         102                     /* 4:4:4 YUV picture */
#define RGB_8BPP_PICTURE        200                      /* RGB 8bpp picture */
#define RGB_16BPP_PICTURE       201                     /* RGB 16bpp picture */
#define RGB_32BPP_PICTURE       202                     /* RGB 32bpp picture */

/* Aspect ratio (ISO/IEC 13818-2 section 6.3.3, table 6-3) */
#define AR_SQUARE_PICTURE       1                           /* square pixels */
#define AR_3_4_PICTURE          2                        /* 3:4 picture (TV) */
#define AR_16_9_PICTURE         3              /* 16:9 picture (wide screen) */
#define AR_221_1_PICTURE        4                  /* 2.21:1 picture (movie) */

/* Plane indices */
#define YUV_PLANE               0
#define RGB_PLANE               0
#define Y_PLANE                 0
#define U_PLANE                 1
#define V_PLANE                 2
#define Cb_PLANE                1
#define Cr_PLANE                2
#define R_PLANE                 0
#define G_PLANE                 1
#define B_PLANE                 2

/* Shortcuts */
#define P_Y planes[ Y_PLANE ].p_data
#define P_U planes[ U_PLANE ].p_data
#define P_V planes[ V_PLANE ].p_data

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


