/*****************************************************************************
 * input.h: input thread interface
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *****************************************************************************/

/*****************************************************************************
 * Constants related to input
 *****************************************************************************/
#define TS_PACKET_SIZE      188                       /* size of a TS packet */
#define PES_HEADER_SIZE     14     /* size of the first part of a PES header */
#define PSI_SECTION_SIZE    4096            /* Maximum size of a PSI section */

/*****************************************************************************
 * ts_packet_t
 *****************************************************************************
 * Describe a TS packet.
 *****************************************************************************/
typedef struct ts_packet_s
{
    /* Nothing before this line, the code relies on that */
    byte_t                  buffer[TS_PACKET_SIZE];    /* raw TS data packet */

    /* Decoders information */
    unsigned int            i_payload_start;
                                  /* start of the PES payload in this packet */
    unsigned int            i_payload_end;                    /* guess ? :-) */

    /* Used to chain the TS packets that carry data for a same PES or PSI */
    struct ts_packet_s      *  p_prev_ts;
    struct ts_packet_s      *  p_next_ts;
} ts_packet_t;

/*****************************************************************************
 * pes_packet_t
 *****************************************************************************
 * Describes an PES packet, with its properties, and pointers to the TS packets
 * containing it.
 *****************************************************************************/
typedef struct pes_packet_s
{
    /* PES properties */
    boolean_t               b_data_loss;  /* The previous (at least) PES packet
           * has been lost. The decoders will have to find a way to recover. */
    boolean_t               b_data_alignment;  /* used to find the beginning of
                                                * a video or audio unit      */
    boolean_t               b_has_pts;       /* is the following field set ? */
    mtime_t                 i_pts; /* the PTS for this packet (if set above) */
    boolean_t               b_random_access;
            /* if TRUE, in the payload of this packet, there is the first byte
             * of a video sequence header, or the first byte of an audio frame.
             */
    u8                      i_stream_id;              /* payload type and id */
    int                     i_pes_size;    /* size of the current PES packet */
    int                     i_pes_real_size;      /* real size of the current
                                                   * PES packet, ie. the one
                                                   * announced in the header */
    int                     i_ts_packets;/* number of TS packets in this PES */

    /* Demultiplexer environment */
    boolean_t               b_discard_payload;  /* is the packet messed up ? */
    boolean_t               b_already_parsed;     /* was it already parsed ? */
    byte_t *                p_pes_header;       /* pointer to the PES header */
    byte_t *                p_pes_header_save;           /* temporary buffer */

    /* Pointers to TS packets (TS packets are then linked by the p_prev_ts and
       p_next_ts fields of the ts_packet_t struct) */
    ts_packet_t *           p_first_ts;   /* The first TS packet containing this
                                           * PES (used by decoders). */
    ts_packet_t *           p_last_ts; /* The last TS packet gathered at present
                                        * (used by the demultiplexer). */
} pes_packet_t;

/*****************************************************************************
 * psi_section_t
 *****************************************************************************
 * Describes a PSI section. Beware, it doesn't contain pointers to the TS
 * packets that contain it as for a PES, but the data themselves
 *****************************************************************************/
typedef struct psi_section_s
{
    byte_t        buffer[PSI_SECTION_SIZE];

    boolean_t     b_running_section;   /* Is there a section being decoded ? */

    u16 i_length;
    u16 i_current_position;
} psi_section_t;


/*****************************************************************************
 * es_descriptor_t: elementary stream descriptor
 *****************************************************************************
 * Describes an elementary stream, and includes fields required to handle and
 * demultiplex this elementary stream.
 *****************************************************************************/
typedef struct es_descriptor_t
{
    u16                     i_id;           /* stream ID, PID for TS streams */
    u8                      i_type;                           /* stream type */

    boolean_t               b_pcr;        /* does the stream include a PCR ? */
    /* XXX?? b_pcr will be replaced by something else: since a PCR can't be shared
     * between several ES, we will probably store the PCR fields directly here,
     * and one of those fields will probably (again) be used as a test of the
     * PCR presence */
    boolean_t               b_psi;  /* does the stream have to be handled by the
                                                                PSI decoder ? */
    /* Markers */
    int                     i_continuity_counter;
    boolean_t               b_discontinuity;
    boolean_t               b_random;

    /* PES packets */
    pes_packet_t *          p_pes_packet;
                                      /* current PES packet we are gathering */

    /* PSI packets */
    psi_section_t *         p_psi_section;          /* idem for a PSI stream */

    /* Decoder informations */
    void *                  p_dec;     /* p_dec is void *, since we don't know a
                                        * priori whether it is adec_thread_t or
                                        * vdec_thread_t. We will use explicit
                                        * casts. */

    /* XXX?? video stream descriptor ? */
    /* XXX?? audio stream descriptor ? */
    /* XXX?? hierarchy descriptor ? */
    /* XXX?? target background grid descriptor ? */
    /* XXX?? video window descriptor ? */
    /* XXX?? ISO 639 language descriptor ? */

#ifdef STATS
    /* Stats */
    count_t                 c_bytes;                     /* total bytes read */
    count_t                 c_payload_bytes;/* total of payload useful bytes */
    count_t                 c_packets;                 /* total packets read */
    count_t                 c_invalid_packets;       /* invalid packets read */
    /* XXX?? ... other stats */
#endif
} es_descriptor_t;

