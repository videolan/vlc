/*****************************************************************************
 * input_ext-intf.h: structures of the input exported to the interface
 * This header provides structures to read the stream descriptors and
 * control the pace of reading. 
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: input_ext-intf.h,v 1.62 2002/03/05 17:46:33 stef Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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

/*
 * Communication input -> interface
 */
/* FIXME ! */
#define REQUESTED_MPEG         1
#define REQUESTED_AC3          2
#define REQUESTED_LPCM         3
#define REQUESTED_NOAUDIO    255

#define OFFSETTOTIME_MAX_SIZE       10

/*****************************************************************************
 * input_bank_t, p_input_bank (global variable)
 *****************************************************************************
 * This global variable is accessed by any function using the input.
 *****************************************************************************/
typedef struct input_bank_s
{
    /* Array to all the input threads */
    struct input_thread_s *pp_input[ INPUT_MAX_THREADS ];

    int                   i_count;
    vlc_mutex_t           lock;                               /* Global lock */

} input_bank_t;

#ifndef PLUGIN
extern input_bank_t *p_input_bank;
#else
#   define p_input_bank (p_symbols->p_input_bank)
#endif

/*****************************************************************************
 * es_descriptor_t: elementary stream descriptor
 *****************************************************************************
 * Describes an elementary stream, and includes fields required to handle and
 * demultiplex this elementary stream.
 *****************************************************************************/
typedef struct es_descriptor_s
{
    u16                     i_id;            /* stream ID for PS, PID for TS */
    u8                      i_stream_id;     /* stream ID defined in the PES */
    u8                      i_type;                           /* stream type */
    boolean_t               b_audio;      /* is the stream an audio stream that
                                           * will need to be discarded with
                                           * fast forward and slow motion ?  */
    u8                      i_cat;        /* stream category: video, audio,
                                           * spu, other */

    char                    psz_desc[20]; /* description of ES: audio language
                                           * for instance ; NULL if not
                                           *  available */

    /* Demultiplexer information */
    void *                  p_demux_data;
    struct pgrm_descriptor_s *
                            p_pgrm;  /* very convenient in the demultiplexer */

    /* PES parser information */
    struct pes_packet_s *   p_pes;                            /* Current PES */
    int                     i_pes_real_size;   /* as indicated by the header */

    /* Decoder information */
    struct decoder_fifo_s * p_decoder_fifo;
    vlc_thread_t            thread_id;                  /* ID of the decoder */

    count_t                 c_packets;                 /* total packets read */
    count_t                 c_invalid_packets;       /* invalid packets read */

    /* Module properties */
    struct module_s *         p_module;
    struct decoder_config_s * p_config;

} es_descriptor_t;

/* Special PID values - note that the PID is only on 13 bits, and that values
 * greater than 0x1fff have no meaning in a stream */
#define PROGRAM_ASSOCIATION_TABLE_PID   0x0000
#define CONDITIONNAL_ACCESS_TABLE_PID   0x0001                   /* not used */
#define EMPTY_ID                        0xffff    /* empty record in a table */
 

/* ES Categories to be used by interface plugins */
#define VIDEO_ES        0x00
#define AUDIO_ES        0x01
#define SPU_ES          0x02
#define NAV_ES          0x03
#define UNKNOWN_ES      0xFF

/*****************************************************************************
 * pgrm_descriptor_t
 *****************************************************************************
 * Describes a program and list associated elementary streams. It is build by
 * the PSI decoder upon the informations carried in program map sections
 *****************************************************************************/
typedef struct pgrm_descriptor_s
{
    /* Program characteristics */
    u16                     i_number;                      /* program number */
    u8                      i_version;                     /* version number */
    boolean_t               b_is_ok;      /* Is the description up to date ? */

    /* Service Descriptor (program name) - DVB extension */
    u8                      i_srv_type;
    char *                  psz_srv_name;

    /* Synchronization information */
    mtime_t                 delta_cr;
    mtime_t                 cr_ref, sysdate_ref;
    mtime_t                 last_cr; /* reference to detect unexpected stream
                                      * discontinuities                      */
    mtime_t                 last_syscr;
    count_t                 c_average_count;
                           /* counter used to compute dynamic average values */
    int                     i_synchro_state;

    /* Demultiplexer data */
    void *                  p_demux_data;

    int                     i_es_number;      /* size of the following array */
    es_descriptor_t **      pp_es;                /* array of pointers to ES */
} pgrm_descriptor_t;

