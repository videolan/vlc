/*****************************************************************************
 * input_ext-intf.h: structures of the input exported to the interface
 * This header provides structures to read the stream descriptors and
 * control the pace of reading. 
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: input_ext-intf.h,v 1.40 2001/06/27 09:53:56 massiot Exp $
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
#define INPUT_MAX_PLUGINS   1
/* FIXME ! */
#define REQUESTED_MPEG         1
#define REQUESTED_AC3          2
#define REQUESTED_LPCM         3
#define REQUESTED_NOAUDIO    255

#define OFFSETTOTIME_MAX_SIZE       10

/*****************************************************************************
 * es_descriptor_t: elementary stream descriptor
 *****************************************************************************
 * Describes an elementary stream, and includes fields required to handle and
 * demultiplex this elementary stream.
 *****************************************************************************/
struct decoder_fifo_s;                         /* defined in input_ext-dec.h */
struct pgrm_descriptor_s;

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
    struct data_packet_s *  p_last;   /* The last packet gathered at present */
    int                     i_pes_real_size;   /* as indicated by the header */

    /* Decoder information */
    struct decoder_fifo_s * p_decoder_fifo;
    vlc_thread_t            thread_id;                  /* ID of the decoder */

#ifdef STATS
    count_t                 c_payload_bytes;/* total of payload useful bytes */
    count_t                 c_packets;                 /* total packets read */
    count_t                 c_invalid_packets;       /* invalid packets read */
#endif
} es_descriptor_t;

/* Special PID values - note that the PID is only on 13 bits, and that values
 * greater than 0x1fff have no meaning in a stream */
#define PROGRAM_ASSOCIATION_TABLE_PID   0x0000
#define CONDITIONNAL_ACCESS_TABLE_PID   0x0001                   /* not used */
#define EMPTY_ID                        0xffff    /* empty record in a table */
 
/* ES streams types - see ISO/IEC 13818-1 table 2-29 numbers */
#define MPEG1_VIDEO_ES      0x01
#define MPEG2_VIDEO_ES      0x02
#define MPEG1_AUDIO_ES      0x03
#define MPEG2_AUDIO_ES      0x04
#define AC3_AUDIO_ES        0x81
/* These ones might violate the norm : */
#define DVD_SPU_ES          0x82
#define LPCM_AUDIO_ES       0x83
#define UNKNOWN_ES          0xFF

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

    int                     i_angle_nb;  /* number of angles/title units */
    int                     i_angle;

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
    vlc_cond_t              stream_wait; /* interface -> input in case of a
                                          * status change request            */

    /* Demultiplexer data */
    void *                  p_demux_data;

    /* Programs descriptions */
    int                     i_pgrm_number;    /* size of the following array */
    pgrm_descriptor_t **    pp_programs;        /* array of pointers to pgrm */

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
} stream_descriptor_t;

/*****************************************************************************
 * i_p_config_t
 *****************************************************************************
 * This structure gives plugins pointers to the useful functions of input
 *****************************************************************************/
struct input_thread_s;
struct data_packet_s;
struct es_descriptor_s;

typedef struct i_p_config_s
{
    int                 (* pf_peek_stream)( struct input_thread_s *,
                                            byte_t * buffer, size_t );
    void                (* pf_demux_pes)( struct input_thread_s *,
                                          struct data_packet_s *,
                                          struct es_descriptor_s *,
                                          boolean_t b_unit_start,
                                          boolean_t b_packet_lost );
} i_p_config_t;

/*****************************************************************************
 * input_thread_t
 *****************************************************************************
 * This structure includes all the local static variables of an input thread
 *****************************************************************************/
struct vout_thread_s;
struct bit_stream_s;

typedef struct input_thread_s
{
    /* Thread properties and locks */
    boolean_t               b_die;                             /* 'die' flag */
    boolean_t               b_error;
    boolean_t               b_eof;
    vlc_thread_t            thread_id;            /* id for thread functions */
    int *                   pi_status;              /* temporary status flag */

    /* Input module */
    struct module_s *       p_input_module;

    /* Init/End */
    void                 (* pf_init)( struct input_thread_s * );
    void                 (* pf_open)( struct input_thread_s * );
    void                 (* pf_close)( struct input_thread_s * );
    void                 (* pf_end)( struct input_thread_s * );
    void                 (* pf_init_bit_stream)( struct bit_stream_s *,
                              struct decoder_fifo_s *,
                void (* pf_bitstream_callback)( struct bit_stream_s *,
                                                boolean_t ),
                              void * );

    /* Read & Demultiplex */
    int                  (* pf_read)( struct input_thread_s *,
                                      struct data_packet_s * pp_packets[] );
    void                 (* pf_demux)( struct input_thread_s *,
                                       struct data_packet_s * );

    /* Packet management facilities */
    struct data_packet_s *(*pf_new_packet)( void *, size_t );
    struct pes_packet_s *(* pf_new_pes)( void * );
    void                 (* pf_delete_packet)( void *,
                                               struct data_packet_s * );
    void                 (* pf_delete_pes)( void *, struct pes_packet_s * );

    /* Stream control capabilities */
    int                  (* pf_set_area)( struct input_thread_s *,
                                          struct input_area_s * );
    int                  (* pf_rewind)( struct input_thread_s * );
                                           /* NULL if we don't support going *
                                            * backwards (it's gonna be fun)  */
    void                 (* pf_seek)( struct input_thread_s *, off_t );

    /* Special callback functions */
    void                 (* pf_file_open )     ( struct input_thread_s * );
    void                 (* pf_file_close )    ( struct input_thread_s * );
#if !defined( SYS_BEOS ) && !defined( SYS_NTO )
    void                 (* pf_network_open )  ( struct input_thread_s * );
    void                 (* pf_network_close ) ( struct input_thread_s * );
#endif

    i_p_config_t            i_p_config;              /* plugin configuration */
    char *                  p_source;

    int                     i_handle;           /* socket or file descriptor */
    int                     i_read_once;        /* number of packet read by
                                                 * pf_read once */
    void *                  p_method_data;     /* data of the packet manager */
    void *                  p_plugin_data;             /* data of the plugin */

    /* General stream description */
    stream_descriptor_t     stream;                            /* PAT tables */

#ifdef STATS
    count_t                 c_loops;
    count_t                 c_bytes;                           /* bytes read */
    count_t                 c_payload_bytes;         /* payload useful bytes */
    count_t                 c_packets_read;                  /* packets read */
    count_t                 c_packets_trashed;            /* trashed packets */
#endif
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
struct input_thread_s * input_CreateThread ( struct playlist_item_s *,
                                             int *pi_status );
void input_DestroyThread( struct input_thread_s *, int *pi_status );

void input_SetStatus( struct input_thread_s *, int );
void input_SetRate  ( struct input_thread_s *, int );
void input_Seek     ( struct input_thread_s *, off_t );
void input_DumpStream( struct input_thread_s * );
char * input_OffsetToTime( struct input_thread_s *, char * psz_buffer, off_t );
int  input_ChangeES ( struct input_thread_s *, struct es_descriptor_s *, u8 );
int  input_ToggleES ( struct input_thread_s *,
                      struct es_descriptor_s *,
                      boolean_t );
int  input_ChangeArea( input_thread_t *, input_area_t * );

