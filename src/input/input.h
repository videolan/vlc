/* Communication plugin -> input */

#define INPUT_READ_ONCE     7   /* We live in a world dominated by Ethernet. *
                                 * Ethernet MTU is 1500 bytes, so in a UDP   *
                                 * packet we can put : 1500/188 = 7 TS       *
                                 * packets. Have a nice day and merry Xmas.  */

/*****************************************************************************
 * input_capabilities_t
 *****************************************************************************
 * This structure gives pointers to the useful methods of the plugin
 *****************************************************************************/
typedef struct input_capabilities_s
{
    /* Plugin properties */
    int                     i_weight; /* for a given stream type, the plugin *
                                       * with higher weight will be used     */

    /* Init/End */
    int                  (* pf_probe)( struct input_thread_s * );
    void                 (* pf_init)( struct input_thread_s * );
    void                 (* pf_end)( struct input_thread_s * );

    /* Read & Demultiplex */
    void                 (* pf_read)( struct input_thread_s *,
                          struct data_packet_s * pp_packets[INPUT_READ_ONCE] );
    void                 (* pf_demux)( struct input_thread_s *,
                                       struct data_packet_s * );

    /* Packet management facilities */
    struct data_packet_s *(* pf_new_packet)( void *, size_t );
    struct pes_packet_s *(* pf_new_pes)( void * );
    void                 (* pf_delete_packet)( void *,
                                               struct data_packet_s * );
    void                 (* pf_delete_pes)( void *, struct pes_packet_s * );

    /* Stream control capabilities */
    int                  (* pf_rewind)( struct input_thread_s * );
                                           /* NULL if we don't support going *
                                            * backwards (it's gonna be fun)  */
    int                  (* pf_seek)( struct input_thread_s *, off_t );
} input_capabilities_t;

/*****************************************************************************
 * Prototypes from input_ext-dec.c
 *****************************************************************************/
void InitBitstream  ( struct bit_stream_s *, struct decoder_fifo_s * );
void NextDataPacket ( struct bit_stream_s * );

/*****************************************************************************
 * Prototypes from input_programs.c
 *****************************************************************************/
void input_InitStream( struct input_thread_s *, size_t );
struct pgrm_descriptor_s * input_AddProgram( struct input_thread_s *,
                                             u16, size_t );
void input_DelProgram( struct input_thread_s *, u16 );
struct es_descriptor_s * input_AddES( struct input_thread_s *,
                                      struct pgrm_descriptor_s *, u16,
                                      size_t );
void input_DelES( struct input_thread_s *, u16 );
int input_SelectES( struct input_thread_s *, struct es_descriptor_s * );