/* Synchro states */
#define SYNCHRO_OK          0
#define SYNCHRO_START       1
#define SYNCHRO_REINIT      2

/*****************************************************************************
 * input_area_t
 *****************************************************************************
 * Attributes for current area (title for DVD)
 *****************************************************************************/
typedef struct input_area_s
{
    /* selected area attributes */
    int                     i_id;        /* identificator for area */
    off_t                   i_start;     /* start offset of area */
    off_t                   i_size;      /* total size of the area
                                          * (in arbitrary units) */

    /* navigation parameters */
    off_t                   i_tell;      /* actual location in the area
                                          * (in arbitrary units) */
    off_t                   i_seek;      /* next requested location
                                          * (changed by the interface thread */

    /* area subdivision */
    int                     i_part_nb;   /* number of parts (chapter for DVD)*/
    int                     i_part;      /* currently selected part */


    /* offset to plugin related data */
    off_t                   i_plugin_data;
} input_area_t;

/*****************************************************************************
 * stream_descriptor_t
 *****************************************************************************
 * Describes a stream and list its associated programs. Build upon
 * the information carried in program association sections (for instance)
 *****************************************************************************/
typedef struct stream_descriptor_s
{
    u16                     i_stream_id;                        /* stream id */
    boolean_t               b_changed;    /* if stream has been changed,
                                             we have to inform the interface */
    vlc_mutex_t             stream_lock;  /* to be taken every time you read
                                           * or modify stream, pgrm or es    */

    /* Input method data */
    int                     i_method;       /* input method for stream: file,
                                               disc or network */
    boolean_t               b_pace_control;    /* can we read when we want ? */
    boolean_t               b_seekable;               /* can we do lseek() ? */

    /* if (b_seekable) : */
    int                     i_area_nb;
    input_area_t **         pp_areas;    /* list of areas in stream == offset
                                          * interval with own properties */
    input_area_t *          p_selected_area;
    input_area_t *          p_new_area;  /* Newly selected area from
                                          * the interface */

    u32                     i_mux_rate; /* the rate we read the stream (in
                                         * units of 50 bytes/s) ; 0 if undef */

    /* New status and rate requested by the interface */
    int                     i_new_status, i_new_rate;
    int                     b_new_mute;          /* int because it can be -1 */
    vlc_cond_t              stream_wait; /* interface -> input in case of a
                                          * status change request            */

    /* Demultiplexer data */
    void *                  p_demux_data;

    /* Programs descriptions */
    int                     i_pgrm_number;    /* size of the following array */
    pgrm_descriptor_t **    pp_programs;        /* array of pointers to pgrm */
    pgrm_descriptor_t *     p_selected_program;   /* currently 
                                                 selected program */
    pgrm_descriptor_t *     p_new_program;        /* Newly selected program */
    /* ES descriptions */
    int                     i_es_number;
    es_descriptor_t **      pp_es;             /* carried elementary streams */
    int                     i_selected_es_number;
    es_descriptor_t **      pp_selected_es;             /* ES with a decoder */
    es_descriptor_t *       p_newly_selected_es;   /* ES selected from
                                                    * the interface */
    es_descriptor_t *       p_removed_es;   /* ES removed from the interface */


    /* Stream control */
    stream_ctrl_t           control;

    /* Statistics */
    count_t                 c_packets_read;                  /* packets read */
    count_t                 c_packets_trashed;            /* trashed packets */
} stream_descriptor_t;

#define MUTE_NO_CHANGE      -1

/*****************************************************************************
 * input_thread_t
 *****************************************************************************
 * This structure includes all the local static variables of an input thread
 *****************************************************************************/
