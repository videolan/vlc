/*****************************************************************************
 * modules.h : Module management functions.
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

/* Number of tries before we unload an unused module */
#define MODULE_HIDE_DELAY 20

/* The module handle type. */
#ifdef SYS_BEOS
typedef int     module_handle_t;
#else
typedef void *  module_handle_t;
#endif

/*****************************************************************************
 * Module capabilities.
 *****************************************************************************/

#define MODULE_CAPABILITY_NULL     0        /* The Module can't do anything */
#define MODULE_CAPABILITY_INTF     1 <<  0  /* Interface */
#define MODULE_CAPABILITY_INPUT    1 <<  1  /* Input */
#define MODULE_CAPABILITY_DECAPS   1 <<  2  /* Decaps */
#define MODULE_CAPABILITY_ADEC     1 <<  3  /* Audio decoder */
#define MODULE_CAPABILITY_VDEC     1 <<  4  /* Video decoder */
#define MODULE_CAPABILITY_MOTION   1 <<  5  /* Video decoder */
#define MODULE_CAPABILITY_IDCT     1 <<  6  /* IDCT transformation */
#define MODULE_CAPABILITY_AOUT     1 <<  7  /* Audio output */
#define MODULE_CAPABILITY_VOUT     1 <<  8  /* Video output */
#define MODULE_CAPABILITY_YUV      1 <<  9  /* YUV colorspace conversion */
#define MODULE_CAPABILITY_AFX      1 << 10  /* Audio effects */
#define MODULE_CAPABILITY_VFX      1 << 11  /* Video effects */

/* FIXME: not yet used */
typedef struct probedata_s
{
    struct
    {
        char * psz_data;
    } aout;
} probedata_t;

