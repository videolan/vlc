/*****************************************************************************
 * netlist_t: structure to manage a netlist
 *****************************************************************************/
typedef struct netlist_s
{
    vlc_mutex_t          lock;

    /* Buffers */
    byte_t *                p_buffers;                 /* Big malloc'ed area */
    data_packet_t *         p_data;                        /* malloc'ed area */
    pes_packet_t *          p_pes;                         /* malloc'ed area */

    /* FIFOs of free packets */
    data_packet_t **        pp_free_data;
    pes_packet_t **         pp_free_pes;
    struct iovec *          p_free_iovec;
    
    /* FIFO size */
    unsigned int            i_nb_pes;
    unsigned int            i_nb_data;

    /* Index */
    
    unsigned int            i_data_start, i_data_end;
    unsigned int            i_pes_start, i_pes_end;
    unsigned int            i_iovec_start, i_iovec_end;
} netlist_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
int                     input_NetlistInit( struct input_thread_s *,
                                           int i_nb_data, int i_nb_pes,
                                           size_t i_buffer_size );
struct iovec *          input_NetlistGetiovec( void * );
void                    input_NetlistMviovec( void *, size_t );
struct data_packet_s *  input_NetlistNewPacket( void * );
struct pes_packet_s *   input_NetlistNewPES( void * );
void            input_NetlistDeletePacket( void *, struct data_packet_s * );
void            input_NetlistDeletePES( void *, struct pes_packet_s * );
void            input_NetlistEnd( struct input_thread_s * );

