/*****************************************************************************
 * modules.h : Module management functions.
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: modules.h,v 1.50 2002/05/18 17:47:46 sam Exp $
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

/*****************************************************************************
 * Module #defines.
 *****************************************************************************/

/* Number of tries before we unload an unused module */
#define MODULE_HIDE_DELAY 50
#define MODULE_SHORTCUT_MAX 10

/* The module handle type. */
#ifdef SYS_BEOS
typedef int     module_handle_t;
#else
typedef void *  module_handle_t;
#endif

/*****************************************************************************
 * Module capabilities.
 *****************************************************************************/
static inline char *GetCapabilityName( unsigned int i_capa )
{
    /* The sole purpose of this inline function and the ugly #defines
     * around it is to avoid having two places to modify when adding a
     * new capability. */
    static char *pp_capa[] =
    {
        "main",
#define MODULE_CAPABILITY_MAIN      0  /* Main */
        "interface",
#define MODULE_CAPABILITY_INTF      1  /* Interface */
        "access",
#define MODULE_CAPABILITY_ACCESS    2  /* Input */
        "demux",
#define MODULE_CAPABILITY_DEMUX     3  /* Input */
        "network",
#define MODULE_CAPABILITY_NETWORK   4  /* Network */
        "decoder",
#define MODULE_CAPABILITY_DECODER   5  /* Audio or video decoder */
        "motion",
#define MODULE_CAPABILITY_MOTION    6  /* Motion compensation */
        "iDCT",
#define MODULE_CAPABILITY_IDCT      7  /* IDCT transformation */
        "audio output",
#define MODULE_CAPABILITY_AOUT      8  /* Audio output */
        "video output",
#define MODULE_CAPABILITY_VOUT      9  /* Video output */
        "chroma transformation",
#define MODULE_CAPABILITY_CHROMA   10  /* colorspace conversion */
        "iMDCT",
#define MODULE_CAPABILITY_IMDCT    11  /* IMDCT transformation */
        "downmix",
#define MODULE_CAPABILITY_DOWNMIX  12  /* AC3 downmix */
        "memcpy",
#define MODULE_CAPABILITY_MEMCPY   13  /* memcpy */
        "unknown"
#define MODULE_CAPABILITY_MAX      14  /* Total number of capabilities */
    };

    return pp_capa[ (i_capa) > MODULE_CAPABILITY_MAX ? MODULE_CAPABILITY_MAX :
                    (i_capa) ];
}

/*****************************************************************************
 * module_bank_t, p_module_bank (global variable)
 *****************************************************************************
 * This global variable is accessed by any function using modules.
 *****************************************************************************/
typedef struct module_bank_s
{
    struct module_s *   first;                   /* First module in the bank */
    int                 i_count;              /* Number of allocated modules */

    vlc_mutex_t         lock;  /* Global lock -- you can't imagine how awful *
                                    it is to design thread-safe linked lists */
} module_bank_t;

#ifndef __PLUGIN__
extern module_bank_t *p_module_bank;
#else
#   define p_module_bank (p_symbols->p_module_bank)
#endif

/*****************************************************************************
 * Module description structure
 *****************************************************************************/
typedef struct module_s
{
    /*
     * Variables set by the module to identify itself
     */
    char *psz_name;                                  /* Module _unique_ name */
    char *psz_longname;                           /* Module descriptive name */

    /*
     * Variables set by the module to tell us what it can do
     */
    char *psz_program;        /* Program name which will activate the module */
    char *pp_shortcuts[ MODULE_SHORTCUT_MAX ];    /* Shortcuts to the module */

    u32   i_capabilities;                                 /* Capability list */
    int   pi_score[ MODULE_CAPABILITY_MAX ];    /* Score for each capability */

    u32   i_cpu_capabilities;                   /* Required CPU capabilities */

    struct module_functions_s *p_functions;          /* Capability functions */

    /*
     * Variables set by the module to store its config options
     */
    struct module_config_s *p_config;      /* Module configuration structure */
    vlc_mutex_t            config_lock;    /* lock used to modify the config */
    unsigned int           i_config_items;  /* number of configuration items */

    /*
     * Variables used internally by the module manager
     */
    boolean_t           b_builtin;  /* Set to true if the module is built in */

    union
    {
        struct
        {
            module_handle_t     handle;                     /* Unique handle */
            char *              psz_filename;             /* Module filename */

        } plugin;

        struct
        {
            int ( *pf_deactivate ) ( struct module_s * );

        } builtin;

    } is;

    int   i_usage;                                      /* Reference counter */
    int   i_unused_delay;                  /* Delay until module is unloaded */

    struct module_s *next;                                    /* Next module */
    struct module_s *prev;                                /* Previous module */

    /*
     * Symbol table we send to the module so that it can access vlc symbols
     */
    struct module_symbols_s *p_symbols;

} module_t;

/*****************************************************************************
 * Module functions description structure
 *****************************************************************************/
