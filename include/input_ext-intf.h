/*****************************************************************************
 * input_ext-intf.h: structures of the input exported to the interface
 * This header provides structures to read the stream descriptors and
 * control the pace of reading. 
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: input_ext-intf.h,v 1.7 2000/12/21 15:01:08 massiot Exp $
 *
 * Authors:
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
#define REQUESTED_MPEG         0
#define REQUESTED_AC3          1
#define REQUESTED_LPCM         2
#define REQUESTED_NOAUDIO    255

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

    /* Demultiplexer information */
    void *                  p_demux_data;
    struct pgrm_descriptor_s *
                            p_pgrm;  /* very convenient in the demultiplexer */

    /* PES parser information */
    struct pes_packet_s *   p_pes;                            /* Current PES */
    struct data_packet_s *  p_last;   /* The last packet gathered at present */
    int                     i_pes_real_size;   /* as indicated by the header */
    boolean_t               b_discontinuity;               /* Stream changed */

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
#define UNKNOWN_ES         0xFF

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
    /* system_date = PTS_date + delta_cr + delta_absolute */
    mtime_t                 delta_cr;
    mtime_t                 delta_absolute;
    mtime_t                 last_cr;
    count_t                 c_average_count;
                           /* counter used to compute dynamic average values */
    int                     i_synchro_state;
    boolean_t               b_discontinuity;

    /* Demultiplexer data */
    void *                  p_demux_data;

    /* Decoders control */
    struct vout_thread_s *  p_vout;
    struct aout_thread_s *  p_aout;

    int                     i_es_number;      /* size of the following array */
    es_descriptor_t **      pp_es;                /* array of pointers to ES */
} pgrm_descriptor_t;

/* Synchro states */
#define SYNCHRO_OK          0
#define SYNCHRO_NOT_STARTED 1
#define SYNCHRO_START       2
#define SYNCHRO_REINIT      3

/*****************************************************************************
 * stream_descriptor_t
 *****************************************************************************
 * Describes a stream and list its associated programs. Build upon
 * the information carried in program association sections (for instance)
 *****************************************************************************/
typedef struct stream_descriptor_s
{
    u16                     i_stream_id;                        /* stream id */
    vlc_mutex_t             stream_lock;  /* to be taken every time you read
                                           * or modify stream, pgrm or es    */

    /* Input method data */
    boolean_t               b_pace_control;    /* can we read when we want ? */
    boolean_t               b_seekable;               /* can we do lseek() ? */
    /* if (b_seekable) : */
    off_t                   i_size;     /* total size of the file (in bytes) */
    off_t                   i_tell;/* actual location in the file (in bytes) */

    /* Demultiplexer data */
    void *                  p_demux_data;

    /* Programs description */
    int                     i_pgrm_number;    /* size of the following array */
    pgrm_descriptor_t **    pp_programs;        /* array of pointers to pgrm */

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
struct aout_thread_s;
struct vout_thread_s;

typedef struct input_thread_s
{
    /* Thread properties and locks */
    boolean_t               b_die;                             /* 'die' flag */
    boolean_t               b_error;
    vlc_thread_t            thread_id;            /* id for thread functions */
    int *                   pi_status;              /* temporary status flag */

    struct input_config_s * p_config;

    struct input_capabilities_s *
                            pp_plugins[INPUT_MAX_PLUGINS];/* list of plugins */
    struct input_capabilities_s *
                            p_plugin;                     /* selected plugin */
    i_p_config_t            i_p_config;              /* plugin configuration */

    int                     i_handle;           /* socket or file descriptor */
    void *                  p_method_data;     /* data of the packet manager */
    void *                  p_plugin_data;             /* data of the plugin */

    /* General stream description */
    stream_descriptor_t     stream;                            /* PAT tables */
    es_descriptor_t **      pp_es;             /* carried elementary streams */
    int                     i_es_number;

    /* List of streams to demux */
    es_descriptor_t **      pp_selected_es;
    int                     i_selected_es_number;

    /* For auto-launch of decoders */
    struct aout_thread_s *  p_default_aout;
    struct vout_thread_s *  p_default_vout;

#ifdef STATS
    count_t                 c_loops;
    count_t                 c_bytes;                           /* bytes read */
    count_t                 c_payload_bytes;         /* payload useful bytes */
    count_t                 c_packets_read;                  /* packets read */
    count_t                 c_packets_trashed;            /* trashed packets */
#endif
} input_thread_t;


/*
 * Communication interface -> input
 */

/*****************************************************************************
 * input_config_t
 *****************************************************************************
 * This structure is given by the interface to an input thread
 *****************************************************************************/
typedef struct input_config_s
{
    /* Input method description */
    int                         i_method;                    /* input method */
    char *                      p_source;                          /* source */

    /* For auto-launch of decoders */
    struct aout_thread_s *      p_default_aout;
    struct vout_thread_s *      p_default_vout;
} input_config_t;

/* Input methods */
#define INPUT_METHOD_NONE           0            /* input thread is inactive */
#define INPUT_METHOD_FILE          10   /* stream is read from file p_source */
#define INPUT_METHOD_UCAST         20                         /* UDP unicast */
#define INPUT_METHOD_MCAST         21                       /* UDP multicast */
#define INPUT_METHOD_BCAST         22                       /* UDP broadcast */
#define INPUT_METHOD_VLAN_BCAST    32            /* UDP broadcast with VLANs */

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
struct input_thread_s * input_CreateThread( struct input_config_s *,
                                            int *pi_status );
void                    input_DestroyThread( struct input_thread_s *,
                                             int *pi_status );
void                    input_PauseProgram( struct input_thread_s *,
                                            struct pgrm_descriptor_s * );
void                    input_PlayProgram( struct input_thread_s *,
                                           struct pgrm_descriptor_s * );
void                    input_FFProgram( struct input_thread_s *,
                                         struct pgrm_descriptor_s * );
void                    input_SMProgram( struct input_thread_s *,
                                           struct pgrm_descriptor_s * );
void                    input_RewindProgram( struct input_thread_s *,
                                             struct pgrm_descriptor_s * );
