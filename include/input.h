/*******************************************************************************
 * input.h: input thread interface
 * (c)1999 VideoLAN
 *******************************************************************************/

/* needs : "netlist.h", "config.h" */

/* ?? missing: 
 *              tables version control */

/*******************************************************************************
 * External structures
 *******************************************************************************
 * These structures, required here only as pointers destinations, are declared 
 * in other headers.
 *******************************************************************************/
struct video_cfg_s;                          /* video configuration descriptor */
struct vout_thread_s;                                   /* video output thread */
struct stream_descriptor_s;                                      /* PSI tables */

/*******************************************************************************
 * Constants related to input
 *******************************************************************************/
#define TS_PACKET_SIZE      188                         /* size of a TS packet */
#define PES_HEADER_SIZE     14       /* size of the first part of a PES header */
#define PSI_SECTION_SIZE    4096              /* Maximum size of a PSI section */

/*******************************************************************************
 * ts_packet_t
 *******************************************************************************
 * Describe a TS packet.
 *******************************************************************************/
typedef struct ts_packet_struct
{
    /* Nothing before this line, the code relies on that */
    byte_t                  buffer[TS_PACKET_SIZE];      /* raw TS data packet */

    /* Decoders information */
    unsigned int            i_payload_start;
                                    /* start of the PES payload in this packet */
    unsigned int            i_payload_end;                      /* guess ? :-) */

    /* Used to chain the TS packets that carry data for a same PES or PSI */
    struct ts_packet_struct *  p_prev_ts;
    struct ts_packet_struct *  p_next_ts;
} ts_packet_t;


/*******************************************************************************
 * pes_packet_t
 *******************************************************************************
 * Describes an PES packet, with its properties, and pointers to the TS packets
 * containing it.
 *******************************************************************************/
typedef struct
{
    /* PES properties */
    boolean_t               b_data_loss;    /* The previous (at least) PES packet
             * has been lost. The decoders will have to find a way to recover. */
    boolean_t               b_data_alignment;  /* used to find the beginning of a
                                                * video or audio unit          */
    boolean_t               b_has_pts;         /* is the following field set ? */
    u32                     i_pts;   /* the PTS for this packet (if set above) */
    boolean_t               b_random_access;
              /* if TRUE, in the payload of this packet, there is the first byte 
               * of a video sequence header, or the first byte of an audio frame.
               */
    u8                      i_stream_id;                /* payload type and id */
    int                     i_pes_size;      /* size of the current PES packet */
    int                     i_ts_packets;  /* number of TS packets in this PES */

    /* Demultiplexer environment */
    boolean_t               b_discard_payload;    /* is the packet messed up ? */
    byte_t *                p_pes_header;         /* pointer to the PES header */
    byte_t *                p_pes_header_save;             /* temporary buffer */

    /* Pointers to TS packets (TS packets are then linked by the p_prev_ts and 
       p_next_ts fields of the ts_packet_t struct) */
    ts_packet_t *           p_first_ts;    /* The first TS packet containing this
                                            * PES (used by decoders). */
    ts_packet_t *           p_last_ts;  /* The last TS packet gathered at present
                                         * (used by the demultiplexer). */
} pes_packet_t;


/*******************************************************************************
 * psi_section_t
 *******************************************************************************
 * Describes a PSI section. Beware, it doesn't contain pointers to the TS
 * packets that contain it as for a PES, but the data themselves
 *******************************************************************************/
typedef struct
{
    byte_t        buffer[PSI_SECTION_SIZE];
  
    boolean_t     b_running_section;     /* Is there a section being decoded ? */

    u16 i_length;
    u16 i_current_position;
} psi_section_t;


/*******************************************************************************
 * es_descriptor_t: elementary stream descriptor
 *******************************************************************************
 * Describes an elementary stream, and includes fields required to handle and
 * demultiplex this elementary stream.
 *******************************************************************************/
typedef struct
{   
    u16                     i_id;             /* stream ID, PID for TS streams */
    u8                      i_type;                             /* stream type */

    boolean_t               b_pcr;          /* does the stream include a PCR ? */
    /* ?? b_pcr will be replaced by something else: since a PCR can't be shared
     * between several ES, we will probably store the PCR fields directly here,
     * and one of those fields will probably (again) be used as a test of the
     * PCR presence */
    boolean_t               b_psi;   /* does the stream have to be handled by the
                                                                 PSI decoder ? */
    /* Markers */
    int                     i_continuity_counter;
    boolean_t               b_discontinuity;
    boolean_t               b_random;

    /* PES packets */
    pes_packet_t *          p_pes_packet;
                                        /* current PES packet we are gathering */

    /* PSI packets */
    psi_section_t *         p_psi_section;            /* idem for a PSI stream */

    /* Decoder informations */
    void *                  p_dec;      /* p_dec is void *, since we don't know a
                                         * priori whether it is adec_thread_t or
                                         * vdec_thread_t. We will use explicit
                                         * casts. */

    /* ?? video stream descriptor ? */
    /* ?? audio stream descriptor ? */
    /* ?? hierarchy descriptor ? */
    /* ?? target background grid descriptor ? */
    /* ?? video window descriptor ? */
    /* ?? ISO 639 language descriptor ? */

#ifdef STATS
    /* Stats */
    count_t                 c_bytes;                       /* total bytes read */
    count_t                 c_payload_bytes; /* total of payload usefull bytes */
    count_t                 c_packets;                   /* total packets read */
    count_t                 c_invalid_packets;         /* invalid packets read */
    /* ?? ... other stats */
#endif
} es_descriptor_t;

