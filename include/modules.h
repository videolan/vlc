/*****************************************************************************
 * modules.h : Module management functions.
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: modules.h,v 1.52 2002/06/01 12:31:57 sam Exp $
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
#define MODULE_CAPABILITY_MAIN      0  /* Main */
#define MODULE_CAPABILITY_INTF      1  /* Interface */
#define MODULE_CAPABILITY_ACCESS    2  /* Input */
#define MODULE_CAPABILITY_DEMUX     3  /* Input */
#define MODULE_CAPABILITY_NETWORK   4  /* Network */
#define MODULE_CAPABILITY_DECODER   5  /* Audio or video decoder */
#define MODULE_CAPABILITY_MOTION    6  /* Motion compensation */
#define MODULE_CAPABILITY_IDCT      7  /* IDCT transformation */
#define MODULE_CAPABILITY_AOUT      8  /* Audio output */
#define MODULE_CAPABILITY_VOUT      9  /* Video output */
#define MODULE_CAPABILITY_CHROMA   10  /* colorspace conversion */
#define MODULE_CAPABILITY_IMDCT    11  /* IMDCT transformation */
#define MODULE_CAPABILITY_DOWNMIX  12  /* AC3 downmix */
#define MODULE_CAPABILITY_MEMCPY   13  /* memcpy */
#define MODULE_CAPABILITY_MAX      14  /* Total number of capabilities */

#define DECLARE_MODULE_CAPABILITY_TABLE \
    static const char *ppsz_capabilities[] = \
    { \
        "main", \
        "interface", \
        "access", \
        "demux", \
        "network", \
        "decoder", \
        "motion", \
        "iDCT", \
        "audio output", \
        "video output", \
        "chroma transformation", \
        "iMDCT", \
        "downmix", \
        "memcpy", \
        "unknown" \
    }

#define MODULE_CAPABILITY( i_capa ) \
    ppsz_capabilities[ ((i_capa) > MODULE_CAPABILITY_MAX) ? \
                          MODULE_CAPABILITY_MAX : (i_capa) ]

/*****************************************************************************
 * module_bank_t: the module bank
 *****************************************************************************
 * This variable is accessed by any function using modules.
 *****************************************************************************/
struct module_bank_s
{
    module_t *   first;                          /* First module in the bank */
    int          i_count;                     /* Number of allocated modules */

    vlc_mutex_t  lock;         /* Global lock -- you can't imagine how awful *
                                    it is to design thread-safe linked lists */
};

/*****************************************************************************
 * Module description structure
 *****************************************************************************/
struct module_s
{
    VLC_COMMON_MEMBERS

    /*
     * Variables set by the module to identify itself
     */
    char *psz_longname;                           /* Module descriptive name */

    /*
     * Variables set by the module to tell us what it can do
     */
    char *psz_program;        /* Program name which will activate the module */
    char *pp_shortcuts[ MODULE_SHORTCUT_MAX ];    /* Shortcuts to the module */

    u32   i_capabilities;                                 /* Capability list */
    int   pi_score[ MODULE_CAPABILITY_MAX ];    /* Score for each capability */

    u32   i_cpu_capabilities;                   /* Required CPU capabilities */

    module_functions_t *p_functions;                 /* Capability functions */

    /*
     * Variables set by the module to store its config options
     */
    module_config_t *p_config;             /* Module configuration structure */
    vlc_mutex_t            config_lock;    /* lock used to modify the config */
    unsigned int           i_config_items;  /* number of configuration items */
    unsigned int           i_bool_items;      /* number of bool config items */

    /*
     * Variables used internally by the module manager
     */
    vlc_bool_t          b_builtin;  /* Set to true if the module is built in */

    union
    {
        struct
        {
            module_handle_t     handle;                     /* Unique handle */
            char *              psz_filename;             /* Module filename */

        } plugin;

        struct
        {
            int ( *pf_deactivate ) ( module_t * );

        } builtin;

    } is;

    int   i_usage;                                      /* Reference counter */
    int   i_unused_delay;                  /* Delay until module is unloaded */

    module_t *next;                                           /* Next module */
    module_t *prev;                                       /* Previous module */

