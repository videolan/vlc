/******************************************************************************
 * generic_decoder.c : generic decoder thread
 * (c)1999 VideoLAN
 ******************************************************************************
 * This decoder provides a way to parse packets which do not belong to any 
 * known stream type, or to redirect packets to files. It can extract PES files 
 * from a multiplexed stream, identify automatically ES in a stream missing 
 * stream informations (i.e. a TS stream without PSI) and update ES tables in
 * its input thread, or just print informations about what it receives in DEBUG 
 * mode.
 * A single generic decoder is able to handle several ES, therefore it can be
 * used as a 'default' decoder by the input thread.
 ******************************************************************************/

/******************************************************************************
 * Preamble
 *******************************************************************************/
#include "vlc.h"

/*#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

#include "config.h"
#include "common.h"
#include "mtime.h"
#include "vlc_thread.h"
#include "thread.h"

#include "intf_msg.h"
#include "debug.h"   */               
/*
#include "input.h"
#include "input_netlist.h"
#include "decoder_fifo.h"

#include "generic_decoder.h"

#include "video.h"
#include "video_output.h"
#include "video_decoder.h"*/

/*
 * Local prototypes
 */
static int      CheckConfiguration  ( gdec_cfg_t *p_cfg );
static int      InitThread          ( gdec_thread_t *p_gdec );
static void     RunThread           ( gdec_thread_t *p_gdec );
static void     ErrorThread         ( gdec_thread_t *p_gdec );
static void     EndThread           ( gdec_thread_t *p_gdec );

static void     IdentifyPES         ( gdec_thread_t *p_gdec, pes_packet_t *p_pes, 
                                      int i_stream_id );
static void     PrintPES            ( pes_packet_t *p_pes, int i_stream_id );

/******************************************************************************
 * gdec_CreateThread: create a generic decoder thread
 ******************************************************************************
 * This function creates a new generic decoder thread, and returns a pointer
 * to its description. On error, it returns NULL.
 * Following configuration properties are used:
 *  GDEC_CFG_ACTIONS    (required)
 * ??
 ******************************************************************************/
gdec_thread_t * gdec_CreateThread( gdec_cfg_t *p_cfg, input_thread_t *p_input,
                                   int *pi_status )
{
    gdec_thread_t * p_gdec;                              /* thread descriptor */
    int             i_status;                                /* thread status */
    
    /*
     * Check configuration 
     */
    if( CheckConfiguration( p_cfg ) )
    {
        return( NULL );
    }

    /* Allocate descriptor and initialize flags */
    p_gdec = (gdec_thread_t *) malloc( sizeof(gdec_thread_t) );
    if( p_gdec == NULL )                                             /* error */
    {
        return( NULL );
    }

    /* Copy configuration */
    p_gdec->i_actions = p_cfg->i_actions;    
    /* ?? */

    /* Set status */
    p_gdec->pi_status = (pi_status != NULL) ? pi_status : &i_status;
    *p_gdec->pi_status = THREAD_CREATE;

    /* Initialize flags */
    p_gdec->b_die = 0;
    p_gdec->b_error = 0;    
    p_gdec->b_active = 1;

    /* Create thread */
    if( vlc_thread_create( &p_gdec->thread_id, "generic decoder", (vlc_thread_func)RunThread, (void *) p_gdec) )
    {
        intf_ErrMsg("gdec error: %s\n", strerror(ENOMEM));
        intf_DbgMsg("failed\n");        
        free( p_gdec );
        return( NULL );
    }   

    /* If status is NULL, wait until the thread is created */
    if( pi_status == NULL )
    {
        do
        {            
            msleep( THREAD_SLEEP );
        }while( (i_status != THREAD_READY) && (i_status != THREAD_ERROR) 
                && (i_status != THREAD_FATAL) );
        if( i_status != THREAD_READY )
        {
            intf_DbgMsg("failed\n");            
            return( NULL );            
        }        
    }

    intf_DbgMsg("succeeded -> %p\n", p_gdec);    
    return( p_gdec );
}

/******************************************************************************
 * gdec_DestroyThread: destroy a generic decoder thread
 ******************************************************************************
 * Destroy a terminated thread. This function will return 0 if the thread could
 * be destroyed, and non 0 else. The last case probably means that the thread
 * was still active, and another try may succeed.
 ******************************************************************************/
void gdec_DestroyThread( gdec_thread_t *p_gdec, int *pi_status )
{
    int     i_status;                                        /* thread status */

    /* Set status */
    p_gdec->pi_status = (pi_status != NULL) ? pi_status : &i_status;
    *p_gdec->pi_status = THREAD_DESTROY;
     
    /* Request thread destruction */
    p_gdec->b_die = 1;
    /* Make sure the decoder thread leaves the GetByte() function */
    vlc_mutex_lock( &(p_gdec->fifo.data_lock) );
    vlc_cond_signal( &(p_gdec->fifo.data_wait) );
    vlc_mutex_unlock( &(p_gdec->fifo.data_lock) );

    /* If status is NULL, wait until thread has been destroyed */
    if( pi_status )
    {
        do
        {
            msleep( THREAD_SLEEP );
        }while( (i_status != THREAD_OVER) && (i_status != THREAD_ERROR) 
                && (i_status != THREAD_FATAL) );   
    }

    intf_DbgMsg("%p -> succeeded\n", p_gdec);
}

