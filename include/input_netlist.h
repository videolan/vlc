/*******************************************************************************
 * input_netlist.h: netlist interface
 * (c)1998 VideoLAN
 *******************************************************************************
 * The netlists are an essential part of the input structure. We maintain a
 * list of free TS packets and free PES packets to avoid continuous malloc
 * and free.
 *******************************************************************************/

/******************************************************************************
 * Prototypes
 ******************************************************************************/
int input_NetlistOpen( input_thread_t *p_input );
void input_NetlistClean( input_thread_t *p_input );
/* ?? implement also a "normal" (non inline, non static) function in input_netlist.c,
   which will be used when inline is disabled */
/* ?? test */ static __inline__ void input_NetlistFreePES( input_thread_t *p_input,
                                  pes_packet_t *p_pes_packet );
static __inline__ void input_NetlistFreeTS( input_thread_t *p_input,
                                 ts_packet_t *p_ts_packet );
static __inline__ pes_packet_t* input_NetlistGetPES( input_thread_t *p_input );

/*******************************************************************************
 * input_NetlistFreePES: add a PES packet to the netlist
 *******************************************************************************
 * Add a PES packet to the PES netlist, so that the packet can immediately be
 * reused by the demultiplexer. We put this function directly in the .h file,
 * because it is very frequently called.
 *******************************************************************************/
static __inline__ void input_NetlistFreePES( input_thread_t *p_input,
                                  pes_packet_t *p_pes_packet )
{
    int             i_dummy;
    ts_packet_t *   p_ts_packet;

    ASSERT(p_pes_packet);

    /* We will be playing with indexes, so we take a lock. */
    vlc_mutex_lock( &p_input->netlist.lock );

    /* Free all TS packets in this PES structure. */
    p_ts_packet = p_pes_packet->p_first_ts;
    for( i_dummy = 0; i_dummy < p_pes_packet->i_ts_packets; i_dummy++ )
    {
	ASSERT(p_ts_packet);

#ifdef INPUT_LIFO_TS_NETLIST
        p_input->netlist.i_ts_index--;
        p_input->netlist.p_ts_free[p_input->netlist.i_ts_index].iov_base
                             = p_ts_packet;
#else /* FIFO */
        p_input->netlist.p_ts_free[p_input->netlist.i_ts_end].iov_base
                             = p_ts_packet;
        p_input->netlist.i_ts_end++;
        p_input->netlist.i_ts_end &= INPUT_MAX_TS; /* loop */
#endif
        p_ts_packet = p_ts_packet->p_next_ts;
    }
    
    /* Free the PES structure. */
#ifdef INPUT_LIFO_PES_NETLIST
    p_input->netlist.i_pes_index--;
    p_input->netlist.p_pes_free[p_input->netlist.i_pes_index] = p_pes_packet;
#else /* FIFO */
    p_input->netlist.p_pes_free[p_input->netlist.i_pes_end] = p_pes_packet;
    p_input->netlist.i_pes_end++;
    p_input->netlist.i_pes_end &= INPUT_MAX_PES; /* loop */
#endif

    vlc_mutex_unlock( &p_input->netlist.lock );
}

/*******************************************************************************
 * input_NetlistFreeTS: add a TS packet to the netlist
 *******************************************************************************
 * Add a TS packet to the TS netlist, so that the packet can immediately be
 * reused by the demultiplexer. Shouldn't be called by other threads (they
 * should only use input_FreePES.
 *******************************************************************************/
static __inline__ void input_NetlistFreeTS( input_thread_t *p_input,
                                            ts_packet_t *p_ts_packet )
{
    ASSERT(p_ts_packet);

    /* We will be playing with indexes, so we take a lock. */
    vlc_mutex_lock( &p_input->netlist.lock );

    /* Free the TS structure. */
#ifdef INPUT_LIFO_TS_NETLIST
    p_input->netlist.i_ts_index--;
    p_input->netlist.p_ts_free[p_input->netlist.i_ts_index].iov_base = p_ts_packet;
#else /* FIFO */
    p_input->netlist.p_ts_free[p_input->netlist.i_ts_end].iov_base = p_ts_packet;
    p_input->netlist.i_ts_end++;
    p_input->netlist.i_ts_end &= INPUT_MAX_TS; /* loop */
#endif

    vlc_mutex_unlock( &p_input->netlist.lock );
}

/*******************************************************************************
 * input_NetlistGetPES: remove a PES packet from the netlist
 *******************************************************************************
 * Add a TS packet to the TS netlist, so that the packet can immediately be
 * reused by the demultiplexer. Shouldn't be called by other threads (they
 * should only use input_FreePES.
 *******************************************************************************/
static __inline__ pes_packet_t* input_NetlistGetPES( input_thread_t *p_input )
{
    pes_packet_t *          p_pes_packet;

#ifdef INPUT_LIFO_PES_NETLIST
    /* i_pes_index might be accessed by a decoder thread to give back a 
     * packet. */
    vlc_mutex_lock( &p_input->netlist.lock );

    /* Verify that we still have PES packet in the netlist */
    if( (INPUT_MAX_PES - p_input->netlist.i_pes_index ) <= 1 )
    {
        intf_ErrMsg("input error: PES netlist is empty !\n");
        return( NULL );
    }

    /* Fetch a new PES packet */
    p_pes_packet = p_input->netlist.p_pes_free[p_input->netlist.i_pes_index];
    p_input->netlist.i_pes_index++;
    vlc_mutex_unlock( &p_input->netlist.lock );

#else /* FIFO */
    /* No need to lock, since we are the only ones accessing i_pes_start. */

    /* Verify that we still have PES packet in the netlist */
    if( ((p_input->netlist.i_pes_end -1 - p_input->netlist.i_pes_start) & INPUT_MAX_PES) <= 1 )
    {
        intf_ErrMsg("input error: PES netlist is empty !\n");
        return( NULL );
    }

    p_pes_packet = p_input->netlist.p_pes_free[p_input->netlist.i_pes_start];
    p_input->netlist.i_pes_start++;
    p_input->netlist.i_pes_start &= INPUT_MAX_PES; /* loop */
#endif /* netlist type */

    /* Initialize PES flags. */
    p_pes_packet->b_data_loss = 0;
    p_pes_packet->b_data_alignment = 0;
    p_pes_packet->b_has_pts = 0;
    p_pes_packet->b_random_access = 0;
    p_pes_packet->b_discard_payload = 0;
    p_pes_packet->i_pes_size = 0;
    p_pes_packet->i_ts_packets = 0;
    p_pes_packet->p_first_ts = NULL;
    p_pes_packet->p_last_ts = NULL;

    return( p_pes_packet );
}