    /*
     * Symbol table we send to the module so that it can access vlc symbols
     */
    module_symbols_t *p_symbols;
};

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
            int  ( * pf_open ) ( intf_thread_t * );
            void ( * pf_close )( intf_thread_t * );
            void ( * pf_run )  ( intf_thread_t * );
        } intf;

        /* Access plugin */
        struct
        {
            int  ( * pf_open )        ( input_thread_t * );
            void ( * pf_close )       ( input_thread_t * );
            ssize_t ( * pf_read )     ( input_thread_t *, byte_t *, size_t );
            void ( * pf_seek )        ( input_thread_t *, off_t );
            int  ( * pf_set_program ) ( input_thread_t *, pgrm_descriptor_t * );
            int  ( * pf_set_area )    ( input_thread_t *, input_area_t * );
        } access;

        /* Demux plugin */
        struct
        {
            int  ( * pf_init )   ( input_thread_t * );
            void ( * pf_end )    ( input_thread_t * );
            int  ( * pf_demux )  ( input_thread_t * );
            int  ( * pf_rewind ) ( input_thread_t * );
        } demux;

        /* Network plugin */
        struct
        {
            int  ( * pf_open ) ( vlc_object_t *, network_socket_t * );
        } network;

        /* Audio output plugin */
        struct
        {
            int  ( * pf_open )       ( aout_thread_t * );
            int  ( * pf_setformat )  ( aout_thread_t * );
            int  ( * pf_getbufinfo ) ( aout_thread_t *, int );
            void ( * pf_play )       ( aout_thread_t *, byte_t *, int );
            void ( * pf_close )      ( aout_thread_t * );
        } aout;

        /* Video output plugin */
        struct
        {
            int  ( * pf_create )     ( vout_thread_t * );
            int  ( * pf_init )       ( vout_thread_t * );
            void ( * pf_end )        ( vout_thread_t * );
            void ( * pf_destroy )    ( vout_thread_t * );
            int  ( * pf_manage )     ( vout_thread_t * );
            void ( * pf_render )     ( vout_thread_t *, picture_t * );
            void ( * pf_display )    ( vout_thread_t *, picture_t * );
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
            int  ( * pf_init )         ( vout_thread_t * );
            void ( * pf_end )          ( vout_thread_t * );
        } chroma;

        /* IMDCT plugin */
        struct
        {
            void ( * pf_imdct_init )   ( imdct_t * );
            void ( * pf_imdct_256 )    ( imdct_t *, float [], float [] );
            void ( * pf_imdct_256_nol )( imdct_t *, float [], float [] );
            void ( * pf_imdct_512 )    ( imdct_t *, float [], float [] );
            void ( * pf_imdct_512_nol )( imdct_t *, float [], float [] );
//            void ( * pf_fft_64p )      ( complex_t * );

        } imdct;

        /* AC3 downmix plugin */
        struct
        {
            void ( * pf_downmix_3f_2r_to_2ch ) ( float *, dm_par_t * );
            void ( * pf_downmix_3f_1r_to_2ch ) ( float *, dm_par_t * );
            void ( * pf_downmix_2f_2r_to_2ch ) ( float *, dm_par_t * );
            void ( * pf_downmix_2f_1r_to_2ch ) ( float *, dm_par_t * );
            void ( * pf_downmix_3f_0r_to_2ch ) ( float *, dm_par_t * );
            void ( * pf_stream_sample_2ch_to_s16 ) ( s16 *, float *, float * );
            void ( * pf_stream_sample_1ch_to_s16 ) ( s16 *, float * );

        } downmix;

        /* Decoder plugins */
        struct
        {
            int  ( * pf_probe)( u8 * p_es );
            int  ( * pf_run ) ( decoder_fifo_t * p_fifo );
        } dec;

        /* memcpy plugins */
        struct
        {
            void* ( * pf_memcpy ) ( void *, const void *, size_t );
            void* ( * pf_memset ) ( void *, int, size_t );
        } memcpy;

    } functions;

} function_list_t;

struct module_functions_s
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
};

/*****************************************************************************
 * Exported functions.
 *****************************************************************************/
void            module_InitBank     ( vlc_object_t * );
void            module_LoadMain     ( vlc_object_t * );
void            module_LoadBuiltins ( vlc_object_t * );
void            module_LoadPlugins  ( vlc_object_t * );
void            module_EndBank      ( vlc_object_t * );
void            module_ResetBank    ( vlc_object_t * );
void            module_ManageBank   ( vlc_object_t * );

VLC_EXPORT( module_t *, __module_Need, ( vlc_object_t *, int, char *, void * ) );
VLC_EXPORT( void, module_Unneed, ( module_t * ) );

#define module_Need(a,b,c,d) __module_Need(CAST_TO_VLC_OBJECT(a),b,c,d)