/* following functions are local */

/******************************************************************************
 * CheckConfiguration: check gdec_CreateThread() configuration
 ******************************************************************************
 * Set default parameters where required. In DEBUG mode, check if configuration
 * is valid.
 ******************************************************************************/
static int CheckConfiguration( gdec_cfg_t *p_cfg )
{
#ifdef DEBUG
    /* Actions (required) */
    if( !(p_cfg->i_properties & GDEC_CFG_ACTIONS) )
    {
        return( 1 );        
    }    
#endif

    return( 0 );
}

/******************************************************************************
 * InitThread: initialize gdec thread
 ******************************************************************************
 * This function is called from RunThread and performs the second step of the
 * initialization. It returns 0 on success. Note that the thread's flag are not
 * modified inside this function.
 ******************************************************************************/
static int InitThread( gdec_thread_t *p_gdec )
{
    /* ?? */

    /* Update status */
    *p_gdec->pi_status = THREAD_START;    

    /* Initialize other properties */
#ifdef STATS
    p_gdec->c_loops = 0;
    p_gdec->c_idle_loops = 0;
    p_gdec->c_pes = 0;
#endif

    /* Mark thread as running and return */
    *p_gdec->pi_status = THREAD_READY;    
    intf_DbgMsg("%p -> succeeded\n", p_gdec);    
    return(0);    
}

/******************************************************************************
 * RunThread: generic decoder thread
 ******************************************************************************
 * Generic decoder thread. This function does only returns when the thread is
 * terminated. 
 ******************************************************************************/
static void RunThread( gdec_thread_t *p_gdec )
{
    pes_packet_t *  p_pes;                                  /* current packet */
    int             i_stream_id;                             /* PES stream id */
    
    /* 
     * Initialize thread and free configuration 
     */
    p_gdec->b_error = InitThread( p_gdec );
    if( p_gdec->b_error )
    {
        free( p_gdec );                                 /* destroy descriptor */
        return;
    }

    /*
     * Main loop - it is not executed if an error occured during
     * initialization
     */
    while( (!p_gdec->b_die) && (!p_gdec->b_error) )
    {
        /* ?? locks à rajouter ? - vérifier les macros (transformer en inline ?) */
        /* ?? on idle loop, increment c_idle_loops */
        while( !DECODER_FIFO_ISEMPTY( p_gdec->fifo ) )
        {            
            p_pes = DECODER_FIFO_START( p_gdec->fifo );
            DECODER_FIFO_INCSTART( p_gdec->fifo );

            /* Extract stream id from packet if required - stream id is used
             * by GDEC_IDENTIFY, GDEC_SAVE_DEMUX and GDEC_PRINT */
            if( p_gdec->i_actions & (GDEC_IDENTIFY | GDEC_SAVE_DEMUX | GDEC_PRINT) )
            {
                i_stream_id = p_pes->p_pes_header[3];                    
            }            

            /* PES identification */
            if( p_gdec->i_actions & GDEC_IDENTIFY )
            {
                IdentifyPES( p_gdec, p_pes, i_stream_id );
            }

            /* PES multiplexed stream saving */
            if( p_gdec->i_actions & GDEC_SAVE )
            {
                /* ?? */
            }

            /* PES demultiplexed stream saving */
            if( p_gdec->i_actions & GDEC_SAVE_DEMUX )
            {
                /* ?? */
            }
                       
            /* PES information printing */
            if( p_gdec->i_actions & GDEC_PRINT )
            {
                PrintPES( p_pes, i_stream_id );
            }                        

            /* Trash PES packet (give it back to fifo) */
            input_NetlistFreePES( p_gdec->p_input, p_pes );            

#ifdef STATS
            p_gdec->c_pes++;            
#endif
        }
#ifdef STATS
        p_gdec->c_loops++;        
#endif        
    } 

    /*
     * Error loop
     */
    if( p_gdec->b_error )
    {
        ErrorThread( p_gdec );        
    }

    /* End of thread */
    EndThread( p_gdec );
}

/******************************************************************************
 * ErrorThread: RunThread() error loop
 ******************************************************************************
 * This function is called when an error occured during thread main's loop. The
 * thread can still receive feed, but must be ready to terminate as soon as
 * possible.
 ******************************************************************************/
static void ErrorThread( gdec_thread_t *p_gdec )
{
    pes_packet_t *  p_pes;                                      /* pes packet */
    
    /* Wait until a `die' order */
    while( !p_gdec->b_die )
    {
        /* Trash all received PES packets */
        while( !DECODER_FIFO_ISEMPTY( p_gdec->fifo ) )
        {            
            p_pes = DECODER_FIFO_START( p_gdec->fifo );
            DECODER_FIFO_INCSTART( p_gdec->fifo );
            input_NetlistFreePES( p_gdec->p_input, p_pes );            
        }

        /* Sleep a while */
        msleep( GDEC_IDLE_SLEEP );                
    }
}