/* Special PID values - note that the PID is only on 13 bits, and that values
 * greater than 0x1fff have no meaning in a stream */
#define PROGRAM_ASSOCIATION_TABLE_PID   0x0000 
#define CONDITIONNAL_ACCESS_TABLE_PID   0x0001                     /* not used */
#define EMPTY_PID                       0xffff      /* empty record in a table */

/* ES streams types - see ISO/IEC 13818-1 table 2-29 numbers */
#define MPEG1_VIDEO_ES          0x01
#define MPEG2_VIDEO_ES          0x02
#define MPEG1_AUDIO_ES          0x03
#define MPEG2_AUDIO_ES          0x04


/*******************************************************************************
 * program_descriptor_t
 *******************************************************************************
 * Describes a program and list associated elementary streams. It is build by
 * the PSI decoder upon the informations carried in program map sections 
 *******************************************************************************/
typedef struct
{
    /* Program characteristics */
    u16                     i_number;                        /* program number */
    u8                      i_version;                       /* version number */ 
    boolean_t               b_is_ok;         /* Is the description up to date ?*/
    u16                     i_pcr_pid;                               /* PCR ES */

    int i_es_number;
    es_descriptor_t **      ap_es;                  /* array of pointers to ES */

#ifdef DVB_EXTENSIONS
    /* Service Descriptor (program name) */
    u8                      i_srv_type;
    char*                   psz_srv_name;
#endif

    /* ?? target background grid descriptor ? */
    /* ?? video window descriptor ? */
    /* ?? ISO 639 language descriptor ? */

#ifdef STATS
    /* Stats */
    /* ?? ...stats */
#endif
} pgrm_descriptor_t;

/*******************************************************************************
 * pcr_descriptor_t
 *******************************************************************************
 * Contains informations used to synchronise the decoder with the server
 * Only input_PcrDecode() is allowed to modify it
 *******************************************************************************/

typedef struct pcr_descriptor_struct
{
    pthread_mutex_t         lock;                     /* pcr modification lock */

    s64                     delta_clock;
                            /* represents decoder_time - pcr_time in usecondes */
    count_t                 c_average;
                             /* counter used to compute dynamic average values */
#ifdef STATS
    /* Stats */
    count_t     c_average_jitter;
    s64         max_jitter;                    /* the evalueted maximum jitter */
    s64         average_jitter;                /* the evalueted average jitter */
    count_t     c_pcr;            /* the number of PCR which have been decoded */
#endif    
} pcr_descriptor_t;

/*******************************************************************************
 * stream_descriptor_t
 *******************************************************************************
 * Describes a transport stream and list its associated programs. Build upon
 * the informations carried in program association sections
 *******************************************************************************/
typedef struct
{
    u16                     i_stream_id;                                 /* stream id */

    /* Program Association Table status */
    u8                      i_PAT_version;                     /* version number */ 
    boolean_t               b_is_PAT_complete;           /* Is the PAT complete ?*/
    u8                      i_known_PAT_sections;   /* Number of section we received so far */
    byte_t                  a_known_PAT_sections[32]; /* Already received sections */

    /* Program Map Table status */
    boolean_t               b_is_PMT_complete;           /* Is the PMT complete ?*/
    u8                      i_known_PMT_sections;   /* Number of section we received so far */
    byte_t                  a_known_PMT_sections[32]; /* Already received sections */

    /* Service Description Table status */
    u8                      i_SDT_version;                     /* version number */ 
    boolean_t               b_is_SDT_complete;           /* Is the SDT complete ?*/
    u8                      i_known_SDT_sections;   /* Number of section we received so far */
    byte_t                  a_known_SDT_sections[32]; /* Already received sections */

    /* Programs description */
    int i_pgrm_number;                     /* Number of program number we have */
    pgrm_descriptor_t **    ap_programs;             /* Array of pointers to pgrm */

#ifdef STATS
    /* Stats */
    /* ?? ...stats */
#endif
} stream_descriptor_t;

/*******************************************************************************
 * input_netlist_t
 *******************************************************************************/