typedef struct function_list_s
{
    union
    {
        /* Interface plugin */
        struct
        {
            int  ( * pf_open ) ( struct intf_thread_s * );
            void ( * pf_close )( struct intf_thread_s * );
            void ( * pf_run )  ( struct intf_thread_s * );
        } intf;

        /* Access plugin */
        struct
        {
            int  ( * pf_open ) ( struct input_thread_s * );
            void ( * pf_close )( struct input_thread_s * );
            ssize_t ( * pf_read ) ( struct input_thread_s *, byte_t *, size_t );
            void ( * pf_seek ) ( struct input_thread_s *, off_t );
            int  ( * pf_set_program ) ( struct input_thread_s *,
                                        struct pgrm_descriptor_s * );
            int  ( * pf_set_area ) ( struct input_thread_s *,
                                     struct input_area_s * );
        } access;

        /* Demux plugin */
        struct
        {
            int  ( * pf_init ) ( struct input_thread_s * );
            void ( * pf_end )  ( struct input_thread_s * );
            int  ( * pf_demux )( struct input_thread_s * );
            int  ( * pf_rewind )   ( struct input_thread_s * );
        } demux;

        /* Network plugin */
        struct
        {
            int  ( * pf_open )( struct network_socket_s * );
        } network;

        /* Audio output plugin */
        struct
        {
            int  ( * pf_open )       ( struct aout_thread_s * );
            int  ( * pf_setformat )  ( struct aout_thread_s * );
            int  ( * pf_getbufinfo ) ( struct aout_thread_s *, int );
            void ( * pf_play )       ( struct aout_thread_s *, byte_t *, int );
            void ( * pf_close )      ( struct aout_thread_s * );
        } aout;

        /* Video output plugin */
        struct
        {
            int  ( * pf_create )     ( struct vout_thread_s * );
            int  ( * pf_init )       ( struct vout_thread_s * );
            void ( * pf_end )        ( struct vout_thread_s * );
            void ( * pf_destroy )    ( struct vout_thread_s * );
            int  ( * pf_manage )     ( struct vout_thread_s * );
            void ( * pf_render )     ( struct vout_thread_s *,
                                       struct picture_s * );
            void ( * pf_display )    ( struct vout_thread_s *,
                                       struct picture_s * );
        } vout;

        /* Motion compensation plugin */
        struct
        {
            void ( * ppppf_motion[2][2][4] ) ( yuv_data_t *, yuv_data_t *,
                                               int, int );
        } motion;

        /* IDCT plugin */
        struct
        {
            void ( * pf_idct_init )    ( void ** );
            void ( * pf_sparse_idct_add )( dctelem_t *, yuv_data_t *, int,
                                         void *, int );
            void ( * pf_idct_add )     ( dctelem_t *, yuv_data_t *, int,
                                         void *, int );
            void ( * pf_sparse_idct_copy )( dctelem_t *, yuv_data_t *, int,
                                         void *, int );
            void ( * pf_idct_copy )    ( dctelem_t *, yuv_data_t *, int,
                                         void *, int );
            void ( * pf_norm_scan )    ( u8 ppi_scan[2][64] );
        } idct;

        /* Chroma transformation plugin */
        struct
        {
            int  ( * pf_init )         ( struct vout_thread_s * );
            void ( * pf_end )          ( struct vout_thread_s * );
        } chroma;

        /* IMDCT plugin */
        struct
        {
            void ( * pf_imdct_init )   ( struct imdct_s * );
            void ( * pf_imdct_256 )    ( struct imdct_s *,
                                         float data[], float delay[] );
            void ( * pf_imdct_256_nol )( struct imdct_s *,
                                         float data[], float delay[] );
            void ( * pf_imdct_512 )    ( struct imdct_s *,
                                         float data[], float delay[] );
            void ( * pf_imdct_512_nol )( struct imdct_s *,
                                         float data[], float delay[] );
//            void ( * pf_fft_64p )      ( struct complex_s * );

        } imdct;

        /* AC3 downmix plugin */
        struct
        {
            void ( * pf_downmix_3f_2r_to_2ch ) ( float *, struct dm_par_s * );
            void ( * pf_downmix_3f_1r_to_2ch ) ( float *, struct dm_par_s * );
            void ( * pf_downmix_2f_2r_to_2ch ) ( float *, struct dm_par_s * );
            void ( * pf_downmix_2f_1r_to_2ch ) ( float *, struct dm_par_s * );
            void ( * pf_downmix_3f_0r_to_2ch ) ( float *, struct dm_par_s * );
            void ( * pf_stream_sample_2ch_to_s16 ) ( s16 *, float *, float * );
            void ( * pf_stream_sample_1ch_to_s16 ) ( s16 *, float * );

        } downmix;

        /* Decoder plugins */
        struct
        {
            int  ( * pf_probe)( u8 * p_es );
            int  ( * pf_run ) ( struct decoder_config_s * p_config );
        } dec;

        /* memcpy plugins */
        struct
        {
            void* ( * pf_memcpy ) ( void *, const void *, size_t );
            void* ( * pf_memset ) ( void *, int, size_t );
        } memcpy;

    } functions;

} function_list_t;

typedef struct module_functions_s
{
    /* XXX: The order here has to be the same as above for the #defines */
    function_list_t intf;
    function_list_t access;
    function_list_t demux;
    function_list_t network;
    function_list_t dec;
    function_list_t motion;
    function_list_t idct;
    function_list_t aout;
    function_list_t vout;
    function_list_t chroma;
    function_list_t imdct;
    function_list_t downmix;
    function_list_t memcpy;

} module_functions_t;

typedef struct module_functions_s * p_module_functions_t;

/*****************************************************************************
 * Exported functions.
 *****************************************************************************/
#ifndef __PLUGIN__
void            module_InitBank     ( void );
void            module_LoadMain     ( void );
void            module_LoadBuiltins ( void );
void            module_LoadPlugins  ( void );
void            module_EndBank      ( void );
void            module_ResetBank    ( void );
void            module_ManageBank   ( void );
module_t *      module_Need         ( int, char *, void * );
void            module_Unneed       ( module_t * );

#else
#   define module_Need p_symbols->module_Need
#   define module_Unneed p_symbols->module_Unneed
#endif