/******************************************************************************
 * EndThread: thread destruction
 ******************************************************************************
 * This function is called when the thread ends after a sucessfull 
 * initialization.
 ******************************************************************************/
static void EndThread( gdec_thread_t *p_gdec )
{
    int *   pi_status;                                       /* thread status */    
    
    /* Store status */
    pi_status = p_gdec->pi_status;    
    *pi_status = THREAD_END;    

#ifdef DEBUG
    /* Check for remaining PES packets */
    /* ?? */
#endif

    /* Destroy thread structures allocated by InitThread */
    free( p_gdec );                                     /* destroy descriptor */

    *pi_status = THREAD_OVER;    
    intf_DbgMsg("%p\n", p_gdec);
}

/******************************************************************************
 * IdentifyPES: identify a PES packet
 ******************************************************************************
 * Update ES tables in the input thread according to the stream_id value. See
 * ISO 13818-1, table 2-18.
 ******************************************************************************/
static void IdentifyPES( gdec_thread_t *p_gdec, pes_packet_t *p_pes, int i_stream_id )
{
    int i_id;                                        /* stream id in es table */
    int i_type;           /* stream type according ISO/IEC 13818-1 table 2-29 */
    
    /* Search where the elementary stream id does come from */
    switch( p_gdec->p_input->i_method )
    {
    case INPUT_METHOD_TS_FILE:                    /* TS methods: id is TS PID */
    case INPUT_METHOD_TS_UCAST:
    case INPUT_METHOD_TS_BCAST:
    case INPUT_METHOD_TS_VLAN_BCAST:
        /* ?? since PID is extracted by demux, it could be usefull to store it
         * in a readable place, i.e. the TS packet descriptor, rather than to
         * re-extract it now */
        i_id = U16_AT(&p_pes->p_first_ts->buffer[1]) & 0x1fff;
        break;        

#ifdef DEBUG
    default:                                             /* unknown id origin */
        intf_DbgMsg("unable to identify PES using input method %d\n", 
                    p_gdec->p_input->i_method );
        break;        
#endif
    }
    
    /* Try to identify PES stream_id - see ISO 13818-1 table 2-18 */
    if( i_stream_id == 0xbd )
    {
        /* Dolby AC-3 stream - might be specific to DVD PS streams */
        i_type = MPEG2_AUDIO_ES;
        intf_DbgMsg("PES %p identified as AUDIO AC3\n", p_pes);
    }
    else if( (i_stream_id & 0xe0) == 0xc0 ) 
    {
        /* ISO/IEC 13818-3 or ISO/IEC 11172-3 audio stream - since there is no
         * way to make the difference between the two possibilities, and since
         * an ISO/IEC 13818-3 is capable of decoding an ISO/IEC 11172-3 stream,
         * the first one is used */
        i_type = MPEG2_AUDIO_ES;
        intf_DbgMsg("PES %p identified as AUDIO MPEG\n", p_pes);
    }
    else if( (i_stream_id & 0xf0) == 0xe0 )
    {
        /* ISO/IEC 13818-2 or ISO/IEC 11172-2 video stream - since there is no
         * way to make the difference between the two possibilities, and since
         * an ISO/IEC 13818-2 is capable of decoding an ISO/IEC 11172-2 stream,
         * the first one is used */        
        i_type = MPEG2_VIDEO_ES;
        intf_DbgMsg("PES %p identified as VIDEO\n", p_pes);
    }
    else
    {
        /* The stream could not be identified - just return */
        intf_DbgMsg("PES %p could not be identified\n", p_pes);
        return;        
    }
    
    /* Update ES table */
    /* ?? */
}

/******************************************************************************
 * PrintPES: print informations about a PES packet
 ******************************************************************************
 * This function will print information about a received PES packet. It is
 * probably usefull only for debugging purposes, or before demultiplexing a
 * stream. It has two different formats, depending of the presence of the DEBUG 
 * symbol.
 ******************************************************************************/
static void PrintPES( pes_packet_t *p_pes, int i_stream_id )
{
    char psz_pes[128];                                   /* descriptor buffer */
 
#ifdef DEBUG
    /* PES informations, long (DEBUG) format - this string is maximum 70 bytes
     * long */
    sprintf(psz_pes, "id 0x%x, %d bytes (%d TS): %p %c%c%c%c",
            i_stream_id, p_pes->i_pes_size, p_pes->i_ts_packets,
            p_pes,
            p_pes->b_data_loss ? 'l' : '-', p_pes->b_data_alignment ? 'a' : '-',
            p_pes->b_random_access ? 'r' : '-', p_pes->b_discard_payload ? 'd' : '-' );
#else
    /* PES informations, short format */
    sprintf(psz_pes, "id 0x%x, %d bytes",
            i_stream_id, p_pes->i_pes_size );
#endif
    intf_Msg("gdec: PES %s\n", psz_pes );
}