/* Special PID values - note that the PID is only on 13 bits, and that values
 * greater than 0x1fff have no meaning in a stream */
#define PROGRAM_ASSOCIATION_TABLE_PID   0x0000
#define CONDITIONNAL_ACCESS_TABLE_PID   0x0001                   /* not used */
#define EMPTY_PID                       0xffff    /* empty record in a table */

/* ES streams types - see ISO/IEC 13818-1 table 2-29 numbers */
#define MPEG1_VIDEO_ES          0x01
#define MPEG2_VIDEO_ES          0x02
#define MPEG1_AUDIO_ES          0x03
#define MPEG2_AUDIO_ES          0x04
#define AC3_AUDIO_ES            0x81
#define DVD_SPU_ES             0x82           /* 0x82 might violate the norm */

/*****************************************************************************
 * program_descriptor_t
 *****************************************************************************
 * Describes a program and list associated elementary streams. It is build by
 * the PSI decoder upon the informations carried in program map sections
 *****************************************************************************/
typedef struct
{
    /* Program characteristics */
    u16                     i_number;                      /* program number */
    u8                      i_version;                     /* version number */
    boolean_t               b_is_ok;       /* Is the description up to date ?*/
    u16                     i_pcr_pid;                             /* PCR ES */

    int i_es_number;
    es_descriptor_t **      ap_es;                /* array of pointers to ES */

#ifdef DVB_EXTENSIONS
    /* Service Descriptor (program name) */
    u8                      i_srv_type;
    char*                   psz_srv_name;
#endif

    /* XXX?? target background grid descriptor ? */
    /* XXX?? video window descriptor ? */
    /* XXX?? ISO 639 language descriptor ? */

#ifdef STATS
    /* Stats */
    /* XXX?? ...stats */
#endif
} pgrm_descriptor_t;

/*****************************************************************************
 * pcr_descriptor_t
 *****************************************************************************
 * Contains informations used to synchronise the decoder with the server
 *****************************************************************************/

typedef struct pcr_descriptor_struct
{
    /* system_date = PTS_date + delta_pcr + delta_absolute */
    mtime_t                 delta_pcr;
    mtime_t                 delta_absolute;

    mtime_t                 last_pcr;

    u32                     i_synchro_state;
    count_t                 c_average_count;
                           /* counter used to compute dynamic average values */
} pcr_descriptor_t;

/*****************************************************************************
 * stream_descriptor_t
 *****************************************************************************
 * Describes a transport stream and list its associated programs. Build upon
 * the informations carried in program association sections
 *****************************************************************************/
typedef struct
{
    u16                     i_stream_id;                        /* stream id */

    /* Program Association Table status */
    u8                      i_PAT_version;                 /* version number */
    boolean_t               b_is_PAT_complete;       /* Is the PAT complete ?*/
    u8                      i_known_PAT_sections;
                                     /* Number of section we received so far */
    byte_t                  a_known_PAT_sections[32];
                                                /* Already received sections */

    /* Program Map Table status */
    boolean_t               b_is_PMT_complete;       /* Is the PMT complete ?*/
    u8                      i_known_PMT_sections;
                                     /* Number of section we received so far */
    byte_t                  a_known_PMT_sections[32];
                                                /* Already received sections */

    /* Service Description Table status */
    u8                      i_SDT_version;                 /* version number */
    boolean_t               b_is_SDT_complete;       /* Is the SDT complete ?*/
    u8                      i_known_SDT_sections;
                                     /* Number of section we received so far */
    byte_t                  a_known_SDT_sections[32];
                                                /* Already received sections */

    /* Programs description */
    int i_pgrm_number;                   /* Number of program number we have */
    pgrm_descriptor_t **    ap_programs;        /* Array of pointers to pgrm */

#ifdef STATS
    /* Stats */
    /* XXX?? ...stats */
#endif
} stream_descriptor_t;

/*****************************************************************************
 * input_netlist_t
 *****************************************************************************/
