/*****************************************************************************
 * thread_ts_data_t: extension of input_thread_t
 *****************************************************************************/
typedef struct thread_ts_data_s
{
    /* To use the efficiency of the scatter/gather IO operations without
     * malloc'ing all the time, we implemented a FIFO of free data packets.
     */
    vlc_mutex_lock          lock;
    struct iovec            p_free_iovec[INPUT_MAX_TS + INPUT_TS_READ_ONCE];
    data_packet_t *         p_free_ts[INPUT_MAX_TS + INPUT_TS_READ_ONCE];
    int                     i_free_start, i_free_end;

    /* The free data packets are stored here : */
    data_packet_t *         p_data_packets;
    byte_t *                p_buffers;
} thread_ts_data_t;