/* FIXME: find a nicer way to do this. */
typedef struct function_list_s
{
    int ( * pf_probe ) ( probedata_t * p_data );

    union
    {
        struct
        {
            int  ( * pf_open )       ( struct aout_thread_s * p_aout );
            int  ( * pf_setformat )  ( struct aout_thread_s * p_aout );
            long ( * pf_getbufinfo ) ( struct aout_thread_s * p_aout,
                                       long l_buffer_info );
            void ( * pf_play )       ( struct aout_thread_s * p_aout,
                                       byte_t *buffer, int i_size );
            void ( * pf_close )      ( struct aout_thread_s * p_aout );
        } aout;

        struct
        {
#define motion_functions( yuv ) \
            void ( * pf_field_field_##yuv ) ( struct macroblock_s * ); \
            void ( * pf_field_16x8_##yuv )  ( struct macroblock_s * ); \
            void ( * pf_field_dmv_##yuv )   ( struct macroblock_s * ); \
            void ( * pf_frame_field_##yuv ) ( struct macroblock_s * ); \
            void ( * pf_frame_frame_##yuv ) ( struct macroblock_s * ); \
            void ( * pf_frame_dmv_##yuv )   ( struct macroblock_s * );
	    motion_functions( 420 )
	    motion_functions( 422 )
	    motion_functions( 444 )
#undef motion_functions
        } motion;

        struct
        {
            void ( * pf_init )         ( struct vdec_thread_s * p_vdec );
            void ( * pf_sparse_idct )  ( struct vdec_thread_s * p_vdec,
                                         dctelem_t * p_block,
                                         int i_sparse_pos );
            void ( * pf_idct )         ( struct vdec_thread_s * p_vdec,
                                         dctelem_t * p_block,
                                         int i_idontcare );
            void ( * pf_norm_scan )    ( u8 ppi_scan[2][64] );
        } idct;

        struct
        {
            int  ( * pf_init )         ( struct vout_thread_s * p_vout );
            int  ( * pf_reset )        ( struct vout_thread_s * p_vout );
            void ( * pf_end )          ( struct vout_thread_s * p_vout );
        } yuv;

    } functions;

} function_list_t;

typedef struct module_functions_s
{
    /* XXX: The order here has to be the same as above for the #defines */
    function_list_t intf;
    function_list_t input;
    function_list_t decaps;
    function_list_t adec;
    function_list_t vdec;
    function_list_t motion;
    function_list_t idct;
    function_list_t aout;
    function_list_t vout;
    function_list_t yuv;
    function_list_t afx;
    function_list_t vfx;

} module_functions_t;

typedef struct module_functions_s * p_module_functions_t;

/*****************************************************************************
 * Macros used to build the configuration structure.
 *****************************************************************************/

/* Mandatory first and last parts of the structure */
#define MODULE_CONFIG_ITEM_START       0xdead  /* The main window */
#define MODULE_CONFIG_ITEM_END         0xbeef  /* End of the window */

/* Configuration widgets */
#define MODULE_CONFIG_ITEM_WINDOW      0x0001  /* The main window */
#define MODULE_CONFIG_ITEM_PANE        0x0002  /* A notebook pane */
#define MODULE_CONFIG_ITEM_FRAME       0x0003  /* A frame */
#define MODULE_CONFIG_ITEM_COMMENT     0x0004  /* A comment text */
#define MODULE_CONFIG_ITEM_STRING      0x0005  /* A string */
#define MODULE_CONFIG_ITEM_FILE        0x0006  /* A file selector */
#define MODULE_CONFIG_ITEM_CHECK       0x0007  /* A checkbox */
#define MODULE_CONFIG_ITEM_CHOOSE      0x0008  /* A choose box */
#define MODULE_CONFIG_ITEM_RADIO       0x0009  /* A radio box */
#define MODULE_CONFIG_ITEM_SCALE       0x000a  /* A horizontal ruler */
#define MODULE_CONFIG_ITEM_SPIN        0x000b  /* A numerical selector */

typedef struct module_config_s
{
    int         i_type;                         /* Configuration widget type */
    char *      psz_text;        /* Text commenting or describing the widget */
    char *      psz_name;                                   /* Variable name */
    void *      p_getlist;          /* Function to call to get a choice list */
    void *      p_change;        /* Function to call when commiting a change */
} module_config_t;

/*****************************************************************************
 * Bank and module description structures
 *****************************************************************************/

/* The module bank structure */
typedef struct module_bank_s
{
    struct module_s *   first; /* First module of the bank */

    vlc_mutex_t         lock;  /* Global lock -- you can't imagine how awful it
                                  is to design thread-safe linked lists. */
} module_bank_t;

/* The module description structure */
typedef struct module_s
{
    boolean_t           b_builtin;  /* Set to true if the module is built in */

    module_handle_t     handle;      /* Unique handle to refer to the module */
    char *              psz_filename;                     /* Module filename */

    char *              psz_name;                    /* Module _unique_ name */
    char *              psz_longname;             /* Module descriptive name */
    char *              psz_version;                       /* Module version */

    int                 i_usage;                        /* Reference counter */
    int                 i_unused_delay;    /* Delay until module is unloaded */

    struct module_s *   next;                                 /* Next module */
    struct module_s *   prev;                             /* Previous module */

    module_config_t *   p_config;    /* Module configuration structure table */

    u32                     i_capabilities;               /* Capability list */
    p_module_functions_t    p_functions;             /* Capability functions */

} module_t;

/*****************************************************************************
 * Exported functions.
 *****************************************************************************/
module_bank_t * module_CreateBank   ( void );
void            module_InitBank     ( module_bank_t * p_bank );
void            module_DestroyBank  ( module_bank_t * p_bank );
void            module_ResetBank    ( module_bank_t * p_bank );
void            module_ManageBank   ( module_bank_t * p_bank );

module_t *      module_Need         ( module_bank_t *p_bank,
                                      int i_capabilities, void *p_data );
void            module_Unneed       ( module_bank_t * p_bank,
                                      module_t * p_module );