typedef struct
{
    vlc_mutex_t             lock;               /* netlist modification lock */
    struct iovec            p_ts_free[INPUT_MAX_TS + INPUT_TS_READ_ONCE];
                                          /* FIFO or LIFO of free TS packets */
    ts_packet_t *           p_ts_packets;
                              /* pointer to the first TS packet we allocated */

    pes_packet_t *          p_pes_free[INPUT_MAX_PES + 1];
                                         /* FIFO or LIFO of free PES packets */
    pes_packet_t *          p_pes_packets;
                             /* pointer to the first PES packet we allocated */

    /* To use the efficiency of the scatter/gather IO operations. We implemented
     * it in 2 ways, as we don't know yet which one is better : as a FIFO (code
     * simplier) or as a LIFO stack (when we doesn't care of the ordering, this
     * allow to drastically improve the cache performance) */
#ifdef INPUT_LIFO_TS_NETLIST
    int                     i_ts_index;
#else
    int                     i_ts_start, i_ts_end;
#endif
#ifdef INPUT_LIFO_PES_NETLIST
    int                     i_pes_index;
#else
    int                     i_pes_start, i_pes_end;
#endif
} input_netlist_t;



/*****************************************************************************
 * input_thread_t
 *****************************************************************************
 * This structure includes all the local static variables of an input thread,
 * including the netlist and the ES descriptors
 * Note that p_es must be defined as a static table, otherwise we would have to
 * update all reference to it each time the table would be reallocated
 *****************************************************************************/

/* Function pointers used in structure */
typedef int  (input_open_t)     ( p_input_thread_t p_input );
typedef int  (input_read_t)     ( p_input_thread_t p_input, const struct iovec *p_vector,
                                   size_t i_count );
typedef void (input_close_t)    ( p_input_thread_t p_input );

/* Structure */
typedef struct input_thread_s
{
    /* Thread properties and locks */
    boolean_t                   b_die;                         /* 'die' flag */
    boolean_t                   b_error;                         /* deadlock */
    vlc_thread_t                thread_id;        /* id for thread functions */
    vlc_mutex_t                 programs_lock; /* programs modification lock */
    vlc_mutex_t                 es_lock;             /* es modification lock */
    int *                       pi_status;          /* temporary status flag */

    /* Input method description */
    int                         i_method;                    /* input method */
    int                         i_handle;          /* file/socket descriptor */
    char *                      psz_source;                        /* source */
    int                         i_port;                     /* port number */
    int                         i_vlan;                /* id for vlan method */
    input_open_t *              p_Open;              /* opener of the method */
    input_read_t *              p_Read;                  /* reading function */
    input_close_t *             p_Close;              /* destroying function */

    /* General stream description */
    stream_descriptor_t *   p_stream;                          /* PAT tables */
    es_descriptor_t         p_es[INPUT_MAX_ES];/* carried elementary streams */
    pcr_descriptor_t *      p_pcr;    /* PCR struct used for synchronisation */

    /* List of streams to demux */
    es_descriptor_t *       pp_selected_es[INPUT_MAX_SELECTED_ES];

    /* Netlists */
    input_netlist_t         netlist;                            /* see above */

    /* Default settings for spawned decoders */
    p_aout_thread_t             p_aout;     /* audio output thread structure */
    p_vout_thread_t             p_vout;               /* video output thread */

#ifdef STATS
    /* Statistics */
    count_t                     c_loops;                  /* number of loops */
    count_t                     c_bytes;                       /* bytes read */
    count_t                     c_payload_bytes;     /* payload useful bytes */
    count_t                     c_packets_read;              /* packets read */
    count_t                     c_packets_trashed;        /* trashed packets */
#endif
} input_thread_t;

/* Input methods */
#define INPUT_METHOD_NONE           0            /* input thread is inactive */
#define INPUT_METHOD_TS_FILE       10       /* TS stream is read from a file */
#define INPUT_METHOD_TS_UCAST      20                      /* TS UDP unicast */
#define INPUT_METHOD_TS_MCAST      21                    /* TS UDP multicast */
#define INPUT_METHOD_TS_BCAST      22                    /* TS UDP broadcast */
#define INPUT_METHOD_TS_VLAN_BCAST 32         /* TS UDP broadcast with VLANs */

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
input_thread_t *input_CreateThread      ( int i_method, char *psz_source, int i_port,
                                          int i_vlan, p_vout_thread_t p_vout,
                                          p_aout_thread_t p_aout, int *pi_status );
void            input_DestroyThread     ( input_thread_t *p_input, int *pi_status );


int             input_OpenAudioStream   ( input_thread_t *p_input, int i_pid );
void            input_CloseAudioStream  ( input_thread_t *p_input, int i_pid );
int             input_OpenVideoStream   ( input_thread_t *p_input, int i_pid );
void            input_CloseVideoStream  ( input_thread_t *p_input, int i_pid );