typedef struct input_thread_s
{
    /* Thread properties and locks */
    boolean_t               b_die;                             /* 'die' flag */
    boolean_t               b_error;
    boolean_t               b_eof;
    vlc_thread_t            thread_id;            /* id for thread functions */
    int                     i_status;                         /* status flag */

    /* Access module */
    struct module_s *       p_access_module;
    int                  (* pf_open)( struct input_thread_s * );
    void                 (* pf_close)( struct input_thread_s * );
    ssize_t              (* pf_read) ( struct input_thread_s *,
                                       byte_t *, size_t );
    int                  (* pf_set_program)( struct input_thread_s *,
                                             struct pgrm_descriptor_s * );
    int                  (* pf_set_area)( struct input_thread_s *,
                                          struct input_area_s * );
    void                 (* pf_seek)( struct input_thread_s *, off_t );
    void *                  p_access_data;
    size_t                  i_mtu;

    /* Demux module */
    struct module_s *       p_demux_module;
    int                  (* pf_init)( struct input_thread_s * );
    void                 (* pf_end)( struct input_thread_s * );
    int                  (* pf_demux)( struct input_thread_s * );
    int                  (* pf_rewind)( struct input_thread_s * );
                                           /* NULL if we don't support going *
                                            * backwards (it's gonna be fun)  */
    void *                  p_demux_data;               /* data of the demux */

    /* Buffer manager */
    struct input_buffers_s *p_method_data;     /* data of the packet manager */
    struct data_buffer_s *  p_data_buffer;
    byte_t *                p_current_data;
    byte_t *                p_last_data;
    size_t                  i_bufsize;

    /* General stream description */
    stream_descriptor_t     stream;

    /* Playlist item */
    char *                  psz_source;
    char *                  psz_access;
    char *                  psz_demux;
    char *                  psz_name;

    count_t                 c_loops;
} input_thread_t;

/* Input methods */
/* The first figure is a general method that can be used in interface plugins ;
 * The second figure is a detailed sub-method */
#define INPUT_METHOD_NONE         0x0            /* input thread is inactive */
#define INPUT_METHOD_FILE        0x10   /* stream is read from file p_source */
#define INPUT_METHOD_DISC        0x20   /* stream is read directly from disc */
#define INPUT_METHOD_DVD         0x21             /* stream is read from DVD */
#define INPUT_METHOD_VCD         0x22             /* stream is read from VCD */
#define INPUT_METHOD_NETWORK     0x30         /* stream is read from network */
#define INPUT_METHOD_UCAST       0x31                         /* UDP unicast */
#define INPUT_METHOD_MCAST       0x32                       /* UDP multicast */
#define INPUT_METHOD_BCAST       0x33                       /* UDP broadcast */
#define INPUT_METHOD_VLAN_BCAST  0x34            /* UDP broadcast with VLANs */


/* Status changing methods */
#define INPUT_STATUS_END            0
#define INPUT_STATUS_PLAY           1
#define INPUT_STATUS_PAUSE          2
#define INPUT_STATUS_FASTER         3
#define INPUT_STATUS_SLOWER         4

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
#ifndef PLUGIN
void   input_InitBank       ( void );
void   input_EndBank        ( void );

struct input_thread_s * input_CreateThread ( struct playlist_item_s *,
                                             int *pi_status );
void   input_StopThread     ( struct input_thread_s *, int *pi_status );
void   input_DestroyThread  ( struct input_thread_s * );

void   input_SetStatus      ( struct input_thread_s *, int );
void   input_Seek           ( struct input_thread_s *, off_t );
void   input_DumpStream     ( struct input_thread_s * );
char * input_OffsetToTime   ( struct input_thread_s *, char *, off_t );
int    input_ChangeES       ( struct input_thread_s *,
                              struct es_descriptor_s *, u8 );
int    input_ToggleES       ( struct input_thread_s *,
                              struct es_descriptor_s *, boolean_t );
int    input_ChangeArea     ( struct input_thread_s *, struct input_area_s * );
int    input_ChangeProgram  ( struct input_thread_s *, u16 );
int    input_ToggleGrayscale( struct input_thread_s * );
int    input_ToggleMute     ( struct input_thread_s * );
int    input_SetSMP         ( struct input_thread_s *, int );
#else
#   define input_SetStatus      p_symbols->input_SetStatus
#   define input_Seek           p_symbols->input_Seek
#   define input_DumpStream     p_symbols->input_DumpStream
#   define input_OffsetToTime   p_symbols->input_OffsetToTime
#   define input_ChangeES       p_symbols->input_ChangeES
#   define input_ToggleES       p_symbols->input_ToggleES
#   define input_ChangeArea     p_symbols->input_ChangeArea
#   define input_ChangeProgram  p_symbols->input_ChangeProgram
#endif