typedef struct
{
    pthread_mutex_t         lock;                  /* netlist modification lock */
    struct iovec            p_ts_free[INPUT_MAX_TS + INPUT_TS_READ_ONCE];
                                             /* FIFO or LIFO of free TS packets */
    ts_packet_t *           p_ts_packets;
                                 /* pointer to the first TS packet we allocated */

    pes_packet_t *          p_pes_free[INPUT_MAX_PES + 1];
                                            /* FIFO or LIFO of free PES packets */
    pes_packet_t *          p_pes_packets;
                                /* pointer to the first PES packet we allocated */

    /* To use the efficiency of the scatter/gather IO operations. We implemented it
     * in 2 ways, as we don't know yet which one is better : as a FIFO (code
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

/*******************************************************************************
 * input_thread_t
 *******************************************************************************
 * This structure includes all the local static variables of an input thread,
 * including the netlist and the ES descriptors
 * Note that p_es must be defined as a static table, otherwise we would have to
 * update all reference to it each time the table would be reallocated 
 *******************************************************************************/

/* function pointers */
struct input_thread_struct;
struct input_cfg_struct;
typedef int (*f_open_t)( struct input_thread_struct *, struct input_cfg_struct *);
typedef int (*f_read_t)( struct input_thread_struct *, const struct iovec *,
                         size_t );
typedef void (*f_clean_t)( struct input_thread_struct * );

typedef struct input_thread_struct
{
    /* Thread properties and locks */
    boolean_t               b_die;                               /* 'die' flag */
    boolean_t               b_error;                               /* deadlock */
    pthread_t               thread_id;             /* id for pthread functions */
    pthread_mutex_t         programs_lock;       /* programs modification lock */
    pthread_mutex_t         es_lock;                   /* es modification lock */

    /* Input method description */
    int                     i_method;                          /* input method */
    int                     i_handle;                /* file/socket descriptor */
    int                     i_vlan_id;                   /* id for vlan method */
    f_open_t                p_open;     /* pointer to the opener of the method */
    f_read_t                p_read;         /* pointer to the reading function */
    f_clean_t               p_clean;     /* pointer to the destroying function */

    /* General stream description */
    stream_descriptor_t *   p_stream;                            /* PAT tables */
    es_descriptor_t         p_es[INPUT_MAX_ES];  /* carried elementary streams */
    pcr_descriptor_t *      p_pcr;      /* PCR struct used for synchronisation */

    /* List of streams to demux */
    es_descriptor_t *       pp_selected_es[INPUT_MAX_SELECTED_ES];
    
    /* Netlists */
    input_netlist_t         netlist;                              /* see above */

    /* ?? default settings for new decoders */
    struct aout_thread_s *      p_aout;      /* audio output thread structure */

#ifdef STATS
    /* Stats */
    count_t                 c_loops;                        /* number of loops */
    count_t                 c_bytes;                    /* total of bytes read */
    count_t                 c_payload_bytes;  /* total of payload useful bytes */
    count_t                 c_ts_packets_read;        /* total of packets read */
    count_t                 c_ts_packets_trashed;  /* total of trashed packets */
    /* ?? ... other stats */
#endif
} input_thread_t;

/* Input methods */
#define INPUT_METHOD_NONE           0              /* input thread is inactive */
#define INPUT_METHOD_TS_FILE       10         /* TS stream is read from a file */
#define INPUT_METHOD_TS_UCAST      20                        /* TS UDP unicast */
#define INPUT_METHOD_TS_MCAST      21                      /* TS UDP multicast */
#define INPUT_METHOD_TS_BCAST      22                      /* TS UDP broadcast */
#define INPUT_METHOD_TS_VLAN_BCAST 32           /* TS UDP broadcast with VLANs */

/*******************************************************************************
 * input_cfg_t: input thread configuration structure
 *******************************************************************************
 * This structure is passed as a parameter to input_CreateTtread(). It includes
 * several fields describing potential properties of a new object. 
 * The 'i_properties' field allow to set only a subset of the required 
 * properties, asking the called function to use default settings for
 * the other ones.
 *******************************************************************************/
typedef struct input_cfg_struct
{
    u64     i_properties;

    /* Input method properties */
    int     i_method;                                          /* input method */
    char *  psz_filename;                                          /* filename */
    char *  psz_hostname;                                   /* server hostname */
    char *  psz_ip;                                               /* server IP */
    int     i_port;                                                    /* port */
    int     i_vlan;                                             /* vlan number */

    /* ??... default settings for new decoders */
    struct aout_thread_s *      p_aout;      /* audio output thread structure */

} input_cfg_t;

/* Properties flags */
#define INPUT_CFG_METHOD    (1 << 0)
#define INPUT_CFG_FILENAME  (1 << 4)
#define INPUT_CFG_HOSTNAME  (1 << 8)
#define INPUT_CFG_IP        (1 << 9)
#define INPUT_CFG_PORT      (1 << 10)
#define INPUT_CFG_VLAN      (1 << 11)

/******************************************************************************
 * Prototypes
 ******************************************************************************/
input_thread_t *input_CreateThread      ( input_cfg_t *p_cfg );
void            input_DestroyThread     ( input_thread_t *p_input );

int             input_OpenAudioStream   ( input_thread_t *p_input, int i_pid
                                          /* ?? , struct audio_cfg_s * p_cfg */ );
void            input_CloseAudioStream  ( input_thread_t *p_input, int i_pid );
int             input_OpenVideoStream   ( input_thread_t *p_input, 
                                          struct vout_thread_s *p_vout, struct video_cfg_s * p_cfg );
void            input_CloseVideoStream  ( input_thread_t *p_input, int i_pid );

/* ?? settings functions */
/* ?? info functions */
