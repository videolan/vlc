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
#define MODULE_HIDE_DELAY 50

/* The module handle type. */
#ifdef SYS_BEOS
typedef int     module_handle_t;
#else
typedef void *  module_handle_t;
#endif

/*****************************************************************************
 * Module capabilities.
 *****************************************************************************/

#define MODULE_CAPABILITY_NULL     0       /* The Module can't do anything */
#define MODULE_CAPABILITY_INTF     1<<0    /* Interface */
#define MODULE_CAPABILITY_INPUT    1<<1    /* Input */
#define MODULE_CAPABILITY_DECAPS   1<<2    /* Decaps */
#define MODULE_CAPABILITY_ADEC     1<<3    /* Audio decoder */
#define MODULE_CAPABILITY_VDEC     1<<4    /* Video decoder */
#define MODULE_CAPABILITY_AOUT     1<<5    /* Audio output */
#define MODULE_CAPABILITY_VOUT     1<<6    /* Video output */
#define MODULE_CAPABILITY_YUV      1<<7    /* YUV colorspace conversion */
#define MODULE_CAPABILITY_AFX      1<<8    /* Audio effects */
#define MODULE_CAPABILITY_VFX      1<<9    /* Video effects */

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
    int ( * p_probe ) ( probedata_t * p_data );

    union
    {
        struct
        {
            int  ( * p_open )       ( struct aout_thread_s * p_aout );
            int  ( * p_setformat )  ( struct aout_thread_s * p_aout );
            long ( * p_getbufinfo ) ( struct aout_thread_s * p_aout,
                                      long l_buffer_info );
            void ( * p_play )       ( struct aout_thread_s * p_aout,
                                      byte_t *buffer, int i_size );
            void ( * p_close )      ( struct aout_thread_s * p_aout );
	} aout;

    } functions;

} function_list_t;

typedef struct module_functions_s
{
    /* The order here has to be the same as above for the #defines */
    function_list_t intf;
    function_list_t input;
    function_list_t decaps;
    function_list_t adec;
    function_list_t vdec;
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
#define MODULE_CONFIG_ITEM_START       0x01    /* The main window */
#define MODULE_CONFIG_ITEM_END         0x00    /* End of the window */

/* Configuration widgets */
#define MODULE_CONFIG_ITEM_PANE        0x02    /* A notebook pane */
#define MODULE_CONFIG_ITEM_FRAME       0x03    /* A frame */
#define MODULE_CONFIG_ITEM_COMMENT     0x04    /* A comment text */
#define MODULE_CONFIG_ITEM_STRING      0x05    /* A string */
#define MODULE_CONFIG_ITEM_FILE        0x06    /* A file selector */
#define MODULE_CONFIG_ITEM_CHECK       0x07    /* A checkbox */
#define MODULE_CONFIG_ITEM_CHOOSE      0x08    /* A choose box */
#define MODULE_CONFIG_ITEM_RADIO       0x09    /* A radio box */
#define MODULE_CONFIG_ITEM_SCALE       0x0a    /* A horizontal ruler */
#define MODULE_CONFIG_ITEM_SPIN        0x0b    /* A numerical selector */

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

