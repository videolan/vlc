/* Structures exported to interface, input and decoders */

/*****************************************************************************
 * stream_ctrl_t
 *****************************************************************************
 * Describe the state of a program stream.
 *****************************************************************************/
typedef struct stream_ctrl_s
{
    vlc_mutex_t             control_lock;

    int                     i_status;
    /* if i_status == FORWARD_S or BACKWARD_S */
    int                     i_rate;
    s64                     i_ref_sysdate;
    s64                     i_ref_clock;

    boolean_t               b_mute;
    boolean_t               b_bw;                           /* black & white */
} stream_ctrl_t;

/* Possible status : */
#define PLAYING_S           0
#define PAUSE_S             1
#define FORWARD_S           2
#define BACKWARD_S          3

#define DEFAULT_RATE        1000
